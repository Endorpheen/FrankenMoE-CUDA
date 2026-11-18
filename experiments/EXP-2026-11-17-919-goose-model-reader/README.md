# EXP-2026-11-17-919: Goose GGUF reader and model planning/ownership

Status: ACCEPTED READER/PLANNER/CONTEXT LIFETIME; MAPPING BORROWED, NO CLI/SERVER.
Base: EXP918, 2b4bb5fe3223ec736d35d069e9e82f5394081bbb.

## Hypothesis fixed before changes

The real mathematical trunk can consume GGUF metadata, tensor lookup and model
configuration produced by Goose instead of the temporary native GGUF reader.
Use caller-owned bounded records and immutable byte spans, reuse accepted GGUF
header/type tables, then bind the same real weights and retain exact logits.
Model mapping/context lifetime must be explicit; probe native POD/OS ABI before
adding any adapter. No new permanent C++ loader or containers are acceptable.

The old structural parser hashes names/skips values and has raw-address access
primitives; archived loaders retain native string/vector stores. They are
references, not an already independent reader. Preserve their original sources
and the immutable accepted EXP918 executable/library set.

## Fixed reader contract and gate

Little-endian GGUF v2/v3, typed scalar/string/array metadata (including string
arrays), complete names/offsets/shapes/types/byte spans and lookup. Parse only a
bounded header window, not weights. Caller supplies the real file length and
disjoint output/hash tables. Validate every length before access; check signed
limits, dimensions, products, quant rows, alignment and cumulative tensor
offsets. Reject duplicate keys/names using hashes plus byte equality, never a
hash alone. Reuse the canonical Goose block/type and public GGUF header tables.

Published state magic is cleared before parsing and set only after complete
success. Scratch tables may be dirty on error and must not be consumed. Input
and records remain borrowed/immutable after success; ownership and lifetime are
the model's responsibility. Engine loading requires complete padded payload
ranges within the supplied file size, even when a native metadata-only reader
would accept a header without payload. Embedded NUL in keys/tensor names is
rejected as ambiguous for native name APIs; metadata value bytes are preserved.

Correctness: independent Python wire-format parser and native GGUF APIs/metadata
serialization, all actual metadata/tensor records on Apex, with Edge0 skeleton
and PLE as shared-format compatibility inputs. Include truncation, malformed
counts/types/strings/arrays, duplicate/bucket-collision names, shape/offset/
padding overflow, short output buffers, forged lookups and semantic mutants.
No tolerance change. A pure reader build must not require C++ or libstdc++.

After reader acceptance, replace the candidate name/config/tensor planning path
and rerun only its dependent full-trunk gate with unchanged arithmetic. Do not
claim model ownership, CLI/session independence or no-C++ runtime on the basis
of a parser test. The final objective still includes generation/task correctness,
backend/CUDA migration and protected-profile speed acceptance.

## M0 and limits

Header input window <=64MiB. Records reference byte spans without copying the
vocabulary or expert weights. Initial test output capacity <=4096 tensors and
512 metadata entries, power-of-two hash tables at load <=0.5. Actual mapping
may cover a large file virtually; only header pages are touched by this gate.
Reuse existing full-model hash/stat receipts. No server/GPU/model inference for
reader/ABI gates; sequential CPU checks, each <=180s, generated C/ELF/logs only
inside a new LlamaServer directory. No accepted binary/source overwrite.

## Results: reader and model path

The final pure-reader library builds with gcc/C only and links **libc only**.
It imports no GGML/GGUF/C++ symbol. Independent wire-format records, native
GGUF descriptors and native serialized metadata bytes agree on:

| file | metadata | tensors | data offset | checks |
| --- | ---: | ---: | ---: | ---: |
| actual Apex |52|753|10990944|745935|
| Edge0 skeleton |57|1079|11012448|746699|
| Edge0 FP8 PLE |5|1|352|23|

The format gate treats PLE I8 as storage only, not decoded integer embeddings.
No Edge0 inference is implied. A334-check synthetic corpus covers v2/v3, every
value type, embedded-NUL value bytes, string-array indexes, duplicates, actual
bucket collisions/forged occupied slots, missing payload, scalar/array/name
bounds, dimensions/byte/padding/cumulative overflow and publication/guard rules.
Canonical bool values are0/1; ambiguous NUL keys/tensor names are refused.
Header-only borrowed spans are tested with the separate actual full-file length.

