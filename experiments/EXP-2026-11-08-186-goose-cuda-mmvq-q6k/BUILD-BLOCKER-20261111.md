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

## Context is valid; the fault is inside lazy pool creation

    FG186T step=4c pool enter dev=0 stream_no=0
    (no step=4d)

`ctx.pool()` finds no pool for (device 0, stream 0) and calls `new_pool_for_device(*this, 0, 0)`; the crash is
inside that lazy creation. Note the same call succeeds in the native kernel on the same graph, so the difference is
in how this module reaches it: `new_pool_for_device` is a member of the flint backend's context and is invoked
from a foreign DSO.

Next probe: call `ctx.pool()` once earlier (before any other work) and print its address, and also try
`ggml_cuda_pool_alloc` from a native peer at the same point, to see whether the difference is the timing of the
first pool creation rather than the call site.

## Layout verified against the right headers, and the crash precedes the library call

Dependency check with `nvcc -M` confirms the module really compiles against
`llama.cpp-flint-cuda/ggml/src/ggml-cuda/common.cuh` and `ggml/include/ggml-cuda.h`, so the context layout is the
one the running backend uses — the "wrong struct offsets" hypothesis is ruled out.

gdb then shows the fault address inside the module, with **no frame** for `new_pool_for_device`, and the symbol is
`T` in the backend and resolves. So the crash happens while evaluating `ctx.pool()`, before the backend member is
entered: the access `pools[device][curr_stream_no]` on a context that is valid from the backend's point of view.

Next probe: print `&ctx` and call `ctx.pool()` once at the very top of the function, before any other use of the
context, to separate "the object is fine but this expression faulting" from "the first pool creation is the
problem".

## The crash point moves: this is memory corruption, not one bad expression

With an early `ctx.pool()` probe added at the top of the function:

    FG186T step=1 enter ctx=0x57401076c7c0 dev=0
    FG186T step=1a pool=0x11cdda69e8b        <- pool() works when called early
    FG186T step=2 dev=0
    FG186T step=3 locals ne00=2048
    FG186T step=4 packet
    FG186T step=4a stream=...
    (stops here)

In the previous build the trace passed 4a, 4b and 4c and stopped in the allocation; now it stops at 4a. The call
that "crashed" earlier now succeeds when made first. A fault whose position moves with unrelated edits is memory
corruption or undefined behaviour, not a specific wrong expression.

Most likely source: the module's packet/`fg186q_packet` and the generated device code were written against the
older backend's layout, and this build's structures/packet expectations differ, so the module writes past its own
buffers before the fault surfaces elsewhere.

Next probe: run the same call under `compute-sanitizer --tool memcheck` with `--check-device-heap` and also build
the module with `-fsanitize=address` on the host part, to catch the first out-of-bounds host write. That is the
instrument that will name the actual defect instead of its shadow.

## Ruled out: quantizer signature and device-side shared memory sizing (partly)

- `quantize_row_q8_1_cuda` takes 13 arguments in this backend (`mmvq.cu:1557`) and the module passes 13 in the
  same order, so the earlier suspicion of a mismatched call is wrong — that call is well-formed.
- `fg186h_launch` computes shared memory as `(v[7][6]-1) * v[7][5] * sizeof(float)`, matching the native formula
  `(nwarps-1)*ncols_dst*warp_size*4` when `v[7][6]=nwarps` and `v[7][5]=warp_size`. That is only correct if the
  Goose planner actually filled `v[7][*]`; if it left any of them zero, `grid` or `block` becomes
  `(0, ...)`, and a zero-sized launch is itself invalid, not a crash inside the kernel.

So the open question narrows to: does the module's own planner fill `v[7][0..6]` before `fg186h_launch`, and with
what values? Next probe prints `v[7][0..6]` right before the launch, and the shared-memory size, which will show
whether the fault is a bad launch geometry rather than the allocation that precedes it.

## Confirmed unstable: the fault moves between 4a and 4c across builds

