# F1-03 - every remaining dependency, justified

Requirement: *every remaining dependency justified with source/symbol/version/license; a replaceable
owned parser, kernel, scheduler or server is not called unavoidable.*

Measured on the accepted release `v0.10.0-exp1476` (`builds/checkpoint-20261119-EXP1476`, the same
files `frankenmoe-release-20261120-exp1476` carries), 2026-11-19. `ldd` on `libllama.so.0` lists 19
shared objects; the bundle ships 24 files, of which 9 are our own `.so` names.

## Runtime, linked by `libllama.so.0`

| library | source | symbol that needs it | version | licence | replaceable by Goose? |
|---|---|---|---|---|---|
| `ld-linux-x86-64.so.2` | glibc | the loader itself | host glibc 2.4x | LGPL-2.1+ | no - OS substrate |
| `libc.so.6`, `libm.so.6`, `libdl.so.2`, `libpthread.so.0`, `librt.so.1` | glibc | `malloc`, `dlopen`, `sqrtf`, clocks | host glibc | LGPL-2.1+ | no - OS substrate (rule 27 counts it as such) |
| `libgcc_s.so.1` | GCC | unwinding, 128-bit helpers | host GCC 13 | GPL-3.0 with the GCC runtime exception | no, while any C or C++ object is linked |
| `libstdc++.so.6` | GNU libstdc++ | C++ containers and RTTI in the carrier sources | host GCC 13 | GPL-3.0 with the runtime exception | **yes, and it is a target**: it disappears when the last C++ source leaves the link (rule 27) |
| `libgomp.so.1` | GCC OpenMP | `GOMP_parallel` from the CPU backend's thread loop | host GCC 13 | GPL-3.0 with the runtime exception | yes, if Goose owns the thread pool; until then it is the only OpenMP implementation linked |
| `libcudart.so.12`, `libcublas.so.12`, `libcublasLt.so.12`, `libcuda.so.1` | NVIDIA CUDA 12 | `cudaMalloc`, `cublasGemmEx`, the driver API | CUDA 12.x installed on this host | NVIDIA proprietary, not redistributable | no - the only way to reach the device |
| `libggml.so.0`, `libggml-base.so.0`, `libggml-cpu.so.0`, `libggml-cuda.so.0` | built here from `vendor/flint` | the tensor library the whole graph runs on | 0.27.0, built from `1b44e307` | MIT | not applicable: these are **our own build**, not an external dependency |

## Our own components in the bundle (not dependencies, listed so they are not mistaken for them)

| file | what it is | status |
|---|---|---|
| `libllama.so.0` | the reconstructed unit (Goose doors plus carriers) | the runtime itself |
| `libllama-server-impl.so` | server implementation, to be replaced by Goose (P8-12) | own |
| `libllama-common.so.0` | CLI/common helpers, includes `common_json` (P10-07) | own |
| `libmtmd.so.0` | multimodal helper from the vendored fork | own build of vendored source, unused by the Apex path |
| `libmoe_mmid_tiled.so` | **debt**: a CUDA kernel built by `EXP-2026-11-18-1008-p1-12-gemv` and linked into the production set (`vendor/flint/ggml/src/CMakeLists.txt:196`) | ours, and a removal target: an experiment artefact in the shipped link is what rules 17 and 27 forbid |

## Build-time, vendored, not linked

| dependency | source | version | licence | why it stays |
|---|---|---|---|---|
| `vendor/flint` | llama.cpp fork, `PROVENANCE.txt` | commit `1b44e307`, copied 2026-11-16 | MIT (`vendor/flint/LICENSE`, ggml authors 2023-2026) | it is the tree the migration is replacing file by file; the licence permits the vendoring |
| nlohmann/json | bundled single header inside `vendor/flint/common` | as vendored with the fork | MIT | still the JSON parser until the Goose adapter is the default (P10-07); it is replaceable and is being replaced, so it is not called unavoidable |
| cpp-httplib | bundled inside `vendor/flint/tools/server` | as vendored with the fork | MIT | serves HTTP and static assets until Goose owns them (P8-09); also replaceable |
| Goose compiler | `/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose`, called by `tools/*.sh` | `2026.3-string`, pinned in `tools/verify-clean-build.sh` | the project's own (Fried Goose) | build-time only; nothing from it is linked into the artifact |
| CUDA toolkit (`nvcc`) | host installation | CUDA 12.x | NVIDIA proprietary | build-time only |

## What this report refuses to claim

- `libmoe_mmid_tiled.so` is not justified as a dependency: it is our experiment artefact, and the
  honest entry for it is a removal target, not a justification.
- `libstdc++` and `libgomp` are listed as replaceable with an owner. Calling them unavoidable would be
  exactly what the requirement forbids.
- The two libraries with no redistribution right (NVIDIA) are not in the bundle and must not be added
  to it; the release ships only our own builds, whose licences are MIT.
- Version strings for the host glibc and GCC are the host's, not the project's; the release does not
  pin them, and this report does not pretend otherwise.

## How to re-measure

```
ldd builds/checkpoint-20261119-EXP1476/libllama.so.0
cat vendor/flint/PROVENANCE.txt
ls builds/checkpoint-20261119-EXP1476
```

## Correction: two dependencies the first pass missed

Written 2026-11-19 evening, after `tests/regression/set-self-contained` reported `libssl.so.3` as a
release dependency. The first version of this report read `ldd` of `libllama.so.0` only; six ELFs in the
release need OpenSSL through `libllama-common` and `libllama-server-impl`:

| library | who needs it | licence | justified? |
|---|---|---|---|
| `libssl.so.3` | `libllama-server-impl.so`, `libllama-common.so*` | Apache-2.0 | yes for HTTPS serving; it is a system library and is not shipped |
| `libcrypto.so.3` | the same, as OpenSSL's companion | Apache-2.0 | yes, same reason |

Neither is bundled; both must exist on the host. That is the ordinary arrangement for TLS, and it is
recorded here because "every dependency justified" was not true of this document an hour ago.

The lesson is the same one the rest of the day produced: a report built from one artefact answers a
question about that artefact, and the wording around it must not quietly widen the claim.
