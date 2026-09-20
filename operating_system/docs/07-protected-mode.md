# Chapter 7 — A20, the GDT, and protected mode

[← BIOS services](06-bios-services.md) · [Contents](README.md) · [Next: Stage 2 →](08-stage2-loader.md)

> 📖 **Line by line:** [stage2.asm](line-by-line/spark-stage2.md)

---

## Goal

Turn the 16-bit machine we booted into the 32-bit machine C expects. Three things stand in the way,
and this chapter is one section each:

1. **The A20 gate**, a compatibility hack from 1984 that makes memory above 1 MiB invisible.
2. **The Global Descriptor Table**, which protected mode requires and which does not exist yet.
3. **`CR0.PE` and a far jump**, the actual switch — and a one-way door.

---

## 1. The A20 gate

### 1.1 Why it exists

Chapter 2, §3.2: a real-mode address is `segment × 16 + offset`, and the maximum is
`0xFFFF × 16 + 0xFFFF = 0x10FFEF`. That needs 21 bits. The 8086 had 20 address lines, so the 21st bit
was simply not connected and the address wrapped to `0x0000EF`.

Software used the wrap. Deliberately. The most-cited example is a routine in CP/M-86 and then in
early DOS that reached low memory through a high segment, but the general pattern — pointer
arithmetic that overflows and is expected to come back round — was common enough that IBM could not
ignore it.

When the 80286 arrived in 1984 with 24 address lines, the wrap stopped happening. Programs that
relied on it broke. IBM's fix, in the PC/AT, was a gate on address line 20 that forces it to zero,
with the gate **closed at power-on** so that old software saw the old behaviour.

Forty years later, your CPU still starts with A20 held low.

### 1.2 What it does to us

With A20 disabled, physical addresses alternate: every odd megabyte is aliased onto the even one
below it. Address `0x100000` is really `0x000000`. Address `0x100500` is really `0x000500`.

So a bootloader that loads a kernel to 1 MiB with the gate shut writes it over the interrupt vector
table and the BIOS data area, and then jumps to 1 MiB and executes the IVT. The symptom is an instant
reboot, and there is nothing on screen to explain it.

### 1.3 Testing for it

```nasm
check_a20:
    pushf
    push ds
    push es
    push di
    push si
    cli

    xor ax, ax
    mov es, ax
    mov di, 0x0500              ; ES:DI = 0x0000:0x0500 = physical 0x000500

    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x0510              ; DS:SI = 0xFFFF:0x0510 = 0x100500

    mov al, [es:di]             ; save both bytes
    push ax
    mov al, [ds:si]
    push ax

    mov byte [es:di], 0x00      ; write 0x00 low
    mov byte [ds:si], 0xFF      ; write 0xFF high
    cmp byte [es:di], 0xFF      ; did the high write land on the low address?

    pop ax                      ; restore, in reverse order
    mov [ds:si], al
    pop ax
    mov [es:di], al

    mov ax, 0
    je .exit                    ; they aliased -> A20 is OFF -> return 0
    mov ax, 1
.exit:
```

Two addresses that are the same address if and only if A20 is disabled. Write different values to
each and read one back: if the high write landed on the low address, they are aliased.

`0xFFFF:0x0510` is `0xFFFF × 16 + 0x510 = 0x100500`, which wraps to `0x000500`. And `0x000500` is
chosen because it is the first byte after the BIOS data area — but "first byte after" is not "unused
by anything", so the routine saves and restores both bytes. That courtesy costs four instructions and
avoids corrupting a BIOS structure we might still need.

`cli` around the whole thing: an interrupt handler that ran in the middle would see two bytes of
memory holding test patterns.

### 1.4 Three ways to open it, in order

