# EXP-2026-11-24-1840 — Expert prefetch in prefill: it already reaches device slots, and it costs traffic

Status: **REJECTED with numbers.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225).

## The premise checked first

The next step assumed the predicted-route prefetch only stages planes in host memory,
which is why it never reaches the device timeline. That is not what the code does:
`pf_submit()` calls `prepare_expert_offsets()` on the regular SLRU path and then
`submit_expert_stage()`, which queues `cuMemcpyHtoDAsync_v2` jobs on
`cuda.expert_stream` into the cache's device slots, exactly like a normal wave. The
prefetch *is* device-side.

What it is not is **prefill-side**: the launch site is gated by `compact`, and a
prefill routed FFN is not compact. Its counter confirms it — 19 958 launches over a
two-request block, which matches the decode layers, not the 480 prefill layers.

## The change, and its measurement

`GOOSE_EXPERT_PREFETCH_PREFILL=1` removes that gate for the measurement only (decode
behaviour unchanged). Acceptance request, warm-up plus two measured:

| arm | prefill tok/s | decode tok/s | text | uploaded bytes |
| --- | ---: | ---: | --- | ---: |
| prefetch in prefill (`runs/pfpre-082715`) | 3115.6 / 3109.4 | 78.769 / 78.535 | `f8664331bd30` | **110.7 GB** |
| baseline protocol | 3122.4 | 78.225 | `f8664331bd30` | **90.7 GB** |

Prefetch launches rose from 19 958 to **40 006** (so it did run during the prefill),
bit-exactness holds, decode is at parity, VmHWM 21 495 168 kB, swap 0 — and the
prefill did **not** get faster while the expert traffic grew by 20 GB, that is
**+22 %**.

## Why it cannot work in prefill

A prefill ubatch of 4 096 tokens routes across essentially the whole expert set of a
layer, so the next layer needs *all* of its planes (~333 MB), not the eight experts a
route prediction names. Prefetching eight of them adds transfers without covering the
layer, and the link is already at 22.5 GB/s, so the extra transfers lengthen the
copies instead of hiding them. The trace's 0.237 s of overlap against 2.232 s of
copies is therefore not a scheduling oversight that a deeper prefetch could fix: the
volume is whole-layer and the medium is saturated.

Covering a layer ahead would mean reserving one full extra layer of cache (333 MB, 5 %
of the 6290 MiB budget) while the current layer is still resident; the cache already
splits into `1904/1531` and a bigger budget was measured worse, so that reservation
does not exist to take.

## Consequence

Prefill stays **3122.4 tok/s**, 8 % short of 3500. On this configuration, measured
levers are exhausted: copies (6 variants), host waits (2), readback residency
(3 attempts), plan granularity (already one plan), expert wave slack (2 depths),
expert prefetch (now measured in prefill as traffic-positive and speed-neutral), cache
policy, ubatch and profile. The remaining routes are outside tuning: more VRAM so the
expert set fits, a model whose expert path does not thrash a 6.3 GB cache, or
speculative whole-layer loading with a reserved cache region — a project with its own
gate.

## Artefacts

- `runs/pfpre-082715` — the prefill-prefetch arm: three requests, VRAM, VmHWM, swap,
  props with `cuda_expert_upload_bytes` = 110.7 GB.
- `runs/slack-082350` — the wave-slack arm (EXP-1839).
- `runs/base32768-20261124-082000` — the device timeline with 0.237 s of overlap.
