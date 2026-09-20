# Chapter 26 — The page fault handler

[← The higher half](25-higher-half.md) · [Contents](README.md) · [Next: The kernel heap →](27-kernel-heap.md)

---

## Goal

Make vector 14 do useful work. Decode the evidence, grow a user stack on demand, distinguish an
expected fault from a bug, and kill the right thing when it is a bug.

This is the chapter where paging stops being a mapping mechanism and starts being a *policy*
mechanism.

---

## 1. A fault is not an error

In a mature kernel, **most page faults are expected**. They are how the kernel finds out that it
needs to do something, and the something is usually routine:

| Fault | What the kernel does |
|---|---|
| Stack grew past its last page | Map another page, retry |
| First touch of a demand-paged executable | Read the page from disk, map it, retry |
| Write to a copy-on-write page | Copy it, mark both writable, retry |
| Access to a memory-mapped file | Read the block, map it, retry |
| Access to a swapped-out page | Read it back from swap, retry |
| Genuine bug | Kill the process, or panic |

Only the last row is an error. The other five are the mechanism by which lazy allocation works, and
they are the reason a 200 MB binary starts instantly and `fork()` is nearly free.

The handler's whole job is telling them apart.

---

## 2. The evidence

```c
void page_fault_handler(registers_t *regs)
{
    uint32_t addr = read_cr2();

    bool present   = (regs->err_code & 0x1) != 0;
    bool write     = (regs->err_code & 0x2) != 0;
    bool user      = (regs->err_code & 0x4) != 0;
    bool reserved  = (regs->err_code & 0x8) != 0;
    bool fetch     = (regs->err_code & 0x10) != 0;
```

### 2.1 `CR2` first, before anything

```c
    uint32_t addr = read_cr2();
```

The very first statement.

`CR2` holds the faulting virtual address and it is **volatile**: a second page fault overwrites it.
So it must be read before anything that could itself fault — which in practice means before any
`kprintf`, because the formatter touches a stack buffer and the console, and either could fault if
things are bad enough.

Reading it into a local is cheap insurance. A handler that calls `kprintf` first and reads `CR2`
second reports the wrong address exactly when the situation is worst.

### 2.2 The five bits

```
    bit 0   0 = page not present        1 = protection violation
    bit 1   0 = read                    1 = write
    bit 2   0 = ring 0                  1 = ring 3
    bit 3       a reserved bit was set in a page table entry
    bit 4       the fault was an instruction fetch
```

The combinations that matter:

| present | write | user | Means |
|---|---|---|---|
| 0 | any | 1 | User touched unmapped memory — stack growth, demand paging, or a wild pointer |
| 0 | any | 0 | **Kernel** touched unmapped memory — almost always a bug |
| 1 | 1 | 1 | User wrote to a read-only page — copy-on-write, or writing to `.text` |
| 1 | 1 | 0 | Kernel wrote to a read-only page — COW on a user's behalf, or a bug |
| 1 | 0 | 1 | User read a kernel page — always a bug |

Bit 3, `reserved`, deserves special mention. It means a 1 was written into a bit the architecture
says must be zero. On plain 32-bit paging that means bits you did not think you were setting, which
almost always means a **corrupted page table** rather than a mapping mistake. If you ever see it,
stop looking at the faulting code and start looking at whatever wrote that table entry.

---

## 3. Growing a stack

```c
    if (user && !present && current_task) {
        vaddr_t bottom = current_task->user_stack_bottom;

        if (addr < bottom && addr + 64 * KiB >= bottom &&
            addr >= USER_STACK_TOP - USER_STACK_SIZE * 8) {

            vaddr_t newpage = ALIGN_DOWN(addr, PAGE_SIZE);

            paging_map(current_directory, newpage, alloc_zeroed_frame(),
                       PTE_WRITABLE | PTE_USER);
            current_task->user_stack_bottom = newpage;

            LOG_DEBUG("stack grew to %08x for pid %d", newpage, current_task->pid);
            return;
        }
    }
```