```nasm
    call check_a20
    test ax, ax
    jnz .a20_done               ; some BIOSes already enabled it

    mov ax, 0x2401              ; int 0x15, AX=0x2401: "enable A20"
    int 0x15
    call check_a20
    test ax, ax
    jnz .a20_done

    in al, 0x92                 ; System Control Port A
    or al, 0x02                 ; bit 1 = A20 enable
    and al, 0xFE                ; bit 0 = FAST RESET -- mask it off, always
    out 0x92, al
    call check_a20
    test ax, ax
    jnz .a20_done

    call enable_a20_keyboard    ; the original, slowest, most compatible way
    call check_a20
    test ax, ax
    jnz .a20_done

    mov si, msg_a20_fail
    call print
    jmp hang
```

There is no way to know in advance which method a given machine supports, so we try them in
increasing order of brutality and **check after each one**. That checking is the part people skip.

**Method 0: it may already be on.** Some BIOSes enable A20 before handing over. Testing first
costs nothing and occasionally skips the whole section.

**Method 1: `int 0x15, AX = 0x2401`.** The polite way. Supported on most machines from the mid-90s
on. Returns `AH = 0` on success, but we do not trust the return value — we re-test.

**Method 2: port `0x92`, "System Control Port A".** Present on PS/2 machines and everything since.
Bit 1 is A20.

> ⚠️ **Bit 0 of port `0x92` is FAST RESET.** Writing a 1 there reboots the machine instantly.
> `and al, 0xFE` is not optional, and it is not paranoia — a naive `or al, 0x02` followed by `out`
> is fine, but a naive `mov al, 0x02` after a read that returned `0x01` is a reboot loop. Mask it
> every time.

**Method 3: the keyboard controller.**

```nasm
enable_a20_keyboard:
    cli
    call .wait_in
    mov al, 0xAD                ; disable the keyboard
    out 0x64, al

    call .wait_in
    mov al, 0xD0                ; read the controller's output port
    out 0x64, al
    call .wait_out
    in al, 0x60
    push ax

    call .wait_in
    mov al, 0xD1                ; write the output port
    out 0x64, al
    call .wait_in
    pop ax
    or al, 2                    ; bit 1 of that port IS the A20 line
    out 0x60, al

    call .wait_in
    mov al, 0xAE                ; re-enable the keyboard
    out 0x64, al
    call .wait_in
    sti
    ret
```

Yes, really: **A20 is wired to a spare output pin of the 8042 keyboard controller.** In 1984 that was
the only chip on the board with a pin going spare. This is the reason that "enable the A20 line"
involves a six-step conversation with the keyboard, and it is the single best illustration in this
book of what backwards compatibility costs.

The `wait_in`/`wait_out` helpers poll the status register at port `0x64`:

```nasm
.wait_in:
    in al, 0x64
    test al, 2                  ; bit 1 = input buffer full
    jnz .wait_in
    ret
.wait_out:
    in al, 0x64
    test al, 1                  ; bit 0 = output buffer full
    jz .wait_out
    ret
```

The 8042 is a 1984 microcontroller running at about 8 MHz. It needs to be given time between bytes,
and the status bits are how it says so.

---

## 2. The Global Descriptor Table

### 2.1 What protected mode needs

In protected mode a segment register holds a selector — an index into a table of descriptors
(Chapter 2, §4). The CPU refuses to enter protected mode usefully without that table existing,
because the first instruction fetch after the switch consults `CS`'s descriptor.

So we build one before flipping the bit.

### 2.2 A descriptor, field by field

```nasm
gdt_code:                       ; selector 0x08 -- ring 0, execute/read
    dw 0xFFFF                   ; limit 0:15
    dw 0x0000                   ; base 0:15
    db 0x00                     ; base 16:23
    db 10011010b                ; P=1 DPL=00 S=1 | Type=1010
    db 11001111b                ; G=1 D=1 L=0 AVL=0 | limit 16:19
    db 0x00                     ; base 24:31
```

Eight bytes, and the fields are interleaved in an order that makes no sense until you know why.

