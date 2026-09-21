# Appendix E — Pin reference

[← Appendix D](D-ascii-table.md) · [Contents](README.md) · [Appendix F →](F-timing-electrical.md)

---

Pinouts for every chip in the book, in one place. Chapter 11 explains the 8086's pins in detail;
Part V covers the peripherals.

---

## 1. 8086 — 40-pin DIP

```
                     ┌──────∪──────┐
             GND  1 ─┤             ├─ 40  VCC  (+5 V)
            AD14  2 ─┤             ├─ 39  AD15
            AD13  3 ─┤             ├─ 38  A16/S3
            AD12  4 ─┤             ├─ 37  A17/S4
            AD11  5 ─┤             ├─ 36  A18/S5
            AD10  6 ─┤             ├─ 35  A19/S6
             AD9  7 ─┤             ├─ 34  BHE/S7
             AD8  8 ─┤             ├─ 33  MN/MX
             AD7  9 ─┤    8086     ├─ 32  RD
             AD6 10 ─┤             ├─ 31  HOLD     (RQ/GT0)
             AD5 11 ─┤             ├─ 30  HLDA     (RQ/GT1)
             AD4 12 ─┤             ├─ 29  WR       (LOCK)
             AD3 13 ─┤             ├─ 28  M/IO     (S2)
             AD2 14 ─┤             ├─ 27  DT/R     (S1)
             AD1 15 ─┤             ├─ 26  DEN      (S0)
             AD0 16 ─┤             ├─ 25  ALE      (QS0)
             NMI 17 ─┤             ├─ 24  INTA     (QS1)
            INTR 18 ─┤             ├─ 23  TEST
             CLK 19 ─┤             ├─ 22  READY
             GND 20 ─┤             ├─ 21  RESET
                     └─────────────┘
```

Names in brackets are the **maximum-mode** function (`MN/MX#` grounded).

| Pin | Name | Dir | Function |
|-----|------|-----|----------|
| 1, 20 | `GND` | — | **both must be connected** |
| 2–16, 39 | `AD15`–`AD0` | I/O, 3-state | address in T1, data in T2–T4 |
| 17 | `NMI` | in | non-maskable interrupt, **rising edge**, always type 2 |
| 18 | `INTR` | in | maskable interrupt, **level**, sampled at the end of each instruction |
| 19 | `CLK` | in | 33% duty cycle, MOS levels, 2–10 MHz |
| 21 | `RESET` | in | active high, ≥ 4 clocks |
| 22 | `READY` | in | low inserts wait states |
| 23 | `TEST#` | in | polled only by `WAIT`; **tie low if unused** |
| 24 | `INTA#` / `QS1` | out | interrupt acknowledge / queue status |
| 25 | `ALE` / `QS0` | out | **address latch enable** — never tri-stated |
| 26 | `DEN#` / `S0#` | out, 3-state | data enable |
| 27 | `DT/R#` / `S1#` | out, 3-state | data transmit/receive |
| 28 | `M/IO#` / `S2#` | out, 3-state | **high = memory** |
| 29 | `WR#` / `LOCK#` | out, 3-state | write strobe |
| 30 | `HLDA` / `RQ/GT1#` | out | hold acknowledge |
| 31 | `HOLD` / `RQ/GT0#` | in | bus request; **tie low if unused** |
| 32 | `RD#` | out, 3-state | read strobe — **both modes** |
| 33 | `MN/MX#` | in | +5 V = minimum, GND = maximum. **Never float.** |
| 34 | `BHE#`/`S7` | out, 3-state | high-bank enable in T1 |
| 35–38 | `A19/S6`–`A16/S3` | out, 3-state | address in T1, status in T2–T4 |
| 40 | `VCC` | — | +5 V ±10%, up to 360 mA |

### 1.1 Status codes

**`S4 S3` (during T2–T4)** — which segment register is in use:

| `S4` | `S3` | Segment |
|------|------|---------|
| 0 | 0 | `ES` |
| 0 | 1 | `SS` |
| 1 | 0 | `CS`, or none |
| 1 | 1 | `DS` |

**`S2# S1# S0#` (maximum mode)** — the bus cycle type:

| `S2#` | `S1#` | `S0#` | Cycle |
|-------|-------|-------|-------|
| 0 | 0 | 0 | interrupt acknowledge |
| 0 | 0 | 1 | read I/O |
| 0 | 1 | 0 | write I/O |
| 0 | 1 | 1 | halt |
| 1 | 0 | 0 | **instruction fetch** |
| 1 | 0 | 1 | read memory |
| 1 | 1 | 0 | write memory |
| 1 | 1 | 1 | passive — no cycle |

**`QS1 QS0`** — what the queue did last clock:

| `QS1` | `QS0` | Meaning |
|-------|-------|---------|
| 0 | 0 | no operation |
| 0 | 1 | first byte of an opcode taken |
| 1 | 0 | **queue emptied** — a jump happened |
| 1 | 1 | a subsequent byte taken |

---

## 2. 8088 — the differences

Same 40-pin package, same pin numbers, three changes:

| Pin | 8086 | **8088** |
|-----|------|----------|
| 2–8 | `AD14`–`AD8` | **`A14`–`A8`** — address only, not multiplexed |
| 39 | `AD15` | **`A15`** |
| 34 | `BHE#/S7` | **`SS0#`** — a status line |
| 28 | `M/IO#` | **`IO/M#`** — **inverted** |

`IO/M#` with `DT/R#` and `SS0#` encodes the cycle type in minimum mode, the way `S2#S1#S0#` does in
the 8086's maximum mode. Chapter 18 §3.

---

## 3. 8284A clock generator — 18-pin DIP

```
                  ┌────∪────┐
          CSYNC 1 ─┤         ├─ 18  VCC
           PCLK 2 ─┤         ├─ 17  X1
           AEN1 3 ─┤         ├─ 16  X2
           RDY1 4 ─┤  8284A  ├─ 15  ASYNC
          READY 5 ─┤         ├─ 14  EFI
           RDY2 6 ─┤         ├─ 13  F/C
           AEN2 7 ─┤         ├─ 12  OSC
            CLK 8 ─┤         ├─ 11  RES
            GND 9 ─┤         ├─ 10  RESET
                  └─────────┘
```

| Pin | Name | Dir | Function |
|-----|------|-----|----------|
| 1 | `CSYNC` | in | clock sync, for multiple 8284As |
| 2 | `PCLK` | out | **peripheral clock** — crystal ÷ 6, 50%, TTL |
| 3, 7 | `AEN1#`, `AEN2#` | in | qualify the ready inputs |
| 4, 6 | `RDY1`, `RDY2` | in | ready inputs |
| 5 | `READY` | out | **synchronised ready** → the 8086 |
| 8 | `CLK` | out | **crystal ÷ 3, 33% duty, MOS** → the 8086 |
| 10 | `RESET` | out | **synchronised reset** → the 8086 |
| 11 | `RES#` | in | raw reset, from an RC network |
| 12 | `OSC` | out | the raw crystal frequency, TTL |
| 13 | `F/C#` | in | 0 = crystal, 1 = `EFI` |
| 14 | `EFI` | in | external frequency input |
| 15 | `ASYNC#` | in | low = two-stage ready synchronisation |
| 16, 17 | `X2`, `X1` | — | **crystal, at 3× the CPU frequency** |

For a single-master 5 MHz system: 15 MHz crystal, `F/C#` = GND, `ASYNC#` = GND, `RDY1` = high,
`AEN1#` = GND, `RDY2` = GND, `AEN2#` = +5 V.

---

## 4. 8288 bus controller — 20-pin DIP

| Pin | Name | Dir | Function |
|-----|------|-----|----------|
| 1 | `IOB` | in | I/O bus mode |
| 2 | `CLK` | in | **the same `CLK` the 8086 sees** |
| 3, 18, 19 | `S2#`, `S1#`, `S0#` | in | status from the 8086 |
| 4 | `DT/R#` | out | data direction |
| 5 | `ALE` | out | address latch enable |
| 6 | `AEN#` | in | address enable |
| 7 | `MRDC#` | out | **memory read** |
| 8 | `AMWC#` | out | advanced memory write |
| 9 | `MWTC#` | out | **memory write** |
| 11 | `IOWC#` | out | **I/O write** |
| 12 | `AIOWC#` | out | advanced I/O write |
| 13 | `IORC#` | out | **I/O read** |
| 14 | `INTA#` | out | interrupt acknowledge |
| 15 | `MCE/PDEN#` | out | master cascade / peripheral data enable |
| 16 | `DEN` | out | **data enable — ACTIVE HIGH**, unlike the 8086's `DEN#` |
| 17 | `CEN` | in | command enable |

Single-master configuration: `IOB` = GND, `AEN#` = GND, `CEN` = +5 V.

**`DEN` needs an inverter before a 74LS245's `OE#`.** Chapter 15 §3.3.

---

## 5. 8255A PPI — 40-pin DIP