Same module, only a print added inside `fg186h_launch` (never reached):

    step=4a stream=...        (previous build stopped here)
    step=4b pad=2048 bytes=4608
    step=4c pool enter dev=0 stream_no=0   (this build stops here)

With no change to the code before `step=4a`, the stop point moves. That is decisive: the defect is not a
specific expression on this path but memory corruption or undefined behaviour that surfaces at whichever point
next touches the damaged state.

Where it most plausibly originates: `ctx.pool()` returns a reference created by the backend's
`new_pool_for_device` and the very next use is `ggml_cuda_pool_alloc(...).get()`; earlier the same `ctx.pool()`
succeeded when called at the top of the function. Combined with the fact that the module was written for an older
backend, the most likely explanation is that the module's packet/device expectations and this backend's object
lifetimes disagree, and something is written past its bounds before the visible fault.

Stopping active work on this path here, deliberately:
- the integration route is proven (backend built, module compiled, shim patches, call routed, gdb frame
  confirmed);
- the defect is host-side UB whose location moves with unrelated edits, so chasing the print that currently
  crashes would be chasing a shadow, exactly the loop the instructions warn against;
- finding the first bad write needs AddressSanitizer actually reaching the module, which the last attempt did not.

Registered as BLOCKED with this diagnosis. The concrete next instrument is an ASan build whose module is
guaranteed to be reached (single-call harness instead of a full server), which is a separate, self-contained task.

## Single-call harness built; ASan cannot coexist with CUDA init here

Built `tests/harness.cpp`: initializes the CUDA backend, allocates Q6_K x F32 tensors, fills them, and calls
`ggml_cuda_mul_mat_vec_q` exactly once. Compiled through nvcc plus a g++ link step with
`-fsanitize=address` (427,384 bytes), because nvcc rejects the flag directly.

Running it fails before the call:

    ggml_cuda_init: failed to initialize CUDA: out of memory
    ggml_backend_cuda_init: invalid device 0

with **8418 MiB free** on the card, and `allocator_may_return_null=1` / `max_allocation_size_mb` change nothing.
So AddressSanitizer's allocator cannot coexist with CUDA's virtual-memory reservation in this environment, which
also explains why the earlier ASan run of the server never reached the module.

Consequence: the harness is ready and useful (it calls the kernel directly, no server, no model), but the
instrument must be Valgrind memcheck or CUDA's own API-error checking, not ASan. Recorded so the next attempt does
not repeat this dead end.

## Breakthrough: the module is correct, and the fault was argument evaluation order

Split `ctx.pool()` and the size into separate statements before constructing the allocation:

    ggml_cuda_pool & p = ctx.pool();
    const size_t sz = ...;
    ggml_cuda_pool_alloc<char> src1_q8(p, sz);

After that, the harness (`tests/harness.cpp`, one call, no server, no model) runs the whole path:

    FG186T step=6 run eligible=1
    FG186T launch grid=(4,1,1) block=(32,4) shm=384 v7=[4,1,1,8,8,32,4]
    FG186Q_CUDA_MMVQ_Q6K host=0x1,cuda_status=0,paths=0x1,guards=0
    harness: returned

and the kernel's output is **bit-identical** to the native kernel on the same input:

    native: out[0]=145453472 out[1]=31870514
    goose : out[0]=145453472 out[1]=31870514
    cmp: IDENTICAL

So the module itself is correct; the earlier "moving crash" was undefined behaviour in that single expression,
where `ctx.pool()` (which lazily creates the pool) and the size expression were evaluated as constructor
arguments.

In the full server the path now reaches `step=4c1` with `pool=0x11cf5c0d9f7` — an unaligned, implausible address —
whereas the harness reports a normal heap address. So in the server the context reference and its layout still
disagree, which is a separate fact to chase next (candidate: the server loads libggml-cuda from
`llama.cpp-flint-cuda/build-cuda/bin`, while the module was compiled against `flint/ggml/src`).

## Same implausible pool address with our own backend build