A user process starts with **one page** of stack (Chapter 32):

```c
    vaddr_t stack_bottom = USER_STACK_TOP - PAGE_SIZE;
    paging_map_range(new_dir, stack_bottom, PAGE_SIZE, PTE_WRITABLE | PTE_USER);
```

When a deep call chain runs off the bottom, the access faults at an address just below the lowest
mapped stack page. The right answer is not "kill it" — it is "map another page and retry".

### 3.1 Why `return` is enough

The handler maps the page and returns. No `eip` adjustment, no re-issue of the load.

That is because a page fault is a **fault**, not a trap: the CPU pushes the address of the
*faulting* instruction, not the one after it (Chapter 16, §10). `iret` re-executes it, the translation
now succeeds, and the program never knows anything happened.

That one property is what makes demand paging possible at all.

### 3.2 Distinguishing a stack from a wild pointer

Three conditions:

```c
        if (addr < bottom &&                                    /* below the stack */
            addr + 64 * KiB >= bottom &&                        /* but not far below */
            addr >= USER_STACK_TOP - USER_STACK_SIZE * 8) {     /* within the limit */
```

**Below the current bottom.** Above it is already mapped, so a fault there means something else.

**Within 64 KiB of it.** A stack grows by pushing, so the gap between the last mapped page and the
faulting address should be small. An address 400 MiB below is a wild pointer that happens to point
downwards.

The 64 KiB figure is a judgement call, and it exists because of a real compiler behaviour: a function
with a large local array can move `ESP` a long way in one instruction, faulting well below the last
mapped page. GCC's `-fstack-clash-protection` emits a probe every page for exactly this reason, and
without it a 100 KiB local array would jump the guard entirely.

**Within an overall limit.** Eight times `USER_STACK_SIZE`, so a runaway recursion is eventually
killed rather than consuming all of memory one page at a time. This is what `ulimit -s` sets on a
real system.

### 3.3 Guard pages

The gap between the stack's maximum extent and the heap is unmapped, and that is the guard.

The failure it prevents is the silent one: a stack that grows into the heap corrupts data structures
rather than faulting, and the crash appears somewhere else entirely. With a guard, the fault happens
at the moment of overflow, with the address in `CR2`.

Kernel stacks are the case where we do *not* have one:

> There is no guard page below it. Chapter 26 adds one, and explains what a kernel stack overflow
> looks like without one: silent corruption of whatever `.bss` variable happens to be allocated just
> underneath.

Kernel stacks come from `kmalloc` (Chapter 29), so the memory below one is another heap block.
Exercise 26.6 adds a guard by allocating them page-aligned with an unmapped page beneath.

---

## 4. Reporting a real fault

```c
    kprintf("\nPAGE FAULT at %08x  (eip=%08x)\n", addr, regs->eip);
    kprintf("  %s, %s, ring %d%s%s\n",
            present ? "protection violation" : "page not present",
            write ? "write" : "read",
            user ? 3 : 0,
            reserved ? ", RESERVED BIT SET IN A PAGE TABLE" : "",
            fetch ? ", instruction fetch" : "");

    if (addr < PAGE_SIZE)
        kprintf("  (address is in the first page: this is a null pointer dereference)\n");
```

Two decisions worth copying.

**Print a sentence, not a bitmask.** `err=00000006` requires the reader to decode it; "page not
present, write, ring 3" does not. The decoding happens once, in the handler, instead of every time
anyone reads a log.

**Name the common case.** An address below `0x1000` is a null pointer dereference — or a small offset
from one, which covers `p->field` and `array[i]` for any sane struct or index. That is why
[`user.ld`](../nimbus/user/user.ld) puts user code at `0x08048000`:

> it leaves the bottom 128 MiB of the address space unmapped, so a null pointer — and anything
> within 128 MiB of one — faults instead of silently reading real memory.

and why Chapter 25 removed the identity mapping.

---

## 5. Who dies

