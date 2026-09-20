# Chapter 0 — Introduction: what an operating system actually is

[Contents](README.md) · [Next: Setting up the toolchain →](01-setup.md)

---

## The question this book answers

Open a terminal and type `ls`. Something finds a program on a disk, makes a copy of your shell,
replaces that copy with the program, gives it a slice of memory nobody else can see, lets it read a
directory it does not have the hardware access to read, catches it when it finishes, and prints its
output to a screen it never touched directly.

Every one of those verbs is a piece of code. This book is about writing that code.

Not about using it, not about configuring it, and not about the theory of it in the abstract. We are
going to write a bootloader that the BIOS will execute, an interrupt handler the CPU will jump to, a
page table the memory management unit will walk, a scheduler that will take the processor away from a
running program against its will, and a filesystem driver that will turn magnetic domains into a byte
stream. At the end there will be a prompt on the screen, and when you type `ls` at it, every one of
those verbs will be something you wrote.

---

## 1. What is an operating system?

The honest answer is that "operating system" means two different things and people slide between them
constantly.

**The kernel** is the program that runs in the CPU's most privileged mode. It is the only code on the
machine allowed to touch the page tables, the interrupt table, the I/O ports and the privileged
instructions. Everything else — your shell, your compiler, your browser — runs in a restricted mode
and must ask the kernel for anything interesting. On Linux the kernel is about 30 million lines; the
part you would recognise as "the operating system proper" is maybe two.

**The operating system** in the broader sense is the kernel plus everything shipped around it: the C
library, the shell, the utilities, the init system, the package manager, the window system. Most of
that is ordinary software with no special privileges at all. `ls` is a 140 KB program that anyone
could write in an afternoon; there is nothing systemic about it.

We build both, but the kernel is where the difficulty is, and it is where the interesting ideas are.
Our userland exists mainly to prove that the kernel works — a kernel with no programs to run is a
kernel you cannot tell is broken.

### The kernel's job, in one sentence

A kernel exists to **share one machine between programs that do not trust each other and do not know
about each other**.

Everything follows from that sentence. If there were only ever one program, and it were perfectly
written, you would not need a kernel — you would compile the program against the hardware and boot
it, which is exactly what an embedded system does and exactly what Spark, our first OS, is. The
moment you want two programs, four questions appear, and a kernel is the answer to all four:

| Resource | The question | Our answer | Chapter |
|---|---|---|---|
| The CPU | Who runs, and for how long? | A timer interrupt and a scheduler | 30–31 |
| Memory | Who can see which bytes? | Page tables and the MMU | 21–28 |
| Devices | Who talks to the disk, and in what order? | Drivers behind a uniform interface | 12–19, 37 |
| Everything | How does a program ask, without being trusted? | System calls, and a privilege boundary | 32–33 |

Those four rows are the shape of the whole book. Parts II through V are one row each.

---

## 2. Four ideas that do all the work

Before any code, four mechanisms are worth understanding as ideas, because every chapter after this
one is an implementation detail of one of them.

### 2.1 Privilege levels: the wall

The x86 CPU has a two-bit field in a register that says how privileged the currently running code is.
Level 0 can do anything. Level 3 cannot execute privileged instructions (`cli`, `lgdt`, `in`, `out`,
writes to control registers), cannot touch pages marked kernel-only, and cannot change its own
privilege level.

That last clause is the important one. There is **no instruction** that raises your privilege.
Not one. The only way for ring 3 code to end up running ring 0 code is to trigger an *interrupt* —
and the kernel, not the program, chose in advance which address every interrupt vector jumps to.

This is the entire security model of every mainstream operating system, and it is enforced by
transistors rather than by code. It is worth pausing on how strong that is: a user program cannot
escalate privilege by being clever, only by finding a bug in the handful of entry points the kernel
published. Chapter 32 builds the wall; Chapter 33 builds the door.

### 2.2 Interrupts: the kernel is event-driven

A kernel does not run in a loop. Most of the time it is not running at all — a user program is. The
kernel wakes up when something happens:

- A device wants attention (a key was pressed, a disk read finished) — a **hardware interrupt**.
- The running program did something impossible (divided by zero, touched an unmapped page) — an
  **exception**.
- The running program asked for something (`int 0x80`) — a **software interrupt**, also called a trap.

All three arrive through the same mechanism: the CPU saves a little state, looks up a table the kernel
installed, and jumps. This is why Chapters 16 and 17 come so early — until interrupts work, the
kernel has no way to *be* an operating system. It can only be a program.

The mental model to build: **a kernel is a pile of interrupt handlers with some data structures
between them.**

### 2.3 Virtual memory: the useful lie

Every address your program uses is fake. When a program loads a value from `0x08049000`, the CPU
hands that number to the memory management unit, which walks a tree of tables the kernel built,
finds the real physical address, and fetches from there instead.

