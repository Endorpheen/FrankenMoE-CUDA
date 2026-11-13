# EXP134: restricted Goose-to-C-to-CUDA path

Status: FEASIBILITY_CONFIRMED, no baseline promotion or model speed claim.
Igor explicitly requested a Goose language compilation path and a first check
of CUDA calls/overhead. CPU profiling EXP133 was prepared but its model requests
were deferred while this requested experiment ran. Production EXP107 stayed
loaded and idle; no runtime library or installed profile was modified.

## Actual compilation path

Pinned aardappel/goose v2026.3 Linux x64 archive, SHA256
06ac30284338269d3996d457412bf4f2b47c1f9a5601de65b6317187fe78d5c9.
The binary reports Goose runtime4d3da949e563. GCC13, NVCC12.4, sm89/RTX4070.
The compiler/cache/source/build stay under
/home/end0/AI/LlamaServer/exp134-goose-cuda; no global installation.

1. silu_gate.goose expresses FP32 SiLU-gate as gate/(1+expf(-gate))*up.
2. Goose emits standalone C. emit_cuda.py extracts the pinned pure scalar
   function, retains its arithmetic statements, adds device/forceinline
   qualifiers, and drops an unreachable host diagnostic after its return.
3. NVCC compiles that generated function into the real CUDA device kernel.
   No Goose runtime, stacks, allocation, TLS or host export wrapper executes
   on the GPU. This is a deliberately restricted adapter, not a general
   upstream Goose CUDA backend or a full-language GPU compiler.
4. A separate AOT Goose host function loops over the same C ABI CUDA bridge
   that the native C++ loop calls. The bridge owns CUDA memory/streams.

Goose's documented native graphics path uses GLSL/SDL3 with Vulkan/Metal/DX12;
its C export capability alone is not a device-code compiler. This experiment
adds the explicit restricted device step rather than treating a host wrapper
as a rewritten GPU kernel.

## Correctness and execution

The Goose-derived and native CUDA kernels ran on an actual RTX4070. Sizes
1,31,256,4096,65537,4194304 cover small tensors and partial-block tails; inputs
include signed zero, tiny values, +/-100, infinities and NaN. All six sizes
match byte-for-byte, maximum finite absolute error0. CUDA memcheck:0 errors.

Normalized disassembly contains the same160 instructions in both SiLU kernels.
Thus this case establishes a working source-language path, not a new faster
machine implementation. The native formula/order follows ggml CUDA's
unary.cuh SiLU and gated multiplication; no fast-math option was introduced.
The reference is the standalone scalar/kernel counterpart, not full graph
integration or model inference. Three FP32 device buffers total48MiB; CUDA
context/driver allocation is additional and was not separately attributed.

## Call-overhead probe

Five alternating caller pairs for each of: empty launch, native SiLU4096,
Goose-derived SiLU4096;4000 launches/block. Five alternating kernel pairs at
4194304 elements,100 launches/block. Total40 serial timing blocks in one
process after correctness/warmup. No model inference or concurrent build.
The same external bridge function is called by both AOT host loops.

Median host enqueue time per empty CUDA launch: C++1.66036us, Goose1.62352us
(observed difference-0.03683us, about-37ns). This small noisy experiment does
not establish a systematic FFI penalty or an end-to-end model speed change.
For4096-element launches, host medians depend on order/kernel and overlap;
all raw samples/ranges are retained rather than selecting a favorable case.

Large SiLU event interval per launch: native122.348us, Goose-derived124.590us.
Both have identical instructions; these observations do not support a language
speedup. Event intervals include idle gaps inside the bracketed CUDA stream,
not just isolated kernel active time. Host enqueue and synchronized wall time
are reported separately in results.json. Startup, data preparation, compilation
and host/device correctness copies are outside the timing blocks.

## Decision and limits

Accepted as a bounded feasibility proof only. No production integration,
Q4_K/Q6_K rewrite, CUDA Graphs lifetime claim, model correctness or full Apex
benchmark is implied. A useful performance candidate still needs a changed
algorithm/layout and real-model qualification; changing source language while
emitting identical GPU instructions supplies no such mechanism.

Sources: https://github.com/aardappel/goose (v2026.3 release), Goose reference
C exports, and Goose gfx documentation. Source and reproduction commands are
stored here; compiler archives/generated runtime/binaries remain local, with
hashes. The small generated device function and normalized instruction hash
make the constrained transformation auditable.

## Specification-driven follow-up

The pinned compiler's docs/goose_spec.md sections7.12-7.14 and implementation.md
sections6.13/9 were inspected alongside src/optimize_loops.h,
src/codegen_stmts.h, src/codegen_emit.h and src/bce.h. A compiler feature probe
proves all12 index checks disappear (four in the nibble unpack function),
produces SIMD clones and runs with AVX2 level1 on5950X under Clang. This checks
actual language capabilities; it is not a kernel speed measurement.

The first GCC13 host-loop probe does not measure Goose's simd specialization:
that feature is Clang-only. Bounds proofs need established length relations;
checkfree is a compile-time requirement, not an unchecked unsafe mode. Sum
loop shaping batches term evaluation but folds results in original order.
Generics specialize before C generation; forced inline cannot be combined
with export/simd. Export wrappers have runtime/thread ownership constraints;
a multithreaded CPU integration must respect them rather than assume arbitrary
worker calls safe. Packed layouts and absent alias guarantees can hurt SIMD.
The SoA column-array document is a proposal, not an implemented facility.

Final CUDA compilation explicitly uses --fmad=false to preserve the relevant
Goose no-implicit-contraction rule. This scalar SiLU has no multiply-add pair,
so both implementations still have identical device instructions; final
numbers above come from that exact final binary. Initial v1 samples/binary
are retained separately and not substituted into the final result.

Upstream bench/run_bench.py selects the best of repeated whole-process times,
including input generation/allocation/teardown. Its speed ratios cannot be
transferred to existing hand-vectorized ggml kernels. Their own bench/design.md
expects no allocation/layout advantage on flat scalar arrays. The next useful
work is real CPU attribution and exact packed-weight/routing experiments with
all paired samples, not extrapolation from the headline suite.
