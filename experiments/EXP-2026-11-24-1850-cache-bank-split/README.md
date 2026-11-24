# EXP-2026-11-24-1850 — Expert cache bank split: both alternatives rejected, 1904/1531 is the optimum

Status: **REJECTED, baseline `37ac0f1d` kept.** Rule 40 followed: preserved archive, preserved
image, one host-side change per arm.

## Baseline first (rule 40)

The accepted binary was re-verified before the experiment, three blocks of warm-up plus
measured request (`runs/baseverify-094543`):

| block | prefill tok/s | decode tok/s | ids | text |
| --- | ---: | ---: | --- | --- |
| 1 | 3098.4 | 77.149 | `38abffce4131` | `f8664331bd30` |
| 2 | 3111.5 | 77.672 | `38abffce4131` | `f8664331bd30` |
| 3 | 3105.9 | 77.574 | `38abffce4131` | `f8664331bd30` |

VmHWM 21 490-21 492 MB, swap 0, VRAM 9 941-10 003 MiB. The baseline reproduces exactly.

## The change

`rebalance_apex_cache()` in the accepted driver splits the expert cache into slots for
the Q6_K down planes and the Q4_K gate/up planes, hard-coded as `q6_slots = 1904,
q4_slots = 1531`. Both constants are host-side: the change touches nothing else — same
archive (`/tmp/opencode/apex-win/lib.a`), same image header, same flags, same
environment. Two arms were built:

| arm | split | prefill tok/s | decode tok/s | fills (3 requests) | uploaded | ids / text |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| baseline | 1904 / 1531 | 3098.4-3111.5 | 77.15-77.67 | **95 076** | **181.4 GB** | match |
| smaller Q6 bank | 1700 / 1735 | 3125.9 / 3127.3 | 77.426 / 77.524 | 119 839 (+26 %) | 230.8 GB (+27 %) | match |
| larger Q6 bank | 2100 / 1340 | 3155.4 / 3127.1 | 77.372 / 76.611 | 120 468 (+27 %) | 233.0 GB (+28 %) | match |

## Verdict

Both alternatives keep the numbers within the block's noise (3098-3155 spans the
baseline range) while raising the miss traffic by 26-28 %. Neither is a gain, and a
higher-traffic configuration is not carried forward: both were reverted, the accepted
`1904/1531` split stays.

The result is informative beyond the rejection: the expert count per bank is already at
its local optimum, and shifting capacity between the two plane types costs traffic in
both directions — consistent with the measured 22.5 GB/s link being the real constraint
and with each layer needing both plane types in the same projection.

## Next hypothesis

The one-touch SLRU policy (`GOOSE_CUDA_DISABLE_ONE_TOUCH_SLRU` exists as its kill
switch) and the prefill workspace double buffering ("2 buffers" at start-up) are the
remaining host-side choices that a single change can move without touching the image.
Each will be tried the same way: one change, A/B against this baseline, bit-exactness
gate, rollback on no gain.

## Artefacts

- `runs/baseverify-094543` — the three baseline blocks.
- `runs/split1735-094856` — the 1700/1735 arm (props before/after, requests, responses).
- `runs/split1340-095028` — the 2100/1340 arm.
- `/tmp/opencode/var-split`, `/tmp/opencode/var-split2` — the two variant builds
  (`b5f66684…` and the 2100/1340 build).
