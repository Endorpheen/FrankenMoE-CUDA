# EXP-2026-11-17-918: real MoE and model-trunk composition in Goose

Status: ACCEPTED CPU MATHEMATICAL TRUNK; NOT AN INDEPENDENT SERVER.
Base: EXP917, 5cfe369faae2f50fff1f9076c21d7364cd63797c.

## Fixed hypothesis and protocol

Goose can compose actual routed/shared FFN arithmetic with the accepted real
GDN/attention paths, using borrowed zero-copy quantized GGUF tensors, then drive
the complete 40-layer trunk/logits. This is an integration milestone, not a
shape-only API. Preserve the independent Apex run bundle unchanged.

Native Apex: ungrouped softmax router, argsort-desc plus top-8 view (not a
different TOP_K operation), gather selected probabilities, normalize with sum
clamped to F16 minimum-normal, optional existing weight scale, separate Q4_K
gate/up and Q6_K down, SwiGLU, post-FFN weighting, ordered expert additions;
gated shared expert uses a separate full-width sigmoid router and SwiGLU.
No dropping or changing expert count. No quantization/activation tolerances are
relaxed. The existing scalar Goose router is not assumed bit-equivalent to the
native vector softmax protocol.

Control: independent native GGML constructors and original quantized payloads.
Candidate: Goose graph policy and checked mapping descriptors. Require exact
dtype/shape/stride/params/dependency equality and bitwise finite computed values,
including actual ids, selected weights, shared contribution and final logits.
Use one/three-token chunks, persistent state/cache, graph destruction and replay.
Compare identical arithmetic/chunking protocols; synthetic tie/degenerate router
inputs supplement, not replace real weights. Test malformed descriptors and
mapping geometry before allocation or mutation, and relevant semantic mutants.

## M0 and memory bounds before running

The 21713463136-byte GGUF already has a verified unchanged EXP913 SHA/stat
receipt. A read-only mapping is virtual address space, not a second resident
weight copy. Tensor metadata belongs to a persistent no-allocation context;
borrowed data pointers live only while that mapping remains valid. Reuse the
accepted Goose GGUF block/type-size table. Reject overflow, short mappings,
unsupported types and bad shapes before making a tensor. Do not expand expert
weights to F32 or bypass the existing 64MiB bounded-read guard.

Layer0 expert payloads total about 498MiB (Q4_K gate/up and Q6_K down); router
and shared weights add a few MiB. A CPU functional graph touches the selected
experts only. Initial per-call component arena <=128MiB and a small persistent
metadata context. A later full-trunk gate needs explicit state/workspace and
memory estimates before it starts; model-speed tests additionally require
residency~99%, a quiet machine and the protected fixed workload/profile.
No speed claim follows from a sparse functional CPU test.

All generated C/ELF stay under a new LlamaServer candidate directory. Only one
sequential CPU gate at a time, each limited to three minutes; no server/GPU
starts for initial FFN/binder gates. No new C++ runtime policy or wrappers to
increase a Goose counter. Native GGML/math/POD/CPU backend and the temporary
fixture GGUF reader remain explicit dependencies until genuinely replaced.

Reaching full logits is still not acceptance of the final no-C++ runtime:
loader/OS lifetime ownership, backend/scheduler/CUDA, tokenizer/server and
generation/session correctness, independent build/link and speed remain required.

## Results

The standalone Goose unit exposes a checked borrowed-weight binder, routed/
shared FFN and the complete layer loop through LM head. It does not call the
native llama model builder. All 40 real trunk layers are evaluated: 30 GDN,
10 full-attention, all routed/shared experts, embedding/final norm/output head.
The MTP block is excluded as required by the pinned no-speculation profile.

Twelve real FFN cases compare 815560 values exactly: token batches1/3,
requested intermediates/default fusion, ordinary top-8, used1 plus scale0.5,
and a zero-router tie fixture with the same real expert weights. Metadata and
source edges agree with independent native constructors. All168 invalid FFN
calls fail before allocation;24 binder boundary cases fail before allocation,
including element-count overflow independent of quantized-byte overflow.
No old scalar-router numerical assumptions are used as an oracle.

