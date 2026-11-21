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
| Specialize large CUDA helpers at call sites with constant mode arguments | Candidate | [EXP-1609 Flash lowering](CUDA-PARITY-EXP1476.md#flash-attention-device-lowering-and-l2-prefetch) |
| Elide empty ownership scopes in borrowed scalar loops | Candidate | [KeelWing Apex CPU sampler](#keelwing-ownership-scopes-in-the-apex-sampler) |

## KeelWing ownership scopes in the Apex sampler

**Problem.** In the composed FrankenMoE host unit, KeelWing emits
ownership-region entry, exit and unwind calls in the inner scan of
`sm_partial_sort`. The scan borrows its input and output slices and has
no owned allocation on the path where a logit does not exceed the heap
threshold. The emitted housekeeping runs for every examined vocabulary
position. The same sampler compiled separately on KeelWing does not
insert that housekeeping into this loop.

**Reproduction.** Use
[`sampler-chain-full.goose`](../../goose/sampling/sampler-chain-full.goose)
at source revision `95d15b3386bb15a353eec52108e135d5839d15e1`, with
file SHA-256 `631a8045b5ca448c12bb39ab8807989a5b1debf6985085b09bd9ded4db091fc4`.
Compare the old compiler `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`
with KeelWing `e2e46ade68cafa25eb05239b6d7d5e2afc674da393637132dc2bfdbd8865d41a`.
Build with GCC 13 at `-O1` and call
`fg_sampler_chain(2, logits, probabilities, ids, 248320, 0, k, 0, 0)`.
The probe restores the same deterministic logits before each call, warms
once, then times 40 calls for each K. The probe SHA-256 is
`a6069de74717fd16c06bfe8bd472c4cec85366666fbfb2b2b01a3c0838280ecb`.

| Top-K on 248,320 logits | Old full unit | KeelWing full unit | KeelWing sampler alone |
|---|---:|---:|---:|
| K=5, ms/call | 0.132 | 17.649 | 0.194 |
| K=20, ms/call | 0.133 | 16.770 | 0.162 |

The top-ID fingerprints matched across all arms for both K values.
An unrelated export using only a growable scalar array still ran the
sampler below 0.18 ms. An unrelated export adding an owned copy
reproduced the regression without ever being called:

```goose
export fn sampler_probe_owned_copy(input: i64) -> i64 {
    var buffer: i64[>..] = [input, 7];
    let owned: i64[] = copy(buffer);
    return owned.len;
}
```

The same sampler then took 14.260 ms for K=5 and 14.552 ms for K=20,
with the same ID fingerprints. In KeelWing's `compiler/emitter.goose`,
`c_owned_helpers` enables `ctx.c_owned_enabled` when it finds any type
with an owned image in the unit. `emit_function` and the statement and
loop emitters consume that unit-wide flag without checking whether the
current function or region uses ownership.

The Goose loop is:

```goose
while i < n {
    if lv[i] > lv[0] {
        pop_heap_to(lv, id, 0, npartial, i);
    }
    i = i + 1;
}
```

The composed KeelWing C function `gg_function_339` enters a temporary
scope to compare `i < n`, enters additional scopes to compare the
logits and increment `i`, and executes
`gg_owned_unwind_to(t33); goto label1;` on the backedge. The isolated
C only performs the bounds-checked comparison, heap update when needed,
and increment. This is host code, so CUDA PTX, SASS and registers do
not apply to this compiler issue.

**Measured impact.** On matching Apex 32K/32 Nsight traces, the 31-step
decode window grew from 1,069.922 to 3,206.916 ms. GPU busy time decreased
from 781.298 to 753.031 ms, while GPU idle grew from 288.624 to 2,453.885 ms.
The interval from the 993,280-byte logit readback to the next 8,192-byte
upload, outside CUDA API calls and before submission, grew from 69.812 to
1,891.565 ms. The Top-K microprobe isolates one contributor to that
interval. The generation loop calls Top-K for token selection and again
for probability reporting. Do not assign the entire interval to Top-K
or sum it with overlapping host waits. The separate KeelWing gprofng
capture also sampled `gg_owned_enter` and `gg_owned_leave` during decode.

**Causal control in FrankenMoE.** Keep both KeelWing CUDA images and the
rest of the host source fixed. Compile only the existing token selection
and probability reporting functions in a separate Goose unit and call
them across the generated ABI. In an unprofiled Apex 32K/512 run with
the same request bytes and effective profile, this reduced decode from
50.923 to 23.948 seconds (10.03 to 21.34 tokens/s). Prefill was 2,075.89
and 2,112.11 tokens/s, respectively. All 512 token IDs, probability rows,
response bytes and SLRU counters matched. Process VRAM peaked at
8,188 MiB in both runs and neither process swapped. This accounts for
26.975 of the 36.729 seconds lost against the old toolchain. The separate
unit is a diagnostic control. Separating the complete HTTP generation
loop as a second control reduced decode further to 21.076 seconds
(24.25 tokens/s) at 2,093.00 tokens/s prefill. The 512 token IDs, all
probability rows, response bytes, SLRU counters and peak process VRAM
still matched the monolithic KeelWing build. This recovered 29.847 seconds
of the loss. The original 36.00 tokens/s decode remains the target.

**Compiler acceptance.** Omit region bookkeeping where a local effect
analysis proves that a region cannot acquire owned allocations, roots,
or cleanup obligations, and no reachable call can mutate it. Preserve
borrow checks and unwind order for allocating or escaping paths. Test
both the composed sampler and an isolated case, including the heap
update path, tied logits, and previously qualified release cases with
allocating loops, nested calls and early exits. Compare selected IDs,
probabilities and state on identical operands. Repeat a real Apex pair
while preserving the KeelWing prefill gain.

**First compiler candidate.** An isolated emitter patch uses typed
scalar and slice operations plus a closed direct-call graph to prove
when function scopes can be omitted. The patch retains ordinary scopes
for unknown calls and ownership-bearing functions. The owned-copy
reproducer improved from 14.006/14.097 to 0.176/0.207 ms for K=5/K=20,
with identical top-ID fingerprints. Three supported ownership tests
passed with AddressSanitizer and UndefinedBehaviorSanitizer. In the
composed C, the Top-K scan contains zero of the former 19 scope entries
while retaining both bounds checks. With the original KeelWing CUDA
images, the unchanged FrankenMoE host source achieved 2,141.76 prefill
and 14.67 decode tokens/s on Apex 32K/512. Token IDs, probabilities,
the response and SLRU counters matched the unpatched KeelWing. Peak
process VRAM was 8,188 MiB with zero swap. This patch verifies the
Top-K fix and recovers only part of the whole-runtime decode loss.

**Second compiler candidate.** Fried Goose commit
[`bb1452fa`](https://github.com/Endorpheen/Fried-Goose/commit/bb1452faa1e3f4e4ff4d715723c91a4fcf4e8c78)
also inlines
empty region transitions and leaves allocation and root cleanup in its
checked slow path. A 248,320-element CPU loop that performs one real
owned copy improved from 5.581 to 1.751 ms across 30 measured iterations
per arm. The result matched. Three supported tests passed with ASan and
UBSan, and five invalid ownership cases kept identical diagnostics.
The unchanged host source with the same KeelWing CUDA images ran Apex
32K/512 at 4,096/4,096 with 2,140.49 prefill and 27.03 decode tokens/s.
Decode fell from 50.923 to 18.906 seconds against unpatched KeelWing.
The 512 token IDs, every probability row, response bytes and SLRU
counters matched. Peak process RSS was 21,481,284 KiB, peak VRAM was
8,188 MiB, and process swap and system swap-out delta were zero.
The old toolchain's 36.00 tokens/s took 14.193 seconds, leaving a
4.713-second decode gap. This was a host-only differential with pinned
device images. Full compiler release qualification is still open.

**Artifacts.** Local experiment directory
`/tmp/franken-goose-keelwing-exp1665.HMxMZL`, including `sampler-probe/`,
`nsys-{a,b}.nsys-rep`, `nsys-decode.json`, `cpu-phase-profile.json`,
`full-runs/`, `sampler-split-full/`, `generation-split-full/` and
`compiler-fixed-full/` and `compiler-v2-full/`. The first compiler patch is in
`build/fried-goose-keelwing-ownership/artifacts/keelwing-owned-scope.patch`.
Status: candidate.

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

## Call-site specialization of large device helpers

**Problem.** The causal Flash Attention entry passes constants
for fixup mode, partition count and initial position through a
large device helper shared with Stream-K. The CUDA compilation
pipeline leaves an out-of-line call on this path. Forcing both
levels inline specializes the causal call but also expands the
Stream-K body, where the fixup and work-range arguments vary.

The original [Goose entry](../../goose/cuda/standalone-flash-mma.goose)
is identical in the two builds:

```goose
flash_causal_part(q, k, v, out, 0, cfg, scale, tile, group, 0, ((visible + 31) / 32) as i32,
    false, false, 0, 1, lane, warp, shared);
```

In the original PTX the entry emits `call.uni` to
`process_tile_apex_nc8_np1_g`. Inline only the 8-query body
and the call moves to `flash_causal_part_g`. Inline both shared
helpers and the kernel calls the now-specialized
`fg_flash_causal_mma_g`. The causal SASS prologue changes from
`IADD3 R1, R1, -0x40, RZ` to `IADD3 R1, R1, -0x38, RZ`.

| SM89 assembly | Default | Inline 8-query body | Inline body and wrapper |
|---|---:|---:|---:|
| Causal local stack, bytes | 64 | 72 | 56 |
| Stream-K local stack, bytes | 168 | 168 | 4032 |

**Compiler candidate.** Specialize the helper for the call
sites with proven constant shape and mode arguments. Keep a
shared entry for variable arguments when expanding it increases
local storage or code size. Validate the lowering with both
callers in the same device image. This experiment used Goose
`664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`,
Clang 18.1.8 and CUDA `ptxas` 12.6.85.

**Status and tests.** Global inlining has a measured resource
regression for Stream-K and remains disabled. Runtime speed for
the inline variants is unmeasured. The first inline variant
passes bitwise comparison for 4, 289 and 4,096 queries and
CUDA memcheck on the first two shapes. For a compiler candidate,
repeat these checks and the uniform/general Stream-K fixup
fixtures, then compare actual kernel time, stack and spills.
See [EXP-1609](CUDA-PARITY-EXP1476.md#flash-attention-device-lowering-and-l2-prefetch)
for the raw artifacts and the unrelated cache-hint screen.
