# EXP-2026-11-17-913: Goose-owned real-weight Qwen3.5 attention branch

Status: ACCEPTED as a real-weight attention component; persistent KV and full
model acceptance remain open. Base: c56830257235c2bb8da88a278632f90ccbff1432.

## Hypothesis and immutable contract

The typed graph API accepted in EXP912 permits the attention branch's operation
order, shapes, IMRoPE parameters, GQA permutations, gate and residual to move
into Goose. The host should supply only metadata, real weight/tensor handles,
borrowed workspaces and the backend compute call. It must not choose the branch's
individual nodes. No performance gain is predicted or claimed by this gate.

Authoritative behavior: vendor/flint/src/models/qwen35moe.cpp and
runtime/reconstructed/src/llama-graph.cpp at the base checkpoint. The old
apex_layer harness is not a real-weight oracle: it creates deterministic F32
weights and uses NORMAL RoPE even though the working architecture selects
IMROPE. Do not port those test assumptions into the new runtime.

Scope: real blk.3 attention and norm weights from the unchanged Apex Q4_K_M
GGUF; real embedding-row inputs; weighted pre-norm, joint QG and separate K/V,
Q/K weighted RMS norms, IMRoPE40 with metadata sections and text positions
[p,p,p,0], GQA permute(0,2,1,3), causal mask, F32-to-F16 K/V conversion on the
cache-free flash path, F32 attention accumulation, sigmoid gate, output matmul,
residual and weighted post-norm. Keep projection and finish APIs separate so
Goose KV updates/cache views can be inserted between them in the next step.
This initial numerical gate does not prove persistent KV/state or model logits.

Reuse the accepted QG splitter and graph constructors. Expose the already
accepted EXP156 POD tensor projections/name formatter as a small shared C ABI
binding; classify it as temporary adapter debt, not migrated behavior. All
shape, reshape/permutation and branch decisions belong in Goose. Add no C++
runtime policy and remove no fallback or architecture support.

## Acceptance fixed before execution

- Cheap composition/typecheck first, then only the local generated-C/ABI gate.
- Independent native GGML reference follows the actual architecture, not the
  candidate module or old harness. Same quantized bytes and arithmetic protocol.
- Exact metadata/edges/parameters for all observed stages and bitwise FP32
  equality for explicitly materialized stages/final values. Also compare final
  outputs with ordinary CPU fusion; do not read an elided intermediate buffer.
- One/three-token cases and zero/nonzero positions; mask causality exercised.
  Mutation of norm scaling, QG layout, IMRoPE type/parameters or sigmoid must
  fail. Freeze criteria; do not relax tolerances after observing a result.
- Read only required GGUF tensor ranges and a few embedding rows, record their
  hashes. Do not duplicate/mmap/load all weights or overwrite the Apex run set.
- Record all native binding debt, lifetimes and unsupported modes explicitly.
  A component gate is not a full model/performance acceptance.

All generated files and binaries remain under a new LlamaServer directory.
Model/server launches: zero planned; this is a small CPU component gate and
uses no GPU while the desktop has competing GPU activity.

## Results

The full model SHA256 was reverified once with an 8 MiB streaming buffer:
`c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355`,
21713463136 bytes, unchanged inode/mtime during the read, 11.78 s. The numerical
gate then read only **15624192 bytes** for eight real weights plus **3456 bytes**
for three real embedding rows. Per-range offsets/types/shapes/SHA256 are in the
gate log; weights remain Q4_K/Q6_K/F32, not synthetic F32 replacements.

All eight cases passed: 1/3 tokens, position base0/17, explicitly requested
intermediates and ordinary CPU fusion. Each observes 29 stages. **1069056
F32/F16 elements** match the independent native GGML reference bit-for-bit.
Type/op/flags, shape/strides, parameters, source links and view owner/offset
match; the final consolidated gate also compares constructor debug names.
Only addressed view elements are compared, not holes between strided rows.
The non-materialized RMS buffers under ordinary fusion are not read; requested
output cases compare those stages explicitly. No tolerance was relaxed.

