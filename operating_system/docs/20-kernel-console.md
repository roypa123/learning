# Chapter 20 — A kernel console

[← The keyboard](19-keyboard.md) · [Contents](README.md) · [Next: What memory is there? →](21-memory-map.md)

---

## Goal

Build the layer between "a keyboard produces keys" and "a program calls `read()` and gets a line".
Echo, line buffering, backspace, and a `/dev/console` node that makes the console a file.

This finishes Part II. At the end of it the kernel has everything it needs to *observe* itself, which
is what makes Part III survivable.

---

## 1. The layer nobody expects to exist

On Unix this is the **tty line discipline**, and it is famously the most confusing part of the
system. It owns three behaviours that feel like they belong somewhere else and do not.

### 1.1 Echo

The characters you type appear on screen because the **kernel** prints them.

Not the keyboard — that produces scancodes. Not the program — it is blocked in `read()` and has not
seen them yet. The console layer receives each key, puts it in a buffer, *and* prints it.

That is why a password prompt has to explicitly ask the kernel to stop echoing, and why the request
is a terminal setting rather than a printing choice:

```c
static bool echo = true;
```

One flag. Turning it off is what `ECHO` in `termios` does.

### 1.2 Line buffering

`read()` does not return until Enter.

The reason is backspace. Once a character has been handed to the program it cannot be taken back, so
if backspace is to work at all, the kernel must hold the line until the user commits to it.

Everything else about cooked-mode terminals follows from that one constraint. It is also why a
program that wants character-at-a-time input — a text editor, a game — has to ask for raw mode, and
why doing so means implementing backspace itself.

### 1.3 Editing

Backspace and Ctrl-U operate on the buffer that has not been delivered yet, so they have to be here.
There is nowhere else they could be.

Our line discipline is about 150 lines, which is roughly what it takes to do the job honestly.

---

## 2. The buffer and the channel

```c
static char     line[CONSOLE_LINE_MAX];
static size_t   line_len   = 0;
static bool     line_ready = false;
static bool     echo       = true;

static int console_wait_channel;
```

One line buffer, one length, one flag, one wait channel.

`console_wait_channel` is an `int` that is never read or written. Its **address** is the token
(Chapter 19, §6). Any unique address would do; a dedicated variable makes the intent obvious and
costs four bytes.

Note what is not here: no per-process buffers, no multiple terminals, no `struct tty`. One console,
one reader at a time. That is a real limitation — two tasks reading the console simultaneously get
arbitrary halves of each line — and it is fine for a kernel with one terminal.

---

## 3. `console_input`: interrupt context

```c
void console_input(int key)
{
    if (key > 0xFF) return;

    char c = (char)key;

    switch (c) {
    case '\n':
    case '\r':
        if (line_len < CONSOLE_LINE_MAX - 1)
            line[line_len++] = '\n';
        line[line_len] = '\0';

        if (echo) console_putc('\n');

        line_ready = true;
        sched_wake(&console_wait_channel);
        break;
    ...
```

Called from the keyboard IRQ, once per key.

**This runs in interrupt context**, which is a hard constraint rather than a style note:

- It must not block. There is no task to put to sleep.
- It must not call `kmalloc`. The heap's lock might be held by the task we interrupted.
- It must not take any lock that task-context code holds with interrupts enabled.

All it does is edit a fixed buffer and wake a sleeper. That is deliberately the smallest thing that
works.

### 3.1 The newline is kept

```c
        if (line_len < CONSOLE_LINE_MAX - 1)
            line[line_len++] = '\n';
```

The newline goes **into** the buffer.

That is what `read()` on a terminal returns, and programs rely on it — `cat` copies what it reads,
and a `cat` whose input has no newlines produces one long line. Our `getline` in userland strips it,
because *that* is a convenience function whose callers want the line without it:

```c
int getline(char *buf, size_t max)
{
    ssize_t n = read(STDIN_FILENO, buf, max - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';
    if (n > 0 && buf[n - 1] == '\n') buf[--n] = '\0';
    return (int)n;
}
```

