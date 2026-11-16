# EXP-2026-11-17-912: typed Goose graph-construction entry

Status: ACCEPTED for typed graph composition; canonical source promoted.
Base: 82381b54 (accepted runtime accounting checkpoint).

## Mechanism and scope fixed before changes

The standalone graph builder owns operation validation, result shape and edges,
but exposes `gb_common(pointer_to_C_packet)`. Existing layer harnesses construct
each packet and choose each subsequent operation in C. That cannot compose a
Goose-owned layer/pass without an additional pointer/packet adapter.

Hypothesis: a bounded `gb_common_args(i64[:])` entry can share the exact existing
constructor body while taking caller-owned Goose values directly. A Goose layer
can then prepare and reuse its own argument workspace, call constructors and
carry tensor handles without per-node C orchestration or a new native binding.
No inference-speed gain is predicted; this is an execution-composition dependency.

Control source SHA256:
`779c55ef041e6b9ff5f285324de493c8c7ab7498e5aaf24e567b5d58c98f9562`.
The control is the current committed module, not the stale manifest's older
hash. Preserve it and all binary outputs. Apex EXP906 stays immutable.

The old packet entry must remain lazy: eagerly copying sixteen words would
read beyond a valid shorter legacy packet. A private accessor chooses the old
packet read or the new bounded values slice; both entries share one operation
body. The new public entry rejects slices shorter than sixteen before mutation.
No operation algorithm, arithmetic kernel or public GGML ABI changes.

## Acceptance plan

1. Compose/check before any native build. Retain source/compiler hashes.
2. Build only the small Goose modules and the single redirected GGML C unit
   needed by the existing independent constructor corpus. Reuse the remaining
   immutable source-built objects in a new LlamaServer directory. This ABI/link
   risk cannot be checked by Goose typechecking alone; no full runtime rebuild.
3. Compare control Goose, candidate legacy entry and candidate typed entry with
   native GGML constructors, across all 36 modes, allocating/no-allocation
   contexts, types/shapes/strides/parameter/name/source/view bytes and malformed
   transactions. Do not accept the candidate merely because two wrappers share
   the same implementation.
4. Check a legacy short packet against a protected page and new short-slice
   rejection without arena mutation. Demonstrate a numeric chain whose node
   orchestration happens inside Goose, with a direct GGML numerical oracle.
5. Keep every failed attempt and selectively repeat affected gates only.
   No C++ removal, model launch or performance claim belongs to this API gate.

If accepted, expose the API by default in the canonical module and update its
specific source/manifest provenance. Other existing manifest failures must stay
visible. Do not claim a full standalone pass or 100% migration from this change.

## Results

The control and candidate use the exact same pinned Goose compiler and unchanged
GGML support objects. One test-only redirected GGML C translation unit was built
with the recorded native flags, and the small Goose/fixture units at -O1. The
existing independent EXP158 corpus was adapted only for the current native
context implementation: it checks fixture RAII balance, not an invented native
heap-leak counter. Native constructors are loaded in separate loader namespaces.
The C redirection is test transport, not production adapter debt or runtime code.

| Gate | Result |
|---|---|
| Frozen control vs native | 36 modes, 16 groups, 558 tensors, 10408 checks |
| Candidate legacy vs native | same, all match |
| Candidate typed vs native | same, all match |
| Candidate native fallback | same checks, zero Goose mode bits |
| Invalid contracts | eight native/candidate pairs per corpus reject before arena mutation |
| Legacy packet boundary | seven words immediately before PROT_NONE, both control and candidate pass |
| Typed short input | lengths -1/0/7/15 return zero with unchanged arena; valid length16 succeeds |
| Goose-owned numeric chain | eight cases, 18432 computed FP32 values exactly match native |
| Packet-read witness | positive legacy control; zero packet reads during Goose orchestration |
| Mutations | 2/2 semantic failures: too-short slice accepted; wrong argument-seven field |
| Full current main composition | 123 modules, 4019 specializations typecheck; no full C/runtime rebuild |

The numerical witness builds RMS norm, real-valued norm scaling, two matmuls and
a residual addition inside Goose using one caller-owned argument workspace.
It tests one/three tokens with F32 weights and mixed Q4_K/Q6_K weights. Tensor
values are deterministic fixtures, not real-model layer weights or logits.
All five requested intermediates match; the ordinary fused graph's computed
intermediates and final result match too. No host code chooses the five node
operations between calls. The packet-read positive control proves the zero
counter on the typed path is a working witness, not an unexercised observer.

## Retained failed numerical attempt

The first numeric test incorrectly inspected the RMS intermediate without
requesting it as an output. The native CPU backend fuses RMS_NORM+MUL and
intentionally does not materialize RMS alone (`ggml-cpu.c` and `ops.cpp`). The
failure was in a test assumption, not a reason to change the runtime or accept a
numeric tolerance. Diagnostics identified node0, and the corrected gate both
requests every intermediate as an output and separately checks the default
fused graph. FP32 bitwise equality was retained. Only the numerical fixture was
rebuilt; the passed constructor/boundary gates and module libraries were reused.
Both failed numeric logs remain evidence, not passing results.

## Source integration and memory scope

Canonical source and frozen accepted candidate have SHA256
`306c2514309017b7b6603ac464c1f5071086d3209f93e0233f770800275473d8`.
`tools/sync-goose-tree.py` now names this frozen experiment as canonical origin;
only this MANIFEST row was refreshed and the new export was added to the symbol
guard. The global tree check still fails on unrelated source drift/unmanifested
files (15 issues in the current working tree); those were not hidden or rehashed.
The graph-builder entry itself passes canonical source/hash checks.

The API borrows at least 16 int64 values (128 bytes) for the duration of a call;
it does not retain the workspace or allocate an extra heap owner. GGML continues
to own its tensor/context storage. Local constructor-test DSO sizes are 1095496 B
control and 1187704 B candidate (+92208 B, includes code/debug information).
That is not an RSS or inference-memory delta. The standalone chain DSO is
285152 B. Hashes, reused object provenance and raw log hashes are in evidence.json.

The immutable EXP906 nine-file Apex set remains unchanged. It does not consume
this generic canonical module; model runs: zero. Prefill, single-response decode,
RAM and VRAM were not remeasured. No speed gain, model regression result, full
standalone pass or removed C++ body is claimed. This enables the next real layer
orchestration step and is available by default in future canonical composition.

## Selective reproduction

`tests/build.py --out <new-LlamaServer-directory> --support <source-built-support>
--step prepare|build|corpus|boundary|numeric-materialized|mutations|main-check`.
Choose the affected step, not the whole sequence, when fixing a failure. Each
step records its PID, exact commands/output/return codes and keeps successful
artifacts immutable. The main-check step composes/typechecks only; it never
invokes a full runtime native build. Native helper/constructor tests are scoped
to this immutable support/header set and the existing 36-mode protocol.

Next: use this API for a real-weight Apex attention/GDN layer and carry its
residual/state in Goose, then compose the forty trunk layers. Synthetic chains
must not substitute for matching real model logits, full prefill/decode, server,
memory and restart acceptance.
