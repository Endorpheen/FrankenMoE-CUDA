# Accepted Apex runtime: source, ownership and ABI map

Checkpoint: `3989367d4da7607d17e1e656eb332bd9aec46c84` (EXP906). This map
supersedes historical coverage claims for this specific binary set, not their
recorded experiment results. It is a link/source audit, not a new model run.

## Immutable control

Run set: `/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run`.
Executable SHA256: `3e4437d9f3e906278944a765bd0ab0e1cc0a96b20c441fe3ea37aa5cc0934237`.
Core SHA256: `4eba824255f06ab1fb8e5b6a9e98ae7bd0706773abc8bf86547d86b60cde98ac`.
CUDA SHA256: `17e79510f37817400c1b268e401d16eb2f392d1ef7f3da979d5ccf975cb32240`.
The other component hashes are in `runtime-map-20261117.json`.

EXP906 is still the current measured baseline: median single-response decode
55.5176 tok/s, prefill 2625.2066 tok/s, 32768 input / 512 output tokens, context
262144, expert cache 4992 MiB, t8/tb16, q4_0 KV, Apex Q4_K_M. All twelve
responses matched; process swap and additional swap-out were zero. Candidate
RSS median was 21350.90 MiB; total-device GPU snapshots had median 11468 MiB
(includes desktop/services). See EXP906 for ranges and measurement limitations.
No speed, memory or correctness improvement is claimed by this audit.

## Actual linked source closure

The nine selected ELF artifacts match their recorded link outputs byte for
byte. There are **602 unique object inputs**, compiled from **350 .cpp,
145 .cu and 107 .c files**. These are translation units, not functional
requirements or a migration percentage. Generated C and bridge C are included
in the last count; templates/headers add code beyond these file counts.

| Component | Object inputs | Role / remaining native implementation |
|---|---:|---|
| libllama.so.0 | 292 | model, context, graph, caches, vocabulary, sampling and bridges |
| libggml-base.so.0 | 9 | tensor/graph representation, allocation, scheduling, quant tables |
| libggml-cpu.so.0 | 17 | CPU backend, threading, quantized/SIMD arithmetic and packing |
| libggml-cuda.so.0 | 145 | host/backend control, copies, device kernels and template instances |
| libggml.so.0 | 2 | backend discovery and registration |
| libllama-common.so.0 | 58 | arguments, chat/Jinja, grammar, parsers, common utilities |
| libmtmd.so.0 | 58 | multimodal loaders, preprocessing and model graph families |
| libllama-server-impl.so | 22 | HTTP, models/routes, task/context/queue integration and bridges |
| llama-server | 19 | entry point and statically linked context/common objects |

Component counts overlap where a static archive is linked into multiple ELF
components. The unique closure is 602; summing component rows is invalid.

The 350 C++ files split as follows: 166 per-architecture model builders,
13 reconstructed core files, 20 other flint core files, 22 GGML control/backend
files, 56 common/chat/parser files, 54 multimodal files, 13 vendor server files,
two reconstructed server context/queue files, two bundled third-party files
(httplib and hash), and two generated build-info/UI files.

All repository-local compiler inputs in this closure are tracked. Two generated
C++ inputs are outside the checkout. Two bridge object compile commands were
not captured by the parent build log: `fx262a.o` and `fx263-pic.o`. Their source
mapping comes from `tools/build-goose-bridges.py`, explicitly marked
**recipe only**, not a witnessed compiler invocation. All object bytes are hashed.
The CUDA relink uses its recorded **support-directory cwd**; treating its
relative object paths as checkout paths would misidentify 144 inputs.

## Principal functions, calls and memory owners

Full object function symbols, undefined references, candidate providers,
direct source includes and lexical Goose identifiers are emitted by
`tools/audit-runtime-closure.py`. They are link references, **not** a dynamic
call graph. Symbols can be demangled with `c++filt`; optimized/inlined functions
are not enumerated as independent bodies. No symbol count is a coverage score.