```
 byte:  0     1     2     3     4     5     6     7
       +-----------+-----------+-----+-----+-----+-----+
       | limit 0:15| base 0:15 |base |acc- |flags|base |
       |           |           |16:23|ess  |+lim |24:31|
       +-----------+-----------+-----+-----+-----+-----+
                                            19:16
```

The 80286 had 6-byte descriptors with a 24-bit base and a 16-bit limit — bytes 0 through 5. The 386
needed a 32-bit base and a 20-bit limit, and rather than redesign the format it bolted the extra
byte of base onto the end and stuffed the extra four bits of limit into the spare nibble of byte 6.

That is the entire explanation. There is no better reason, and you will meet the same scar tissue in
the IDT (Chapter 16), where a 32-bit handler address is split across bytes 0–1 and 6–7.

**The access byte**, `10011010b`:

| Bits | Value | Meaning |
|---|---|---|
| 7 | 1 | `P`, present. A descriptor with this clear faults on use. |
| 6–5 | 00 | `DPL`, descriptor privilege level. 0 = kernel. |
| 4 | 1 | `S`, this is a code or data segment (not a system descriptor like a TSS) |
| 3 | 1 | executable — this is a code segment |
| 2 | 0 | conforming. 0 means code here cannot be called from a lower privilege level. |
| 1 | 1 | readable. Code segments are execute-only unless this is set. |
| 0 | 0 | accessed. The CPU sets this; we leave it clear. |

For the data segment, `10010010b`: the same, but bit 3 clear (not executable), bit 2 is
"expand-down" rather than "conforming", and bit 1 is "writable".

**The flags nibble**, the top four bits of `11001111b`:

| Bit | Value | Meaning |
|---|---|---|
| 7 | 1 | `G`, granularity. The limit counts 4 KiB pages, not bytes. |
| 6 | 1 | `D/B`, default operand size. 1 = 32-bit. |
| 5 | 0 | `L`, 64-bit code. Not us. |
| 4 | 0 | `AVL`, available for software. Nobody uses it. |

`G = 1` with a limit of `0xFFFFF` gives `0xFFFFF × 4096 = 0xFFFFF000`, plus the 4095 bytes inside the
last page: exactly 4 GiB.

`D = 1` is what makes `mov eax, ebx` assemble to the short form in this segment. With `D = 0` the
CPU would treat the segment as 16-bit code and every 32-bit instruction would need a prefix.

### 2.3 The null descriptor

```nasm
gdt_null:                       ; selector 0x00
    dq 0
```

Required by the architecture to be all zeros. Its purpose is to make selector 0 invalid, so that a
segment register left uninitialised — which contains 0 — faults on first use instead of addressing
something plausible.

It is the same idea as leaving the first page of the address space unmapped so that null pointer
dereferences fault (Chapter 26). Making the default value the *invalid* value converts a silent bug
into a loud one.

### 2.4 The GDT register

```nasm
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; limit = size in bytes, MINUS ONE
    dd gdt_start                ; base = the linear address of the table
```

Six bytes: a 16-bit limit and a 32-bit base. This is what `lgdt` reads.

> ⚠️ **The limit is size minus one.** The CPU stores "the byte offset of the last valid byte". A
> 24-byte table has limit 23. Writing 24 creates a fourth descriptor made of whatever bytes follow
> the array, and the fault it eventually causes points anywhere but at this line.

The base must be a **linear** address. `DS` is 0 and `ORG` is `0x7E00`, so the address NASM computed
for `gdt_start` is already linear. If `DS` were not zero it would not be, and this is a real bug
people hit when they move the GDT into a different segment.

---

## 3. The switch

```nasm
    cli

    lgdt [gdt_descriptor]

    mov eax, cr0
    or  eax, 1                  ; CR0 bit 0 = PE, Protection Enable
    mov cr0, eax

    jmp CODE_SEG:protected_entry
```

Four steps. Each one matters.

### 3.1 `cli`, and why it is permanent this time