Goose model planning now performs architecture/metadata lookup, names for all
layers, configuration arithmetic, complete tensor inventory, shape/type checks,
zero-copy binding, state/context creation, RoPE parameter packing and model/
frame lifetimes. The current arithmetic profile qualifies F32/F16/BF16/Q4_K/
Q6_K weights, ungrouped Qwen3.5 MoE without active clamps/rope scaling; other
formats/families keep their native paths and are not silently redirected.
Tensors must belong to the trunk or the explicitly skipped MTP layer. Renaming
an existing output cannot silently select a tied head and ignore that tensor.

Model preparation binds733 actual weights with **candidate native GGUF/name
queries0**. All configurations/descriptors match the native reference. The
dependent40-layer gate retains2502720 exact frame values/1241600 logits and
all recurrent/KV bytes through prefill3, decode1 and checkpoint/replay. The
candidate creates/closes its own frame contexts in Goose, refuses model close
while a frame is live and rejects double closes. RoPE's four I32 words live in
model-owned storage; the caller no longer packs a temporary native array.

The native24-byte init-params ABI was checked directly on Linux x86_64
little-endian, including both CPU-data and metadata-only allocation policies.
Model lifecycle/failure gates pass79 checks: short buffers, inconsistent/nonfinite
metadata, bad shapes, early budget failure, missing last weight, unknown output,
partial-load cleanup, reload generation, stale frames and live-frame leases.
Owner access is serialized by the caller; concurrent inference/error-atomic
session rollback is not established by this lifecycle gate.

Seven semantic mutants fail assertions: premature parse publication, omitted
payload bound, wrong string-array span, hash-only equality, ignored inventory,
premature model close and corrupted owned RoPE bytes. A separate **positive**
constant-hash diagnostic verifies that distinct names remain distinguishable
even when every full hash is identical. The hash-only mutant fails that same
case; crashes/compiler failures are not counted as semantic rejection.

The accepted full CPU loader fixture peaks at3810980KiB; observed process-tree
swap0, limit4GiB/180s. This is a sparse short-context comparison including the
reference, not production RAM or throughput. No server/GPU or speed comparison
was run. The immutable EXP906 Apex profile/binaries remain unchanged.

## Ownership boundary and compiler evidence

No C/C++ runtime adapter/body was added. Direct existing GGML context/clear APIs
are used, and the shared `qz_st_u8` byte-store primitive is reused. The isolated
component gate extracts its exact existing C bodies with provenance; the main
composition already links their single shared host definition. Do not add a
second production definition. Native GGML allocation/POD/CPU backend and
libstdc++ remain necessary for the combined model, although the pure parser
does not need them.

Mapping/input/table storage remains borrowed from the caller until model close.
The pinned compiler's minimal foreign-slice probe rejects `extern fn ... ->
const u8[:]` with `nested references and slices do not cross`. This establishes
that specific interface limit, not impossibility of every OS/mapping design.
No compiler source was modified and no fake permanent C++ mmap wrapper added.
Standalone OS/mapping/CLI/session ownership remains an explicit next task.

An integration typecheck initially found ambiguous public exports specialized
by internal literal/view callers. Private implementations plus stable public
slice wrappers preserve the C ABI and allow model consumers to specialize
internally. Prior pure/model sets were independently byte-copied and SHA-checked
before source changes. The final complete-inventory guard was validated in a
new model-v4 directory; successful unaffected parser gates were reused.

C++ replaced-and-removed0 lines; dead C++ deleted0 lines. The historical
protected closure remains248213 C++ lines/350 files; the minimum necessary
subset is unclassified. Native model loading for other architectures and the
working server still use the old bodies, so no deletion is justified here.
Final independent build, tokenizer/sampling/real tasks, HTTP/session, no-C++
backend/CUDA and protected32K/512 speed acceptance remain open.

The three canonical modules are byte-identical and enabled in the default
composition. Its130-module unit typechecks4125 specializations, emits C and
compiles as one C object:1110 global symbols, no duplicates. This full object
gate was justified by integration of new native POD/FFI/public exports; no
native llama/CUDA rebuild, link/server acceptance or performance comparison is
implied. Every command/output/exit is retained; the long C compile was tracked
by its actual process/child PIDs and was not restarted.

Selective reproduction: `tests/build.py --out <new-LlamaServer-directory>
--support <support> --step <check|build|synthetic|synthetic-extra|apex|
edge-skeleton|edge-ple|model-check|model-build|model-fixture|model-gate>`.
Keep reader and model outputs separate; reuse passing stages while inputs
remain unchanged. `tests/model_contracts.py <model.so>` runs79 lifetime/failure
checks without inference. `tests/mutations.py --out <reader-or-model-output>
--support <support> --name <named-probe>` runs one independent negative gate
or the positive constant-hash diagnostic. The foreign-slice source is an
intentional compiler-refusal probe, not a canonical runtime module.
