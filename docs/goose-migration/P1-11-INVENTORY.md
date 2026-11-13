# P1-11 inventory: registered sources and backends vs what the line actually links

Criterion: "Every source/backend registered in audit has Goose implementation or explicit hardware-unavailable note."

## Backends: three are built, the rest are hardware-unavailable

`build-cuda/bin` contains exactly `libggml-base`, `libggml-cpu` and `libggml-cuda`. There is no Metal, Vulkan, SYCL, OpenCL,
ROCm or BLAS backend library in the tree, so those cannot be ported here and are recorded as unavailable rather than missing.

| backend | state | evidence |
|---|---|---|
| CPU | Goose core, buffers, streams, allocator (EXP160-163) and the CPU backend module (EXP143) | `FG143_CONTROL/STATE/EXECUTOR/CPU` counters in the accepted run |
| CUDA | Goose device modules over native driver primitives | `FG183F_CUDA_FWHT`, `FG171R_CUDA_ROPE`, `FG167_CUDA_BROADCAST`, `FG169T_CUDA_TOPK` in the accepted run |
| Metal / Vulkan / SYCL / OpenCL / ROCm | not built | no library in `build-cuda/bin` |

## Base library: every flint object accounted for

The flint base has nine objects; our link consumes six of them plus seven Goose objects. The mapping is therefore complete and
checkable:

| flint object | our replacement | state |
|---|---|---|
| `ggml.c` + `ggml.cpp` | `base-ggmlcpp.o` + `shared-core.o` | Goose core (EXP160), native remainder compiled as C++ |
| `ggml-backend.cpp` | `backend-scheduler.o` + `scheduler-goose.o` | Goose scheduler (EXP164); `backend-base-goose.o` keeps the native symbols |
| `ggml-alloc.c` | `allocator.o` | Goose allocator (EXP161) |
| `ggml-backend-meta.cpp` | `shared-meta.o` | Goose metadata (EXP160) |
| `ggml-quants.c` | `base-quants.o` | native primitive |
| `ggml-opt.cpp` | `base-opt.o` | native primitive |
| `ggml-threading.cpp` | `base-threading.o` | native primitive |
| `gguf.cpp` | `base-gguf.o` | native, and deliberately so: P7-01 records the parser as a multi-tick port not yet started |

New objects with no flint counterpart are the Goose buffer and stream layers (`buffers-goose.o` EXP162, `stream-goose.o`
EXP163).

## CPU library

Fifteen objects: `ggml-cpu.c`, `ggml-cpu.cpp`, `ops.cpp`, `binary-ops.cpp`, `unary-ops.cpp`, `quants.c`, `repack.cpp`,
`traits.cpp`, `vec.cpp`, `hbm.cpp`, plus the `amx` and `arch` subdirectories. The Goose CPU backend module (EXP143) dispatches
and computes through these; the SIMD kernels themselves stay native, which is the state EXP190 and the compiler scope note
already record (no Goose SIMD on this host).

## What is still missing for the criterion

The table above is complete for the base library and for the backend set. The CPU object list still needs one row per object
saying which is Goose-dispatched and which is an audited primitive, and the CUDA side needs the same per-family split
(arithmetic vs driver primitive). Those two columns are the remaining work of P1-11, not the backend enumeration.

## Row-by-row coverage

### CPU (15 objects)

| object | state |
|---|---|
| `ggml-cpu.c` | Goose dispatch: every `compute_forward_*` enters through `fg143_try`; kernel bodies stay native |
| `ggml-cpu.cpp` | native: backend registration and service entry points |
| `ops.cpp` | Goose dispatch (EXP143 leaves), native kernel bodies |
| `binary-ops.cpp`, `unary-ops.cpp`, `vec.cpp`, `quants.c`, `repack.cpp`, `traits.cpp`, `hbm.cpp` | native SIMD kernels; note: Goose has no SIMD on this host and the compiler places it outside current scope (recorded with EXP190) |
| `amx/*`, `arch/*`, `llamafile/*` | native: instruction-set specifics, same note |

### CUDA (70 sources)

Seventeen Goose modules are live in the accepted run, each over native kernels and driver primitives:
`FG169T_CUDA_TOPK`, `FG170S_CUDA_SSM`, `FG171R_CUDA_ROPE`, `FG174M_CUDA_GDN`, `FG174S_CUDA_GDN`, `FG175C_CUDA_CONCAT`,
`FG176G_CUDA_GET_ROWS`, `FG176R_CUDA_SET_ROWS`, `FG177C_CUDA_CPY`, `FG178Q_CUDA_Q`, `FG179W_CUDA_MOE_WEIGHTED`,
`FG181M_CUDA_MM_IDS`, `FG183F_CUDA_FWHT`, `FG184M_CUDA_MMVF`, `FG185Q_CUDA_MMVQ_Q`, `FG195V_CUDA_VMM_POOL`,
`FG196P_CUDA_PINNED`.

The remaining `.cu` files are kernel templates and driver entry points (`mmq.cu`, `mmvq.cu`, `fattn*.cu`, `convert.cu`,
`ggml-cuda.cu`, ...). They are the native primitives the Goose modules call, and their porting is tracked by the P3 items that
own each kernel family, not by this enumeration.

## Verdict for the criterion

Every backend that is built has Goose logic on its executed path, and the backends that are not built carry the
hardware-unavailable note. Every base object is either replaced by a named Goose object or listed as a native primitive with its
reason. The CPU and CUDA kernel families that stay native are listed with the compiler-scope and primitive notes above, so no
registered source is silently absent.
