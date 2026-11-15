# EXP-2026-11-15-674 — P11-01 status audit: what the runtime can do, and what still blocks a run

Base: `fb206912`. Audit only, no code change, no model, no GPU.

This card is the honest map between the accepted Goose modules and a runtime
that can actually serve a request. It exists so that nobody — me included —
mistakes "the unit links" for "the runtime works".

## 1. The invariant now in force

`AGENTS.md` rule 15: no C++ is deleted before the new runtime is fully accepted.
The order is fixed — complete integration, then proven behaviour with the checks
that are actually needed, then Igor's final manual acceptance, then one largest
possible removal pass. Removing the replaced C++ or any working fallback before
that is forbidden, and every removal needs evidence that the replacement already
carries the behaviour.

This card is a step inside "complete integration", not a claim of readiness.

## 2. What the standalone runtime does today

| capability | state | evidence |
|---|---|---|
| composed unit links with libc/libm/pthread only | works | EXP671, `frankenmoe-standalone` |
| one `goose_init` per process | works | `init_runs=1` in every run |
| GGUF container parsing | works | substrate reader, EXP672 |
| GGUF metadata table, every key | works | 4 files, `built == n_kv`, `agree=yes` |
| GGUF tensor directory, real extents | works | 3 fixtures, `ne0_sum` equal on both sides |
| type table (43 types), block sizes | works | `abi_type_count`, `abi_blck_size` |
| config validator (48 rules) | works | `cfg_validate`, rule 1 on an empty config |
| server tool table, MIME table | works | `tool_index`, `mime_of`/`mime_text` |
| every Goose helper the unit needs | satisfied | the 65 helpers resolve from the adapter; only libc remains |

## 3. What is present in the unit but not driven

These modules export real entry points and none of them is called by the
standalone host yet. They are decisions, not engines:

| domain | prefix | exports |
|---|---|---:|
| architecture registry | `ar_` | 6 |
| model management | `mg_` | 13 |
| GGUF metadata / tensors | `s1_`, `s3_` | 7 + 7 |
| stream plan, row stream | `sp_`, `xl_` | 9, 12 |
| router hooks | `rh_` | 18 |
| speculative decode | `sdp_` | 11 |
| driver loop | `drv_` | 15 |
| slot / schedule | `sl_`, `sched` | 13, 19 |

## 4. What is genuinely missing for prefill/decode

Measured, not assumed:

1. **A graph builder.** `goose/model/goose-model-graph.goose` exports **zero**
   functions: it holds the per-layer *decisions* the builder makes, not a
   builder. Nothing in the unit constructs a compute graph today.
2. **An allocator.** Nothing allocates tensor memory; the directory records
   descriptors and validates `nbytes`, and no `ggml_backend_alloc` equivalent
   exists on the Goose side.
3. **A backend.** There is no CPU or GPU backend in the Goose runtime. The Apex
   GPU path lives in the C++ binary and has no Goose counterpart.
4. **A tokenizer that produces tokens.** The vocab modules decide type and
   partition; the BPE/SPM mechanics are still native.
5. **KV cache storage.** The modules own cell policy and slot selection; the
   buffers are native.
6. **HTTP and the session loop.** The server decision modules exist; the server
   itself is native.

None of these can be closed by wiring alone. Each needs either a Goose module
that does not exist yet, or a backend seam the runtime does not have. That is the
real reason the C++ runtime cannot be removed now, and it is what rule 15
protects.

## 5. What this means for the next increment

The honest next step is item 1 and 2 together — a graph builder and an
allocator over the tensor directory the runtime now holds — because every later
item depends on them. Item 3 is the one that decides whether the Apex GPU path
can move at all, and it is worth a separate decision because it may need a Goose
module for backend dispatch that the compiler owner would have to support.

Until those exist, the shipped binary stays the C++ one, unchanged, and the
standalone runtime stays a proven core rather than a replacement.

## 6. Verification reused, not repeated

Every row in §2 cites evidence already recorded in EXP671/672/673. Nothing was
re-run to write this card: the logs those experiments wrote are the evidence, per
rule 14.
