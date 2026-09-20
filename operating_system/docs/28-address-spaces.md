# Chapter 28 — Address spaces

[← The kernel heap](27-kernel-heap.md) · [Contents](README.md) · [Next: What a process is →](29-what-is-a-process.md)

---

## Goal

Give every process its own view of memory. Create an address space, clone one, destroy one — and
understand precisely which parts are shared and which are copied, because getting that wrong
produces bugs that look like anything but a paging problem.

This finishes Part III.

---

## 1. What an address space is

A `CR3` value, and the tree hanging off it.

```c
typedef struct page_directory {
    uint32_t *entries;        /* virtual address: 1024 entries we can write   */
    paddr_t   phys;           /* physical address: what goes into CR3         */
} page_directory_t;
```

Two fields for one thing, and the reason is in the comment:

> recomputing `V2P()` every time is exactly the kind of thing that works until the day the directory
> is allocated outside the kernel window.

`entries` is what our code writes. `phys` is what the CPU reads. Carrying both means the conversion
happens once, at creation, in the one place that knows it is valid.

Switching is one instruction:

```c
void paging_switch_directory(page_directory_t *dir)
{
    current_directory = dir;
    write_cr3(dir->phys);
}
```

and from that instruction on, the same virtual addresses mean different physical memory.

---

## 2. The split: shared top, private bottom

```c
page_directory_t *paging_new_directory(void)
{
    page_directory_t *dir = (page_directory_t *)kmalloc(sizeof(page_directory_t));
    if (!dir) return NULL;

    dir->phys    = alloc_zeroed_frame();
    dir->entries = (uint32_t *)P2V(dir->phys);

    for (uint32_t i = KERNEL_PAGE_NUMBER; i < 1024; i++)
        dir->entries[i] = kernel_directory->entries[i];

    return dir;
}
```

Entries 0–767 zero. Entries 768–1023 copied from the kernel directory.

```
    entry 1023  +------------------+
                |                  |
                |  SHARED          |  identical in every address space
                |  (the kernel)    |  256 entries = 1 GiB
                |                  |
    entry 768   +------------------+
    entry 767   |                  |
                |  PRIVATE         |  different in every address space
                |  (the process)   |  768 entries = 3 GiB
    entry 0     +------------------+
```

### 2.1 The entries are copied; the tables are shared

This is the distinction the whole chapter turns on.

The loop copies **1024 four-byte values**. Each of those values is a *pointer to a page table*. So
after the loop, both directories point at the **same** page tables.

Not copies of them. The same physical frames.

```
    kernel_directory.entries[768]  =  0x00504007
    new_directory.entries[768]     =  0x00504007      <- same frame
                                          |
                                          v
                                    +-----------+
                                    | page table|      one table, two references
                                    +-----------+
```

### 2.2 Why that matters

The source says it plainly:

> a change to a kernel mapping made in one address space is visible in all of them, automatically,
> because they share the tables rather than copies of them. Get that wrong — copy the entries
> instead of sharing the tables — and a kmalloc that happens to grow the heap becomes visible only to
> the process that did it.

Picture the failure. Process A calls `write()`. Inside the kernel, `kmalloc` needs more arena, so
`heap_grow` calls `paging_map_range`, which adds entries to a kernel page table.

If B had its own *copy* of that table, B's next syscall would page-fault on the new heap memory — in
the kernel, at a random address, and only sometimes, depending on which process happened to grow the
heap first.

Sharing the tables makes it automatic. Every kernel mapping created after any number of processes
exist is visible to all of them, immediately, with no bookkeeping.

### 2.3 What else it buys

**A syscall needs no `CR3` reload.** The kernel is already mapped at the same addresses. Entering it
changes privilege level and nothing else — no TLB flush, no address space switch.

**Interrupt handlers work in any context.** An interrupt arrives while some process is running; the
handler runs in whatever address space was current, and needs the kernel's code, stack and data
mapped. Sharing the top gigabyte makes that free.

**The direct map works everywhere.** `P2V` on any physical frame is valid in every address space,
which is why `paging_clone_directory` can read one process's tables and write another's while a third
is current.

---

## 3. Cloning, for `fork`

