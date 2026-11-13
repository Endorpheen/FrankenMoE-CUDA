# EXP366: alternate CUDA architectures compile, and what stays hardware-unverified

P3-18 asks for three things: sm89 executed, alternate hardware configurations covered by compile/emulation
evidence with an explicit hardware-unverified ledger, and vendor services not treated as owned-code exemptions.

This host has one GPU (RTX 4070, compute capability 8.9, driver 580.178.04), so no second arm can execute
anything. That is exactly the state the criterion anticipates.

## What was executed

Every module's GPU fixture on sm_89: 41 fixtures, all zero failures, and all four compute-sanitizer tools clean
(EXP365). This experiment adds nothing to that; it points at it rather than re-running it.

## What was compiled

Each experiment's recorded `gpu-fixture-build` command was replayed with `--generate-code` pointed at every
architecture this nvcc supports, one object per architecture, nothing linked and nothing run:

    compute_50, 52, 53, 60, 61, 62, 70, 72, 75, 80, 86, 87, 89, 90

| result | modules |
|---|---:|
| compile on all 14 architectures | 56 / 56 |
| partial (some architecture refused) | 0 |
| recorded command unusable | 0 |

So the Goose-emitted CUDA sources are architecture-neutral at the PTX/SASS level for every target this toolkit
knows, including the oldest ones (compute_50) where the instructions are emulated, and Blackwell (compute_90).

Artifact: `runtime/arch-compile-coverage/compile-coverage.json` with a per-module, per-architecture matrix.

## The hardware-unverified ledger

Compiling is not running, and the file says so in its own note so the distinction cannot be lost:

| configuration | state | why |
|---|---|---|
| sm_89 (RTX 4070) | **executed and verified** | 41 fixtures, 4 sanitizer tools, all zero |
| compute_50 ... compute_90 except 8.9 | **compiles, never executed** | one GPU on this host; no emulator run |
| multi-GPU allreduce | **not ported, not exercised** | `allreduce.cu` is 977 lines behind `GGML_USE_NCCL`; NCCL is not installed (`find_package(NCCL)` fails, no `nccl.h`, and `nm -D libggml-cuda.so | grep -c nccl` is 0, so the path is not even in the built DSO) |

Two GPUs would be needed to exercise allreduce, and there is one. The honest state is "hardware-unverified",
not "done" and not "N/A".

## Vendor services are not exemptions

The criterion's last clause guards against the tempting reading "the driver did it, so it is not our code".
The ledger already separates these, and this experiment does not widen it:

- cuBLAS, cuBLASLt, `mma`/`ldmatrix`, `cudaMemcpy` and the allocator are **external primitives**: they compute
  nothing this plan claims to own, and the orchestration around them is Goose (P3-10: dispatch policy ported,
  10/10 mutations; cuBLAS stays external by design).
- NCCL would be a vendor service providing the communication itself. Because it is absent from this build, the
  allreduce path is recorded as unported work rather than as an owned primitive -- otherwise "NCCL does it"
  would quietly delete a requirement.

## Criterion check

| clause | state |
|---|---|
| sm89 executed | yes, EXP365's matrix |
| alternate configurations have compile evidence | yes, 56/56 modules on 14 architectures |
| explicit hardware-unverified ledger | yes, table above plus `compile-coverage.json` |
| vendor services are not owned-code exemptions | yes, recorded, and the absent NCCL path stays open |

The open remainder is single: multi-GPU allreduce needs a second GPU and NCCL. That is a hardware boundary
recorded in the ledger, which is what the criterion allows, not unported work that could be done here.