```
                     ┌──────∪──────┐
             PA3  1 ─┤             ├─ 40  PA4
             PA2  2 ─┤             ├─ 39  PA5
             PA1  3 ─┤             ├─ 38  PA6
             PA0  4 ─┤             ├─ 37  PA7
              RD  5 ─┤             ├─ 36  WR
              CS  6 ─┤             ├─ 35  RESET
             GND  7 ─┤             ├─ 34  D0
              A1  8 ─┤             ├─ 33  D1
              A0  9 ─┤   8255A     ├─ 32  D2
             PC7 10 ─┤             ├─ 31  D3
             PC6 11 ─┤             ├─ 30  D4
             PC5 12 ─┤             ├─ 29  D5
             PC4 13 ─┤             ├─ 28  D6
             PC0 14 ─┤             ├─ 27  D7
             PC1 15 ─┤             ├─ 26  VCC
             PC2 16 ─┤             ├─ 25  PB7
             PC3 17 ─┤             ├─ 24  PB6
             PB0 18 ─┤             ├─ 23  PB5
             PB1 19 ─┤             ├─ 22  PB4
             PB2 20 ─┤             ├─ 21  PB3
                     └─────────────┘
```

| `A1` | `A0` | Register |
|------|------|----------|
| 0 | 0 | Port A |
| 0 | 1 | Port B |
| 1 | 0 | Port C |
| 1 | 1 | **Control (write only)** |

Chapter 47.

---

## 6. 8253/8254 timer — 24-pin DIP

| Pin | Name | Dir | Function |
|-----|------|-----|----------|
| 1–8 | `D7`–`D0` | I/O | data bus |
| 9 | `CLK0` | in | counter 0 clock |
| 10 | `OUT0` | out | counter 0 output |
| 11 | `GATE0` | in | counter 0 gate |
| 12 | `GND` | — | |
| 13 | `OUT1` | out | |
| 14 | `GATE1` | in | |
| 15 | `CLK1` | in | |
| 16 | `GATE2` | in | |
| 17 | `OUT2` | out | |
| 18 | `CLK2` | in | |
| 19 | `A0` | in | register select |
| 20 | `A1` | in | register select |
| 21 | `CS#` | in | chip select |
| 22 | `RD#` | in | |
| 23 | `WR#` | in | |
| 24 | `VCC` | — | |

| `A1` | `A0` | Register |
|------|------|----------|
| 0 | 0 | Counter 0 |
| 0 | 1 | Counter 1 |
| 1 | 0 | Counter 2 |
| 1 | 1 | **Control (write only)** |

Chapter 48.

---

## 7. 8259A interrupt controller — 28-pin DIP

| Pin | Name | Dir | Function |
|-----|------|-----|----------|
| 1 | `CS#` | in | chip select |
| 2 | `WR#` | in | |
| 3 | `RD#` | in | |
| 4–11 | `D7`–`D0` | I/O | data bus — **this is how the type number is delivered** |
| 12–14 | `CAS0`–`CAS2` | I/O | cascade lines |
| 15 | `GND` | — | |
| 16 | `SP#/EN#` | I/O | slave program / enable buffer |
| 17 | `INT` | out | **→ the 8086's `INTR`** |
| 18–25 | `IR7`–`IR0` | in | **the eight interrupt requests** |
| 26 | `INTA#` | in | **← the 8086's `INTA#`** |
| 27 | `A0` | in | register select |
| 28 | `VCC` | — | |

| `A0` | Write | Read |
|------|-------|------|
| 0 | ICW1, OCW2, OCW3 | IRR / ISR (after OCW3) |
| 1 | ICW2, ICW3, ICW4, OCW1 | IMR |

PC ports: master `20h`/`21h`, slave `A0h`/`A1h`. Chapter 49.

---

## 8. 8251A USART — 28-pin DIP

| Pin | Name | Dir | Function |
|-----|------|-----|----------|
| 1–4, 27–28, 5–6 | `D7`–`D0` | I/O | data bus |
| 8 | `TxD` | out | **transmit data** |
| 9 | `TxRDY` | out | transmit buffer empty |
| 10 | `GND` | — | |
| 11 | `RxD` | in | **receive data** |
| 12 | `RxRDY` | out | a character has been received |
| 13 | `TxE` | out | transmitter fully empty |
| 14 | `SYNDET/BD` | I/O | sync detect / break detect |
| 15 | `CLK` | in | internal timing, ≥ 30× the baud rate |
| 16 | `RESET` | in | |
| 17 | `C/D#` | in | **0 = data, 1 = control/status** |
| 18 | `RD#` | in | |
| 19 | `WR#` | in | |
| 20 | `CS#` | in | |
| 21 | `TxC#` | in | **transmit clock — baud × the mode factor** |
| 22 | `RxC#` | in | **receive clock** |
| 23 | `DSR#` | in | data set ready |
| 24 | `DTR#` | out | data terminal ready |
| 25 | `RTS#` | out | request to send |
| 26 | `CTS#` | in | clear to send |

Chapter 50.

---

## 9. 8237A DMA controller — 40-pin DIP