This one indirection buys an implausible number of things at once:

- **Isolation.** Two processes both use address `0x08049000` and get different memory, because their
  page tables differ. Neither can name the other's memory, because there is no number they could
  write down that would reach it.
- **The illusion of a big machine.** A program can be linked as though it owns 4 GiB starting at a
  fixed address, on a machine with 128 MiB and eleven other programs running.
- **Permissions per page.** Code can be read-only and data non-writable, so a bug that scribbles over
  a function pointer faults instead of succeeding.
- **Tricks.** Copy-on-write makes `fork()` nearly free. Demand paging means a 200 MB binary starts
  instantly because only the pages that are touched are ever read. Memory-mapped files make a file
  look like an array.

Part III is seven chapters because this is where the difficulty concentrates, and because everything
in Part IV is impossible without it.

### 2.4 The uniform interface: everything is a file

Unix's best idea. A directory on a FAT16 partition, the keyboard, the write end of a pipe, and a
regular file all present the same seven operations. Programs are written against that interface, so
`cat` works on all of them and contains no conditional distinguishing them.

The cost is a layer of indirection — a struct full of function pointers — and the benefit is that
adding a filesystem does not require changing a single program. Chapter 39 builds it, and by
Chapter 43 you will have written a pipe that `cat` reads from without `cat` knowing.

---

## 3. What we build

### Spark, the basic OS — Part I, six chapters

About 600 lines, no features, and the most important thing in the book.

The BIOS reads 512 bytes from the first sector of the disk into memory at `0x7C00` and jumps to
them. Those 512 bytes are yours. From there, Spark:

1. loads a second stage off the disk, because 512 bytes is not enough room to do anything;
2. opens the A20 gate, a compatibility hack from 1984 that otherwise makes memory above 1 MiB
   invisible;
3. builds a Global Descriptor Table and sets one bit of `CR0`, which switches the CPU from the
   16-bit machine it boots as into the 32-bit machine C expects;
4. copies a kernel to 1 MiB and jumps into C code;
5. prints to the screen by storing bytes at `0xB8000`.

That is it. It cannot read input, it has no interrupts, it cannot run a program. But after writing it,
nothing about booting a computer is mysterious to you, and that turns out to be the difference
between following a tutorial and understanding a system.

### Nimbus, the medium OS — Parts II–VI, thirty-six chapters

About 9,000 lines. A real, if small, kernel. From the [README](../README.md):

```
nimbus> ls /bin
sh  ls  cat  echo  hexdump
sleep  true  false  forktest
nimbus> forktest
parent pid is 4
parent 4: 0
  child  5: 0
parent 4: 1
  child  5: 1
...
child 5 exited with 7
nimbus> echo hello > /mnt/greeting.txt
nimbus> cat /mnt/greeting.txt
hello
nimbus> ls /bin | hexdump | cat
00000000  73 68 20 20 6C 73 20 20  63 61 74 20 20 65 63 68  |sh  ls  cat  ech|
```

Reading that transcript as a list of things that had to work:

- **`forktest`** proves preemptive multitasking: two processes interleave without either yielding,
  because a timer interrupt takes the CPU away from them (Chapters 18, 30, 31).
- **`fork` returning twice** is real, and Chapter 34 shows the exact stack manipulation that makes
  one call produce two returns in two address spaces.
- **`echo hello > /mnt/greeting.txt`** is a fork, a `dup2`, an `exec`, a FAT16 cluster allocation and
  a directory entry update (Chapters 34, 42, 43, 46).
- **`ls /bin | hexdump | cat`** is three processes, two pipes, and six file descriptors, none of
  which knows it is not talking to a file (Chapter 43).

---

## 4. How this book is organised, and how to read it

Every chapter has the same four parts.

**Theory first.** What is the hardware doing, why does this mechanism exist, and what did people do
before it? The history matters more than you would expect: half of the x86's strangeness is
backwards compatibility with a decision made in 1978, and knowing *which* half means you stop looking
for a reason that is not there.

**The code, in blocks.** Every block of code in the repository appears in the book, with an
explanation of what each line does, why it is written that way, and what breaks if you change it.
Where there was a real choice, the book names the alternative and says what it would cost.

**Run it.** The repository is arranged so that it builds and boots at the end of *every* chapter.
You are never looking at half a subsystem. Each chapter ends with the exact command and a description
of what you should see.

**Exercises**, graded: 🟢 five minutes, 🟡 an evening, 🔴 a feature the book deliberately left out.

### The two paths through the book

**If you are new to systems programming**, read it in order and type the code rather than copying it.
The typing matters — the bugs you introduce are the ones that teach you what each line was for. Do
not skip Part 0, and do not skip Chapter 47 (debugging), which is worth skimming early because you
will need it during Part I.

