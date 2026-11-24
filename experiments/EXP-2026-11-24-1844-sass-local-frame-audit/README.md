# EXP-2026-11-24-1844 — SASS audit: the hottest kernel carries the largest local frame and sits at the register ceiling

Status: **MEASUREMENT (audit), no change adopted.** Baseline `37ac0f1d` unchanged.

## Correction first

An earlier count in this session reported zero `LDL`/`STL` for these kernels. That was
my script's error: the pattern was written so it matched a literal backslash and never
matched the instructions. The numbers below are the corrected ones, produced by
compiling the shipped PTX (`build-inputs/goose-cuda-image.h`, `.target sm_80`,
`.version 8.3`) with `ptxas -arch=sm_89 -v` and counting local accesses in
`nvdisasm` output. Compiling the whole image was necessary because the fragment calls
an extern device function (`fg_device_mmq_wide_g`).

## The audit

| kernel | local frame | registers | SASS LDL | SASS STL | instructions | prefill time (trace) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| **goose_flash_causal_mma** | **264 B** | **255** | **158** | **172** | 15 316 | **482 ms (12.9 %)** |
| goose_flash_stream_mma | — | — | 87 | 89 | 18 727 | (off by default) |
| goose_gdn_mma | 288 B | 88 | 68 | 86 | 3 394 | — |
| goose_q4_attention_merge | 32 B | 38 | 29 | 72 | 880 | — |
| **goose_gdn_mma_v32** | **80 B** | **128** | **20** | **22** | 2 231 | **322 ms (8.6 %)** |
| goose_gated_delta_net | 384 B | 40 | 12 | 69 | 884 | — |
| goose_rms_reference | 64 B | 38 | 16 | 16 | 1 101 | 225 ms (6.0 %) |
| goose_gdn_decode | 0 B | 56 | 0 | 0 | 439 | (already fixed by EXP-1719) |

No kernel spills: every stack frame is a declared local array that is genuinely
accessed, not a spill area.

Occupancy from the register counts at a 256-thread block (65 536 registers per SM):

| registers | blocks/SM | threads/SM | occupancy |
| ---: | ---: | ---: | ---: |
| 255 (`flash_causal_mma`) | 1 | 256 | **12.5 %** |
| 128 (`gdn_mma_v32`) | 2 | 512 | 25 % |
| 88 (`gdn_mma`) | 2 | 512 | 25 % |
| 38 (`q4_attention_merge`, `rms_reference`) | 6-7 | — | high |

## What this means

The registry's dominant method — remove the per-thread local frame so values live in
registers or parameter space (EXP-1603: 208 B frame → 0, vector kernel 668 → 517 ms;
EXP-1719: 384 B frame and 101 local stores → 0, 494 ms of decode) — applies to our
hottest kernel, not to the one I first picked. `goose_flash_causal_mma` combines:

- the largest local frame of any kernel (264 B) with 330 local accesses per launch;
- the register ceiling (255 of 255), which caps it at one block per SM, that is
  12.5 % occupancy;
- the largest share of prefill kernel time (482 ms, 12.9 %).

The causal entry passes a long by-value argument list to `process_tile`
(`standalone-flash-mma.goose`), which is the same pattern the registry describes for
the vector plans: a large by-value aggregate materialised into a local frame instead
of staying in parameter space. `goose_gdn_decode` shows the end state already achieved
elsewhere (0 B, 0 local accesses).

## Next experiment

Reduce the local frame of the causal Flash entry in
`goose/cuda/standalone-flash-mma.goose` (and, if the frame comes from the shared
`process_tile` wrapper, the wrapper's by-value list), rebuild the CUDA image, and
gate on all four levels rather than PTX alone:

1. PTX: no `.local` declaration for that entry;
2. `ptxas -v`: stack frame 0, spills 0, registers reported;
3. `nvdisasm`: `LDL`/`STL` counts, and the resulting occupancy from the register
   count;
4. runtime: 512 IDs and text `f8664331bd30`, then three alternating pairs against
   `37ac0f1d` with VRAM, VmHWM and swap.

The build is `tools/build-goose-cuda-kernels.sh` for the image plus the existing
runtime build; it is a heavy step and is run as a tracked background job with its log.

## Artefacts

- `/tmp/opencode/mmq.ptx`, `mmq.cubin`, `mmq.sass` — extracted PTX, compiled cubin and
  disassembly used for the table (reproducible: extract the byte array from
  `build-inputs/goose-cuda-image.h`, then `ptxas -arch=sm_89 -v`, then `nvdisasm -c`).
- `runs/base32768-20261124-082000` — the trace with the per-kernel times.