Two layers, two conventions, each right for its level. Conflating them is how you end up with a shell
that prints a blank line after every command.

### 3.2 Backspace takes three characters

```c
    case '\b':
    case 0x7F:
        if (line_len > 0) {
            line_len--;
            if (echo) {
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
        }
        break;
```

Chapter 10, §4: backspace **moves**, it does not erase. To visibly delete you send move-left, space,
move-left.

A driver that sends only `\b` produces a cursor that moves but leaves the character visible, which
looks correct until you type over it. This is the single most common visual bug in a first console.

`0x7F` is DEL. Some keyboards and many terminal emulators send it for the backspace key — the
distinction between BS (`0x08`) and DEL (`0x7F`) has been a mess since the 1970s, and the practical
answer is to accept both.

### 3.3 Ctrl-U, and the signal we do not have

```c
    case 21:              /* Ctrl-U: kill the whole line */
        while (line_len > 0) {
            line_len--;
            if (echo) { console_putc('\b'); console_putc(' '); console_putc('\b'); }
        }
        break;

    case 3:               /* Ctrl-C */
        line_len = 0;
        if (echo) console_write("^C\n", 3);
        line_ready = true;
        line[0] = '\0';
        sched_wake(&console_wait_channel);
        break;
```

Ctrl-U erases the line, one character at a time, so the screen stays in step.

Ctrl-C is a compromise and the source says so:

> A real kernel delivers SIGINT to the foreground process group here. We have no signals, so all this
> does is abandon the current line, which at least makes Ctrl-C feel like it does something.

Real Ctrl-C handling needs signals, which need: a pending-signal mask per process, delivery on the
way back to userland, a user-space signal handler frame built on the user stack, and a `sigreturn`
system call to unwind it. Plus the concept of a *foreground process group*, so that Ctrl-C kills the
`cat` and not the shell.

That is a chapter, and Chapter 46, §8 sketches it. Returning an empty line is the honest 10%.

---

## 4. `console_read_line`: task context, and a race

```c
ssize_t console_read_line(char *buf, size_t max)
{
    for (;;) {
        uint32_t flags = irq_save();
        if (line_ready) { irq_restore(flags); break; }
        irq_restore(flags);

        sched_block(&console_wait_channel);
    }

    uint32_t flags = irq_save();

    size_t n = line_len;
    if (n > max - 1) n = max - 1;
    memcpy(buf, line, n);
    buf[n] = '\0';

    line_len   = 0;
    line_ready = false;

    irq_restore(flags);
    return (ssize_t)n;
}
```

### 4.1 The lost wakeup

The flag is set by an interrupt handler, so it must be tested carefully. The classic race:

```
    task:       test line_ready        -> false
    interrupt:                            set line_ready, call sched_wake
    task:       sched_block(...)       -> sleeps forever
```

The wakeup arrived *between* the test and the sleep, and there is nobody left to deliver it again.

Disabling interrupts across the test-and-sleep closes it — as long as `sched_block` re-enables
interrupts *as part of switching away*, which it does, because `schedule()` restores the flags of the
task it switches *to*.

This is a genuinely subtle point and it is worth stating precisely: the window is closed not because
interrupts are off during the sleep, but because the act of sleeping is what re-enables them.

> ⚠️ **The loop matters too.** `sched_wake` wakes everyone blocked on the channel. By the time this
> task runs, another may have consumed the line. `while`, never `if` — the same rule as Chapter 19,
> §6.1 and Chapter 36.

### 4.2 Copying under `irq_save`

The second critical section protects the copy. Without it, a keystroke arriving mid-`memcpy` could
append to `line` while we are reading it, and the length we captured would no longer match.

It is short — a `memcpy` of at most 256 bytes — which is the right shape for an interrupt-disabled
section. Chapter 36 has the rule: hold it for a handful of instructions, never across anything that
might block.

