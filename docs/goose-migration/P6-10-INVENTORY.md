# P6-10 — sampling and grammars: live inventory (2026-11-18)

Measured on the shipped `libllama.so.0` (accepted EXP906 lineage, sha 4eba8242…)
and on `vendor/flint/src/llama-sampler.cpp`.

## Already Goose in the shipped library

| seam | call site (disassembly) | Goose entry |
|---|---|---|
| chain accept | `llama_sampler_chain_accept` | `fg387s_enabled`, `fg387s_decide` |
| chain apply | `llama_sampler_chain_apply` | `fg387s_enabled`, `fg387s_decide` |
| softmax | `llama_sampler_softmax_impl` | `fg384s_softmax` |
| distribution pick | `llama_sampler_dist_apply` | `fg386s_pick` |

Witnessed live in an Apex run: `FG384S_SOFTMAX`, `FG385S_SAMPLER_GUARDS`,
`FG386S_DIST_PICK`, `FG387S_SAMPLER_CHAIN`.

## Implemented in `goose/sampling/sampler-chain-full.goose` (474 lines)

`sm_mt_seed`, `sm_mt_next`, `sm_partial_sort`, `sm_greedy`, `sm_select`
(with in-place softmax), `sm_apply_top_k`, `sm_apply_top_p`, `sm_apply_min_p`,
`sm_apply_temp`.

## Not in the shipped library

The `sm_*` exports are absent from `libllama.so.0` (`nm -D` finds none), so the
full chain is not linked; only the four seams above are live.

## Still native, required by P6-10

Unsorted Top-P, Typical-P, XTC, DRY, Mirostat, Adaptive-P, the remaining RNG
operations, grammars and constrained generation. The current
`vendor/flint/src/llama-sampler.cpp` contains no `fg38x` hooks; the shipped
library's hooks come from the build-time tree, so the seam must be re-declared
when the sampler is rebuilt from this source.

## Plan

1. Add the missing filters to the Goose chain (typical-p first: it is the only
   one with a pure, order-sensitive reference in the native sampler).
2. Differential gate: the full chain against the native sampler for each filter
   and for the order of application, fixed seed, state save/restore.
3. Link `sampler-chain-full.goose` into the shipped `libllama` and wire the
   filter seams next to the existing `fg384s`/`fg386s`/`fg387s` ones.
4. Prove execution with counters, then run the Apex generation and the three
   paired A/B on the cumulative baseline (rule 31).
5. Remove the replaced native filter bodies only after the gates pass.

## Typical-P reference (for the Goose transcription)

`llama_sampler_typical_apply` (llama-sampler.cpp:1916): if `p >= 1` return;
softmax in place with sort; `entropy = Σ -p*log(p)`; `score_i = |(-log p_i) - entropy|`;
sort indices by score ascending; `cum += p[idx]`, stop when `cum > p` and
`i >= min_keep-1`; keep the first `last_idx` tokens, then sort the kept set by
probability descending. Any transcription must preserve that order exactly,
including the non-stable sort behaviour, and must use the same softmax that
`fg384s` already provides.
