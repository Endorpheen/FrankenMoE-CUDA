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

## What is still open

One block still differs: reference `225.75`, Goose `5.81e17`. With the three defects above ruled out, the
remaining suspects are the per-lane accumulation (integer lanes vs the reference's `aux32[8]` layout) and the
float-bit exchange through the bridge. The fp16 decode itself is verified separately (0x3C00 → 1.0,
0x3000 → 0.125, 0x2c00 → 0.0625).

Nothing is integrated until the gate is 0 failures.

## Next

Fix the synthetic block first so the reference gives a non-zero, then re-check the unpack. The gate stays
bit-exact: an approximate match is not a port.
