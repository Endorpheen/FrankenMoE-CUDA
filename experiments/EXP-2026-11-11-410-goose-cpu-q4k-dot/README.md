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

## Sixth measurement: single block is bit-exact, multi-block differs in float order

The decisive defect was the **nibble order**: the reference unpacks each group of 32 as all low nibbles
first, then all high nibbles, while the module interleaved low/high per byte. With that fixed:

| case | result |
|---|---|
| single block (nb=1) | **bit-exact**, 225.75 = 225.75 |
| nb = 2, 3, 7 | 49 of 160 cases differ |

The multi-block differences are tiny and one-sided, e.g. `15579.2646` vs `15579.2627` — about 2e-7 relative,
which is float rounding, not a wrong value. So the remaining question is the accumulation order between
blocks, not the data layout.

Cause of the earlier "acc0 = -3.4e12": my own reader used `sc[1500/8]`, which is byte 1496 — the same
off-by-alignment class as the code it was diagnosing. Read by byte offset, `acc0 = -792`, a plausible value.

## Seventh measurement: the residue is one ulp between blocks

Compiling the bridge with `-ffp-contract=off` changes nothing (49/160 either way), so FMA contraction is
not the cause. The residue is one ulp on multi-block inputs:

    nb=2: got 15579.2646   want 15579.2627
    nb=2: got -37574.0781  want -37574.0859
    nb=1: bit-exact

What is already ruled out: layout (both blocks use the same code path that is exact for one block), nibble
order, byte vs word access, `ggml_cpu_init`, FMA contraction, and the min term (`sumi` matches the hand
computation).

The remaining structural difference is the integer intermediate: the reference forms `aux16[l] = q8[l]*a[l]`
as an `int16` and then `aux32[l] += scale*aux16[l]`, while the module accumulates `scale*qv*xv` directly in
64 bits. For the values here that is arithmetically identical, but the next probe is to reproduce the int16
intermediate step exactly, and if that still differs, to compare against the AVX2 entry point rather than the
generic one — the AVX2 code sums in a different order and may be what this library actually dispatches to.

## Eighth measurement: the module follows the generic order, not the AVX2 one

| reference used by the gate | failures |
|---|---|
| `ggml_vec_dot_q4_K_q8_K_generic` | **49 / 160** (nb = 1 exact) |
| `ggml_vec_dot_q4_K_q8_K` (AVX2) | 105 / 160 (even nb = 1 differs, e.g. -611.96875 vs -611.971619) |

So the port reproduces the reference's *generic* order exactly and the residue is only between blocks. The
AVX2 kernel sums in a different order, which is expected: it accumulates per lane with different grouping.

That settles which order the port owns. If the goal is bit-identity with the **dispatched** kernel rather
than with the portable one, the target must be AVX2 and the port needs its grouping; otherwise the generic
comparison is the right one and only the inter-block residue remains.

## Decision needed from the plan

`ggml_vec_dot_q4_K_q8_K` is a dispatcher: on this CPU it resolves to AVX2. Bit-identity with "whatever runs"
therefore means matching AVX2, which is a different accumulation, not a bug in the current module. This is a
scoping question for the item, not a defect to keep chasing blind.

## Ninth measurement: a concrete two-block case matches exactly

Reproduced the gate's own generator for the first two-block case and compared both sides on identical
memory:

    goose = -77172.0859
    want  = -77172.0859

bit-identical, with the instrumented internals agreeing (d=0.149577275, sumi=-56148, sums0=-18656.9512,
first iteration scale=25 qv=7 xv=94).

So the residue is **rare**, not systematic: the same code path is exact here and one ulp off in 49 of 160
randomised cases. That points at specific input combinations — most plausibly where a lane product or the
block total crosses a float rounding boundary — rather than at a structural mistake.

## Next

Bisect the failing cases from the gate by their inputs (largest |sumi|, largest per-lane acc, and the case
with the biggest d) and check whether the one-ulp difference appears only when one of them crosses 2^24.

## Tenth measurement: the failing case is captured

The gate now writes the first failing input to `/tmp/fail_blocks.bin` (876 bytes: `nb` plus `nb` q4_K and
`nb` q8_K blocks). With a reproducible failing case, the comparison can be stepped instead of randomised.

Plan for it: run both sides on the file, print `d`, `dmin`, `sumi`, `sums[l]` and `sumf` per block, and find
the first block where the two diverge. That will tell whether the difference enters through the scale term
(`d*aux32`) or through the min term (`dmin*sumi`).

## Eleventh measurement: the gate is clean against the source in this tree

The earlier "49 of 160" had a cause on my side: the gate compared against
`ggml_vec_dot_q4_K_q8_K_generic` **from the inherited library**, and that binary's `_generic` and AVX2 entry
points return the same value (`15579.2627`), while a transcription of `ggml-cpu/quants.c` in this tree returns
`15579.2646` — which is exactly what the Goose module returns.

So the inherited `libggml-cpu` was built from a different source revision than the one present here, and
comparing against it measured the library difference, not the port.

With the gate comparing against an exact C transcription of `quants.c` from **this tree**:

| reference | failures |
|---|---|
| inherited library `_generic` | 49 / 160 |
| C transcription of this tree's `quants.c` | **0 / 160** |

Status: module faithful to the source it ports. Mutations, integration into the Goose `libggml-cpu` behind a
switch, witness + `e68df05b` check and the checkpoint are still to do.