| Pin | Name | Dir | Function |
|-----|------|-----|----------|
| 1 | `IOR#` | I/O | I/O read |
| 2 | `IOW#` | I/O | I/O write |
| 3 | `MEMR#` | out | memory read |
| 4 | `MEMW#` | out | memory write |
| 6 | `READY` | in | |
| 7 | `HLDA` | in | **← the 8086's hold acknowledge** |
| 9 | `RESET` | in | |
| 10 | `DACK2#` | out | DMA acknowledge, channel 2 |
| 11 | `DACK3#` | out | |
| 12 | `DREQ3` | in | DMA request, channel 3 |
| 13–15 | `DREQ2`–`DREQ0` | in | |
| 16–23 | `DB0`–`DB7` | I/O | data bus |
| 24 | `EOP#` | I/O | end of process / terminal count |
| 25–32 | `A0`–`A7` | I/O | address |
| 33–36 | `A8`–`A11` | out | |
| 37 | `DACK0#` | out | |
| 38 | `DACK1#` | out | |
| 39 | `AEN` | out | address enable |
| 40 | `HRQ` | out | **→ the 8086's `HOLD`** |

Chapter 51.

---

## 10. Memory and glue

### 10.1 28-pin JEDEC memory (2764, 27128, 27256, 6264, 62256)

```
         VPP/A14  1 ─┐   ┌─ 28  VCC
             A12  2 ─┤   ├─ 27  WE / A13 / PGM
              A7  3 ─┤   ├─ 26  A13 / CS2 / NC
              A6  4 ─┤   ├─ 25  A8
              A5  5 ─┤   ├─ 24  A9
              A4  6 ─┤   ├─ 23  A11
              A3  7 ─┤   ├─ 22  OE
              A2  8 ─┤   ├─ 21  A10
              A1  9 ─┤   ├─ 20  CE / CS1
              A0 10 ─┤   ├─ 19  D7
              D0 11 ─┤   ├─ 18  D6
              D1 12 ─┤   ├─ 17  D5
              D2 13 ─┤   ├─ 16  D4
             GND 14 ─┘   └─ 15  D3
```

**Drive `CE#` from the address decoder and `OE#` from `RD#`** — Chapter 16 §1.2.

### 10.2 74LS373 — octal transparent latch

| Pin | Function |
|-----|----------|
| 1 | `OE#` — **tie to GND** for address latching |
| 11 | `LE` — **tie to `ALE`** |
| 3, 4, 7, 8, 13, 14, 17, 18 | `D0`–`D7` |
| 2, 5, 6, 9, 12, 15, 16, 19 | `Q0`–`Q7` |
| 10 | GND |
| 20 | VCC |

### 10.3 74LS245 — octal bus transceiver

| Pin | Function |
|-----|----------|
| 1 | `DIR` — **tie to `DT/R#`**; 1 = A→B |
| 19 | `OE#` — **tie to `DEN#`** |
| 2–9 | `A1`–`A8` |
| 11–18 | `B8`–`B1` |
| 10 | GND |
| 20 | VCC |

### 10.4 74LS138 — 3-to-8 decoder

| Pin | Function |
|-----|----------|
| 1, 2, 3 | `A`, `B`, `C` — select inputs, `C` most significant |
| 4, 5 | `G2A#`, `G2B#` — **active-low enables** |
| 6 | `G1` — active-high enable |
| 7, 9–15 | `Y7#`, `Y6#`–`Y0#` — active-low outputs |
| 8 | GND |
| 16 | VCC |

Enabled only when `G1 = 1` **and** `G2A# = 0` **and** `G2B# = 0`.

---

## 11. The tie-off checklist

Every input that is not driven must be tied to a defined level. Floating inputs are the commonest
cause of a board that almost works.

```
   8086   MN/MX#  (33)  -> +5 V for minimum mode, GND for maximum
          NMI     (17)  -> GND if unused
          INTR    (18)  -> GND if unused, or the 8259A's INT
          TEST#   (23)  -> GND if there is no 8087
          HOLD    (31)  -> GND if there is no DMA
          READY   (22)  -> from the 8284A; never leave floating
          both GND pins (1, 20) connected

   8284A  F/C#    -> GND to use the crystal
          ASYNC#  -> GND
          RDY1    -> +5 V for no wait states
          AEN1#   -> GND
          RDY2    -> GND
          AEN2#   -> +5 V (disabled)

   8288   IOB, AEN# -> GND
          CEN       -> +5 V

   EPROM  VPP, PGM# -> +5 V in normal operation

   74LS   every unused input tied high or low
```

Plus a **0.1 µF decoupling capacitor across the supply pins of every chip**, as close to the package
as the layout allows.

---

[← Appendix D](D-ascii-table.md) · [Contents](README.md) · [Appendix F →](F-timing-electrical.md)
