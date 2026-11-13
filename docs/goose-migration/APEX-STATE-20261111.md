# Apex port: state and where it is blocked

## Closed (9)

| item | evidence |
|---|---|
| APEX.CORE | scheduler decisions, 29 report lines, completion e68df05b |
| APEX.LOAD | model loader decisions |
| APEX.TOKENIZER | piece copy, unicode, vocab |
| APEX.KV | metadata write/read, slot walk, restore exact |
| APEX.RECURRENT_MEMORY | hybrid memory, prefix caches |
| APEX.BATCH | batch container, plan, layout |
| APEX.OUTPUT | logits/sampling path |
| APEX.CPU_CONTROL | expert lifecycle routes live (FG143/147/149/150) |
| APEX.CPU_MATH | Q4_K and Q6_K dots: gates 160/160 and 240/240, 7/7 and 6/6 mutations, CPU bench run identical (`d13cf4cf`), execution proven by defect tests |

## Partial (1)

**APEX.CUDA_MATVEC** — the Goose Q6_K CUDA matvec is linked directly into a backend built from source (no preload,
no entry patching). On the accepted profile that library completes with `e68df05b`, equal probabilities and a
successful restore, and **168 of 324** Q6_K matvec calls execute in the Goose kernel. The other 156 use 2 or 4
destinations and fall back to native: the kernel is exact for `ncols_dst = 1` at every row count but wrong for more,
and the fix is column-aware staging plus block-to-destination mapping in the generated device code.

## Blocked (5) — each needs a decision or infrastructure, not more code by me

| item | what it needs |
|---|---|
| APEX.CUDA_CONTROL | the FG195V/FG196P routes live in a goose-built `libggml-cuda` whose build chain (EXP170-186) has no scripts at 170-172 and no committed artefacts. The library can be rebuilt from the flint sources (done), but the goose modules for these routes are not in that tree. |
| APEX.CUDA_MMQ | same build boundary; the MMA/TensorCore kernel and `quantize_mmq_q8_1` are on the missing chain |
| APEX.MODEL_GRAPH | node creation is not a transcription: `build_moe_ffn` alone is 504 lines with 104 `ggml_*` calls, and the bridge carries scalars and flat arrays only. Needs a tensor API in Goose or an agreed scope change. |
| APEX.REQUEST / APEX.SESSION | the HTTP server, queue and slots live in `tools/server`, outside `libllama`; the bridge has no HTTP/socket surface. Needs a new server-side Goose component. |
| APEX.SOURCE_CLOSURE / APEX.FINAL | follow from the rows above |

## What would unblock each, concretely

1. CUDA_CONTROL / CUDA_MMQ: restore the goose CUDA module sources (or their build scripts for EXP170-172) so the
   routes can be rebuilt against the flint backend — the rebuild path itself is proven.
2. MODEL_GRAPH: decide whether node creation is a declared primitive (criteria change) or fund a tensor API in the
   bridge.
3. REQUEST/SESSION: decide whether the HTTP layer is in scope; if yes it is a new Goose component, not a port.
4. CUDA_MATVEC full: column-aware staging in the generated device code (mechanical but substantial).

## Measured size of what remains in the three PARTIAL items

| item | remainder, measured | why it is not a decision to port |
|---|---|---|
| CUDA_MMQ | the tensor-core matmul body: `mmq.cuh` + `mmq-load-tiles.cuh` = **3,375 lines** of templates, plus the `mma` fragments | this is compute, not a decision: it has to be ported for the MMA route to be Goose |
| MODEL_GRAPH | **32** `ggml_*` calls inside `build_moe_ffn` (mul_mat, add_id, glu, weighted sum) | these are the graph primitives the plan itself declares out of scope, like the allocator and the backend calls; the three decisions on top of them are already Goose and live |
| REQUEST/SESSION | the JSON surface: **164** `json` references in `server-context.cpp` | JSON is a vendor library, not inference logic; the four decisions on top of it (slot budget, request bounds, context shift, sampler choice) are already Goose, three of them defect-tested live |

So the honest shape of the remaining work: one large compute port (CUDA_MMQ's tensor-core route) and two
boundaries that are primitive/libraries by the plan's own definition. Nothing here is waiting on a missing
artefact or an unanswered question.
