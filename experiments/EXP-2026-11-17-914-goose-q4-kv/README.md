# EXP-2026-11-17-914: persistent Q4 attention KV controlled by Goose

Status: ACCEPTED COMPONENT, NOT FULL-MODEL INFERENCE.
Base: df9b82f46c8b44d644b17f2f12e1cbd33db7c260.

Hypothesis fixed before changes: the accepted real-weight project/finish split
can surround persistent non-transposed Q4_0 K/V writes and views, with write-to-
read graph dependencies and session-prefix/checkpoint decisions owned by Goose.
Reuse SET_ROWS, view primitives and the native padding policy; add no C++ policy.

Scope: one stream, contiguous prefix, FA on, Q4_0 K/V. Cache storage outlives
per-call graphs. State records capacity, valid prefix, padding, pending count
and a generation ticket. Begin prepares one ticket for the whole model step;
every layer build shares it without advancing the prefix. Commit occurs only after
successful backend completion. Abort keeps the previous valid prefix and
invalidates its ticket. Host checkpoints copy only active-prefix bytes and
metadata; they require host byte slices and are not direct GPU-pointer readers.
No claim is made for multi-stream, SWA, arbitrary slot moves, RNN state or the
public llama state-file format. Existing native paths remain intact.

Acceptance fixed before testing: identical native arithmetic/chunking protocol
must yield bitwise-equal active KV bytes and attention outputs. Test prefill3,
decode1, graph destruction/rebuild, same-prefix replay, rewind, host save/restore,
invalid bounds/geometry and stale/duplicate commit. Errors must not report a
successful partial state. Independent native oracle follows actual cpy/get
layouts and explicitly expands writes before attention. Goose instead carries
write dependencies in the read-view sources. Do not weaken tolerances for a
different GEMM/GEMV chunking protocol; compare like with like.

KV equality alone is not generation correctness. Full-model/logit/task quality,
recurrent state, GPU and 32K/512 acceptance remain mandatory later. No throughput
gain is predicted by this component and the immutable Apex run set is unchanged.

Use cheap composition/typecheck before a small generated-C/ABI CPU gate. Reuse
the unchanged model SHA/stat receipt from EXP913; rehash only if it changes.
All build outputs stay in a new LlamaServer directory; no model server is started.

The initial six-step numerical gate passed. Boundary review identified that
read-view strides require compact cache storage, while shape checks alone do
not establish it. Before acceptance add explicit contiguous/single-stream
geometry checks and malformed-layout cases before any graph allocation. This
does not change the numerical or performance acceptance criteria. Preserve the
passing initial artifacts independently before editing their source.

## Results and integration

The final gate computes six exact real-weight cached-attention steps and
abandons one prepared graph. Every per-call graph is destroyed, while Q4 K/V
storage remains in a separate context. Active-prefix bytes, attention outputs,
restore/replay and retry agree bitwise with the independent native oracle.
Previous valid prefixes and bytes outside the write window remain unchanged.
The graph itself reaches both SET_ROWS writes through its read-view sources;
it does not depend on a host explicitly expanding those writes.

Additional gates: 49 planner checks, 42 invalid-layout calls, 50 host-state and
checkpoint checks. Stale/duplicate tickets, short buffers, forged metadata,
overflow, pending snapshots and invalid geometry are refused. Errors in the
tested checkpoint cases leave the destination and state unchanged. Six mutants
fail semantic assertions: missing write dependency, cursor, causal mask,
restored prefix, restored V bytes and missing layout guard.

Canonical `goose/model/qwen35-kv.goose` is byte-identical to this source and is
included in the default composition, with manifest/provenance/symbol entries.
The 125-module unit typechecks (4046 specializations). No full native build was
needed: the isolated generated-C link/ABI was tested directly. The previous
passing v2 candidate has an independently copied/hash-verified runtime-store
backup; v3 outputs are in a new LlamaServer directory.

Peak fixture RSS: 59380 KiB (57.99 MiB), not whole-model RAM. GPU/model-server
runs: zero; no speedup or prefill/decode claim. The immutable Apex executable
and all runtime libraries remain untouched. The eight actual weight payloads
and selected embedding rows use EXP913's unchanged model integrity receipt.

No new C/C++ runtime code, FFI adapter or permanent wrapper was added. Native
GGML allocation, tensor POD access, SET_ROWS/math and CPU execution remain
dependencies; this still needs C++ through the backend and is not the final
Goose-only runtime. C++ replaced-and-removed: 0 lines. Dead C++ deleted: 0
lines. Remaining native closure counts and the scope-specific retention audit
are recorded in evidence; they are not a proof of the minimum necessary C++.
The unchanged EXP906 closure inventory contains 248213 C++ source lines in 350
files; the minimum necessary subset is still unclassified, not zero.
The native cache cannot be removed: full server/generation and other supported
cache layouts have not switched to the Goose path.

Host save/restore requires disjoint borrowed byte buffers, a quiescent backend
and CPU-addressable cache bytes (or a completed explicit device-to-host stage).
Abort invalidates metadata/tickets; it does not undo arbitrary writes from an
already executing graph. The caller must stop/await that work before retry.
`q35_kv_init` is for a fresh session whose old graphs/tickets are no longer live.
The model-step owner calls begin once, builds all layers with that ticket,
then commits once after successful backend completion. A layer does not commit
independently. The current numerical gate covers one real layer, not all layers.

Reproduce selectively with `tests/build.py --out <new-LlamaServer-directory>
--support <support-directory> --model-proof <EXP913-model-proof.json> --step
<check|build|fixture|state|gate|mutations|main-check>`. Reuse successful stages
while their inputs remain unchanged. Preserve the initial logs and warnings;
do not rebuild for a report.
