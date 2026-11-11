# CUDA_MATVEC: module correct, rebuild chain missing

EXP186 (Goose Q6_K CUDA matvec) is functionally complete and was measured byte-exact: 4/4 outputs identical,
Apex content sha256 `e68df05b`, memcheck clean. It was rejected on speed (-3.66% decode) and therefore left opt-in,
so the symbol never entered the active `libggml-cuda` (verified: 0 occurrences of `GGML_GOOSE_CUDA_MMVQ_Q6K` in the
staged library, and a run with the flag set produced no Q6_K witness).

Under the owner's decision that speed is not a veto and that accepted improvements are enabled by default, the
source flag was changed from opt-in to opt-out (`GGML_GOOSE_CUDA_MMVQ_Q6K=0` disables). Rebuilding requires
`build_runtime.py`, which reads `runtime/exp185/build/commands.json`, `runtime/exp185/build/native-mmvq.o` and
`runtime/exp185/run/libggml-cuda.so`. In this restored tree only `runtime/exp185/measured/` and
`runtime/exp185/results/` exist, so the object chain is gone and the module cannot be relinked as-is.

Next: reconstruct the exp185 build chain (native-mmvq.o + host.o + base library) from the surviving
`measured/.../run/libggml-cuda.so` and the sources, then enable and verify.

## Reconstruction progress

`EXP-2026-11-08-185-goose-cuda-mmvq-q4k/build_runtime.py` does run here and produced
`runtime/exp185/build/cuda-mmvq-q4k.o` and `runtime/exp185/build/host.o` before stopping, because it consumes
`runtime/exp184/build/commands.json`, which is also missing.

So the dependency chain is 180 -> 181 -> 182 -> 183 -> 184 -> 185 -> 186, each experiment's `build_runtime.py`
relying on the previous one's artefacts. Each link must be rebuilt in order (nvcc), starting at EXP180, before the
Q6_K module can be relinked into the active library. The sources and scripts for every link are present; only
their build outputs are missing.

## Chain reconstruction attempted and stopped

Ran every `build_runtime.py` from EXP170 upward, in order. Result:

- EXP170, 171, 172: **no `build_runtime.py` at all**, so their build outputs cannot be produced here;
- EXP173: fails with an empty message (its own prerequisites are missing);
- EXP174 and 175: stop on the missing `runtime/exp174/build/commands.json`;
- EXP176..186: `IndexError: list index out of range`, because they read the previous link's artefacts.

So the chain cannot be reconstructed in this tree by running the scripts: its foundation (EXP170-172) has no
build scripts, and everything above consumes artefacts that were never committed. What survives is the built
`libggml-cuda.so` under `runtime/exp185/measured/.../run/`, which contains the Q4_K module but not Q6_K.

Consequence for CUDA_MATVEC: the Q6_K module is functionally verified (byte-exact, `e68df05b`) but cannot be
linked into the active library from what is present. Options, none of which I will choose unilaterally: (a) treat
the surviving exp185 library as the base and splice the Q6_K module into it at the binary level, (b) rebuild the
CUDA backend from the flint sources with both modules compiled in, or (c) record the item as blocked by missing
build infrastructure.

## A working route exists: build the CUDA backend from the flint sources

The surviving-artefact route was abandoned in favour of rebuilding the backend from source, which needs no
missing artefacts:

    cmake -S llama.cpp-flint-cuda -B /tmp/build-cuda-goose -DGGML_CUDA=ON -DCMAKE_CUDA_COMPILER=/usr/bin/nvcc \
          -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF
    ninja -j16 ggml-cuda    ->  bin/libggml-cuda.so.0.27.0  (145,811,304 bytes)

The EXP186 module then compiles **unchanged** in that tree:

    nvcc -c source/cuda-runtime.cu -O3 -std=c++17 -arch=sm_89 \
         -I source -I <flint>/ggml/src/ggml-cuda -I <flint>/ggml/include -I <flint>/ggml/src
    -> /tmp/fg186.o  (128,664 bytes, rc=0, warnings only)
    gcc -c source/host-unit.c -> /tmp/fg186_host.o   (defines fg186qh_run)

Remaining unresolved externals of the module: `fg186qh_run` (satisfied by the host object), `ggml_abort` and
`ggml_type_size` (satisfied by libggml-base), CUDA runtime symbols (resolved at load), and the deliberately
absent `fg185q_native_ggml_cuda_mul_mat_vec_q` — the module calls the native kernel under that name, so the
shim must supply it, pointing at the original `ggml_cuda_mul_mat_vec_q` in the loaded library.

Next step: build a small shim library from these two objects that (a) exports
`fg185q_native_ggml_cuda_mul_mat_vec_q` forwarding through a saved pointer, and (b) installs the module's
`ggml_cuda_mul_mat_vec_q` by entry patching, exactly as the CPU item did; then verify with the A -> B -> A
defect test and the e68df05b run.
