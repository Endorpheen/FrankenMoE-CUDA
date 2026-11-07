# Complete Goose CPU/CUDA compute blocks

Experimental continuation of checkpoint EXP-2026-11-07-136. Production runtime
and compiler source remain unchanged. Apex is the primary iteration target;
Qwen is reserved for major milestones/model-specific work.

## Scope and execution

Goose owns the complete CUDA RMSNorm row computation: vector/scalar accumulation,
warp/block reductions, normalization and fused multiplication/addition/scaling.
Native C++ retains outer tensor addressing/launch and primitive CUDA intrinsic
bridges. The header extracts reachable emitted C bodies unchanged and adds device
qualifiers. No Goose stack allocator is reachable. This is not a full CUDA backend.

Goose owns the complete CPU Q8_K activation quantizer: signed maximum/tie order,
scale, nearest-integer arithmetic, quant bytes, block sums and zero-block behavior.
Only a float-bit-copy primitive remains in the C bridge. V1 uses a flat float slice;
v2 borrows fixed 256-float input blocks and enables better bounds elimination and
vectorization. Both execute inside actual Apex inference with the Goose RMSNorm.

## Correctness and rollback

CUDA: 180 standalone cases, 10,208,376 bitwise-equal values, input preservation and
output canaries. Shapes cover scalar/vector, warp/block reductions, partial vectors,
strides, excess channel/sample loops and all fusion variants. Actual Apex 512/32
and 32K/128 plus repeats match unchanged EXP107 content, IDs and top-5 probabilities.
GPU witnesses grow from 1,191,981 to 72,870,442 processed rows; rollback is zero.

CPU: 126 standalone cases plus 256 calls in eight threads match full Q8_K bytes,
inputs and canaries. Zero blocks preserve the native unwritten bsums field.
These checks also pass against native C rebuilt with identical flags. Actual Apex
processes 217,344 nonempty blocks and passes 65 sampled native full-block shadow
comparisons. Total 581,120 calls includes empty calls. Rollback trace is zero.

GGML_GOOSE_RMS=0 selects the retained original RMS kernel. GGML_GOOSE_Q8K=0 selects
the retained native quantizer independently. Timing libraries omit diagnostic
counters. Previous graph/free-list Goose components remain the same in both controls.

## Preliminary performance

No controlled quiet acceptance or language-superiority claim is made.

| RMSNorm, Apex 32K/128, mean of two | Native block | Goose block |
|---|---:|---:|
| Prefill tok/s | 2944 | 2947 |
| Decode tok/s | 29.37 | 28.37 |
| GPU MiB | 3348-3354 | 3348-3354 |

Possible small RMS decode regression remains open. Candidates are retained and
larger experiments continue; production promotion remains a separate decision.

| Q8_K, Apex 32K/128, one matched pair | Native Q8_K | Goose Q8_K v2 |
|---|---:|---:|
| Prefill tok/s | 2873.34 | 2877.27 |
| Decode tok/s | 28.46 | 28.83 |
| GPU MiB | 3348 | 3348 |
| Anonymous RSS MiB | 711.37 | 711.35 |

Both Q8_K runs retain the same Goose CUDA RMSNorm. Differences may be noise.

| Q8_K operation, 256 values, median ns | Time |
|---|---:|
| Frozen production native C (generic SSE) | 216.05 |
| Goose v2 | 169.91 |
| Native C rebuilt with identical new flags | 155.66 |

Thus the operation beats the old build but does not beat equally compiled C.
The full v1/v2 and same-flags series are retained, not just the best sample.

## Rejected behavior retained

CUDA v1 differed by 1 ULP in scalar multiply/add fusion. Explicit final FMA in
Goose matches the native CUDA arithmetic; all 180 cases then pass.
CPU default native-ISA FMA contraction differed by one quant byte in a seeded case.
Disassembly showed frozen native MULPS+ADDPS versus Goose VFMADD. Per-module
-ffp-contract=off restores byte equality. No compiler source fix was made.
V1 CPU operation was approximately 1.5x slower; fixed input blocks recover speed.

## Standalone reproduction

Run `python3 tools/check_q8k.py`. It compiles frozen Goose-emitted C and the exact
native algorithm control with identical flags in a temporary directory and runs
the deterministic/random/multithread byte checks. No model weights or compiler
producer binary is required. CC may select the C compiler.

CUDA fixture reproduction additionally needs NVCC, a CUDA device and the exact
compatible GGML headers/libraries used by the source baseline. Compile fixture.cu
with its source directory and matching GGML include directories; do not substitute
an arbitrary current GGML ABI. Full model command/environment records and producer
receipt are included. Original local build scripts preserve their recorded paths;
only the checkpoint CUDA generated-header include was made relative.

Qwen allocation graph/allocator checks passed 32/4 and 128/16 earlier; those are
not coverage claims for this new compute pair. Its proof is retained separately.
Full CPU/GPU scheduling and the independent comparative memory-safety audit remain
future work. No production component was removed or default profile changed.