```c
page_directory_t *paging_clone_directory(page_directory_t *src)
{
    page_directory_t *dst = paging_new_directory();
    if (!dst) return NULL;

    for (uint32_t di = 0; di < KERNEL_PAGE_NUMBER; di++) {
        if (!(src->entries[di] & PTE_PRESENT)) continue;

        uint32_t *src_table = (uint32_t *)P2V(src->entries[di] & PTE_FRAME_MASK);

        paddr_t   dst_table_phys = alloc_zeroed_frame();
        uint32_t *dst_table      = (uint32_t *)P2V(dst_table_phys);

        dst->entries[di] = dst_table_phys | (src->entries[di] & PTE_FLAGS_MASK);

        for (uint32_t ti = 0; ti < 1024; ti++) {
            if (!(src_table[ti] & PTE_PRESENT)) continue;

            paddr_t new_frame = pmm_alloc_frame();
            if (new_frame == PMM_NO_FRAME)
                panic("fork: out of memory cloning address space");

            memcpy(P2V(new_frame),
                   P2V(src_table[ti] & PTE_FRAME_MASK),
                   PAGE_SIZE);

            dst_table[ti] = new_frame | (src_table[ti] & PTE_FLAGS_MASK);
        }
    }

    return dst;
}
```

Note the loop bound: `di < KERNEL_PAGE_NUMBER`. **Only the user half.** The kernel half came from
`paging_new_directory` and is shared.

### 3.1 Three levels of copying

Read the function carefully and there are three different things happening at three levels:

**Directory entries** — a new table is allocated for each present entry. Not shared: the child must
be able to change its own mappings.

**Page tables** — a fresh frame each, filled in below.

**Data pages** — a fresh frame each, with the contents `memcpy`'d.

That last one is the expensive one, and §4 is about it.

### 3.2 The flags are preserved

```c
            dst_table[ti] = new_frame | (src_table[ti] & PTE_FLAGS_MASK);
```

New frame, **old flags**. A read-only page stays read-only; a user page stays user.

Copying the frame address without the flags would give the child a writable copy of the parent's
read-only `.text`, which is a silent loss of protection rather than a crash — the kind of bug that is
only noticed when something overwrites its own code.

### 3.3 Why `P2V` works for both

`memcpy(P2V(new_frame), P2V(src_table[ti] & PTE_FRAME_MASK), PAGE_SIZE)` reads from the parent's
physical frame and writes to the child's, **without either address space being current**.

That is the direct map earning its keep (Chapter 24, §2.1). With recursive page tables instead, this
function would need a temporary mapping for every single page — and that is precisely the case
Chapter 24 gave for choosing the direct map.

---

## 4. The cost, and copy-on-write

This copies **every** user page. For a shell that has just forked to run `ls`, that is the entire
address space — code, data, stack — duplicated and then immediately thrown away by `exec`.

```
    sh: 200 KiB of address space
    fork():   200 KiB copied, ~50 page allocations, ~50 memcpys
    exec():   all of it freed, a new address space built
```

The source is honest:

> That is honest, simple, and wasteful: fork() followed immediately by exec() — which is what a shell
> does for every command — copies an entire address space and then throws it away.

### 4.1 What COW does instead

1. Mark every user page **read-only** in both address spaces, and set `PTE_COW` — one of the three
   software-available bits (Chapter 23, §3.8).
2. Both processes read happily from the shared frames.
3. On a **write**, the fault handler sees `present && write && (pte & PTE_COW)`, allocates a frame,
   copies one page, maps it writable in *this* address space, and returns.
4. When the last sharer's reference count drops to 1, clear `COW` and restore writability without
   copying.

`fork` then costs: a directory, one table per present directory entry, and a walk to flip bits. No
data copying at all. A `fork`+`exec` pair copies perhaps two pages — the stack page the shell writes
to before `exec`.

### 4.2 What it needs

**A per-frame reference count.** An array of `uint16_t` indexed by frame number: 64 KiB for 256 MiB
of RAM. Incremented when a page becomes shared, decremented on unmap, and the page is only freed at
zero.

That array is the real cost of COW, and it is the thing that makes it a structural change rather than
a handler tweak.

**`CR0.WP`.** Chapter 24, §5.5. Without it, a kernel write into a COW page on a process's behalf —
`read()` writing into a buffer, say — succeeds silently, modifies the shared copy, and corrupts the
other process.

### 4.3 Why the simple version first

> This version is the one to understand first, because the COW version is this plus a fault handler
> and a reference count, and both of those are easier to follow once you have seen what they are
> replacing.

Chapter 34 implements it. It is about eighty lines on top of what exists.

---

## 5. Destroying an address space