```c
    if (user) {
        kprintf("  killing pid %d\n", current_task ? current_task->pid : -1);
        isr_dump_registers(regs);
        task_exit(-11);                   /* what SIGSEGV would be */
    }

    isr_dump_registers(regs);
    panic("page fault in kernel mode at %08x, eip=%08x", addr, regs->eip);
```

**Ring 3 fault: kill the process.** The kernel is intact; one program had a bug.

**Ring 0 fault: panic.** The kernel has no idea what state its data structures are in, and continuing
risks writing corruption to a disk.

`-11` because SIGSEGV is signal 11 on every Unix. We have no signals (Chapter 20, §3.3), but using
the number a shell would report costs nothing and means the exit status is familiar.

### 5.1 The case in between

There is a third case that this handler gets wrong, and it is worth knowing about.

A *kernel-mode* fault on a *user* address, during `copy_from_user`, is not a kernel bug — it is a
user program passing a bad pointer. Our answer is to validate the pointer first
(Chapter 33, §1) so that the fault never happens.

Real kernels use a **fixup table** instead: the copy routines are marked, and the fault handler looks
up the faulting `eip` in a table of "if a fault happens here, jump there instead". That lets the copy
fail gracefully with `-EFAULT` without validating every page up front, which is faster and handles
the case where another thread unmaps the page mid-copy.

Linux's `__get_user` and `.fixup` sections are exactly this. It is about fifty lines and Exercise
33.7.

---

## 6. Demand paging, which we do not do

Right now `task_exec` reads the whole executable into memory before mapping it (Chapter 34):

```c
    uint8_t *image = (uint8_t *)kmalloc(node->length);
    ...
    ssize_t got = vfs_read(node, 0, node->length, image);
```

Demand paging would instead map the pages not-present, record where the file is, and let the fault
handler read one page at a time:

1. `exec` maps the segments with `P = 0`, storing the file offset in the other 31 bits (which the CPU
   ignores when `P` is clear — Chapter 23, §3.1).
2. First touch faults.
3. The handler reads the offset out of the entry, reads one page from the file, maps it, returns.

What that buys: a 200 MB binary starts in milliseconds, and pages that are never touched are never
read. Most large programs touch a small fraction of themselves.

What it costs: the handler must be able to do disk I/O, which means it must be able to *block*, which
means page faults can no longer be handled with the simple synchronous code above. That is a real
structural change and it is why we do not.

---

## 7. Copy-on-write, which Chapter 34 does

`fork` currently copies every user page (Chapter 28). COW instead:

1. Mark every page in both address spaces read-only, and set `PTE_COW` — one of the three
   software-available bits.
2. Both processes read happily from the shared pages.
3. On a write, the fault handler sees `present && write && (pte & PTE_COW)`, allocates a new frame,
   copies, maps it writable in *this* address space, and returns.
4. When the last sharer's refcount drops to 1, clear `COW` and restore writability.

Step 4 needs a per-frame reference count, which is the real cost — an array of `uint16_t` indexed by
frame number, 64 KiB for 256 MiB of RAM.

This is why `CR0.WP` matters (Chapter 24, §5.5): without it, a kernel write into a COW page on a
process's behalf would succeed silently and corrupt the other process.

---

## 8. Running it

### 8.1 Stack growth

```c
static void deep(int n)
{
    volatile char pad[512];
    pad[0] = (char)n;
    if (n > 0) deep(n - 1);
}

int main(void) { deep(200); return 0; }
```

200 frames × ~528 bytes = about 105 KiB, which is 26 pages.

```
[    1.230] dbg  stack grew to bffff000 for pid 4
[    1.230] dbg  stack grew to bfffe000 for pid 4
[    1.231] dbg  stack grew to bfffd000 for pid 4
...
```

Twenty-six lines, one per page, and the program completes.

### 8.2 A null dereference

```c
int main(void) { *(int *)0 = 42; return 0; }
```

