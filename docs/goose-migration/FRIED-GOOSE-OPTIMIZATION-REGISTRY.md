# Fried Goose compiler optimization registry

Record reusable compiler candidates discovered in FrankenMoE. For each
low-level inefficiency, inspect the lowering and determine whether the
cause is specific to runtime dispatch, device topology or compiler code
generation. A kernel workaround supplies evidence for a general compiler
optimization when it removes the same unnecessary work under a stated
legality condition.

Each entry records the cause, affected operations, pinned source and
toolchain, before-and-after generated code, measured impact, candidate
compiler change, correctness tests, status and experiment links. Separate
isolated compiler effects from end-to-end measurements of a larger change.
Preserve rejected and superseded results with their reasons. Use these
statuses: candidate, handed over, implemented, verified or rejected.
The compiler agent owns implementation. Keep FrankenMoE optimization
moving independently and retain its tested production path.

| Candidate | Status | FrankenMoE evidence |
|---|---|---|
| Read CUDA parameter aggregates directly when their address does not need to escape | Candidate | [EXP-1603 vector plans](CUDA-PARITY-EXP1476.md#vector-plans-in-cuda-kernel-parameters) |

## Read-only CUDA parameter aggregates

**Problem.** A 208-byte by-value shape plan in `goose_vector_ops` was
materialized as a private local array for every CUDA thread. The kernel
only needs to read the original parameter fields. In the generated C,
Goose's `GS_NOINLINE` on `cv_norm_g` kept a device call that received
the aggregate's address. Clang could no longer leave the plan in the
kernel parameter address space, so it emitted local stores and reloads.
The local frame was a materialized object. `ptxas` reported zero spill
bytes in both variants.

**Goose source.** This excerpt from
[`standalone-vector.goose`](../../goose/cuda/standalone-vector.goose)
is identical in both variants. The helper reads shape words through
`fg_vector_word`.

```goose
let op = cv_word(cfg, 0);
if op == CV_RMS || op == CV_L2 {
    cv_norm(x, y, dst, cfg, op, group, lane, param0);
    return;
}
```

**Lowering experiment.** Compile the same Goose source and by-value
OpenCL kernel wrapper through `tools/build-goose-cuda-kernels.sh`. The
initial build leaves the `static GS_NOINLINE void cv_norm_g(...)`
declaration in the generated fragment. The tuned build substitutes
`static __attribute__((always_inline)) void cv_norm_g(...)` for that
helper in the device fragment. All arithmetic and the 208-byte launch
ABI are identical. The evidence uses Goose executable SHA-256
`664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`,
Clang 18.1.8, CUDA `ptxas` 12.6.85 and `sm_89` assembly.

Recorded PTX excerpts before the device-only override:

```ptx
.param .align 8 .b8 goose_vector_ops_param_3[208],
.local .align 8 .b8 __local_depot7[208];
ld.param.u64 %rd128, [goose_vector_ops_param_3];
st.local.u64 [%rd1], %rd128;
ld.local.u64 %rd5, [%rd1];
```

Recorded PTX excerpts after the override:

```ptx
.param .align 8 .b8 goose_vector_ops_param_3[208],
mov.b64 %rd221, goose_vector_ops_param_3;
mov.u64 %rd1, %rd221;
ld.param.u64 %rd4, [goose_vector_ops_param_3];
ld.param.u64 %rd5, [%rd1+16];
```

SM89 SASS excerpt from the initial entry:

```text
STL.64 [R1], R4 ;
LDL.64 R2, [R1] ;
```

SM89 SASS excerpt from the tuned entry, with plan operands in constant
parameter space:

```text
IMAD.MOV.U32 R0, RZ, RZ, c[0x0][0x178] ;
IMAD.MOV.U32 R2, RZ, RZ, c[0x0][0x17c] ;
```

| Isolated assembly property | Initial by-value | Tuned by-value |
|---|---:|---:|
| Vector per-thread local frame, bytes | 208 | 0 |
| Vector SASS local load/store instructions | 49 | 0 |
| Vector registers per thread in offline assembly | 45 | 40 |
| Spill bytes, either variant | 0 | 0 |

**Measured effect.** EXP-1603 compares the entire vector plan change
against the previous explicit-upload path. On Apex 32K/512, two clean
pairs save 783 and 441 ms of decode, with about 612 ms median gain.
The matched 448-step CUDA prefix removes 327,936 H2D copies of 208 bytes.
Vector kernel time falls from 668.262 to 516.712 ms and reference RMSNorm
from 283.235 to 239.189 ms. The compiler-local-frame result is isolated
in the assembly table. Execution time for inlining alone has not been
isolated. The reference RMSNorm's unrelated 64-byte local frame remains.

**Compiler candidate.** For the CUDA lowering, track the address space
and read-only, non-escaping uses of a by-value aggregate through device
helpers. Inline helpers when that preserves resource usage, or
specialize their interface to consume the required fields as values.
Retain a materialized copy for mutable or escaping addresses and
unproven callees. Respect the kernel parameter ABI, valid alignment,
branch semantics and target register limits. Measure the code-size and
occupancy tradeoff before applying the optimization broadly.

**Correctness and acceptance.** The runtime change passes 15 strided
vector cases, windowed CUDA multiply at 4,096 planes and its tail,
weighted RMS with strided input and broadcast weights, and Compute
Sanitizer with zero errors. The paired Apex captures match text, IDs,
probabilities and expert counters, with identical peak VRAM and zero
swap. A compiler-only candidate still needs a paired build with the
same by-value ABI and the runtime override removed. Check that pure
reads use parameter space, that mutable and externally escaping cases
preserve semantics, and that resource usage and runtime do not regress.

Raw generated fragments, PTX/SASS, assembly logs and captures are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/vector-params-exp1603`.
See the [complete runtime experiment](CUDA-PARITY-EXP1476.md#vector-plans-in-cuda-kernel-parameters).
