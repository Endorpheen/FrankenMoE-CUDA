# EXP-2026-11-23-1813 — Two-slot MMQ plan pipeline: neutral

Status: **REJECTED as a gain (neutral).** Baseline `37ac0f1d` unchanged:
prefill 3122.4, decode 78.225.

## What was tried

The remaining host cost was 306 synchronous plan uploads per prefill request
(2.23 MB each, 1.28 s of a 1.56 s prompt at 4096 tokens, EXP-1811/1812). This
candidate gives the plan its own non-blocking stream plus two device slots:

- `upload_plan_async` copies into a pinned slot and queues
  `cuMemcpyHtoDAsync_v2` on a dedicated stream, ordered by an event recorded on
  the legacy stream and fenced back by a second event, so the consumer keeps the
  same position in the stream;
- `cuda.mmq_tiles` is reserved with two halves and the kernel reads the slot of
  its own projection, so the plan of projection k+1 may land while projection k
  still reads the other slot;
- engagement is printed once: `Goose CUDA: MMQ plan travels on its own stream`;
- `GOOSE_MMQ_PLAN=0` restores the old copy.

## Measurement

A = `checkpoints/cand-prefill-jobpack-split` (`37ac0f1d`), B = this build
(`492fbd51`), both with `GOOSE_DECODE_SPLIT=4` and `GOOSE_DECODE_SPLIT_COUNT=1`,
warm-up plus measured request, alternating A B B A.

| arm | prefill tok/s | decode tok/s | prompt_ms | client s | exact |
| --- | ---: | ---: | ---: | ---: | --- |
| pd-A1 | 3118.6 | 78.225 | 10507.4 | 17.131 | yes |
| pd-A2 | 3111.5 | 78.288 | 10531.2 | 17.149 | yes |
| pd-B1 | 3108.2 | 77.720 | 10542.3 | 17.207 | yes |
| pd-B2 | 3116.7 | 77.017 | 10513.7 | 17.237 | yes |

Medians: prefill 3115.0 -> 3112.5 (**-0.08 %**), `prompt_ms` 10519 -> 10528,
decode 78.256 -> 77.369 inside the spread. Peak VRAM 8774 MiB in both arms,
VmSwap 0, all four runs exact.

A 4096-token prompt did show a small gain (1533.7 ms against 1562.5 ms, -1.8 %),
which does not survive at 32768 tokens: the host has slack at the acceptance
size, the plan copies overlap the GPU there already, and the change only adds
two stream operations and an event pair per projection.

## What the four rejected attempts together establish

| attempt | axis | result |
| --- | --- | --- |
| EXP-1810 | pinnned ring for `upload_packet` | -2.95 % |
| EXP-1811 | pinned ring for `decode_upload` | +6.2 % at 4096 tokens |
| EXP-1812 | (diagnosis only) uploader is `launch_mmq` inside the driver | — |
| this card | plan on its own stream, two device slots | -0.08 % |

The host side of prefill is therefore not the limiter. The prefill window is
6.43 s of MMQ span with the GPU busy 3.98 s of it: about 2.4 s of the compute
window is GPU idle, and that idle is the same expert-transfer dependency that
EXP-1802 measured in decode (2.688 ms per step exposed, 0 of 96 162 copies
predictable). Transport and scheduling of host work cannot remove it.

## Artefacts

`runs/pd-A1-20261123-212*`, `runs/pd-B1/B2-20261123-212*`,
`runs/plan1-210847`, `runs/plan2-210919`, `runs/plan3-211351` (engagement and the
4096-token reading).
