# Chapter 19 — The PS/2 keyboard

[← The PIT](18-pit-timer.md) · [Contents](README.md) · [Next: A kernel console →](20-kernel-console.md)

---

## Goal

Turn switch closures into characters. Claim IRQ 1, decode scancode set 1, track modifier state, and
bridge an interrupt handler to a task that might be asleep.

It is stranger than it sounds, because a keyboard does not send characters and never has.

---

## 1. What a keyboard actually sends

A keyboard sends **scancodes**: one number when a key goes down and a different number when it comes
up. The number identifies a *physical switch on the board* and nothing else.

Scancode 30 is "the key below Tab and left of S". On a US layout that is A. On a French AZERTY layout
it is Q. On a Dvorak keyboard it is also A, because Dvorak is remapped in software and the hardware
does not know.

So turning switch positions into text is a software problem with no correct answer, only conventions.
This file implements the simplest useful set: scancode set 1, a US layout, shift, caps lock and
control.

### 1.1 Make and break

```c
    bool released = (scancode & SC_RELEASE) != 0;
    uint8_t code  = scancode & 0x7F;
```

Bit 7 clear means the key went down (a "make" code). Bit 7 set means it came up (a "break" code). So
pressing A sends `0x1E` and releasing it sends `0x9E`.

That gives 128 distinct keys, which was enough in 1981 and stopped being enough in 1986.

### 1.2 The extended prefix

```c
#define SC_EXTENDED   0xE0
```

When the 101-key keyboard arrived with arrow keys, a numeric keypad and right-hand Ctrl and Alt, IBM
had run out of single-byte codes — and could not renumber, because software read one byte.

So the new keys send `0xE0` followed by a scancode that duplicates an existing one. The right Ctrl is
`0xE0 0x1D`; the left Ctrl is `0x1D`. Software that ignores the prefix sees both as left Ctrl, which
is exactly the backwards compatibility IBM wanted.

```c
    if (scancode == SC_EXTENDED) {
        extended = true;
        return;
    }
```

A one-byte state machine. Note it returns without producing anything — the next interrupt carries the
actual code.

> The Pause key sends *six* bytes: `E1 1D 45 E1 9D C5`. It is the only key with an `E1` prefix, it
> sends its make and break codes together, and it cannot be held down. We ignore it, as does almost
> everything.

---

## 2. The keymaps

```c
static const char keymap_normal[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,   '*', 0,   ' ',
};

static const char keymap_shift[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    ...
};
```

Index by scancode, get a character. Zero means "no character" — either an unused scancode or a key
that does not produce text.

Two tables rather than one plus a transformation, because shift changes 47 of them in ways that have
no rule. `1` becomes `!`, `[` becomes `{`, `'` becomes `"`. There is no arithmetic relationship; it is
a lookup either way, and two tables are clearer than one table plus a second table of exceptions.

The layout of the first row — `1 2 3 4 5 6 7 8 9 0 - =` at scancodes 2–13 — reflects the physical
order of keys on the board, which is why the table reads like a picture of a keyboard.

---

## 3. Modifiers are state, not characters

```c
    switch (code) {
    case SC_LSHIFT:
    case SC_RSHIFT:
        if (released) modifiers &= ~MOD_SHIFT; else modifiers |= MOD_SHIFT;
        return;
    case SC_LCTRL:
        if (released) modifiers &= ~MOD_CTRL;  else modifiers |= MOD_CTRL;
        return;
    case SC_LALT:
        if (released) modifiers &= ~MOD_ALT;   else modifiers |= MOD_ALT;
        return;
    case SC_CAPSLOCK:
        if (!released) modifiers ^= MOD_CAPS;
        return;
    default:
        break;
    }
```

Shift, Ctrl and Alt are **held**: set on make, cleared on break.

Caps Lock is a **toggle**: flipped on make, ignored on break.

Treating them the same way is why some hobby kernels have a Caps Lock that only works while you hold
it down. It is a two-line bug and it is immediately obvious once you know to look for it.

### 3.1 Caps Lock affects letters only

```c
    if (modifiers & MOD_CAPS) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
```

Caps Lock with Shift held gives lowercase — which is why the transformation is a swap rather than an
uppercase.

And it applies to letters only. Caps Lock does *not* turn `1` into `!`; that is Shift's job. This is
why Caps Lock cannot be implemented by simply setting the shift flag, which is the shortcut that
produces a Caps Lock that types `!!!` when you meant `111`.