Pointed the server at the backend built from the same sources the module was compiled against
(`LD_LIBRARY_PATH=/tmp/build-cuda-goose/bin` first; `ldd` confirms it is the one picked) and reran. The trace is
unchanged:

    FG186T step=4c1 pool=0x11cf9ede005 size=4608

The address is unaligned and nearly identical in shape to the one seen with the flint build (0x11cf5c0d9f7), while
the harness reports a normal heap address (0x5d4580588bb0). Since swapping the library did not change it, the
difference is not which backend is loaded but what the module is handed as `ctx` in the server process.

Two candidates remain, both concrete:
1. the server process has more than one copy of libggml-cuda mapped (a `dlopen` by absolute path would bypass
   LD_LIBRARY_PATH), so the object was created by one copy while the patched function lives in the other;
2. the goose-modified `libggml-base` (fg163_backend_call) passes a different context than a plain backend would.

Next probe: print `&ctx`, `dladdr(&ctx)` and `dladdr(ggml_cuda_mul_mat_vec_q)` from inside the module, and dump
`/proc/self/maps` filtered for ggml-cuda while it runs. That settles both candidates at once.

## The server really uses stage-run's libggml-cuda, and swapping it changes nothing

`dladdr` from inside the shim shows the patched symbol belongs to
`stage-run/libggml-cuda.so.0` (base 0x736350200000), **not** to the flint build directory that `ldd` reports for a
separately started binary. The maps dump confirms exactly one copy of that file is mapped.

I then replaced `stage-run/libggml-cuda.so.0.27.0` with the build compiled from the same sources as the module
(sha 219af9757810 -> 83ef33dc89e2), ran again, and restored the original afterwards (hash verified back to
219af9757810). Result: identical trace and the same implausible `pool=0x11d0039ab04`.

So the layout hypothesis is closed too: with the library built from the very sources the module uses, the module
still reads a bad `pools[device][curr_stream_no]` reference in the server, while the same module works perfectly in
the harness. What differs is the process: the server builds the CUDA backend through the goose-modified
`libggml-base` (fg163_backend_call), the harness calls the backend API directly.

Next probe: print, from the module, the address of `ctx` and the address the *native* call would receive
(capture `ctx` in the shim before patching), to confirm whether the object passed to the patched entry is the same
one the backend created.

## The context is the same object; the pool slot itself is already corrupt

Printing the fields from inside the module, both processes show an identical, valid context:

    harness: ctx=0x622ce303e900 dev=0 name='CUDA0' curr_stream=0
    server : ctx=0x61f6233877c0 dev=0 name='CUDA0' curr_stream=0

and reading the slot directly:

    server : step=1a slot=0x560edec164b0 slot0=0x11d06d4263d
    server : step=1b pool=0x11d06d4263d

So the context pointer, the device, the name and the stream index are all correct, but the `pools[0][0]` slot
already holds an implausible pointer before the first pool creation, while the harness has a valid one there.

Consequence: the module is not mis-reading the context — something in the server writes a bad value into that slot
before the patched entry runs. Candidate: the goose-modified backend code in this process (fg symbols in
libggml-cuda / libggml-base) writing into the context structure, or a pool type whose object is placed in CUDA's
VMM range and whose pointer must not be dereferenced as host heap.

Next probe: log the slot value at process start and after each backend call, and compare with the native path's own
reads, to find who writes it.

## The slot pointer is not merely odd, it is unreadable

Probed the object the slot points at:

    FG186T step=1a slot=0x5b5b7dc244a0 slot0=0x11d0a2c2258
    (trace ends here)

Reading `*(void **)&pool` — the vtable pointer — faults. So `pools[0][0]` in the server process does not point at
any mapped object: it is garbage, while the harness has a valid pool object there.

Summary of what is now established, and why this stops here for now:

- the module itself is correct: bit-identical to native in the harness on the same input (`cmp IDENTICAL`);
- the fault was originally UB in a constructor argument and is fixed;
- in the server the context pointer, device, name and stream index are all valid, yet one end-of-struct member
  holds an unreadable pointer before the first pool creation;
