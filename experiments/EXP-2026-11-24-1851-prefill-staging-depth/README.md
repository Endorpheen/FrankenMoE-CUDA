# EXP-2026-11-24-1851 — Prefill staging buffers: 2 is optimal, 1 and 3 are both worse

Status: **REJECTED (both alternatives), accepted configuration confirmed.** Rule 40 followed:
one host-side change per arm, preserved archive, image, flags and environment.

## Arms

All arms use the accepted binary's archive and image; only the prefill staging depth
changes — either an environment switch (`GOOSE_CUDA_PREFILL_TEMP_DOUBLE=0`) or one
constant (`planes = 2` → `3`) in the accepted driver source, recompiled with the same
flags.

| arm | buffers | prefill tok/s | decode tok/s | fills (3 requests) | uploaded | VRAM peak |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| accepted | 2 | 3098.4-3111.5 | 77.15-77.67 | **95 076** | **181.4 GB** | 9 941-10 003 MiB |
| single | 1 | 2993.6 / 2978.9 | 77.313 / 77.745 | 117 644 (+24 %) | 224.6 GB (+24 %) | 10 706 MiB |
| three | 3 | 3112.8 / 3105.0 | 77.230 / 77.052 | 117 644 (+24 %) | 224.6 GB (+24 %) | 10 672 MiB |

All arms are bit-exact: ids `38abffce4131`, text `f8664331bd30`, VmHWM ≈21 494 MB,
swap 0.

## Verdict

- **Single buffer costs 3.9 % of prefill and 24 % more expert traffic**: the second
  buffer is a real accepted gain, not a neutral choice. It is the mechanism that lets a
  wave be staged while the previous one is still resident.
- **Three buffers behave like one**: the fill count returns to the single-buffer value,
  which means the staging ring in this implementation assumes exactly two planes; a
  third plane breaks the reuse it was meant to extend. Rejected on that evidence.
- Accepted configuration: **two buffers**, unchanged.

## Sequence of the cycle so far (all on the preserved baseline)

| hypothesis | result |
| --- | --- |
| expert cache bank split 1700/1735 | +26 % fills, no gain → reverted |
| expert cache bank split 2100/1340 | +27 % fills, no gain → reverted |
| one-touch SLRU disabled | decode −6 %, fills +29 % → policy confirmed, kept |
| prefill staging 1 buffer | prefill −3.9 %, fills +24 % → reverted |
| prefill staging 3 buffers | fills +24 %, no gain → reverted |

Net: no new gain adopted, the baseline `37ac0f1d` and all accepted optimisations stand,
and five host-side alternatives are now excluded with numbers.

## Next hypothesis

The staging ring's two-plane assumption is the interesting part: with two planes the
wave for the next projection can only be staged one deep. Making the ring correct for N
planes is a contained host change (offset arithmetic and the release/wait events) and
would let a third plane be tested as intended rather than as a broken variant. That is
the next experiment, with the same gate.

## Artefacts

- `runs/notouch-095236` — one-touch SLRU disabled.
- `runs/singlebuf-095410` — single staging buffer.
- `runs/buf3-095542` — three staging buffers.
- `/tmp/opencode/var-3buf` — the three-buffer build.