The real-mode interrupt vector table at `0x0000` is meaningless in protected mode — the CPU consults
the IDT instead, and we have not built one. Any interrupt at all would look up a descriptor in a
table that does not exist, fail, fail again trying to report the failure, and triple-fault.

And the timer is ticking 18.2 times a second. So this `cli` is not a short critical section; it stays
in force until the kernel installs an IDT, which for Nimbus is Chapter 16 and for Spark is never.

### 3.2 `lgdt`

Loads the 6-byte pointer into the GDTR. It does not change any segment register and does not validate
anything. The CPU keeps using the descriptors it cached when the segment registers were last loaded —
which, in real mode, are not descriptors at all.

### 3.3 `CR0.PE`

One bit. The machine is now in protected mode.

But `CS` still holds a real-mode segment value, the hidden descriptor cache behind it still holds
real-mode base and limit, and the CPU has already fetched and decoded the next few instructions using
real-mode rules.

### 3.4 The far jump

```nasm
    jmp CODE_SEG:protected_entry
```

This is the instruction that actually completes the transition, and it does two things at once.

It **reloads `CS`** from the GDT, which is the only way to get a protected-mode code descriptor into
it — Chapter 2, §2.4: `CS` is not assignable.

It **flushes the prefetch queue**, discarding instructions that were decoded under the old rules.

> 🔧 **On real hardware.** This is one of the two places in the book where the difference between
> "the CPU executes instructions" and what it actually does is visible. On a 386 the prefetch queue
> is 16 bytes; on a modern CPU it is a deep out-of-order pipeline. In both cases a far jump is a
> serialising instruction and clears it. Without one, the behaviour is genuinely undefined.

After this instruction, `[BITS 32]` applies and the machine is a 386.

### 3.5 Reloading the rest

```nasm
[BITS 32]
protected_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000
```

`CS` was reloaded by the jump. Every other segment register still holds a real-mode value, which is
now an invalid selector. Load them all before touching memory or the stack.

If you forget `SS`, the next `push` uses a garbage descriptor and faults. If you forget `DS`, the
next memory access does. These are not subtle failures, but they happen after the jump, in 32-bit
code, with no way to print an error — so they present as a triple fault immediately after the mode
switch, which is a symptom with about ten possible causes.

`ESP = 0x90000` is 576 KiB: below the 640 KiB line, above everything we have loaded, and clear of the
BIOS data area. The kernel switches to its own stack within a few instructions
([`entry.asm`](../spark/kernel/entry.asm)).

---

## 4. What we just lost

The BIOS is gone. `int 0x10` and `int 0x13` no longer work and will not work again.

Concretely, this means:

- **No printing** until we write a VGA driver (Chapter 10, and properly in Chapter 12).
- **No disk access** until we write an ATA driver (Chapter 37).
- **No memory map** unless we asked for it before the switch.

That last one is the trap. If you need `int 0x15 E820`'s memory map — and any real kernel does — you
must call it in real mode and stash the results somewhere the 32-bit kernel can find them. A
bootloader that switches modes and *then* realises it needs the memory map has to either go back to
real mode (possible, awkward, and requires an unreal-mode trick) or start over.

Spark does not need the memory map because it does not manage memory. Nimbus gets it from multiboot,
which is to say from a bootloader that did exactly this work before handing over.

---

## 5. Going back, and why we do not

It is possible to return to real mode: clear `CR0.PE`, far jump to a 16-bit segment, restore the
real-mode segment registers. Bootloaders that need BIOS services after entering protected mode do
exactly this, and it is how DOS extenders worked for a decade.

It is fiddly — you need a 16-bit code descriptor in the GDT, the segment limits have to be set to
`0xFFFF` before the switch back, and interrupts have to be off throughout — and we never need it.

The related trick worth knowing the name of is **unreal mode**: enter protected mode, load a
descriptor with a 4 GiB limit into `DS`, then return to real mode *without reloading `DS`*. The
hidden descriptor cache keeps the 4 GiB limit, so real-mode code can address all of memory with
32-bit offsets while still calling BIOS services. It is an accident of the architecture that Intel
never documented and never broke, and it is how a number of DOS games addressed more than a megabyte.

