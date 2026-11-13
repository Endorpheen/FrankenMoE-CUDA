# EXP-2026-10-30-088: additional near-tail prefix checkpoint

Status: REJECTED at the model numerical gate. New goal: at least5% improvement beyond accepted EXP-087 remains unmet; its memory saving is not counted again. Original hypothesis and protocol are preserved below.

## Hypothesis before changes

MEASURED: EXP-087 cache-repeat at configured context262144 restores32764 of32768 tokens and spends166.368ms processing the remaining4. The server already requires at least one input token to regenerate logits. SOURCE-OBSERVED: prompt filling creates near-end boundaries at4+n_ubatch and4 tokens from the end. A bounded additional checkpoint one token before the end may reduce repeat time to first token while retaining the existing4-token and periodic anchors for branches.

ESTIMATE: replacing three of four replayed token evaluations could remove up to about120ms from the observed warm prefill, before additional checkpoint save/restore and graph costs. This is an upper bound, not a measured gain or a cold-prefill/decode improvement. Adding a checkpoint increases CPU state storage by approximately one existing63MiB snapshot in this case; the original total/count budgets must remain enforced.

## Reference and scope

Immutable reference: project commit c3c52dcc8f15a6c960d2777b98e2f8a50620c54d, EXP-087 runtime and manifests in results/exp087-lazy-reserve/. Keep lazy reservation, kernels, weights, precision, full262144 context, EOS, existing prefix anchors and older accepted builds. Any implementation uses a separate directory under /home/end0/AI/LlamaServer/. No installed Desktop/runtime/profile edits. No model-name branch for the new policy.

First gate: inspect creation-before-decode, stored token counts, strict recurrent position bounds, resume/rewind, all checkpoint conditions and eviction order. Determine whether an added boundary actually produces a usable checkpoint and whether splitting3+1 changes numerical behavior or cold overhead. Do not replace the old4-token boundary. Reject the direction if it cannot preserve branch coverage, failure fallback or bounded memory.

Upstream rationale: https://github.com/ggml-org/llama.cpp/pull/20288 explains that the small tail excludes mutable reasoning-open tokens. Retaining the4-token anchor is therefore a compatibility requirement, not cosmetic. Add real templated-chat coverage for regenerate and a follow-up with changed reasoning prefix, in addition to native token-array probes. A newer snapshot must never be selected beyond the actual shared prefix; existing invalidation and restore bounds remain mandatory.

Parent source finding before implementation: retaining the4-token boundary alone does not protect its snapshot under a full cache. Existing victim selection can choose that now-interior point after a new1-token point is appended. The optional point must therefore have count/byte headroom before splitting and must not force an existing anchor eviction; skip it under pressure, and avoid a duplicate save causing an eviction. Add a full-count/tight-budget regression gate. This is an implementation requirement, not evidence that an unchecked extra array entry is safe.

## Pre-stated qualification and acceptance

Primary metric: client time to first token for warm32K repeated requests, with near-identical short-tail branches as separate practical coverage. Do not label this whole-response, cold-prefill or decode speedup. Secondary gates: cold request, ordinary24K-shared branch, near-end branch sharing32764 tokens, early branch, disabled policy and original Flint behavior. EOS stays enabled. Selected token IDs and ordered top5 IDs must match, absolute probability delta<=0.001; investigate changed state/batch arithmetic instead of relaxing tolerance.

Client timing definition: curl trace send-header timestamp to receipt of the first nonempty generated tokens array, including whitespace/special tokens. HTTP headers, empty progress events and time_starttransfer alone do not establish TTFT. The offline parser is checked against an existing real trace and fragmented empty-content events before new measurements.

Only after source feasibility, announce a bounded manual qualification block. If qualified, require at least five balanced serial matched pairs against EXP-087, median client TTFT reduction>=5%, at least4/5 wins, process swap0/no request-window swap-out, and no material (>3%) cold-prefill/decode regression. Keep count/byte limits and full-capacity behavior; new default behavior can only have a disabling flag. No automatic acceptance from one short repeated fixture. Record exact scope and added RAM; no universal speed claim.

## Build isolation gate

Control relinking is bit-identical to the EXP-087 server implementation: SHA256 8a963ce35bc919880354b226f5737c6c5d3f2dc47184c291badfe8470a209168. The context object and full static archive also match exactly. Preparation must preserve adjacent server headers and macro filename paths, and copy the original ten-member static archive before replacing its context object. Initial incomplete-header/archive attempts are discarded build-setup diagnostics, not engine candidates. The candidate retains all measured EXP-087 runtime libraries; any future changed component will be identified separately.

## First qualification and dependent refinement

The1-token-tail variant is REJECTED at the numerical gate. It reaches cache_n32767/prompt_n1 and client warm TTFT60.015ms versus164.812ms control, but ordered top5 diverges at output index3 and selected IDs diverge at index65. The first chosen-probability delta is0.00001407; autoregressive divergence later increases the matched-position maximum to0.10393. No latency acceptance follows from this failed correctness gate. The final late-branch request was not run after the failure. The control window also had one page of system swap-out and is excluded from performance acceptance.

One dependent refinement before any promotion: test tail2 (2+2 instead of3+1), preserving the same4-token anchor and exact correctness criteria. Mechanism hypothesis: avoid the single-token recurrent/decode path introduced by the final1-token batch, while still reducing repeated input work. This is a specific batching-path check, not tolerance relaxation or selecting the best timing. One candidate start/two manual requests (cold and repeat), approximately one minute; compare its IDs/top5/probabilities against the same EXP-087 qualification control. If numerical correctness still fails, stop this tail-splitting direction rather than continuing arbitrary tail choices.

Final: tail2 also fails. The ordered top5 differs from output index0 and selected IDs diverge from index35; maximum matched-position probability delta0.06392. Its diagnostic repeated-request TTFT is134.496ms versus164.812ms control, with cache_n32766/prompt_n2. No five-pair timing series, pressure/persistence/model compatibility acceptance or production promotion follows. Three starts/seven requests total; EXP-087 restored and hashes verified. Full evidence and limits: results/exp088-near-tail-checkpoint/.
