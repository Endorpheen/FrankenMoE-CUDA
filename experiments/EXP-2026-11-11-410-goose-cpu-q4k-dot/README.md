# EXP410: Q4_K x Q8_K dot in Goose (APEX.CPU_MATH) — in progress

## Status

Module compiles and the gate exists. **It does not match the reference yet** (160 of 160 cases differ), so
nothing is claimed: no integration, no route, no acceptance.

## What is built

| piece | state |
|---|---|
| Goose module | full super-block path: packed 6-bit scale/min unpack with the reference's three masks, nibble unpack, integer lane accumulation, `d * aux32` into eight float lanes, `- dmin * Σ(bsum*min)`, then the lane sum |
| bridge | byte/int16/uint32 access, fp16 decode and float-bit exchange; layout sizes checked at compile time (`144` and `292`) |
| gate | 160 cases over nb 1, 2, 3, 7 against `ggml_vec_dot_q4_K_q8_K_generic`, bit-for-bit comparison |

## What has been fixed so far

| defect | evidence | fix |
|---|---|---|
| the reference returned 0 for every input | with `ggml_cpu_init()` a plain block gives 256, without it 0 — the dispatch table was empty | gate calls `ggml_cpu_init()` first |
| the packed scales were read as 64-bit words | scale unpack produced values like 5.8e17 | 32-bit word reads |
| mins were looked up at byte 16 | the reference keeps `scales` in `utmp[0..1]` and `mins` in `utmp[2..3]`, i.e. mins start at byte **8** | offset corrected |

## Instrumented measurement

The bridge prints the raw result bits: for the single-block probe the module returns `1560351732` =
`0x5D00…`, i.e. the `5.81e17` value, while the reference gives `225.75`. The fp16 decode is verified, the
mins offset is fixed, the scale words are read as 32-bit. So the corruption happens inside the module's
block loop, not in the data exchange.

A float of `0x5D00…` is around 2^59, which is not a plausible accumulator for 256 products of small
integers — it looks like a bit pattern rather than an arithmetic result, so the next probe is to write the
intermediate `d`, `sumi` and `sums[0]` back through the scratch buffer and read them in the bridge.

## What is still open

One block still differs: reference `225.75`, Goose `5.81e17`. With the three defects above ruled out, the
remaining suspects are the per-lane accumulation (integer lanes vs the reference's `aux32[8]` layout) and the
float-bit exchange through the bridge. The fp16 decode itself is verified separately (0x3C00 → 1.0,
0x3000 → 0.125, 0x2c00 → 0.0625).

Nothing is integrated until the gate is 0 failures.

## Next

Fix the synthetic block first so the reference gives a non-zero, then re-check the unpack. The gate stays
bit-exact: an approximate match is not a port.

## Fourth measurement: the scale/min bytes are right, the min term is not

Dumped the unpacked bytes for the probe block:

    sc[0..15] = 11 22 33 04 09 0a 0b 1c 15 26 37 08 19 1a 1b 2c

Compared against the reference's `utmp` arithmetic by hand for this block: `ut0 & 0x3f3f3f3f` keeps `04`
because `0x04 & 0x3f = 0x04`, so my earlier suspicion that the mask was dropped was wrong — the bytes are as
the reference computes them.

Hand-computing `sumi` for this block from those mins and the probe's `bsums = i-8` gives about **-172**, but
the module reports **35182**. So the per-subblock min term is where the disagreement now sits, and the next
probe is to dump `bsums[0..3]` and `mins[0..3]` as the loop sees them.

Byte stores are fixed (a 64-bit store per byte was overwriting neighbours), the reference is initialised
with `ggml_cpu_init()`, and `d` is confirmed (0.125 x 0.5 = 0.0625).

## Fifth measurement: two more layout bugs, result now plausible but still wrong

| defect | how it showed | fix |
|---|---|---|
| `bsums` read at byte 4 | probe bsums came back as -259/255/513/-765 instead of -8/-7/-6/-5 | `block_q8_K` puts `bsums` after `qs[256]`, i.e. at byte **260** |
| nibbles stored with a 64-bit store | `acc[0]` was -6.6e18 | byte store and byte load |

After both fixes: `sumi` = **-172**, exactly the value computed by hand from the dumped mins, and the
result moved from 5.81e17 to **-36.75** against a reference of **225.75**. So the min term is now correct
and the remaining error is in the per-lane accumulation.

`acc[0]` still reads -3.4e12 while its plausible magnitude is ~4e6, so the next probe is the first
iteration's `scale`, `qv` and `xv` values.