---

## 5. Making the console a file

```c
static ssize_t console_node_read(vfs_node_t *node UNUSED, off_t offset UNUSED,
                                 size_t size, uint8_t *buf)
{
    return console_read_line((char *)buf, size);
}

static ssize_t console_node_write(vfs_node_t *node UNUSED, off_t offset UNUSED,
                                  size_t size, const uint8_t *buf)
{
    console_write((const char *)buf, size);
    return (ssize_t)size;
}
```

Two adapters, and the console becomes an ordinary VFS node.

Once this exists, the shell needs no special case. Its stdin is fd 0; fd 0 is an open file; the open
file points at this node; `read()` on it calls `console_node_read`. Exactly the same code path as
reading a file off the disk.

**That uniformity is the entire argument for a VFS**, and this is the first place it pays. Chapter 39
builds the layer properly; Chapter 43 uses it for pipes; and at no point does `cat` learn what it is
reading from.

### 5.1 `offset` is ignored

Both functions take an `offset` and ignore it. A console is not seekable — there is no "byte 100 of
the keyboard".

The syscall layer knows:

```c
    ssize_t n = vfs_read(f->node, f->offset, len, (uint8_t *)buf);
    if (n > 0 && !(f->node->flags & VFS_CHARDEVICE)) f->offset += (off_t)n;
```

Character devices do not advance the file offset. Without that check, reading 10 bytes from the
console would move the offset to 10, and the next read would pass 10 as an offset that the console
ignores — harmless, but the offset would grow forever and `lseek` would report nonsense.

---

## 6. Running it

```c
    console_init();
    ...
    sti();

    char line[128];
    for (;;) {
        kprintf("nimbus> ");
        ssize_t n = console_read_line(line, sizeof(line));
        kprintf("you typed %d bytes: %s", (int)n, line);
    }
```

```
nimbus> hello world
you typed 12 bytes: hello world
nimbus> abc<backspace><backspace>xyz
you typed 5 bytes: axyz
nimbus>
```

The first interactive prompt. Type, backspace, press Enter, and the kernel reads a line.

### 6.1 A first shell, in twenty lines

Worth doing before Chapter 46, because it exercises everything Part II built:

```c
static void mini_shell(void)
{
    char line[128];

    for (;;) {
        kprintf("nimbus> ");
        console_read_line(line, sizeof(line));

        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (strcmp(line, "help") == 0) {
            kprintf("help mem uptime clear panic\n");
        } else if (strcmp(line, "mem") == 0) {
            pmm_dump_stats();
            heap_dump();
        } else if (strcmp(line, "uptime") == 0) {
            uint64_t ms = timer_ms();
            kprintf("up %u.%03u s\n", (uint32_t)(ms / 1000), (uint32_t)(ms % 1000));
        } else if (strcmp(line, "clear") == 0) {
            vga_clear();
        } else if (strcmp(line, "panic") == 0) {
            panic("requested by the user");
        } else if (line[0]) {
            kprintf("unknown: %s\n", line);
        }
    }
}
```

That is a working kernel debugger. `mem` after every allocation-heavy operation in Part III will tell
you immediately whether something is leaking, and it costs twenty lines you were going to want
anyway.

---

## 7. What is missing, and what each would cost

**Line editing beyond backspace.** Left and right arrows, insert mode, Home and End. The arrow keys
already arrive (Chapter 19, §7) — the console drops them:

```c
    if (key > 0xFF) return;
```

Handling them means tracking a cursor position within the line, redrawing the tail on every insert,
and moving the hardware cursor separately from the character cursor. About eighty lines, and
Exercise 20.4.

**History.** A ring of previous lines, recalled with up and down. Another forty lines on top of
editing, and genuinely pleasant to have.

**Tab completion.** Needs the VFS (Chapter 39) to list candidates, and needs the console to know
about the shell's notion of a current word — which is why real systems put completion in the shell,
in raw mode, rather than in the kernel.

