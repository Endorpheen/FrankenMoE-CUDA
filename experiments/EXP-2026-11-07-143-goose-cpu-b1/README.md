# EXP-2026-11-07-143: common CPU B1 migration (in progress)

Status: **IN_PROGRESS / NOT B1 ACCEPTANCE**. Production and compiler source are unchanged.
This snapshot preserves a growing shared CPU implementation, not a complete CPU backend.

The pinned completed self-hosted EXP300 Stage3 producer is recorded in
`source/producer.json` (SHA256 454151b3449b208b62335d942dc2226eb19fdd3bfccec3c0a42ecdc13ef71e61).
Goose owns tensor arithmetic, selected quantized decoding, per-worker graph policy,
and recurrent GDN/KDA, SSM scan, RWKV6/7 and gated linear attention computation.
Original operator dispatch, remaining arithmetic, native thread/OS/backend services
and other residual implementations remain explicitly incomplete.

## Latest control and numerical-contract checkpoint

The same common candidate now executes operation selection,task-count policy,
scratch arithmetic and the active OpenMP graph-job lifecycle in Goose. There
are96operation leaves; this is not96migrated arithmetic kernels. Native backend
extra/tiled scratch providers,unfinished compute,physical pool initialization,
OS affinity and non-OpenMP support remain explicit residuals.

Current evidence supersedes earlier bounded counts below:326/326 differential
graph/status cases pass (including reuse,cancellation and recovery on1/2/8/16
workers);960/960planning cases match the preserved pre-control B1 and rollback;
66/66wide Apex operator contracts pass. All three suites pass scoped
ASan/UBSan/leak checks. Instrumented compute/contract output matches release
bytes; instrumented planning matches the preserved reference bytes.

F32 SWIGLU now reproduces the original AVX2 FMA/exponent polynomial for complete
8lane chunks with scalar tails,implemented in Goose. All9wide SWIGLU cases
match native bytes. GLU clamp/OAI comparison-selection preserves the source
std::min/std::clamp signed-zero contract; no speed optimization was performed.
The prior zero-sign-only cross-optimization mismatch is retained in local
diagnostic records and described in compiler feedback; it was not a sanitizer
memory finding. General NaN payload/ISA and full model equivalence remain open.

The earlier model timings below belong to the frozen254case diagnostic binary,
not this newer control/numerical candidate. No repeated model request was run.
Disable the whole new control/job policy with`GGML_GOOSE_CPU_CONTROL=0`.

## Functional evidence

- 278 actual GGML differential cases pass at 1/3 workers: 234 byte-exact;
  44 within the predeclared F32 abs2e-5+rel2e-5 tolerance. NaN payload equality
  remains open. This is selected coverage, not all operators/types.
- Scoped ASan/UBSan/leak checks pass all 278 cases with release-identical output.
  New common/executor/stateful units, CPU core, metadata bridge and fixture are
  instrumented; preserved native/earlier objects and other modules are not.
- A separate functional investigation passes 66 cases at 1/8/16 workers:
  GET_ROWS on 147 real Apex embedding rows is byte-exact; wide GLU layouts stay
  within the same tolerance. It has no timing measurements and no full-model run.
- Actual GGML traces witness all five new recurrent operation families. No new
  stateful Goose model execution was witnessed in the GPU-placement diagnostic.

## One owner-requested Apex diagnostic

The single saved-request 32768/128 run of the earlier 254-case candidate completed.
Original C++ / B1: prefill 2867.846 / 2620.936 tok/s; decode 27.978 / 26.738 tok/s;
RSS 21311.262 / 21313.555 MiB; VRAM 3348 / 3348 MiB. The C++ control is historical,
with the same profile/request, not a fresh quiet pair. No performance acceptance.

**Full response equivalence is not confirmed:** text/IDs/top5 differ, first token
divergence at index 5, and earlier token distributions already differ. Cause is
unestablished. Passing isolated fixtures does not close this model correctness gate.
Goose GET_ROWS, GLU and graph-executor process counters are positive (including
normal startup warmup). No repeated inference, microbenchmark or speed tuning.

Raw diagnostic records and the cumulative comparison are in
`../../docs/goose-migration/apex-results/` and `../../GOOSE_MIGRATION_PLAN.md`.
The pre-extension diagnostic source/binary remain frozen locally under
`diagnostics/b1-254-before-recurrent-extension/` in the live experiment tree.
No model weights, compiler binaries or runtime binaries are included here.

## Reproduction and rollback

`source/reproduce/` and `build-receipts/` preserve the live-tree generation,
integration and functional commands. They require the recorded pinned producer,
GGML reference headers/objects and earlier archived Goose modules; this snapshot
is not a standalone compiler or model distribution. The functional embedding
fixture reconstructs a local-only input from the installed Apex GGUF.

Disable the new CPU module with `GGML_GOOSE_CPU_BACKEND=0` and new graph loop with
`GGML_GOOSE_CPU_EXECUTOR=0`; frozen original and EXP141 bundles remain external
controls. Full candidate source/link closure and actual-model equivalence are open.
