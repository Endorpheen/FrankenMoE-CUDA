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

## What is wrong so far

First measurement on a single block: reference `0`, Goose `5.8e17`. Two facts to resolve, in this order:

1. the **reference returns 0** on my synthetic block, which is suspicious in itself — either the synthetic
   block is invalid for this path or I am calling the reference with the wrong convention;
2. the Goose result is huge, which points at the scale unpack or at the fp16 header decode.

The obvious 32-bit-vs-64-bit word bug was already found and fixed (the packed scales were being read as
64-bit words); the remaining difference is not that.

## Next

Fix the synthetic block first so the reference gives a non-zero, then re-check the unpack. The gate stays
bit-exact: an approximate match is not a port.
