# EXP-2026-11-24-1846 — The Flash kernel wants its 255 registers: capping them costs prefill

Status: **REJECTED.** Baseline `37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## The geometry, measured not assumed

From the launch site in the driver:

```
cuda.launch(cuda.flash_mma, tiles_per_head, heads, 1, 128, 1, 1, FLASH_MMA_SHARED_BYTES = 33792)
```

so the block is **128 threads**, the grid is (tiles_per_head × heads), and each block
reserves **33 792 B** of shared memory. With 255 registers per thread the SM can hold

- registers: 2 blocks (255 × 128 = 32 640 of 65 536) → **256 threads per SM = 12.5 %**;
- shared memory: 2 blocks (67 584 B of ~100 KB usable), so registers and shared memory
  agree on 2 blocks.

Registers are therefore the binding limit, and the earlier 12.5 % figure was computed
with the real block size rather than an assumed 256-thread block.

## The test

The device image is PTX, so the driver's JIT can be given a register cap without
rebuilding anything: `GOOSE_CUDA_MAXREG` sets `CU_JIT_MAX_REGISTERS` on
`cuModuleLoadDataEx`. One binary, two arms, warm-up plus measured in each:

| arm | prefill tok/s | decode tok/s | text |
| --- | ---: | ---: | --- |
| default JIT (255 registers, 12.5 % occupancy) | 3119.9 | 78.087 | `f8664331bd30` |
| cap 128 (≈25 % occupancy) | **2986.9** | 79.011 | `f8664331bd30` |

The cap was applied (`Goose CUDA: JIT register cap 128 applied to the main image`).
Prefill fell **4.3 %** while decode rose 1.2 %, and correctness held.

## What it settles

The Flash causal kernel's register pressure is a **deliberate trade that pays**: the
255-register allocation buys instruction-level parallelism that is worth more than the
occupancy it costs. Doubling occupancy by capping registers is a net loss for prefill.

This also redirects the local-frame plan from EXP-1844: its 264-byte frame of real
arrays is the *price* of keeping data out of the register file while the kernel is
already at the 255-register ceiling. Removing those arrays would push ptxas either
above the ceiling or into spills, which is exactly what the cap experiment shows to be
harmful. The registry's frame-removal method (EXP-1603, EXP-1719) is therefore not
applicable to this kernel as it stands — it applies where a kernel has spare registers
(e.g. `goose_gdn_decode` at 56 registers, which is already fixed, or
`goose_q4_attention_merge` at 38 with 29/72 local accesses).

## Artefacts

- `runs/reg-085043` — both arms: requests, responses, server logs with the cap line.
