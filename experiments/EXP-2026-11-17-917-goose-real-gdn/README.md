# EXP-2026-11-17-917: real-weight recurrent GDN layer controlled by Goose

Status: ACCEPTED COMPONENT, NOT FULL-MODEL INFERENCE.
Base: EXP914, e1a4c648eb1dfb7ce9cc32b3494361991c91b102.

## Hypothesis and fixed acceptance

The accepted typed graph builder, Qwen attention graph helpers and GDN view/
conv primitives can orchestrate a complete real-weight recurrent layer with
persistent convolution and matrix state, not merely its shape decisions.
The arithmetic protocol follows the native qwen35moe and delta-net-base fused
path (one sequence, K=1): real projections, sigmoid beta, softplus alpha,
pre-scaled negative A, convolution/SiLU, RMS-based L2 normalization, fused GDN,
gated output normalization, output projection, residual/post norm.

The old gdn_l2norm export calls native L2_NORM. Current models.h instead uses
`scale(rms_norm(x, eps/head_dim), 1/sqrt(head_dim))`. Preserve the actual graph
and floating protocol, not a mathematically similar substitute. Fused GDN
handles unequal key/value head counts itself; do not duplicate the fallback's
head repetition. Its kernel already applies the query scale exactly once.

Acceptance is bitwise equivalence to independent native GGML constructors with
the same fused arithmetic and chunking. Use real layer-0 weights and selected
embedding rows as fixed inputs, not a claim of preceding model hidden states.
Check intermediate metadata/edges and materialized values, persistent conv and
matrix state after prefill3/decode1, destroyed/rebuilt call graphs, snapshot/
restore/replay and abort-before-execution. Check invalid geometry before writes
and relevant semantic mutations. Do not weaken tolerances after a failure.

The candidate returns the output and explicit conv/state copy roots; the generic
executor must expand these roots and await successful backend completion. Caller
owns borrowed handles, buffers and state lifetime. A state copy is not a whole-
model transaction; model-step commit/rollback integration remains required.

One sequential four-thread CPU component gate, target under three minutes,
bounded real tensor reads and a per-call arena at most 256 MiB. No model server,
GPU, whole-file mmap, changes to the protected Apex bundle or C++ deletion.
Reuse the unchanged EXP913 full model hash/stat receipt. Keep every build/test
log with command/output/exit; preserve old artifacts and rerun only changed
dependencies. New runtime C/C++ or adapter code must be explicit debt.

Next acceptance beyond this component: routed/shared MoE and full trunk/logits,
then generation/session correctness and CUDA/performance. Component equality
does not establish a Goose-only engine, model quality or acceleration.

## Observed mechanism and results

Real geometry: hidden2048, head128, key-heads16, value-heads32, channels8192,
kernel4. Eleven actual GGUF tensors total 23421696 B. QKV is Q6_K, Z/beta/alpha/O
are Q4_K, scalar/norm/conv weights F32. `ssm_a` is already negative transformed
A (range -72.3307953 to -0.0186040681); no second exponentiation is performed.
Persistent conv storage is 98304 B; recurrent matrix storage is 2097152 B.
Both belong to a separate owner context, not a per-call graph. Weights/input
copies and CPU intermediates are bounded to the component arena, not the model.

Eight computed steps and two unexecuted abandoned graphs passed. Observed
intermediates and ordinary-fusion final outputs/states compare 7239360 F32
values bitwise; inputs are real selected embedding rows. Snapshot after
prefill3, decode1, restore and replay decode1 match exactly. Core outputs are
nonzero, all checked values finite, weights immutable. The typed path makes
zero packet reads after a positive legacy witness. 210 invalid construction
calls are refused before allocation or workspace writes. 115 F32-config/host
checkpoint checks cover bounds, guards, forged metadata and transactional
short-buffer failures. Host snapshots use borrowed disjoint CPU buffers and a
quiescent backend; device staging and full-model transactions remain outside.

Five mutants fail semantic assertions: beta activation, A-gate operator, conv
tail offset, config scale and restored matrix bytes. No crash or compilation
failure is counted as a killed mutation. Peak accepted-fixture RSS is83964KiB
(82.00MiB), not whole-model memory. GPU/server runs zero. No speed claim.

## Failures retained, not hidden

1. A test named `inspect.py` shadowed Python's standard module during argparse
   startup. It is now `inspect_gguf.py`; the successful metadata receipt remains
   valid and was not regenerated.
2. The first real graph exposed the old typed builder's padded-1D restriction
   on all norm operations. Native RMS and its CPU kernel permit independent
   outer strides. Only the RMS arm now uses row-contiguity. Non-RMS guards are
   unchanged. This is a correctness fix, not a numerical tolerance change.
3. A fixture compared addresses of two independently allocated inline transpose
   views. It now checks exact op/type/shape/stride/params, view ownership/offset
   and dependencies. A diagnostic run identified stage14/src1/op39 first.
4. A mutation anchor initially matched attention as well as GDN; it was scoped
   to the following GDN reshape. A mutant directory named `gate` collided with
   an existing fixture; its name is now `gate-operator`. The completed beta
   check was reused; only remaining mutants were run.

Initial artifacts and logs are retained, and before editing their shared source
independent hash-verified copies were made under runtime-store/sets/
exp917-before-strided-rms-fix. The unchanged successful library was reused after
fixture-only changes. Existing EXP912 support objects and corpus executable were
reused for constructor regression: legacy/typed and native rollback each pass
36 modes, 558 paired tensors and 10408 checks with eight native-error pairs.

No C/C++ runtime bodies or native adapters were added. `sqrtf` is the existing
system math ABI; positive-normal F32 parameter encoding is owned by Goose,
checked against native F32 rounding. Native allocation/POD/math/backend and
C++ dependencies still remain. C++ replaced-and-removed0 lines; dead C++
deleted0 lines; historical baseline closure248213 C++ lines/350 files, minimum
necessary subset still unclassified. Do not delete native GDN/cache bodies
before complete runtime and architecture acceptance.

The canonical GDN module is included by default and byte-identical to this
source. The corrected graph builder has its own immutable source snapshot here;
EXP912's frozen control/candidate snapshots are preserved. The 126-module main
composition passes typecheck (4055 specializations), and no destructive tree
sync or full native build was run. Symbol/provenance entries are updated.

Reproduce with `tests/build.py --out <new-LlamaServer-directory> --support
<support-directory> --model-proof <unchanged-EXP913-proof> --step <stage>`.
Stages are check/build/fixture/gate/contracts/regression/mutations/main-check.
The accepted fixture name is `gate-contracts`. A failed fixture-only check uses
a new `--fixture-name`; a single remaining mutation uses `--mutation <name>`.
Existing binary/log paths are not overwritten. The metadata helper is optional
when its unchanged prior receipt is available. Reuse passing checks while their
inputs remain unchanged; the constructor regression intentionally reuses the
EXP912 corpus/dispatch/support objects after validating their provenance.
The frozen graph-builder snapshot retains the canonical file's existing empty
EOF line to preserve exact bytes; the sole `diff --check` EOF notice is recorded
as intentional. Other staged files pass the whitespace check.