| Source / principal entry | Calls / existing Goose seam | Ownership and what remains |
|---|---|---|
| server main, server-http, server-context / request task and slot processing | common chat/template/tokenization, llama_decode, sampling and response formatting | Native HTTP listener, tasks, slots, queues, cancellation and request lifetime remain. Experimental Goose request/schema/HTTP decisions are not a standalone transport. |
| llama-model-loader.cpp / tensor inventory, load, mapping and host registration | fg201l, fg286m, fg377s, fg389s, fg403s, fg404s | Loader owns file/mmap objects during loading; model retains mapped weights and tensor buffers. Native GGUF/file traversal and lifetime orchestration remain. Goose range/shape/alignment decisions borrow buffers. |
| llama-model.cpp / model construction, load and build_arch_graph | native per-architecture virtual builders; q35 plan seam is in the qwen35moe builder, not this object | Model owns layers, vocabulary, contexts, weight buffers and architecture pimpl. No direct fg undefined references occur in this object; that does not imply no downstream Goose. |
| llama-hparams.cpp / layer traits | fg200g_is_recr | Model owns hparams; a recurrent-layer decision is not the complete model runtime. |
| llama-context.cpp / decode, graph_compute, sched_copy | fg244s, fg391s, fg405s, fg406s; scheduler/backend APIs and MoE copy callback | Context owns scheduler, batch allocator, compute/result buffers and memory state; borrows the model. Native execution orchestration remains. |
| llama-graph.cpp / build_moe_ffn, graph/result input reuse | fg200g, fg408s, fg_gr_check | Graph tensors belong to GGML contexts; result/input objects retain metadata. Goose decisions do not own the full live graph lifecycle. |
| llama-batch.cpp / batch allocation and split planning | fg237s..fg243c, fg291b, fg383s, fg398s | Native vectors and split containers own storage; flat bridge arguments borrow it synchronously. Full batching and engine scheduling are not replaced. |
| llama-kv-cache.cpp / init_batch, find slots, seq operations and state I/O | fg199k, fg204x..fg206d, fg209i, fg227n..fg230s, fg233c, fg345w, fg367s, fg373s, fg381s, fg392s..fg395s | Native cache owns backend buffers, tensor contexts and shared cell metadata. Goose computes selected placement/cell/layout decisions; serialization, allocation and transfer remain. |
| llama-memory-recurrent.cpp / sequence updates and recurrent state I/O | fg210k, fg231a, fg232r, fg382s, fg396s | Native memory object owns recurrent tensors/contexts and sequence metadata. State must outlive individual graph executions. |
| llama-memory-hybrid.cpp / init_batch and delegated state operations | fg375s, fg376s, fg397s | unique_ptr members own attention and recurrent memories; model hparams is borrowed. Delegation decisions are Goose; owning lifecycle remains native. |
| llama-moe-cache.cpp / impl::copy, copy_experts, moe_cache_slru::plan | fg262a, fg263c, slru_plan / fg756s | Native impl owns device banks, host slot maps, vectors and cache groups. Goose mutates borrowed SLRU arrays; native wrapper reconciles evictions, then enqueues ordered H2D/copies. |
| llama-sampler.cpp / softmax, filters, distribution and chain | fg212s, fg380s, fg384s..fg388s | Native chain/sampler objects own RNG and candidate history; Goose selection/filter decisions borrow candidate arrays. Sampling state and complete chain ownership remain. |
| llama-vocab.cpp and unicode.cpp / tokenizer, piece conversion, Unicode handling | fg202t, fg213p, fg390s, fg401s, fg402s; fg215u..fg218t and Unicode modules | Model-owned vocabulary and native tokenizer work queues remain; exported Goose decisions/tables do not constitute a standalone tokenizer engine. |
| GGML base/CPU/CUDA | scheduler graph execution, backend async transfer, matmul/MMQ/MMVQ, attention/GDN | Backend buffers/streams/events and tensor contexts have distinct owners. Current arithmetic and most control are still native; a migrated dispatch choice does not migrate its kernel. |

The ordinary request dependency is server task -> template/tokenizer ->
llama_decode -> context graph/scheduler -> MoE copy callback -> borrowed SLRU
policy and ordered transfers -> CPU/CUDA kernels -> result synchronization ->
sampler -> piece/HTTP response. Function-pointer callbacks, virtual architecture
dispatch and asynchronous dependencies require runtime evidence beyond `nm`.

## ABI and threading boundaries

- Public llama/GGML entry points use their existing C ABI. Internal C++ classes,
  STL containers and virtual methods are not Goose ABI types.
- The compiler is pinned by `runtime/compiler/current.json` (SHA256
  `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`).
  Generated headers, not handwritten guesses, define slice layouts. EXP905
  showed that the same symbol name can link despite an incompatible old ABI.
- SLRU uses raw addresses of int32 arrays plus generated `sl_i64` slices for
  int64 state. The native owner keeps those arrays alive throughout the call;
  the policy must not retain their addresses after return. Host primitive byte
  access belongs to `slru-runtime-shim.c`.
