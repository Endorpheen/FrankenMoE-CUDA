# EXP-2026-11-24-1846 — The Flash kernel wants its 255 registers: capping them costs prefill

Status: **REJECTED.** Baseline `37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## The geometry, measured not assumed

From the launch site in the driver:

```
cuda.launch(cuda.flash_mma, tiles_per_head, heads, 1, 128, 1, 1, FLASH_MMA_SHARED_BYTES = 33792)
```

so the block is **128 threads**, the grid is (tiles_per_head × heads), and each block
reserves **33 792 B** of shared memory.

**Correction.** The occupancy figures in the first version used 2 048 threads per SM.
On sm_89 the limit is **1 536** threads per SM (48 warps), so every percentage below is
restated against the correct ceiling:

- registers: 255 × 128 = 32 640, so two blocks fit in 65 536 → **256 threads per SM,
  that is 16.7 % of 1 536** (not 12.5 %);
- shared memory: 2 × 33 792 = 67 584 B within the ~100 KB usable, so both resources
  agree on two blocks.

The same correction applies to the other kernels: 128 registers give four blocks
(512 threads, **33.3 %**), 88 registers five blocks (640 threads, 41.7 %), and 38
registers are limited by the thread ceiling rather than by registers.

Registers are therefore the binding limit, and the earlier 12.5 % figure was computed
with the real block size rather than an assumed 256-thread block.

## The test

The device image is PTX, so the driver's JIT can be given a register cap without
rebuilding anything: `GOOSE_CUDA_MAXREG` sets `CU_JIT_MAX_REGISTERS` on
`cuModuleLoadDataEx`. One binary, two arms, warm-up plus measured in each:

| arm | prefill tok/s | decode tok/s | text |
| --- | ---: | ---: | --- |
| default JIT (255 registers, 12.5 % occupancy) | 3119.9 | 78.087 | `f8664331bd30` |
| cap 128 (≈33 % occupancy) | **2986.9** | 79.011 | `f8664331bd30` |

The cap was applied (`Goose CUDA: JIT register cap 128 applied to the main image`).
Prefill fell **4.3 %** while decode rose 1.2 %, and correctness held.

## What it does and does not settle

**Scope correction.** `CU_JIT_MAX_REGISTERS` applies to the **whole module**, not to
`goose_flash_causal_mma`. The 4.3 % prefill loss therefore says that a 128-register cap
on this image is a net loss, which may come from any of the 83 kernels in it; it says
nothing specific about the Flash kernel's own response.

**Register capping is not scalarization.** A cap forces ptxas to fit within a limit
that may be wrong for the code; removing a local frame changes where a *specific*
array lives while leaving the register allocator free. The two experiments are not
substitutes, and the earlier claim that the frame "cannot help this kernel" is
withdrawn.

What remains established: the kernel runs 128-thread blocks with two blocks per SM
(16.7 % of 1 536 threads) and 255 registers, and the image as a whole loses 4.3 % of
prefill when registers are capped at 128. The frame-removal work continues on the
kernels that carry frames and have register headroom, measured per kernel.

## Artefacts

- `runs/reg-085043` — both arms: requests, responses, server logs with the cap line.