```c
void paging_free_directory(page_directory_t *dir)
{
    if (!dir || dir == kernel_directory) return;

    for (uint32_t di = 0; di < KERNEL_PAGE_NUMBER; di++) {
        if (!(dir->entries[di] & PTE_PRESENT)) continue;

        uint32_t *table = (uint32_t *)P2V(dir->entries[di] & PTE_FRAME_MASK);
        for (uint32_t ti = 0; ti < 1024; ti++)
            if (table[ti] & PTE_PRESENT)
                pmm_free_frame(table[ti] & PTE_FRAME_MASK);

        pmm_free_frame(dir->entries[di] & PTE_FRAME_MASK);
    }

    pmm_free_frame(dir->phys);
    kfree(dir);
}
```

Three levels again, in reverse: data pages, then tables, then the directory.

### 5.1 The loop bound, again

```c
    for (uint32_t di = 0; di < KERNEL_PAGE_NUMBER; di++)
```

> Only the user half. Freeing the kernel tables would unmap the kernel from every other process,
> since they share them — and the machine would die on the next system call anywhere in the system.

This is the bug that the shared-table design makes possible, and it is spectacular: one process exits
and the entire machine stops, in a syscall, in an unrelated process.

The guard is one loop bound, and it is worth a comment in the source because a reader who does not
know the tables are shared would "fix" it.

### 5.2 You cannot free the one you are standing on

`task_exit` cannot call this on its own directory — `CR3` still points at it, and the kernel stack we
are running on is mapped through it.

That is why the work is split (Chapter 34):

```c
void task_exit(int status)
{
    ...
    t->state = TASK_ZOMBIE;
    ...
}

pid_t task_wait(int *status)
{
    ...
            if (t->state == TASK_ZOMBIE) {
                ...
                if (t->directory && t->directory != kernel_directory)
                    paging_free_directory(t->directory);

                if (t->kernel_stack)
                    kfree((void *)(t->kernel_stack - KERNEL_STACK_SIZE));

                t->state = TASK_UNUSED;
                return pid;
            }
```

The parent frees it, later, from a different stack and a different address space.

**That is the entire reason zombies exist.** Plus one more: the exit status has to survive until
somebody asks for it.

---

## 6. Switching, and the ordering

From [`sched.c`](../nimbus/kernel/sched.c):

```c
    if (next->directory && next->directory != current_directory)
        paging_switch_directory(next->directory);

    tss_set_kernel_stack(next->kernel_stack);

    switch_context(&prev->context, next->context);
```

Two things before the switch, in this order, and the comment explains why:

> CR3 first: after this instruction we are running in the new task's address space. That is safe only
> because the kernel half of every directory is identical, so the code we are executing, the stack we
> are on and the task structs we are touching are all still mapped. This is the payoff for sharing
> the kernel's page tables in `paging_new_directory()`, and it is why a kernel that copies them
> instead crashes exactly here.

Read that again with §2.1 in mind. The instruction `write_cr3(dir->phys)` changes the meaning of
every address — including the address of the very next instruction, the stack pointer, and the `next`
pointer we are about to use.

It works because all three are in the shared half. If the kernel half were per-process, the
instruction after `mov cr3` would fetch from an unmapped address.

### 6.1 The `!=` check

```c
    if (next->directory && next->directory != current_directory)
```

Skipping the reload when the directory is unchanged avoids a full TLB flush.

That matters more than it looks: two kernel threads, or a process being scheduled after an interrupt
that did not change address spaces, would otherwise throw away every cached translation for no
reason. Chapter 23, §5.3 — reloading `CR3` is the sledgehammer.

---

## 7. Running it

### 7.1 Two processes, two views

```c
    int x = 42;
    printf("before fork: &x = %p, x = %d\n", &x, x);

    if (fork() == 0) {
        x = 99;
        printf("child:  &x = %p, x = %d\n", &x, x);
        exit(0);
    }
    wait(NULL);
    printf("parent: &x = %p, x = %d\n", &x, x);
```

```
before fork: &x = 0xbffffee4, x = 42
child:  &x = 0xbffffee4, x = 99
parent: &x = 0xbffffee4, x = 42
```

**The same address, different values.** That is the entire point of Part III, in three lines of
output.

### 7.2 Checking the translation

```c
    kprintf("pid %d: %08x -> %08x\n", current_task->pid, 0xbffffee4,
            paging_virt_to_phys(current_task->directory, 0xbffffee4));
```

```
pid 4: bffffee4 -> 006a2ee4
pid 5: bffffee4 -> 007c1ee4
```

Two processes, the same virtual address, two physical frames.

