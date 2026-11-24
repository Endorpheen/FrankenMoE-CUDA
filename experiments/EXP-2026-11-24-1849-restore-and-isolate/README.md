# EXP-2026-11-24-1849 — The rebuild gap is broader than two kernels; the RMS scalarization measures neutral

Status: **MEASUREMENT + BLOCKER.** Baseline `37ac0f1d` unchanged.

## What was done

1. Both kernels missing from the rebuild were **restored from their experiment patches**
   into the tracked sources:
   - `goose_moe_add_shared_residual`: export in `goose/cuda/standalone-vector.goose`,
     `__kernel` in `integration/goose-standalone/cuda-device.cl`
     (EXP-2026-11-22-1717 patches);
   - `goose_rms_decode_2048`: export in `goose/cuda/standalone-rms-reference.goose`,
     `__kernel` in `integration/goose-standalone/cuda-rms-reference.cl`
     (EXP-2026-11-22-1721 patches).
   The driver side of both was already in the tree; only the device sources had never
   been upstreamed.
2. The image was rebuilt: **31 entries, nothing missing** relative to the accepted image
   (30 + the new one-vector RMSNorm), confirmed by name.
3. The one-vector RMSNorm kernel was isolated behind `GOOSE_RMS_ONE` so that a single
   binary launches either entry — the same image, the same host code, only the selected
   kernel differs.

## Measurements

Image equivalence, after the restore:

| arm | prefill tok/s | decode tok/s | text |
| --- | ---: | ---: | --- |
| accepted `37ac0f1d` | **3100.7** | **78.176** | `f8664331bd30` |
| rebuilt image with both kernels restored | 2173.2 | 53.563 | `d45882943669` |

Restoring the two kernels did **not** restore the runtime: the rebuilt image is still
30 % slower in prefill, 32 % slower in decode, and produces different text. So the
drift is broader than the two missing entries — the other 28 entries are semantically
different too, which is consistent with the rest of this project's history: many device
changes were applied as experiment patches and only partly upstreamed.

Isolation of the scalarization inside the rebuilt image (same binary, one flag):

| arm | prefill tok/s | decode tok/s | text | VmHWM | swap |
| --- | ---: | ---: | --- | ---: | ---: |
| `GOOSE_RMS_ONE=0` (general entry) | 2173.2 | 53.563 | `d45882943669` | 21 779 572 kB | 0 |
| `GOOSE_RMS_ONE=1` (one-vector entry) | 2172.3 | 53.220 | `d45882943669` | 21 502 680 kB | 0 |

The scalarization is **neutral** (−0.04 % prefill, −0.6 % decode, identical text) and it
does not add memory. Its kernel-level result stands (0 B stack, 36 registers, 0 local
accesses against 64 B, 40, 4+20), but the frame removal does not convert into request
time on this kernel: 222 ms of kernel time with 16 local accesses per launch is under
1 % of the request, which is below this block's noise.

## Consequence

- The kernel change is verified and neutral; it is not adopted, and no gain is claimed.
- The blocking issue is the **reproducibility of the accepted image**: the tracked tree
  does not produce it, even after restoring the two kernels it was missing. Any further
  kernel-level work (the Flash frame, the GDN frames, the mmq variants) is unmeasurable
  on this base until the build reproduces `37ac0f1d`.
- The concrete requirement: the build must apply the experiment patch set that produced
  the accepted image, or the accepted image must be accompanied by its exact build
  inputs. Anything else mixes a 30 % regression with the change under test.

## Artefacts

- `runs/rmsone-091945` — both arms: requests, responses, VRAM, VmHWM, logs.
- `/tmp/opencode/img-restored/` — the rebuilt image with both kernels restored.
- `/tmp/opencode/arm-restored/frankenmoe-infer` — the binary (`bd0d0a6b…`).
- `experiments/EXP-2026-11-22-1717-shared-residual-fusion/`,
  `experiments/EXP-2026-11-22-1721-direct-rms-decode/` — the patches that were restored.
