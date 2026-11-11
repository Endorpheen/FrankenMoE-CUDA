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

## Shim built, patch installs, process aborts

`libfg186q.so` was built from the module object, the host object and the shim (141,456 bytes; exports the mangled
`ggml_cuda_mul_mat_vec_q` and `fg185q_native_ggml_cuda_mul_mat_vec_q`). On a real run the patch installs:

    FG186S_PATCH target=0x7462284b0920 ours=0x746237769490 enabled

and then the server never becomes ready: the log stops right after the earlier items' witnesses, so the process
dies inside the patched path rather than failing to load.

So the module is binary-compatible enough to link and patch, but its kernel does not survive execution against
this backend build. The next probe is to run the same case under `compute-sanitizer` and to compare the module's
`mmvq.cuh` expectations with the flint build's, since the module was written against the older backend.

Recorded; not accepted; the active runtime is untouched and still uses the native kernel.

## Loading the module library alone aborts the process

The abort is not caused by the patch. With `GGML_GOOSE_CUDA_MMVQ_Q6K=0` (shim skips patching) the server still
never becomes ready, so merely loading `libfg186q.so` breaks the process; the log stops at the same place.

The first cause found was concrete and is fixed: the library was linked without `libstdc++`, so `__once_proxy`
was undefined and even `timeout` reported a symbol lookup error under the preload. Relinked with `-lstdc++`, the
preload loads, but the server still dies.

Remaining difference from a working build: this object carries its own CUDA fat-binary registration
(`__cudaRegisterFatBinary*`) compiled with `-arch=sm_89` outside the backend's CMake target, and it is loaded as an
independent DSO rather than linked into `libggml-cuda`. A DSO with its own fat-binary registration, loaded into a
process that already registered its CUDA modules, is the likely source.

Next: either compile the module into the backend target (CMake, so registration happens once) or keep the module
device-free and compile only host code here, calling the existing device kernels.

## Exact fault location

Fixed the last load-time issue (the module expects its native callback under a C++ mangled name; the shim now
defines exactly that symbol). The patch then installs and the process reaches the kernel. Under gdb the fault is
localised:

    Thread 1 "llama-server" received signal SIGSEGV
    #0 ggml_cuda_mul_mat_vec_q(...) from /tmp/libfg186q.so
    #1 ggml_backend_cuda_graph_compute from .../llama.cpp-flint-cuda/build-cuda/bin/libggml-cuda.so.0
    #2 fg163_backend_call from stage-run/libggml-base.so.0

So the call is routed into the Goose module and the module itself crashes on first use against this backend. The
module and its generated device code were written for the older backend; the host structures line up (the patch
and the callback resolve), but the kernel body does not survive.

This is now a normal porting defect to debug with compute-sanitizer on a single call, not an infrastructure
blocker: the build route works end to end (backend from flint, module compiled unchanged, shim patching, symbol
resolution, call routed).

Active runtime untouched; native kernel still used there.

## compute-sanitizer is clean: the fault is host-side

    compute-sanitizer --tool memcheck ./llama-server ...
    ========= ERROR SUMMARY: 0 errors      (process still exits 11)

So no device memory error is reported, and the crash is in host code inside the module, before or around the
kernel launch. The exact instruction requires the module's own address map (the gdb frame was at an offset that
does not map one-to-one here).

Next probe: add stage prints to `cuda-runtime.cu`'s host part (packet fill, quantize call, launch, read-back) and
rebuild; the last print before the crash names the step. This is the same incremental approach that closed the CPU
items, and it costs one rebuild per step.

## Stage trace: the crash is between the packet fill and the quantize call

Instrumented the host path and reran:

    FG186T step=1 enter
    FG186T step=2 dev=0
    FG186T step=3 locals ne00=2048
    FG186T step=4 packet
    (no step=5)

So the module reaches the call with a valid context and valid tensor geometry, and dies before the quantizer is
invoked — i.e. in the staging allocation region:

    ggml_cuda_pool_alloc<char> src1_q8(ctx.pool(), ne13*ne12*ne11*ne10_padded*sizeof(block_q8_1)/QK8_1);

Next: split that region with two more prints (`ctx.stream()` / pool size / after alloc) to pin the exact
expression. This is the last mile: everything else in the module's host path is reached and sane.

Active runtime untouched.

## Exact expression: the staging allocation crashes

Second-stage trace:

    FG186T step=4a stream=0x5f229644abd0
    FG186T step=4b pad=2048 bytes=4608
    FG186T step=4c pool enter
    (no step=4d)

So the crash is inside `ggml_cuda_pool_alloc<char> src1_q8(ctx.pool(), 4608)`: the context and stream are valid, and
the request is only 4.5 KiB, so it is not an out-of-memory condition — the allocation path itself faults for this
module.

Observation that matters: `ctx.stream()` and `ctx.pool()` are inlined against the flint headers, and this module
was compiled with exactly those headers, so the offsets agree; the remaining candidate is that the allocation is
reached before the backend's pool has been set up for this device, or through a pool type mismatch.

Next: print `ctx.device`, the pool type via a virtual call guard, and try allocating through the backend's own
`ggml_cuda_pool_alloc` in a native peer call for comparison.
