# Appendix F — Timing and electrical data

[← Appendix E](E-pin-reference.md) · [Contents](README.md) · [Appendix G →](G-glossary.md)

---

The numbers a real design needs. Chapter 13 explains how to use them; this is the table to work
from.

---

## 1. How to read a timing parameter name

Every parameter is `tXXYY` — "from X to Y".

| Fragment | Means |
|----------|-------|
| `CL` | **CL**ock going **L**ow |
| `CH` | **C**lock going **H**igh |
| `AV` | **A**ddress **V**alid |
| `AX` | **A**ddress invalid (X = don't care) |
| `DV` | **D**ata **V**alid |
| `DX` | **D**ata invalid |
| `RL` / `RH` | `RD#` going **L**ow / **H**igh |
| `WL` / `WH` | `WR#` going **L**ow / **H**igh |
| `LL` / `LH` | `A`**L**`E` going **L**ow / **H**igh |
| `RYH` | `READY` going **H**igh |

So `tCLAV` is "clock low to address valid" and `tDVCL` is "data valid before clock low".

### 1.1 Maximum versus minimum

**A maximum is a promise the 8086 makes.** `tCLAV(max) = 110 ns` means the address will be valid no
later than 110 ns after the clock edge. Design assuming the worst.

**A minimum is a requirement the 8086 imposes.** `tDVCL(min) = 30 ns` means you must present data at
least 30 ns before it samples. Design to exceed it.

**If the signal is an 8086 output, the number is a promise. If it is an input, the number is a
demand.** Mixing those up is the commonest error in timing analysis.

---

## 2. 8086 AC characteristics, 5 MHz

| Parameter | Description | Min | Max | Unit |
|-----------|-------------|-----|-----|------|
| `TCLCL` | **clock period** | 200 | 500 | ns |
| `TCLCH` | clock **low** time | 118 | — | ns |
| `TCHCL` | clock **high** time | 69 | — | ns |
| `TCH1CH2` | clock rise time | — | 10 | ns |
| `TCL2CL1` | clock fall time | — | 10 | ns |
| `TDVCL` | **data setup** before the sampling edge | 30 | — | ns |
| `TCLDX` | **data hold** after it | 10 | — | ns |
| `TR1VCL` | `READY` setup (8284A input) | 35 | — | ns |
| `TRYHCH` | `READY` setup to the clock high edge | 118 | — | ns |
| `TCHRYX` | `READY` hold | 30 | — | ns |
| `TINVCH` | `INTR`, `NMI`, `TEST#` setup | 30 | — | ns |
| `TILIH` | input rise time | — | 20 | ns |
| `TCLAV` | **clock low → address valid** | 10 | **110** | ns |
| `TCLAX` | address hold after clock low | 10 | — | ns |
| `TCLAZ` | clock low → address float | 10 | 80 | ns |
| `TLHLL` | **`ALE` pulse width** | 67 | — | ns |
| `TCHLH` | clock high → `ALE` high | — | 80 | ns |
| `TCHLL` | clock high → `ALE` low | — | 85 | ns |
| `TAVAL` | **address valid before `ALE` falls** | 60 | — | ns |
| `TLLAX` | address hold after `ALE` falls | 50 | — | ns |
| `TCLDV` | clock low → data valid (write) | 10 | 110 | ns |
| `TCHDX` | data hold after clock high | 10 | — | ns |
| `TCVCTV` | clock → control valid | 10 | 110 | ns |
| `TCVCTX` | control hold | 10 | — | ns |
| `TCLRL` | **clock low → `RD#` low** | 10 | 165 | ns |
| `TCLRH` | clock low → `RD#` high | 10 | 150 | ns |
| `TRLRH` | **`RD#` pulse width** | 325 | — | ns |
| `TRHAV` | `RD#` high → next address active | 85 | — | ns |
| `TCLWL` | clock low → `WR#` low | 10 | 165 | ns |
| `TCLWH` | clock low → `WR#` high | 10 | 150 | ns |
| `TWLWH` | **`WR#` pulse width** | 340 | — | ns |
| `TWHDX` | **data hold after `WR#` rises** | 88 | — | ns |
| `TCLDOX` | data hold after clock low | 10 | — | ns |
| `TAZRL` | address float → `RD#` low | 0 | — | ns |
| `TDXDL` | `DEN#` inactive → `DT/R#` low | 0 | — | ns |

### 2.1 The same parameters at 8 and 10 MHz

| Parameter | 5 MHz | 8 MHz (8086-2) | 10 MHz (8086-1) |
|-----------|-------|----------------|-----------------|
| `TCLCL` min | 200 | 125 | 100 |
| `TCLCH` min | 118 | 68 | 53 |
| `TCHCL` min | 69 | 44 | 39 |
| `TCLAV` max | 110 | 60 | 50 |
| `TDVCL` min | 30 | 20 | 15 |
| `TCLDX` min | 10 | 10 | 10 |
| `TRLRH` min | 325 | 205 | 150 |
| `TWLWH` min | 340 | 220 | 165 |
| `TWHDX` min | 88 | 68 | 55 |

---

## 3. The memory-access budget

The calculation that decides whether a memory chip is fast enough. Chapter 13 §6.

### 3.1 From address valid

```
   available  =  3 × TCLCL  −  TCLAV(max)  −  TDVCL(min)
```

| Clock | 3 × `TCLCL` | − `TCLAV` | − `TDVCL` | = available |
|-------|-------------|-----------|-----------|-------------|
| 5 MHz | 600 | 110 | 30 | **460 ns** |
| 8 MHz | 375 | 60 | 20 | **295 ns** |
| 10 MHz | 300 | 50 | 15 | **235 ns** |

Then subtract the glue logic on the critical path:

| Component | Typical delay |
|-----------|---------------|
| 74LS373 latch | 30 ns |
| 74LS138 decoder | 30 ns |
| 74LS32 gate (bank split) | 15 ns |
| 74LS245 transceiver (return path) | 12 ns |
| PCB traces and connectors | 5 ns |
| **Total** | **~90 ns** |

| Clock | Available to the memory chip |
|-------|------------------------------|
| 5 MHz | **~370 ns** |
| 8 MHz | ~205 ns |
| 10 MHz | ~145 ns |

### 3.2 With wait states

Each wait state adds one full `TCLCL`:

| Wait states | 5 MHz | 8 MHz | 10 MHz |
|-------------|-------|-------|--------|
| 0 | 370 ns | 205 ns | 145 ns |
| 1 | 570 ns | 330 ns | 245 ns |
| 2 | 770 ns | 455 ns | 345 ns |
| 3 | 970 ns | 580 ns | 445 ns |

### 3.3 From `RD#` low

Some memories specify an output-enable access time instead:

```
   available  =  TRLRH(min)  −  TDVCL(min)  −  transceiver delay
              =  325 − 30 − 12  =  283 ns   at 5 MHz
```

**A chip must satisfy both constraints.** The address path usually binds for EPROMs; the `OE#` path
for fast SRAMs.

---

## 4. Memory device timings

| Device | Type | Capacity | `tACC` | `tOE` | Works at 5 MHz, 0 waits? |
|--------|------|----------|--------|-------|--------------------------|
| 2716-1 | EPROM | 2 K × 8 | 350 ns | 120 ns | yes |
| 2716 | EPROM | 2 K × 8 | 450 ns | 120 ns | **no — 1 wait** |
| 2732A-20 | EPROM | 4 K × 8 | 200 ns | 70 ns | yes |
| 2764-20 | EPROM | 8 K × 8 | 200 ns | 75 ns | yes |
| 2764-25 | EPROM | 8 K × 8 | 250 ns | 100 ns | **yes**, 120 ns margin |
| 2764-45 | EPROM | 8 K × 8 | 450 ns | 150 ns | **no — 1 wait** |
| 27128-25 | EPROM | 16 K × 8 | 250 ns | 100 ns | yes |
| 27256-20 | EPROM | 32 K × 8 | 200 ns | 75 ns | yes |
| 27256-30 | EPROM | 32 K × 8 | 300 ns | 120 ns | yes, 70 ns margin |
| 6116-12 | SRAM | 2 K × 8 | 120 ns | 60 ns | comfortably |
| 6264-12 | SRAM | 8 K × 8 | 120 ns | 60 ns | comfortably |
| 6264-15 | SRAM | 8 K × 8 | 150 ns | 70 ns | comfortably |
| 62256-10 | SRAM | 32 K × 8 | 100 ns | 50 ns | comfortably |
| 62256-15 | SRAM | 32 K × 8 | 150 ns | 70 ns | comfortably |
| 4164-15 | DRAM | 64 K × 1 | 150 ns | — | needs refresh |
| 41256-12 | DRAM | 256 K × 1 | 120 ns | — | needs refresh |

**EPROMs are the problem, and they are exactly what you need at power-on.** The usual arrangement is
no wait states for RAM and one for ROM, with the wait-state generator triggered by the ROM's chip
select (Chapter 13 §7.3).

### 4.1 SRAM write timings (6264)

| Parameter | Description | Min |
|-----------|-------------|-----|
| `tWC` | write cycle time | 150 ns |
| `tWP` | write pulse width | 90 ns |
| `tDW` | **data setup before `WE#` rises** | 60 ns |
| `tDH` | **data hold after `WE#` rises** | 0 ns |
| `tAW` | address valid to end of write | 120 ns |

The 8086 gives `TWLWH` = 340 ns of write pulse and `TWHDX` = 88 ns of data hold — comfortably
beyond all of these.

---

## 5. 8086 DC characteristics

| Parameter | Description | Min | Max | Unit |
|-----------|-------------|-----|-----|------|
| `VIL` | input low voltage | −0.5 | **0.8** | V |
| `VIH` | input high voltage | **2.0** | VCC+0.5 | V |
| `VOL` | output low voltage | — | **0.45** | V (at 2.0 mA) |
| `VOH` | output high voltage | **2.4** | — | V (at −400 µA) |
| `ICC` | supply current | — | **360** | mA |
| `ILI` | input leakage | — | ±10 | µA |
| `ILO` | output leakage (3-state) | — | ±10 | µA |
| `CIN` | input capacitance | — | 15 | pF |
| `CIO` | I/O capacitance | — | 20 | pF |

### 5.1 The clock input is not TTL

| Parameter | Min | Max |
|-----------|-----|-----|
| `VIL` (`CLK`) | −0.5 | **0.6 V** |
| `VIH` (`CLK`) | **3.9 V** | VCC+1.0 |

A 74LS gate guarantees only 2.4 V high and 0.5 V low. **It cannot drive `CLK` reliably.** This is a
real electrical reason for the 8284A, not a commercial one. Chapter 12 §1.2.

### 5.2 Drive capability

```
   sink current  (VOL = 0.45 V) :  2.0 mA
   source current (VOH = 2.4 V) :  400 µA
   capacitive load               :  100 pF
```

**The capacitance is the binding limit.** One 74LS input is about 5 pF; one MOS memory input about
10 pF; a PCB trace about 1 pF per 2 cm. Four or five devices plus the traces reaches 100 pF, and
beyond that the edges slow and your timing margin evaporates.

**Buffer the bus.** Two 74LS245s and three 74LS373s is cheap insurance, and every commercial 8086
design used them.

---

## 6. 8086 absolute maximum ratings

**Exceeding any of these can destroy the device.**

| Parameter | Rating |
|-----------|--------|
| Ambient temperature under bias | 0 °C to +70 °C |
| Storage temperature | −65 °C to +150 °C |
| Voltage on any pin with respect to GND | −1.0 V to +7 V |
| Power dissipation | 2.5 W |

The commercial-grade part is specified from 0 °C to 70 °C. Industrial (−40 to +85 °C) and military
(−55 to +125 °C) versions existed at higher cost.

---

## 7. Bus cycle summary

At 5 MHz with no wait states:

| Event | T-state | Signal |
|-------|---------|--------|
| Address valid | T1, +110 ns | `AD15`–`AD0`, `A19`–`A16`, `BHE#` |
| `ALE` high | T1 | — |
| `ALE` falls — **address latched** | end of T1 | — |
| Address removed, `AD` floats (read) | T2 | — |
| `RD#` falls | T2, +165 ns | — |
| `DEN#` falls | T2 | — |
| Data expected | T3 | — |
| `READY` sampled | T3 | — |
| Wait states inserted if `READY` is low | between T3 and T4 | — |
| Data latched by the CPU | end of T3 | needs 30 ns setup, 10 ns hold |
| `RD#` rises | T4 | — |
| `DEN#` rises | T4 | — |

**One bus cycle = 4 × 200 = 800 ns** at 5 MHz.

| Clock | Bus cycle | Bus cycles per second |
|-------|-----------|----------------------|
| 5 MHz | 800 ns | 1,250,000 |
| 8 MHz | 500 ns | 2,000,000 |
| 10 MHz | 400 ns | 2,500,000 |

---

## 8. Logic family comparison

For choosing glue logic.

| Family | Propagation delay | Supply current per gate | Notes |
|--------|------------------|------------------------|-------|
| 74 (standard TTL) | 10 ns | 2 mA | obsolete |
| **74LS** | **10 ns** | **0.4 mA** | **the standard choice for 8086 designs** |
| 74S | 3 ns | 4 mA | fast, power hungry |
| 74ALS | 4 ns | 0.2 mA | better than LS in both respects |
| 74F | 3 ns | 1 mA | fast |
| 74HC | 8 ns | ~0 static | CMOS; **inputs are not TTL compatible** |
| **74HCT** | 10 ns | ~0 static | **CMOS with TTL-compatible inputs** — use this for a new build |

**For a modern reconstruction, use 74HCT.** It is TTL-compatible, draws almost no static current,
and is still in production. 74LS is increasingly hard to buy.

---

## 9. A worked timing analysis

**Design:** 5 MHz 8086, minimum mode, 2764-25 EPROM (250 ns) and 6264-15 SRAM (150 ns), one 74LS373
(30 ns), one 74LS138 (30 ns), one 74LS32 for the bank split (15 ns), one 74LS245 (12 ns).

**Step 1 — the window.**

```
   3 × TCLCL                     600 ns
```

**Step 2 — subtract the CPU's own delays.**

```
   − TCLAV(max)                 −110 ns
   − TDVCL(min)                 − 30 ns
                                 ──────
                                  460 ns
```

**Step 3 — subtract the glue on the critical path.**

The path is: `AD` pins → 373 → 138 → 32 → memory `CE#` → memory → 245 → `AD` pins.

```
   74LS373                        30 ns
   74LS138                        30 ns
   74LS32                         15 ns
   74LS245 (return)               12 ns
   PCB                             5 ns
                                 ──────
                                   92 ns

   available to the memory   460 − 92  =  368 ns
```

**Step 4 — compare.**

| Device | `tACC` | Margin | Verdict |
|--------|--------|--------|---------|
| 6264-15 SRAM | 150 ns | **218 ns** | ✔ comfortable |
| 2764-25 EPROM | 250 ns | **118 ns** | ✔ 32% margin |

**Step 5 — check the `OE#` path.**

```
   TRLRH(min)                    325 ns
   − TDVCL(min)                 − 30 ns
   − 74LS245                    − 12 ns
                                 ──────
                                  283 ns
```

2764 `tOE` = 100 ns, 6264 `tOE` = 70 ns. ✔ Not binding.

**Step 6 — check the write path.**

```
   TWLWH = 340 ns  ≥  6264 tWP = 90 ns      ✔
   TWHDX =  88 ns  ≥  6264 tDH =  0 ns      ✔
   data valid well before WR# rises          ✔
```

**Conclusion: no wait states needed. Tie `RDY1` high.**

Had the EPROM been a 2764-45 (450 ns), 450 > 368 would fail and one wait state would give
`660 − 92 = 568 ns` — a 118 ns margin, which is fine.

---

## 10. Design margin

How much margin is enough?

| Margin | Verdict |
|--------|---------|
| < 0 | **will not work** |
| 0–10% | works on the bench, fails in the field |
| 10–20% | acceptable for a hobby build |
| **20–30%** | **a sensible target** |
| > 50% | you are paying for speed you do not need |

The numbers in §2 are already worst-case, so a positive margin is genuinely a margin. But allow for:

- **temperature** — propagation delays rise roughly 0.3% per °C;
- **supply voltage** — delays rise as VCC falls within its ±10% band;
- **device spread** — "typical" figures in a datasheet are not guarantees;
- **capacitive loading** — every delay figure assumes a specified load, usually 50 pF.

**Design with maximums, and leave 20%.**

---

[← Appendix E](E-pin-reference.md) · [Contents](README.md) · [Appendix G →](G-glossary.md)