The native oracle follows the plain single-stream path extracted from the
actual source, with declared metadata-derived parameters. It is not the old
harness or a call back into the candidate. Real embedding rows are used as
nontrivial inputs to blk.3; they are **not** hidden states produced by executing
the preceding three layers. Therefore no model-logit/generation equivalence is
claimed. The test observes a positive legacy packet-read control and **zero
packet reads during the Goose branch**, proving node orchestration has moved.
Four native CPU compute threads run the graph; construction is serialized in
the caller-owned context, not claimed thread-safe for a shared context.

Six -O0 mutations were killed by explicit semantic assertions before executing
their invalid graph: missing pre-norm scale, wrong QG gate offset, absent RoPE
sections, SiLU replacing sigmoid, identity replacing the GQA permutation and a
missing causal mask. LD_PRELOAD is restricted to each test child; neither the
candidate library nor any user/server process is modified. A compiler failure
or crash never counts as a killed mutation.

## Canonical integration and ABI debt

`goose/model/qwen35-attention.goose` is byte-identical to the accepted module
and joins the default main module list. The whole **124-module composition**
passes typecheck (4032 specializations); this is not a module-count coverage
metric and no full native runtime rebuild was performed.

The already accepted EXP156 POD `fg156_tget`, `fg156_tset`, `fg156_name`
contracts are exposed once in the existing
`integration/goose-standalone/graph-bindings.c`. Each has exactly one defined
symbol in the consolidated library. `source/projections.c` is the frozen proof
snapshot; fresh builds do not link a second copy. No C++ runtime file or native
reshape/permutation algorithm was added. The three ABI bindings are recorded
as **temporary debt** in adapter-debt.json, not completed migration.

After consolidation only the affected binding/library/fixture was rebuilt;
the emitted Goose C and prior evidence were reused. The consolidated library
passes the same eight cases, including names, with **59068 KiB peak RSS** for
the test process (about 57.7 MiB). This includes the fixture/backend/compiler
runtime and is not a whole-model memory measurement or an A/B memory gain.

Artifacts: `/home/end0/AI/LlamaServer/exp913-goose-real-attention-20261117`;
the consolidated library lives in `integrated/attention.so`, SHA256
`e2a4054f2592282f080eb4f16848d6acc92ae213224d235d256c06984e955395`.
The module SHA256 is
`28f2cfcb56aecae4e31d6b3c030d11eeeaa20d7c25add0febb7645821bff533d`.
Commands, outputs, return codes, model/range proofs and all raw hashes are
retained in evidence.json and the named logs.

## Ownership and unsupported scope

The caller owns the GGML context and valid tensor handles. Weights, input,
mask and positions must remain alive through compute; argument/config/section
and node workspaces must honor Goose borrow rules and be disjoint from mutated
tensor metadata. The constructor copies parameters and does not retain those
workspace addresses. Views inherit their real source owners. The underlying
native tensor/view allocator and numerical backend remain explicit dependencies.

The project/finish split is ready for a future KV update, but this acceptance
tests **fresh F16-converted K/V**, not persistent Q4_0 KV, restore/rewind,
multi-stream sessions, LoRA, bias/sinks/optional rotations, recurrent layers,
MoE, logits, CUDA execution or HTTP serving. Other existing native architectures
and fallbacks were not deleted or narrowed. Full-model speed/peak RAM/VRAM/swap
were not remeasured. All nine immutable fast-Apex ELF files still match their
manifest; the protected EXP906 profile is unchanged.

## Reproduction and next step

`tests/build.py --out <new-LlamaServer-directory> --support <source-built-support>
--model <pinned-Apex-GGUF> --step model-hash|check|build|fixture|gate|mutations`.
Run only the affected step after a failure. The retained `integrate` step uses
a nested output to preserve the initial proof artifacts and reuses emitted C;
`main-check` composes/typechecks only. No model/server is started by these steps.

Next: insert persistent Q4 K/V writes/views between project and finish, with
explicit lifetimes, dependencies and checkpoint/restore gates. Then close the
real-weight GDN and MoE paths and compose the forty trunk layers, embedding
lookup and LM head. This component does not complete the thread's full-runtime
or performance objective.