**Multiple consoles.** Alt-F1 through Alt-F6. The VGA buffer is 32 KiB and a screen is 4000 bytes, so
six virtual consoles fit in hardware (Chapter 12, §5.1). Each needs its own line buffer, cursor
position and foreground process. Exercise 20.7.

**A real `termios`.** Raw versus cooked mode, `VMIN`/`VTIME`, per-terminal settings, and an `ioctl`
to change them. This is the thing that makes a text editor possible, and it is the point at which the
line discipline stops being 150 lines.

---

## 8. Part II, in retrospect

Ten chapters, and the kernel can now:

| | |
|---|---|
| Be loaded | Multiboot, no bootloader of our own (11) |
| Print | VGA with scrolling and a cursor (12) |
| Log | Serial, survives a crash (13) |
| Format | One `printf` for four sinks (14) |
| Describe itself | GDT with ring 3 segments and a TSS (15) |
| Catch its own mistakes | IDT, 32 exceptions, a register dump (16) |
| Hear the hardware | PIC remapped, 16 IRQ lines, EOI (17) |
| Keep time | PIT at 100 Hz, uptime, preemption flag (18) |
| Read input | PS/2, scancodes, modifiers, a ring buffer (19) |
| Hold a conversation | Echo, line buffering, `/dev/console` (20) |

None of it manages memory, runs a program, or reads a disk. What it does is **observe itself**, which
is exactly what Part III needs — because Part III is where hobby kernels die, and the difference
between debugging it and guessing at it is the serial log and the `mem` command you now have.

---

## 9. Exercises

🟢 **20.1** Add a `Ctrl-L` that clears the screen and redraws the prompt and the partial line.

🟢 **20.2** Turn off echo, type a line, and confirm it still reads correctly. This is `getpass`.

🟢 **20.3** Remove the `console_putc(' ')` from the backspace sequence and watch what happens when
you backspace and retype.

🟡 **20.4** Implement left and right arrows. Track a cursor index within the line, insert at that
position, redraw the tail, and reposition the hardware cursor. Then handle Home and End.

🟡 **20.5** Add a history ring of eight lines, recalled with up and down. Note that this needs
Exercise 20.4 first, because recalling a line means redrawing one.

🟡 **20.6** Find the lost-wakeup race by deleting the `irq_save` around the test in
`console_read_line`, then add a `timer_spin_ms(1)` between the test and the `sched_block` to widen
the window. Confirm it hangs.

🔴 **20.7** Implement six virtual consoles switched with Alt-F1..F6. Each gets a 4000-byte region of
the VGA buffer, its own cursor and its own line buffer. Use the CRTC start address register to
switch.

🔴 **20.8** Implement raw mode: a flag that makes `read()` return as soon as any character is
available, with no echo and no line editing. Then write a simple full-screen editor as a userland
program once Part VI exists.

---

## What we covered

- The line discipline as a real layer, owning three behaviours that appear to belong elsewhere: echo,
  line buffering, and editing.
- Why `read()` cannot return before Enter, and why every property of cooked-mode terminals follows
  from that.
- Interrupt-context constraints: no blocking, no allocation, no locks held by task context.
- The newline kept in the buffer at the kernel layer and stripped at the libc layer, and why both are
  right.
- Backspace as three characters, and `0x7F` versus `0x08`.
- Ctrl-C without signals, and exactly what signals would cost.
- The lost-wakeup race, and the precise reason disabling interrupts across test-and-sleep closes it.
- Two adapter functions that turn the console into a file, and the character-device check that stops
  the offset growing forever.
- A twenty-line kernel shell that will pay for itself in Part III.

**Part II is finished.** [Chapter 21](21-memory-map.md) starts Part III by asking a question the
kernel has never needed to answer: how much memory is there, and which parts of it are real?

---

[← The keyboard](19-keyboard.md) · [Contents](README.md) · [Next: What memory is there? →](21-memory-map.md)