The full-model fixture binds733 candidate tensors directly into one read-only
GGUF mapping. The independent reference uses native descriptors and constructors.
Prefill3, decode1, then restore/replay decode1 match bitwise: final accepted
three-step gate compares2502720 frame values, including1241600 logits, and all
conv/matrix/Q4 KV bytes. All model steps share one ticket and commit once after
backend completion; layer construction does not advance the prefix. Each graph
is destroyed. Per step there are6408 graph nodes including both branches,61
candidate execution roots and164 observed frames. The final logits are finite.

The full gate additionally verifies residual/head dependencies and that each
recurrent root copies into its actual persistent destination before computing.
Six semantic mutations are detected: binding offset, selected-weight division,
shared gate, omitted FFN residual, omitted head norm, and omitted conv-copy root.
Crashes, compilation failures and resource stops are not counted as successful
negative gates. The earlier residual mutant hit a resource/time guard; its old
logger did not distinguish which bound fired. It is retained, not interpreted
as a numerical/code result. A minimal structural probe now detects the error
before the divergent full computation. The same stricter fixture passes the
unchanged positive case; no limit was raised.

Before the full gate, memory was budgeted as a192MiB persistent state arena,
512MiB per-call graph arena, roughly128MiB snapshot storage plus sparse mapped
weight pages, capped at4GiB process-tree RSS and180s. Persistent candidate and
reference tensors consume137684480B including metadata. Accepted fixture peak
RSS3801220KiB (3.625GiB); observed process-tree peak and VmSwap are in the raw
log, with maximum process swap0. This sparse short-context CPU footprint is
not production RAM, a memory improvement, or a valid throughput benchmark.
The earlier FFN-only fixture peak was115500KiB. VRAM was not used for inference.

Default127-module composition typechecks (4062 specializations), with canonical
source/provenance/symbol entries. No full native rebuild or destructive tree
sync was run. Existing successful library/check/FFN gates were reused after
fixture-only changes; the initial parser error (`block` is reserved) and const
shape ABI failure are retained. The latter was fixed in the caller by copying
four descriptor words, not copying weights or rebuilding the library.

## Boundaries and remaining debt

Mapping lifetime, GGUF metadata/name lookup, model configuration and execution
are still supplied by the temporary C++ test fixture. It calls the Goose trunk
and the generic GGML CPU executor; no new C/C++ runtime wrapper or adapter was
added. `ggml_get_no_alloc` is a direct existing allocator query. Native tensor
allocation/POD, math/CPU backend, GGUF reader and libstdc++ remain dependencies.
The loader must own the mapping for the entire lifetime of the bound tensors;
the borrowed binder must not be used after unmap. Whole-model checkpoint error
atomicity is not established by the fixture's quiescent valid-snapshot loop.

C++ replaced-and-removed0 lines; dead C++ deleted0 lines. Historical protected
closure248213 C++ lines/350 files remains intact; the minimum necessary subset
is not yet classified. Do not call this a no-C++ runtime, a model-quality/task
gate, a tokenizer/sampler/server test or a GPU/performance result. The protected
Apex bundle, profiles and baseline55.52/2625.21 are unchanged. New prefill/decode,
TTFT and performance deltas remain UNKNOWN; cache residency was not measured
because no speed comparison was performed.

Selective reproduction uses `tests/build.py --out <new-LlamaServer-directory>
--support <support> --model-proof <EXP913-proof> --step <stage>`.
Accepted fixtures: `gate-shape` for FFN, `gate-restore` for the added binder
overflow check, `gate-wiring` for full trunk/restore and minimal root checks.
Use `--mutation <name>` with stage mutations, or mutation-gate to reuse an
already compiled unchanged mutant. Full runs enforce the stated RSS/time bound.
Successful outputs/logs are not overwritten; changed module/backend inputs
require a new output directory. Raw artifact hashes are in evidence.json.