### 7.3 Memory accounting

```
nimbus> mem
pmm: 1348/32512 frames used (5 MiB / 127 MiB)
nimbus> forktest
...
nimbus> mem
pmm: 1348/32512 frames used (5 MiB / 127 MiB)
```

Same before and after. `fork` allocated an address space, `exit` marked a zombie, `wait` freed it,
and the count returned exactly.

If it does not, something leaks — and the usual candidate is a page table freed without its pages, or
a directory freed without its tables.

---

## 8. What could go wrong

| Symptom | Cause |
|---|---|
| Triple fault immediately after `mov cr3` | Kernel half not mapped in the new directory |
| Kernel heap growth invisible to other processes | Kernel tables copied instead of shared |
| The whole machine dies when one process exits | Kernel tables freed in `paging_free_directory` |
| Child can write to its `.text` | Flags not preserved in the clone |
| Frame count grows every fork | Pages, tables or the directory leaked on exit |
| Panic "already free" on exit | Something freed twice — usually a shared frame treated as private |
| Faults in `wait()` | Freeing an address space while it is still current |

---

## 9. Part III, in retrospect

Eight chapters, and the kernel can now:

| | |
|---|---|
| Know what memory exists | The firmware map, and the holes in it (21) |
| Allocate physical frames | A bitmap, a hint, and a loud double-free check (22) |
| Understand translation | Two levels, a dozen flag bits, and the TLB (23) |
| Build page tables | Create on demand, map ranges, read-only text, `CR0.WP` (24) |
| Run where it was linked | Two linker sections and one absolute jump (25) |
| Act on faults | Stack growth, guard pages, and killing the right thing (26) |
| Allocate objects | `kmalloc`, coalescing, and magic numbers that fail loudly (27) |
| Isolate processes | Shared kernel, private user, clone and destroy (28) |

None of it runs a program yet. But every mechanism Part IV needs is now in place — and the thing that
makes Part IV possible at all is the last row: two processes can have the same address mean different
memory.

---

## 10. Exercises

🟢 **28.1** Print the first ten directory entries of two different processes side by side. Confirm
0–767 differ and 768–1023 are identical.

🟢 **28.2** Change `paging_free_directory`'s loop bound to 1024 and run `forktest`. Describe the
failure precisely.

🟢 **28.3** Remove the flags preservation from `paging_clone_directory` and have a child write to its
own `.text`.

🟡 **28.4** Count the frames a `fork` costs: instrument `pmm_alloc_frame`, run a fork, and report
directory + tables + pages separately.

🟡 **28.5** Make `paging_new_directory` *copy* the kernel page tables instead of sharing them. Then
allocate enough in one process to grow the heap and watch another process fault.

🟡 **28.6** Add `paging_dump(dir)` that prints every present mapping as a range, coalescing
consecutive pages with the same flags. Use it to look at a user process's address space.

🔴 **28.7** Implement copy-on-write fully: the `PTE_COW` bit, the per-frame refcount array, the fault
handler case, and the refcount decrement in `paging_free_directory`. Measure `fork` with `rdtsc`
before and after.

🔴 **28.8** Implement `mmap` for anonymous memory: a syscall that reserves a range and maps it
lazily, with the page fault handler allocating on first touch. Then make userland `malloc` use it
for large allocations instead of `sbrk`.

---

## What we covered

- An address space as a `CR3` value and a tree, with both addresses of the directory carried
  deliberately.
- The 768-entry/256-entry split, and the crucial distinction: the *entries* are copied, the *tables*
  are shared.
- Three things sharing buys — automatic visibility of kernel mappings, syscalls with no `CR3` reload,
  and interrupt handlers that work in any context — and the exact bug you get from copying instead.
- Cloning at three levels, why the flags must come along, and why `P2V` lets it work with neither
  address space current.
- What copying every page costs a shell, what COW replaces it with, and the per-frame refcount that
  is the real price.
- Freeing an address space in reverse, the loop bound that stops it killing the machine, and why you
  cannot free the one you are standing on — which is why zombies exist.
- The ordering in a context switch, and why `mov cr3` mid-function is safe only because of §2.
- Three lines of output that demonstrate the whole of Part III.

**Part III is finished.** [Chapter 29](29-what-is-a-process.md) starts Part IV by asking what a
process actually is — and the answer is shorter than you would expect.

---

[← The kernel heap](27-kernel-heap.md) · [Contents](README.md) · [Next: What a process is →](29-what-is-a-process.md)