- swapping the backend library for one built from identical sources changes nothing, and `maps` shows a single
  copy;
- so something in this process writes a bad value into that slot, and that writer is not the module.

Deliberately stopping active work on this path again: the remaining question is ownership/state inside the
goose-built backend, and chasing it further is a different task from porting the kernel. Registered BLOCKED with
the complete evidence chain so whoever picks it up starts from the slot, not from the beginning.

## Module correctness widened across shapes

The harness now takes ncols_dst and nrows, and the Goose kernel was compared with the native one on five shapes,
same inputs, output buffers compared byte for byte:

    ncols_dst=1 nrows=4  -> IDENTICAL
    ncols_dst=1 nrows=8  -> IDENTICAL
    ncols_dst=2 nrows=4  -> IDENTICAL
    ncols_dst=4 nrows=4  -> IDENTICAL
    ncols_dst=1 nrows=16 -> IDENTICAL

So the kernel is not merely correct for the single shape the first probe used; it matches the native kernel for
multi-column and larger-row cases as well. That strengthens the conclusion that the remaining failure is
state/ownership in the server process, not the port.

## The slot pointer is not mapped at all, and it is not the goose CUDA modules

Two final checks:

1. Membership of the pointed address in `/proc/self/maps`:

       FG186T slotmap NOT MAPPED for 0x11d1206e293

   So `pools[0][0]` holds an address outside every mapping in the process — not a wrong object, not a pool in a
   VMM range, simply unmapped memory.

2. Ran with all fourteen `GGML_GOOSE_CUDA_*` modules disabled. The slot is still garbage
   (`0x11d1037e197`), so the goose CUDA modules are not the writer.

Combined with earlier steps, the picture is complete on the module side and open only on the process side:

- the kernel is byte-identical to native for five shapes in the harness;
- the context pointer, device, name and stream are valid in the server;
- the pool slot at the end of the same object is unmapped garbage before first use;
- the backend library in use is stage-run's, replacing it with an identical-source build changes nothing, and
  the goose CUDA modules are not the writer.

What remains is an ownership/state defect in the goose-integrated backend context in this process. That is a
distinct item, not kernel porting, and it is fully documented here.

## Backend creation is observable; the context it hands out is the one the module receives

Added a shim wrapper for `ggml_backend_cuda_init` (calls the backend's own symbol, then reports what it returned):

    FG186S_INIT backend=0x6287b77a9710 ctxguess=0x7c0ef3591840
    FG186T step=1 enter ctx=0x6287b6591840 dev=0 name='CUDA0' curr_stream=0
    FG186T step=1a slot=0x6287b6592530 slot0=0x11d162286c1

The backend handle never equals the context (as expected), and the context the module is handed is a normal heap
object with valid device/name/stream fields. The slot is already garbage at the first call.

So the writer is somewhere between backend construction and the first matvec, and it is not the goose CUDA
modules (they were all disabled with the same result). This is the exact interval a hardware watchpoint covers:
`watch *(void **)(ctx + offsetof(pools))` immediately after `ggml_backend_cuda_init` returns, then let the server
run and see which instruction writes.

That is the next concrete step, and it needs the context offset, which the module already prints. Nothing further
can be concluded from prints alone.

## Watchpoint attempt under gdb did not run

Tried the planned hardware watchpoint: break on `ggml_backend_cuda_init`, `finish`, compute the context from the
returned backend (`+0x18`), watch `ctx+0xcf0` (the slot offset measured from the module's own prints), then
continue. The batch session ended without reaching the watchpoint:

    0x00007ffff7f989fb in ggml_cuda_mul_mat_vec_q(...) from /tmp/libfg186q.so
    Couldn't get registers: Нет такого процесса

i.e. the process dies early under this gdb+preload combination, before the watch is armed usefully. Running the
same stack interactively (or with the harness instead of the server) is a better environment for the watchpoint,
because the harness reaches the same kernel in seconds.

This is recorded as a failed attempt, not as a result. The evidence chain stands as documented above; the open
question (who writes the slot) needs a watchpoint in a stable session.