---

## 6. Running it

```bat
build spark
run spark
```

```
Spark: stage1
Spark: -> stage2
Spark: stage2 at 0x7E00
Spark: loading kernel...
Spark: kernel loaded
Spark: A20 via port 0x92
Spark: A20 enabled
Spark: entering protected mode
```

and then the screen changes to the Spark banner, which is Chapter 10's code running in 32-bit mode.

The A20 line tells you which method worked. In QEMU it is usually port `0x92`; on real hardware it is
often the BIOS call, and occasionally A20 is already on and the line does not appear at all.

### If it triple-faults

The symptom is QEMU rebooting instantly and looping. `-no-reboot` (which `run.bat` passes) turns that
into a stop, and `-d int` shows you the sequence:

```bash
qemu-system-i386 -fda bin/spark.img -boot a -no-reboot -d int -D log.txt
```

Look for three consecutive exceptions. The pattern
`v=0d` (general protection) → `v=08` (double fault) → `check_exception old: 0x8 new 0xd` is a triple
fault, and the `IP=` on the *first* one tells you which instruction started it.

The usual causes, in order of frequency:

| First exception | Likely cause |
|---|---|
| `v=0d` right after `mov cr0` | The far jump's selector is wrong, or the GDT limit is off by one |
| `v=0d` on the first `push` | `SS` not reloaded |
| `v=0e`, or nonsense addresses | A20 not actually enabled — the kernel went to address 0 |
| Nothing at all, instant reboot | The GDT base is wrong, so `lgdt` loaded garbage |

---

## 7. Exercises

🟢 **7.1** Change the GDT limit from `gdt_end - gdt_start - 1` to `gdt_end - gdt_start`. Does it
still boot? Explain why, and why it is still wrong.

🟢 **7.2** Remove `mov ss, ax` from `protected_entry` and boot. Which instruction faults?

🟡 **7.3** Work out the descriptor bytes for a ring 3 data segment with base 0 and limit 4 GiB, by
hand, then check against [`gdt.c`](../nimbus/kernel/gdt.c)'s `gdt_set_entry(4, ...)`.

🟡 **7.4** Disable the port `0x92` method by commenting it out, and see which method your QEMU falls
through to. Then disable that one too.

🟡 **7.5** Write a version of `check_a20` that tests the boot signature at `0x7DFE` against
`0xFFFF:0x7E0E` instead of using `0x500`. Why is that the more common version in the wild, and what
is wrong with it?

🔴 **7.6** Set `G = 0` in both descriptors and change the limit so that the segments are exactly
1 MiB. Then have the kernel try to write to `0x200000` and watch the general protection fault. This
is segmentation actually doing something, which is worth seeing once before we abandon it.

---

## What we covered

- A20: a 1984 compatibility gate that aliases every odd megabyte onto the even one below, still
  closed at power-on, and wired to the keyboard controller because that chip had a spare pin.
- A test for it that does not corrupt the BIOS data area, and four ways to open it tried in order
  with a re-check after each.
- The FAST RESET bit that will reboot your machine if you do not mask it.
- A GDT descriptor field by field, why the layout is interleaved, and what each bit of the access
  byte and the flags nibble does.
- The null descriptor, and the principle of making the default value the invalid one.
- `lgdt`, `CR0.PE`, and the far jump that reloads `CS` and flushes the pipeline — the seam between
  two different machines.
- What we lose at that instant, and the rule that follows: take everything you need from the BIOS
  first.
- Unreal mode, as a piece of vocabulary.

[Chapter 8](08-stage2-loader.md) covers the rest of stage 2 — loading the kernel, the segment
arithmetic that gets past 64 KiB, and the copy to 1 MiB.

---

[← BIOS services](06-bios-services.md) · [Contents](README.md) · [Next: Stage 2 →](08-stage2-loader.md)