- Async transfers must complete before their host inputs/banks can be reused or
  unregistered. Pinned mmap remains model-owned; registration is not a new copy
  of all weights. EXP906 adds approximately 18.1645 GiB of unreclaimable pinning
  while loaded and releases it at shutdown.
- The accepted test profile is `-np 1`, t8/tb16. This is not proof of general
  concurrent API safety. Several bridges have plain `static int g_started`
  initialization guards; they need a focused concurrent-entry audit before any
  multi-client/multi-context acceptance. No race failure is claimed without a
  reproducer. SLRU/cache mutable arrays also need a defined serialization owner.
- Platform, allocator and CUDA ABI bindings may remain external primitives.
  Own runtime orchestration, native kernels and bridge logic remain explicit
  migration debt; bundled C++ libraries are not hidden by calling them substrate.

## Existing Goose and functional acceptance

The accepted source-built runtime passes the EXP905/906 full-request and restart
gates. This accepts the **integrated implementation**, not a fully Goose-owned
standalone engine. Eleven Goose compiler invocations appear directly in the
parent command log; nested SLRU generation and generated C included by bridges
are additional inputs, so eleven is not a total Goose-module count.

Standalone evidence must be revalidated for the exact standalone artifact.
Historical loader, one-layer attention/GDN/MoE, KV reuse and schema gates remain
useful fixtures, but do not prove a composed model pass, tokenizer engine,
chunked prefill, CUDA scheduling and HTTP serving. Other architectures share
the loader, tensors, backends and APIs. The 166 native builder files remain in
the build; no removal or support reduction is authorized by this map.

## Audit findings and next bounded tasks

1. `migration-status.sh` now requires explicit `--probe-load` before executing
   its model loader. Default accounting is read-only and labels historical data.
2. Clean-checkout Goose consistency has 83 failures: 72 exact-hash local files
   were never tracked, three hashes drifted, seven linked source modules lack
   manifest entries, and one canonical source comparison fails. The 72 missing
   files can be recovered by exact recorded hash; the three changed bodies must
   be traced to their actual accepted revisions, not silently rehashed.
3. Repair source provenance/manifest without changing the accepted runtime.
   Then choose one standalone execution capability, preserving native fallbacks
   and architecture compatibility. Source ownership or a bound export is not
   sufficient evidence to delete C++.
4. For performance, first measure current cache hit/miss and transfer intervals,
   bridge/CPU overhead and graph reuse on EXP906. Historical traces are hints.
   No new optimization is accepted by this map. Current TTFT and critical-path
   fractions remain UNKNOWN; overlapping time must not be summed.

## Reproduction and evidence

The compact source inventory is `runtime-map-20261117.json`. The full local
symbol/dependency inventory is `Backup/exp907-runtime-map/closure.json`.
The audit command and all attempted-check outputs are retained under that
ignored directory; immutable build records are under
`/home/end0/AI/LlamaServer/franken-recovery-20261116`. The audit checks all nine
linked artifact hashes before producing a result, expands static archives, and
fails on an object without command or explicitly labeled recipe provenance.

Regression gate: `python3 -m unittest discover -s tools/tests -p test_runtime_closure.py -v`.
It verifies relative-cwd resolution, implicit static-archive inputs, generated
source mapping, and that default status cannot execute a supplied probe.
The opt-in probe check uses a harmless fake shell executable and fake model
file, never Apex. Model launches in this experiment: **zero**.

## EXP912: a Goose-callable graph-construction entry

The canonical graph builder now exposes `gb_common_args(i64[:])` alongside its
legacy packet-pointer API. The new entry accepts a caller-owned workspace of at
least sixteen int64 values and shares the same constructor body. Goose can
prepare operations, carry tensor handles and invoke the next constructor without
a per-node C packet callback. Tensor/context and auxiliary pointer lifetimes
remain the caller's responsibility; this API does not replace native allocation.

All 36 constructor modes match native GGML: 10408 checks/558 paired tensors per
route for frozen control, candidate legacy and candidate typed; native fallback
also passes. Protected-page and short-slice gates pass. A five-node chain is
orchestrated wholly in Goose and matches 18432 computed FP32 values, with a
positive legacy packet-read witness and zero packet reads on the typed path.
The 123-module main composition typechecks with the entry; no full runtime
build or model run was needed. Two targeted mutations fail semantic assertions.