```
PAGE FAULT at 00000000  (eip=08048095)
  page not present, write, ring 3
  (address is in the first page: this is a null pointer dereference)
  killing pid 5
  eax=0000002a ebx=00000000 ...
  eip=08048095 cs=001b eflags=00000212
  user esp=bfffefd4 ss=0023  (fault came from ring 3)
```

`cs=001b` is `SEL_UCODE` — ring 3, as the error code said. The shell survives and prints its prompt.

### 8.3 A kernel fault

```c
    *(volatile int *)0x50000000 = 1;      /* in kmain, unmapped */
```

```
PAGE FAULT at 50000000  (eip=c0104b12)
  page not present, write, ring 0
  ...
*** KERNEL PANIC ***
page fault in kernel mode at 50000000, eip=c0104b12
```

The machine stops. Which is correct — and the serial log has everything.

---

## 9. What could go wrong

| Symptom | Cause |
|---|---|
| `CR2` shows the wrong address | Read after a `kprintf` that itself faulted |
| Infinite fault loop | Handler returns without fixing anything; the instruction re-executes |
| Stack growth never triggers | Distance check too tight, or `user_stack_bottom` not updated |
| A wild pointer grows the stack | Distance check too loose |
| Kernel panics on a bad user pointer | Missing validation in the syscall layer |
| "RESERVED BIT SET" | A corrupted page table — look at whoever wrote it |
| Double fault instead of a page fault | The fault happened with no valid kernel stack — check `esp0` |

The infinite loop is the one that costs the most time. The handler *must* either change something
that makes the faulting access succeed, or not return. A path that logs and returns spins forever at
100% CPU, printing the same line.

---

## 10. Exercises

🟢 **26.1** Trigger each of the five `err_code` combinations in §2.2 from userland and confirm the
handler's sentence in each case.

🟢 **26.2** Make the stack distance check 4 KiB instead of 64 KiB and run the `deep` test with a
1 KiB local array. Watch it get killed.

🟢 **26.3** Write a user program that recurses infinitely. Confirm the overall limit stops it, and
find how many pages it got.

🟡 **26.4** Move the `read_cr2()` call to after the first `kprintf` and construct a case where the
reported address is wrong. (Hint: make `kprintf` itself fault by corrupting the console node.)

🟡 **26.5** Add a fault counter per process and print it in `ps`. Then run the `deep` test and
confirm the count matches the number of stack pages.

🟡 **26.6** Give kernel stacks a guard page: allocate them page-aligned with an unmapped page below,
and confirm that a deliberately recursive kernel function faults cleanly instead of corrupting the
heap.

🔴 **26.7** Implement demand paging for ELF segments. Store the file offset in the unused bits of a
not-present PTE, and read one page in the handler. You will need the fault handler to be able to
block, which means it must run in task context — work out what that changes.

🔴 **26.8** Implement copy-on-write, including the per-frame reference count. Measure `fork` time
before and after with `rdtsc`.

---

## What we covered

- Most page faults are not errors, and the handler's job is telling the five expected kinds from the
  one that is a bug.
- `CR2` read first, before anything that could itself fault.
- The five error-code bits, the combinations that matter, and the one that means "your page table is
  corrupt" rather than "your code is wrong".
- Stack growth: why `return` is enough, and the three conditions that distinguish a stack from a wild
  pointer.
- Why the 64 KiB window exists, and what `-fstack-clash-protection` is for.
- Guard pages as the difference between a clean fault and silent corruption — including the kernel
  stacks that do not have one yet.
- Printing a sentence instead of a bitmask, and naming the null-pointer case.
- Kill the process for ring 3, panic for ring 0, and the third case that needs a fixup table.
- What demand paging and copy-on-write would each add, and the structural cost of each.

[Chapter 27](27-kernel-heap.md) builds `kmalloc`: block headers, splitting, coalescing, and the magic
numbers that catch three of the four classic heap bugs at the moment they happen.

---

[← The higher half](25-higher-half.md) · [Contents](README.md) · [Next: The kernel heap →](27-kernel-heap.md)