### 3.2 Ctrl and the ASCII table

```c
    if (modifiers & MOD_CTRL) {
        if (c >= 'a' && c <= 'z')      c = (char)(c - 'a' + 1);
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 1);
        else if (c == '[')             c = 27;
    }
```

Ctrl-A is 1, Ctrl-C is 3, Ctrl-D is 4, Ctrl-U is 21.

That mapping is not a convention invented by terminals — **it is the ASCII table**. The control
characters occupy code points 0–31, and each one shares its low five bits with the letter it is named
after:

```
    'A' = 0x41 = 1000001
    ^A  = 0x01 = 0000001      <- bits 6 and 7 cleared
```

So "Ctrl" literally means "clear the top two bits", which is why Ctrl-[ is escape (`[` is `0x5B`,
clearing bits 6–7 gives `0x1B`) and why Ctrl-Space is NUL on some terminals.

Knowing this makes the whole control-character set memorable instead of arbitrary.

---

## 4. Reading port 0x60, and the interrupt that never comes back

```c
static void keyboard_callback(registers_t *regs UNUSED)
{
    uint8_t scancode = inb(PS2_DATA);
    ...
}
```

One line, and it is the most important line in the driver.

**Reading port `0x60` is what clears the interrupt at the keyboard controller.** If the handler
returns without reading it, IRQ 1 never fires again — the controller will not raise the line while it
still has an unread byte.

The symptom is that the keyboard appears to die after exactly one keystroke, which is the classic
sign of a handler that returned early on some path.

Which is why the read is unconditionally first, before any of the branching.

### 4.1 Draining the buffer at init

```c
void keyboard_init(void)
{
    while (inb(PS2_STATUS) & 0x01)
        (void)inb(PS2_DATA);
    ...
}
```

Same problem, at boot. The BIOS used the keyboard, and it may have left a byte in the controller's
output buffer. If one is sitting there when we unmask IRQ 1, no new interrupt will ever be generated
and the keyboard is dead before it starts.

Bit 0 of the status port at `0x64` means "output buffer full". Loop until it is clear.

---

## 5. The ring buffer

```c
#define KBD_BUFFER_SIZE 128

static volatile int      kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;
```

The producer is an interrupt handler that cannot block. The consumer may be a task that is asleep. A
fixed-size ring buffer is the standard bridge: the producer writes and advances `head`, the consumer
reads and advances `tail`, and neither waits for the other.

```c
static void kbd_push(int key)
{
    uint32_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next == kbd_tail) return;
    kbd_buffer[kbd_head] = key;
    kbd_head = next;
}
```

### 5.1 Empty versus full

`head == tail` means empty. Full is `(head + 1) % size == tail`, which wastes one slot — 127 usable
entries in a 128-entry array.

The alternative is a separate count, which costs a variable and an increment/decrement that must stay
in step with two indices. The wasted slot is the cheaper answer and it is what nearly every ring
buffer does.

### 5.2 Which end to drop

```c
    if (next == kbd_tail) return;
```

When the buffer is full we drop the **new** key, not the oldest.

Dropping the newest loses the character you just typed. Dropping the oldest loses one you typed
earlier and *already saw echoed*, which is worse — the screen and the buffer disagree.

Either way, a full 128-entry keyboard buffer means something is very wrong: nobody has read a key in
a long time, which usually means the reader is blocked or dead.

### 5.3 The concurrency argument

On one CPU, with the producer in an interrupt handler:

- The producer only writes `head` and `kbd_buffer[head]`.
- The consumer only writes `tail`.
- The consumer disables interrupts while it touches the indices.

```c
int keyboard_getkey_nonblock(void)
{
    uint32_t flags = irq_save();

    int key = -1;
    if (kbd_tail != kbd_head) {
        key = kbd_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    }

    irq_restore(flags);
    return key;
}
```

That is sufficient. There is no other writer to race with, and the interrupt cannot preempt a section
in which interrupts are disabled.

On an SMP machine this would need memory barriers and careful ordering, because the producer could be
running on another core simultaneously. Chapter 48.

---

## 6. Blocking

```c
int keyboard_getkey(void)
{
    for (;;) {
        int key = keyboard_getkey_nonblock();
        if (key >= 0) return key;

        sched_block((void *)&kbd_buffer);
    }
}
```

and on the producing side:

```c
    sched_wake((void *)&kbd_buffer);
```

The address of the buffer is used as a **wait channel** — a token compared for equality, with nothing
stored at it. Chapter 35 covers the mechanism; the thing to notice here is that using the address of
the object you are waiting for makes the pairing between sleeper and waker impossible to get wrong.

### 6.1 Why a loop and not an `if`

`sched_wake` makes every blocked task runnable. Between being made runnable and actually running,
another task may have taken the key.

So the woken task must **re-check** rather than assume. `while`, never `if`. This is the same rule as
Chapter 36's semaphores and it is the single most common concurrency bug in kernel code.

### 6.2 Waking from an interrupt handler

```c
    sched_wake((void *)&kbd_buffer);
```

is called from `keyboard_callback`, which is interrupt context.

That is safe because `sched_wake` only moves tasks onto the run queue — it changes a state field and
returns. Actually *switching* to the woken task from here would not be safe, for the reasons
Chapter 31 gives.

The distinction is worth internalising: **waking is safe from an interrupt, switching is not.**

---

## 7. Extended keys

```c
    if (extended) {
        extended = false;
        if (!released) {
            switch (code) {
            case 0x48: kbd_push(KEY_UP);       return;
            case 0x50: kbd_push(KEY_DOWN);     return;
            case 0x4B: kbd_push(KEY_LEFT);     return;
            case 0x4D: kbd_push(KEY_RIGHT);    return;
            ...
            }
        }
        if (code == SC_LCTRL) modifiers &= ~MOD_CTRL;
        if (code == SC_LALT)  modifiers &= ~MOD_ALT;
        return;
    }
```

Non-character keys get codes above `0xFF`:

```c
#define KEY_ESCAPE    0x100
#define KEY_UP        0x101
#define KEY_DOWN      0x102
...
#define KEY_F1        0x110   /* F1..F12 are 0x110..0x11B */
```

so that a single `int` can carry either "the letter a" or "the up arrow" without a second channel.
The buffer is `int`, not `char`, for exactly this reason.

The console (Chapter 20) drops anything above `0xFF`:

```c
    if (key > 0xFF) return;
```

A real terminal turns them into escape sequences — `"\x1b[A"` for up — and then the shell's line
editor has to *parse* escape sequences, which is a surprising amount of work for an up arrow. Chapter
20's exercises add it.

---

## 8. The 8042 controller

Two ports:

| Port | Read | Write |
|---|---|---|
| `0x60` | The data byte | A byte to the keyboard |
| `0x64` | Status | A command to the controller |

Status bits:

- **Bit 0** — output buffer full: the controller has a byte for us.
- **Bit 1** — input buffer full: the controller is still reading the last byte we wrote.

We met this chip in Chapter 7, §1.4, driving the A20 gate, because in 1984 it was the only chip on
the board with a spare output pin. The same chip also carries the CPU reset line, which is how
`sys_reboot` works:

```c
static int32_t sys_reboot(registers_t *regs UNUSED)
{
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;) hlt();
}
```

Command `0xFE` pulses the reset line. Wait for bit 1 to clear first, so the controller is ready to
accept the command.

On a modern machine there is no 8042 — the keyboard is USB and the chipset emulates a PS/2 controller
for compatibility. The emulation includes the reset line, because too much software uses it.

---

## 9. What we do not do

**LEDs.** Caps Lock does not light up. Setting it means sending command `0xED` to the keyboard
(not the controller) followed by a bitmask, and waiting for an `0xFA` acknowledgement. About twenty
lines, and Exercise 19.5.

**Typematic repeat.** Holding a key does not repeat it. The keyboard has a built-in repeat rate,
configurable with command `0xF3` — and QEMU does implement it, so held keys *do* repeat. On real
hardware you would set the rate rather than implement repeat in software.

**Scancode set 2.** The PS/2 default is set 2, and the 8042 translates it to set 1 for
compatibility. Every PC boots with translation on. Turning it off (controller command byte bit 6)
gets you set 2, which is more regular — break codes are `0xF0` followed by the make code, rather than
bit 7 — and which nothing expects.

**Other layouts.** The keymap is US. A UK layout differs in about eight keys; a French one in
thirty. Making it switchable is a table pointer and a syscall.

---

## 10. Running it

```c
    keyboard_init();
    ...
    sti();

    kprintf("type something:\n");
    for (;;) {
        int k = keyboard_getkey();
        if (k < 0x100) kprintf("%c", (char)k);
        else           kprintf("[key %03x]", k);
    }
```

Type, and characters appear. Hold shift, and capitals appear. Press an arrow key and `[key 101]`
appears.

The things to test, because each exercises a different branch:

| Test | Exercises |
|---|---|
| `abc` | The basic path |
| `ABC` with shift held | Modifier state, second keymap |
| Caps Lock, then `abc` | Toggle versus hold, letters-only transformation |
| Caps Lock + Shift + `a` | The swap, not the uppercase |
| Ctrl-C | The ASCII bit-clearing |
| Arrow keys | The `0xE0` state machine |
| Holding a key | Typematic repeat from the hardware |
| `!@#$` | The shift keymap's punctuation row |

---

## 11. What could go wrong

| Symptom | Cause |
|---|---|
| Nothing at all | IRQ 1 masked, or the handler not registered |
| Exactly one keystroke, then dead | Port `0x60` not read on some path |
| Dead from boot | BIOS left a byte in the buffer; the init drain is missing |
| Shift is stuck on | Break codes not handled — check bit 7 |
| Caps Lock only works while held | Treated as a hold rather than a toggle |
| Caps Lock types `!` instead of `1` | Implemented by setting the shift flag |
| Arrow keys produce letters | `0xE0` prefix ignored |
| Characters appear in the wrong order | Head/tail confusion in the ring buffer |
| Input is lost under load | Buffer full — check who is not reading |

---

## 12. Exercises

🟢 **19.1** Print the raw scancode for every key instead of decoding. Press a few keys and record
make and break codes. Confirm the bit-7 rule.

🟢 **19.2** Make Caps Lock a hold rather than a toggle and observe the bug. Then make it set the
shift flag and type `1234`.

🟢 **19.3** Print `modifiers` as binary on every key. Watch shift set and clear.

🟡 **19.4** Add a UK layout: `"` and `@` swapped, `#` where `\` is, and a `£` on Shift-3 (code page
437 has it at `0x9C`). Add a key combination to switch layouts at runtime.

🟡 **19.5** Light the Caps Lock LED. Send `0xED` to port `0x60`, wait for `0xFA`, then send the
bitmask (bit 0 scroll, bit 1 num, bit 2 caps). Handle the acknowledgement in the interrupt handler —
which means the handler now has a small state machine.

🟡 **19.6** Set the typematic rate with command `0xF3`. Try the fastest (30 Hz, 250 ms delay) and the
slowest.

🟡 **19.7** Make the ring buffer's full condition use a count instead of wasting a slot, and get the
interrupt-safety right. Then argue about which version you prefer.

🔴 **19.8** Turn off scancode translation in the controller command byte and decode set 2 directly.
Break codes become `0xF0` + make code. Note how much more regular it is, and how nothing else in the
world expects it.

🔴 **19.9** Add the PS/2 mouse on IRQ 12. It shares the controller, needs command `0xA8` to enable
the second port, and sends three-byte packets. Draw a cursor on the text screen by inverting the
attribute byte of one cell.

---

## What we covered

- A keyboard sends switch positions, not characters, and the translation has no correct answer.
- Make and break codes, the 128-key limit, and the `0xE0` prefix that got round it without breaking
  software that read one byte.
- Two keymaps rather than one plus rules, because shift's punctuation changes follow no pattern.
- Modifiers as state: held for shift/ctrl/alt, toggled for caps — and the two bugs from conflating
  them.
- Ctrl as "clear the top two bits", which makes the control-character set memorable rather than
  arbitrary.
- Reading port `0x60` as the thing that clears the interrupt, and the boot-time drain that prevents a
  keyboard dead on arrival.
- A ring buffer between an interrupt and a sleeping task: empty versus full, which end to drop, and
  why one CPU plus `irq_save` is sufficient.
- Blocking on a wait channel, re-checking in a loop, and the rule that waking is interrupt-safe while
  switching is not.
- Non-character keys above `0xFF`, and why the buffer is `int`.
- The 8042 as the chip that also carries A20 and the reset line.

[Chapter 20](20-kernel-console.md) puts a line discipline on top: echo, line buffering, and the first
interactive prompt.

---

[← The PIT](18-pit-timer.md) · [Contents](README.md) · [Next: A kernel console →](20-kernel-console.md)