**If you have written a kernel before**, Parts 0 and I will be familiar; start at Chapter 15 and use
the [line-by-line](line-by-line/) references rather than the narrative. The chapters most likely to
contain something you have not seen are 25 (the higher half, and why `boot.asm` is split into two
linker sections), 30 (the context switch, instruction by instruction), 34 (how `fork` returns twice),
and 36 (the races we actually have on a uniprocessor).

---

## 5. What is deliberately not here

Being clear about this up front is a courtesy, and it also tells you what the shape of a *real*
kernel is.

| Not here | Why | Where it is discussed |
|---|---|---|
| x86-64 / long mode | Four levels of page tables and a different calling convention, for no new ideas | Ch. 48 |
| SMP (multiple CPUs) | Needs the APIC, ACPI table parsing, and every data structure re-examined | Ch. 48 |
| A network stack | An entire second book. Genuinely. | Ch. 48 |
| USB | The most complicated interface on a modern PC by a wide margin | Ch. 48 |
| A journaling filesystem | FAT16 teaches the ideas; journalling is a separate large topic | Ch. 42 |
| Signals | Real, and the interaction with the scheduler is subtle | Ch. 46 |
| Dynamic linking | `.so` files, PLT, GOT, a dynamic loader in userland | Ch. 44 |
| Security beyond rings | No users, no permissions, no capabilities | Ch. 48 |

And one that deserves more than a table row: **Nimbus has no security model beyond the ring 3
boundary.** There is one user, and it is root. That is fine for a teaching kernel and would be
catastrophic in anything else, and Chapter 33 is careful to point out which of its checks are the
ones that would actually matter.

---

## 6. What you need to know already

**C.** You should be comfortable with pointers, structs, function pointers and casts. You do not
need to be an expert; you do need `uint8_t *p = (uint8_t *)0xB8000; p[0] = 'A';` to read as an
ordinary statement rather than a magic incantation.

**Assembly: no.** Chapter 3 teaches the roughly forty x86 instructions this book uses. If you have
never written any, that chapter is enough; if you have, skim it for the NASM syntax conventions.

**Hardware: no.** Chapter 2 covers everything about the machine that we rely on.

What you *do* need is a tolerance for a specific kind of failure. In application programming, a
mistake produces an exception with a stack trace. In kernel programming, a mistake produces a machine
that reboots instantly, with no output, forever. There is no runtime to catch you, no message, and
frequently no relationship between where the fault appears and where the bug is.

That is not a reason to be intimidated — it is a reason to build good habits early, which is why
serial logging (Chapter 13) comes before almost everything else, and why Chapter 47 exists.

---

## 7. A note on the two big lies

Two things in this book are described as simpler than they are, and it is worth knowing which.

**"The BIOS loads the boot sector."** On any machine made after about 2012 the firmware is UEFI, not
BIOS, and UEFI does something quite different: it reads a FAT32 partition, finds a PE executable, and
calls it with a table of services. Our code runs because UEFI firmware almost always includes a
compatibility support module that emulates the BIOS, and because QEMU defaults to a real BIOS
(SeaBIOS). Chapter 4 explains the difference honestly and says what a UEFI version of Chapter 5 would
look like.

**"The CPU executes instructions."** It does not, really. It decodes them into micro-operations,
reorders them, executes several at once, speculates past branches, and retires them in order to
maintain the illusion. Almost everywhere this does not matter. It matters in exactly two places in
this book — the `jmp` after setting `CR0.PE` (Chapter 7) and TLB invalidation (Chapter 24) — and the
book flags both.

---

## 8. Before you start

Three practical things.

**Use QEMU, not real hardware, until the very end.** QEMU boots in a second, can be stopped and
inspected with GDB, logs every interrupt on request, and cannot brick anything. Real hardware takes
thirty seconds per attempt and tells you nothing. Chapter 47 covers running on a real machine, and
Chapter 48 lists the ways in which real hardware is less forgiving.

**Set up serial logging on day one.** It is Chapter 13, and it is tempting to skip because the screen
already works. Do not. A `kprintf` that survives a crash is worth more than any other debugging
tool you will have.

**Commit after every chapter.** When the kernel stops booting — and it will, and the symptom will be
a blank screen with no clue whatsoever — `git diff` against a version that worked is frequently the
fastest path to the answer. It is not a substitute for understanding, but at 1 a.m. it is a
substitute for despair.

---

## Where we are going next

[Chapter 1](01-setup.md) installs three tools: NASM, an i686-elf cross-compiler, and QEMU. It
explains at some length *why* the cross-compiler is not optional — the answer is more interesting
than it sounds, and it is the first real lesson about what "freestanding" means. By the end of it
you will have booted a five-line assembly program on an emulated PC and watched it print a character
to a screen, with nothing between your code and the hardware.

Then we start writing an operating system.

---

[Contents](README.md) · [Next: Setting up the toolchain →](01-setup.md)
