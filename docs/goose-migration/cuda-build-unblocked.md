# Building the Goose `libggml-cuda` here: unblocked

Igor corrected the earlier judgement again: the ggml build is integration work. The CUDA one is now
solved too, without touching production or the compiler.

## Two blockers, two fixes

| blocker | what it looked like | fix |
|---|---|---|
| nvcc 12.6 against the system glibc | `cospi`/`sinpi`/`rsqrt` exception-specification clashes in `bits/mathcalls.h`; `-ccbin g++-12/13` and `-std=c++03/11` all fail | not used: the build goes through clang 21, which handles this glibc |
| clang rejects an nvcc-only flag | `clang++: error: unknown argument: '-extended-lambda'` | a thin wrapper drops that flag before calling clang++ |
| `GGML_CUDA_FA_QUANTS` without quotes | `use of undeclared identifier 'q4_0'` — the value is passed unquoted and nvcc tolerates it while clang does not | the wrapper quotes that one definition |

Wrapper kept at `runtime/reconstructed/tools/clang-cuda-wrap`.

## Result

    cmake <flint tree> -DGGML_CUDA=ON -DCMAKE_CUDA_COMPILER=tools/clang-cuda-wrap \
          -DCMAKE_CUDA_ARCHITECTURES=89 -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build . --target ggml-cuda -j 16

builds `bin/libggml-cuda.so.0.27.0` with **0 errors**.

So the artefact boundary I recorded for APEX.CUDA_MATVEC and APEX.CUDA_MMQ is gone: their remaining work
is porting the Q6_K and MMA paths into a library this tree can build, not waiting for a toolchain.

## Next

Port EXP186 (Q6_K matvec) into this build with its own switch, rebuild, and require: byte-exact outputs,
memcheck clean, the witness live, completion `e68df05b`, and one short checkpoint. Performance is recorded,
not vetoed, per Igor's latest order.
