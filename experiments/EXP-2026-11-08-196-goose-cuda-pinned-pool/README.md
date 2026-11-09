# EXP196: pinned-memory and host-registration policy in Goose (P3-17)

Second half of P3-17's pool work. The VMM pool arithmetic was EXP195; this stage transfers the
decision code around pinned host memory and mmap'd host-buffer registration.

## The path was confirmed before porting

An LD_PRELOAD counter over the CUDA runtime entry points, loaded into a real 32K/128 Apex run:

    FG196_PINNED cudaMallocHost=3 bytes=677719168 cudaFreeHost=3
                 cudaHostRegister=120 cudaHostUnregister=120
                 cudaMemcpyAsync_h2d=60077 cudaMemcpyAsync_d2h=10962

Both paths execute during inference: three pinned allocations totalling 647 MiB, and 120
registrations. So this is live code, not a seam nothing takes.

The probe needed one adjustment to be usable at all: the mandated runner overwrites
`LD_PRELOAD` from a saved audit environment, so the probe is attached only in a separate copy
(`run_apex_probe.py`) that is not used for any timing figure.

## Scope transferred

Goose owns, for each of the four entry points, everything that is a decision:

| function | Goose decides |
|---|---|
| `ggml_cuda_host_malloc` | whether to attempt at all (`GGML_CUDA_NO_PINNED`), and the MiB figure in the failure log |
| `ggml_backend_cuda_host_buffer_type_alloc_buffer` | whether a failed pin falls back to the CPU buffer type |
| `ggml_backend_cuda_register_host_buffer` | whether to attempt (`GGML_CUDA_REGISTER_HOST`), success/failure, whether to log |
| `ggml_backend_cuda_unregister_host_buffer` | whether to call, and whether the sticky error is cleared |

`cudaMallocHost`, `cudaHostRegister` and `cudaHostUnregister` stay with the caller: they are the
hardware boundary, and moving them would make the module untestable without a GPU.

One deliberate implementation choice: the MiB figure is computed in integer thousandths rather
than in floating point. The native code prints `size / 1024.0 / 1024.0`; a fixed-point value
gives the same digits without dragging float formatting into Goose, and it is exactly
`floor(size * 1000 / 1048576)`.

## Gate

| check | result |
|---|---|
| differential vs native transcription | **1272 checks, 0 failures** |
| sizes | 0, 1, 1023, 1024, 4095, 4096, 1 MiB-1, 1 MiB, 647 MiB, 4 GiB, 1 TiB+12345 |
| flags | both values of each of `disabled`, `enabled`, plus a zero/non-zero pointer |
| driver results | 0 and seven error codes including 4 and 801 |
| MiB reference | independent `floorl(size * 1000 / 1048576)` in long double, not the same integer division |

## Wiring and evidence

The stage carries its own copy of `shared-cuda.cu` (EXP195's copy plus these four call sites),
so EXP160 and EXP195 stay exactly as they were, and links both bridge objects.

Server stderr after a full Apex pass:

    FG196P_CUDA_PINNED host_plan=3 host_fail=0 fallback=0 reg_plan=120 reg_fail=0 unreg=240 native=0

`host_plan=3` and `reg_plan=120` match the probe's `cudaMallocHost=3` and
`cudaHostRegister=120` exactly, which is the evidence that the Goose bodies run on the same
events rather than merely build. `unreg=240` is two calls per unregistration by design: the
native code returns early when registration is off, so the pre-check and the outcome are two
separate decisions.

Rollback arm (`GGML_GOOSE_CUDA_PINNED=0`) reports `native=246` with every Goose counter at
zero, so the toggle restores the inline native bodies.

Output text sha256 stayed `e68df05b`, identical to the reference.

## Paired measurement (native decisions vs Goose decisions, same library)

| run | mode | prefill tok/s | decode tok/s |
|---|---|---|---|
| n1 | native | 2572.01 | 25.59 |
| n2 | native | 2581.10 | 25.84 |
| g1 | Goose | 2573.01 | 25.65 |
| g2 | Goose | 2566.03 | 26.87 |

| arm | prefill mean | decode mean |
|---|---|---|
| native | 2576.56 | 25.71 |
| Goose | 2569.52 | 26.26 |
| delta | **−0.27%** | **+2.12%** |

Prefill, the more stable of the two, moves by 0.27% on sixteen decision events per pass. The
decode figure is inside the 1–2 tok/s within-arm spread this machine shows, so it is not a
speed-up claim; read the row as "no regression detected".

## What is still open in P3-17

Transfer/residency policy proper — the 60077 H2D and 10962 D2H copies the same probe recorded —
is not touched here. The pinned pool and host registration are the allocation side only.