This is an accepted composition capability, not full-model or server coverage.
The EXP906 binary set does not consume this canonical module and remains
unchanged. See EXP912 for the independent oracles, preserved failed numerical
test assumption about non-materialized CPU fusion outputs, and raw evidence.

## EXP913: real-weight attention controlled by Goose

The canonical Qwen3.5 attention module now owns the projection/norm/IMRoPE/GQA/
gate/output/residual sequence, exposed as project and finish operations with a
cache-free composition witness. This separation leaves a place for the upcoming
Goose KV update between projection and attention. The module is included by
default in canonical composition; three existing EXP156 POD field/name bindings
are exported once by graph-bindings.c and remain explicit temporary adapter debt.

Eight real-Apex-weight cases match an independent native GGML reference:
29 observed stages and 1069056 F32/F16 values, one/three tokens, positions0/17,
requested intermediates and ordinary CPU fusion. Type/op/flags, shape/strides,
parameters, names, source edges and view ownership agree. The positive legacy
packet witness works and the Goose branch makes zero C packet reads. Six
structural mutations are killed. The whole 124-module composition typechecks.

The input uses selected real embedding rows, not hidden states from a preceding
three-layer pass. K/V are fresh with F16 conversion, not the protected profile's
persistent Q4 KV. Therefore this proves a real-weight mathematical component,
not native-model logits, decode history, server behavior or inference speed.
The complete model SHA matches EXP906; only 15624192 weight bytes and three
1152-byte embedding rows enter the component. Integrated gate peak RSS59068KiB
is a test-process measurement, not whole-model memory. See EXP913.

## EXP914: persistent Q4 attention KV controlled by Goose

The default Goose composition now includes compact single-stream Q4_0 KV
planning, positions/indices/causal masks, whole-model-step tickets, abort/rewind
and active-prefix host checkpoints. Attention read views depend directly on
SET_ROWS outputs, so cache writes remain reachable without a host-side expansion
order. Cache storage outlives graphs; backend completion precedes the single
model-step commit. Twelve public exports add no native adapter or C++ policy.

Six real-weight steps match native Q4 K/V and attention output bits across graph
destruction, decode, checkpoint restore/replay, rewind and retry after an
unexecuted abort. Gates include 49 planner checks, 42 invalid layouts, 50 host
checkpoint checks and six semantic mutants. The complete 125-module composition
typechecks; fixture peak RSS59380KiB describes the small CPU process only.

This is a component acceptance, not final generation correctness or a speedup.
Persistent recurrent state, MoE, full logits, CUDA/backend staging and HTTP
execution remain open. Host checkpoint buffers must be CPU-addressable and the
backend quiescent. Full-model cache equality, task correctness and performance
gates are still required. Native cache implementations remain live dependencies
of the current server and other cache modes; no C++ deletion is justified here.

## EXP917: real-weight recurrent GDN and persistent state

Goose now orchestrates the real Apex recurrent layer: weighted pre-norm,
QKV/Z/beta/alpha projections, dt/negative-A gate, convolution and state tail,
RMS-based Q/K normalization, fused GDN, gated output norm/projection and
residual/post-norm. The module owns its derived F32 parameter packing and host
state snapshots; the executor expands explicit output/conv-copy/state-copy roots.
Default composition includes it (126 modules/4055 specializations typecheck).

Eight real-weight prefill/decode steps compare 7239360 F32 values bitwise,
including persistent conv/matrix state and restore/replay across graph
destruction. Two unexecuted aborts leave state unchanged. Additional gates:
210 invalid construction inputs, 115 config/host checks and five semantic
mutations. Peak CPU fixture RSS83964KiB is not model RAM or a speed result.

This real path exposed an overstrict shared constructor guard: native RMS
supports row-contiguous views with independent outer strides. Only the RMS arm
was corrected; other norm guards are unchanged. Legacy/typed constructor
regression and native rollback pass all 36 modes/558 tensors/10408 checks each.
Existing binaries and EXP912 snapshots remain preserved.

The old GDN L2 export is not used as a substitute: current native models use
RMS(eps/head_dim) followed by scale(1/sqrt(head_dim)). Fused GDN performs the
query scale and key-head mapping internally; neither is repeated by Goose.
Native GGML allocation/POD/math, CPU backend and C++ runtime still remain.
No new adapter or C/C++ runtime body was added or removed. Full MoE/trunk/logits,
model-step transactions, generation/session, CUDA and final performance gates
are open; no Goose-only engine or acceleration is claimed from this component.