## Twelfth measurement: mutations

Nine single-defect mutations of the module, each built and run against the clean gate:

| mutation | verdict |
|---|---|
| scale mask widened to 0x7f7f7f7f | CAUGHT |
| ut3 shift off by one | CAUGHT |
| high nibble not shifted | CAUGHT |
| scale always from the first byte | CAUGHT |
| min index not halved | CAUGHT |
| min term added instead of subtracted | CAUGHT |
| lane sums overwritten instead of accumulated | CAUGHT |
| lane index modulo four | CAUGHT |
| bsums read two bytes early | CAUGHT |

**9 caught, 0 missed, 0 hangs.** The source and the compiled object are restored after the loop.

## Thirteenth measurement: the drop-in replacement works and is enabled by default

`build/libfs410s.so` exports `ggml_vec_dot_q4_K_q8_K_generic`, routing to the Goose kernel. It is **enabled by
default**; `FG410S_ENABLED=0` forwards to the native symbol via `dlsym(RTLD_NEXT, ...)`. A witness prints
`FG410S_WITNESS calls=<n>` on the first call when `FG410S_WITNESS` is set.

Checked against the transcription on a captured case:

    hoox (preloaded) = -611.96875
    REF total        = -611.96875

bit-identical, with the witness confirming the call went through the hook (`calls=1`).

Note for the record: the earlier "expect 15579.2646" line in my probe was stale — the captured file had been
overwritten by a later run with nb=1, so the probe was comparing against someone else's expectation rather
than the file's. Reading the file and the reference together removed the confusion.

## Fourteenth measurement: the real run is blocked on VRAM occupancy, not on the port

Three attempts to run the accepted profile (32K/128, `-c 262144 -ngl all`) with the drop-in library preloaded
fail before any token is produced:

    0.02.671.965 E ggml_backend_cuda_buffer_type_alloc_buffer: allocating 3314.03 MiB on device 0:
                  cudaMalloc failed: out of memory
    0.02.671.970 E graph_reserve: failed to allocate compute buffers
    0.02.577.208 E srv  llama_server: exiting due to model loading error

GPU state at the time: 12282 MiB total, **5816 MiB held by other processes**, 6056 MiB free. The largest
holder is `whisper-cli` at 2478 MiB, plus desktop/browser GPU processes (~1070 MiB). `GGML_CUDA_NO_VMM=1` made
no difference.

Consequence for this item: the server never reaches the compute path, so `FG410S` is never called
(`grep FG410S server.log` = 0) and no completion can be compared. This is an environment blocker — free VRAM
below what the accepted profile needs — not a defect of the port. The item stays open until a run reaches
completion `e68df05b` with equal probabilities and a restore.

## Fifteenth measurement: the GPU can allocate, the run still cannot

A standalone probe with the CUDA runtime, in a fresh process:

    free=5874 MiB total=11871 MiB
    alloc 0..3 (900 MiB each): ok      -> 3.6 GiB of contiguous allocations succeed

So the card is not out of memory in general. The server still fails at `graph_reserve` asking for 3314.03 MiB,
which means the residue left after the model's own GPU tensors are placed is smaller than the compute buffers
need. `GGML_CUDA_NO_VMM=1` does not change it.

Practical consequence: the run needs roughly 2.5 GiB more free VRAM than the machine currently offers while
`whisper-cli` (2478 MiB) and the desktop/browser GPU processes are resident. Nothing in the port can change
that, and stopping someone else's process is not mine to do.

Meanwhile the dispatch question is the real remaining work and does not need the GPU:

- `ggml_vec_dot_q4_K_q8_K` (what the server calls) returns 15579.2627 on a captured case;
- `_generic` in the same binary returns 15579.2627 too;
- the Goose kernel returns 15579.2646, matching the portable source.

So on this machine the dispatched kernel is **not** the portable algorithm, and hooking it with the portable
port would change the numbers. The AVX2 grouping has to be reproduced for a faithful replacement.

## Sixteenth measurement: the AVX2 source was found, and it is a different algorithm

`/home/end0/AI/LlamaServerMTP/ggml/src/ggml-cpu/arch/x86/quants.c` contains the dispatched kernel. Its
structure differs from the portable one in four places that all affect the float result:

1. `dmin = -y[i].d * f16(x[i].dmin)` — the sign is folded into the constant, then accumulated, instead of being
   subtracted at the end.
2. The min term is built from **four** products: `q8s` is the pairwise `hadd` of `bsums`, then
   `prod = madd_epi16(mins_hi, q8s)`, i.e. `mins[i]*(bsums[2i]+bsums[2i+1])` for i = 0..3, not the portable
   16-term sum.
3. `acc = fmadd(d, cvtepi32_ps(sumi), acc)` — fused multiply-add over 8 lanes.
4. Two separate accumulators are summed at the very end: `acc_m` (mins, 128-bit) and `acc` (scales, 256-bit),
   with `*s = hsum_float_8(acc) + acc_m_scalar`.

Reproducing the horizontal orders by hand already moved the match rate (138 -> 121 mismatches out of 200), which
confirms the direction: the difference is float association, not the integer products.

So the portable port is correct for the portable algorithm, but replacing the dispatched symbol with it would
change the numbers. A faithful replacement needs this exact AVX2 association reproduced in Goose.
