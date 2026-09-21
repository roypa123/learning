# Chapter 18 — 8088 versus 8086

[← I/O interfacing](17-io-interfacing.md) · [Contents](README.md) · [Next: Addressing modes →](19-addressing-modes.md)

---

## Goal

Account precisely for the differences between the two chips, which matters for three reasons: the
IBM PC used the 8088, so most period software and most documentation is written for it; exam
questions ask; and the differences illustrate what a data bus width actually costs.

This chapter closes Part II.

---

## 1. The one-sentence summary

> The 8088 is an 8086 with an 8-bit external data bus and a 4-byte instruction queue. Everything
> visible to software is identical.

Same registers, same flags, same instruction set, same addressing modes, same segmentation, same
1 MiB address space, same interrupt structure, same clock speeds. **A program cannot tell which chip
it is running on** except by timing itself.

---

## 2. The differences, completely

| | 8086 | 8088 |
|---|------|------|
| External data bus | 16 bits, `AD15–AD0` | **8 bits, `AD7–AD0`** |
| Internal data paths | 16 bits | 16 bits |
| Pins 8–15 | `AD8`–`AD15` (multiplexed) | **`A8`–`A15` (address only, not multiplexed)** |
| Pin 34 | `BHE#/S7` | **`SS0#`** |
| Pin 28 | `M/IO#` | **`IO/M#` — inverted** |
| Instruction queue | 6 bytes | **4 bytes** |
| Queue refill threshold | ≥ 2 bytes free | **≥ 1 byte free** |
| Bytes per fetch cycle | 2 | **1** |
| Memory banks | two (even/odd) | **one** |
| Word access, even address | 1 bus cycle | **2 bus cycles** |
| Word access, odd address | 2 bus cycles | 2 bus cycles |
| Alignment penalty | yes | **none — everything costs the same** |
| I/O cycles | no automatic wait state | **one automatic wait state** |
| Transistors | ~29,000 | ~29,000 |
| Package | 40-pin DIP | 40-pin DIP |
| Address space | 1 MiB | 1 MiB |
| Instruction set | identical | identical |

---

## 3. The pin differences in detail

Only three pins actually change.

### 3.1 Pins 8–15 become plain address lines

On the 8086, `AD15`–`AD8` are multiplexed. On the 8088 there is no upper data byte, so those pins
carry `A15`–`A8` for the **whole** bus cycle — they are not multiplexed at all.

**Consequence for the board:** the 8088 needs only **two** address latches instead of three for
`A15`–`A0`… no, still two plus the high one, because `A7`–`A0` are still multiplexed with `D7`–`D0`.
Concretely:

```
   8086:  latch A7-A0, A15-A8, and A19-A16+BHE  ->  three 74LS373s
   8088:  latch A7-A0 and A19-A16+SS0           ->  two 74LS373s
          A15-A8 come straight off the pins
```

And only **one** 74LS245 transceiver instead of two. So an 8088 board is two chips cheaper — which
is precisely the saving IBM was after.

### 3.2 `BHE#` becomes `SS0#`

There are no banks, so there is nothing for `BHE#` to select. Pin 34 instead carries `SS0#`, a status
line which — combined with `IO/M#` and `DT/R#` — gives the same information the 8086's `S2#S1#S0#`
gives in maximum mode:

| `IO/M#` | `DT/R#` | `SS0#` | Cycle |
|---------|---------|--------|-------|
| 1 | 0 | 0 | interrupt acknowledge |
| 1 | 0 | 1 | read I/O port |
| 1 | 1 | 0 | write I/O port |
| 1 | 1 | 1 | halt |
| 0 | 0 | 0 | instruction fetch |
| 0 | 0 | 1 | read memory |
| 0 | 1 | 0 | write memory |
| 0 | 1 | 1 | passive |

So an 8088 in *minimum* mode can distinguish an instruction fetch from a data read, which an 8086 in
minimum mode cannot.

### 3.3 `M/IO#` is inverted to `IO/M#`

On the 8086, pin 28 is **high for memory**. On the 8088 it is **low for memory**.

This is a genuine incompatibility and a classic source of bugs when a design is converted. Every
decoder that includes this signal needs its polarity flipped.

Why did Intel invert it? Compatibility with the **8085**, which had an `IO/M#` pin with that
polarity. The 8088 was aimed at customers upgrading 8085 boards.

---

## 4. Why the 8088 is slower

Everything follows from one fact: **the 8088 moves one byte per bus cycle where the 8086 moves two.**

### 4.1 Instruction fetching

A typical 8086 instruction is about 2.5 bytes and takes roughly 10 clocks to execute.

**8086:** one fetch cycle (4 clocks) brings 2 bytes. To feed a 2.5-byte instruction it needs 1.25
fetch cycles = 5 clocks of bus time per 10 clocks of execution. The bus is busy half the time, and
there is slack for operand accesses.

**8088:** one fetch cycle (4 clocks) brings 1 byte. A 2.5-byte instruction needs 2.5 fetch cycles =
10 clocks of bus time per 10 clocks of execution. **The bus is saturated by instruction fetch
alone**, with nothing left for operands.

So the 8088's queue is usually empty and the EU is usually waiting. That is why its queue is only 4
bytes — a deeper one would never fill.

### 4.2 Data access

Every 16-bit memory access is two bus cycles on an 8088, always. `mov ax, [si]` costs 8 clocks of bus
time instead of 4.

### 4.3 The measured result

On mixed code, an 8088 runs roughly **25–40% slower** than an 8086 at the same clock. The exact
figure depends heavily on the code:

| Code character | 8088 penalty |
|----------------|-------------|
| Register-only arithmetic in a tight loop (fits in the queue) | small — 5–10% |
| Byte operations on memory | moderate — 20% |
| Word operations on memory | large — 40%+ |
| `REP MOVSW` block copy | **~50%** — every word is two cycles |
| Long instructions (many prefix/displacement bytes) | large — fetch-bound |

One practical consequence: on an 8088, **`REP MOVSB` and `REP MOVSW` copy at the same speed** (the
word version does two byte-cycles), whereas on an 8086 `MOVSW` is nearly twice as fast. Code
optimised for a PC therefore looks different from code optimised for a 16-bit 8086 board.

---

## 5. Alignment does not matter on an 8088

On an 8086, a word at an odd address costs an extra bus cycle (Chapter 10 §4). On an 8088, **every**
word costs two cycles regardless, so there is no penalty for misalignment and no benefit to
`align 2`.

This is why so much PC-era code is carelessly aligned: it cost nothing on the machine it was written
for. Run that code on an 80286 or an 8086 and it is measurably slower.

The reverse also holds: code you write today with `align 2` costs nothing on an 8088 and helps
everywhere else. Keep the habit.

---

## 6. The automatic I/O wait state

The 8088 inserts **one wait state into every I/O bus cycle**, making them 5 clocks minimum instead of
4. The 8086 does not.

Rationale: the peripherals of the era were 8080-generation parts with slow bus timing, and IBM's
designers wanted the margin without adding external logic.

The practical effect is that software delay loops calibrated on a PC — "this `OUT` takes about 1 µs"
— are wrong on an 8086 board, generally too short. Where a peripheral needs recovery time, use the
explicit `jmp $+2` idiom of Chapter 17 §7 rather than relying on instruction timing.

---

## 7. The 80188 and the other family members

| Part | Bus | Queue | Notes |
|------|-----|-------|-------|
| 8086 | 16-bit | 6 bytes | the original |
| 8088 | 8-bit | 4 bytes | the IBM PC |
| 80186 | 16-bit | 6 bytes | +10 instructions, +integrated peripherals |
| 80188 | 8-bit | 4 bytes | 80186 with an 8-bit bus |
| 80C86/80C88 | as above | as above | CMOS, fully static — can be clocked down to DC |

The CMOS versions matter for one reason: **being fully static, they can be single-stepped by stopping
the clock**, which an NMOS 8086 cannot (Chapter 12 §1.3). If you are building hardware to experiment
with, an 80C88 is far more pleasant to debug.

---

## 8. Which one is "the" 8086 for study purposes

Textbooks and exam syllabi teach the **8086**, because:

- it is the architecturally clean version — the banks and `BHE#` illustrate how a 16-bit bus works;
- the timing arithmetic is simpler;
- the differences are easy to state afterwards, as this chapter does.

Period software and most practical DOS programming assumes the **8088**, because that is what the
PC had. In practice this affects only performance reasoning and the port addresses of §3 in
Chapter 17.

**Everything in Parts III and IV of this book applies identically to both chips.** The instruction
set, the encodings, the flags and the DOS/BIOS interface do not change. Only Part II's hardware and
Chapter 32's cycle counts differ, and where they do, this book says so.

---

## 9. Summary

```
  8088 = 8086 with an 8-bit external data bus and a 4-byte queue.
  Software-identical: same registers, flags, instructions, addressing, segmentation.

  three pin changes:
     pins 8-15   AD8-AD15 (multiplexed)  ->  A8-A15 (address only)
     pin 34      BHE/S7                  ->  SS0    (status)
     pin 28      M/IO                    ->  IO/M   (INVERTED)

  board saving:  two latches instead of three, one transceiver instead of two

  performance:  every 16-bit access takes two bus cycles
                instruction fetch alone saturates the bus
                25-40% slower on mixed code, ~50% on REP MOVSW
                no alignment penalty, because everything is already slow
                one automatic wait state on every I/O cycle

  IBM chose it for the 5150 because 8-bit support chips were cheap,
  which is why the world's dominant architecture descends from the
  budget variant of a compromise design.
```

---

## Exercises

**18.1** Name the three pins that differ between the 8086 and the 8088, and say what each becomes.

**18.2** An 8086 design's memory decoder uses `M/IO#` directly as a 74LS138 `G1` enable. What must
change to convert the design to an 8088?

**18.3** How many 74LS373 latches and 74LS245 transceivers does an 8088 system need, and why is that
fewer than an 8086 system?

**18.4** Why is the 8088's instruction queue 4 bytes rather than 6? Give the reasoning in terms of
bus bandwidth.

**18.5** An instruction averages 2.5 bytes and 10 clocks. Show arithmetically why the 8088's bus is
saturated by instruction fetch alone while the 8086's is not.

**18.6** How many bus cycles does `mov ax, [0x0200]` take on an 8086? On an 8088? What about
`mov ax, [0x0201]`?

**18.7** Why does `align 2` make no difference on an 8088?

**18.8** `REP MOVSW` copies 1000 words. How many bus cycles does the copy itself take on each chip?

**18.9** A delay loop calibrated on an IBM PC runs too fast on an 8086 trainer board at the same
clock frequency. Give two reasons.

**18.10** Can a program determine at run time whether it is executing on an 8086 or an 8088? If so,
describe a method; if not, explain why not.

**18.11** Why is `IO/M#` inverted relative to `M/IO#`? What earlier chip does the 8088 match?

**18.12** Which of the following differ between the two chips: the number of interrupt vectors, the
addressing modes, the reset address, the instruction queue depth, the clock duty cycle requirement,
the number of memory banks?

Answers in [Appendix H](H-exercise-solutions.md#chapter-18).

---

[← I/O interfacing](17-io-interfacing.md) · [Contents](README.md) · [**Part III begins: Addressing modes →**](19-addressing-modes.md)
