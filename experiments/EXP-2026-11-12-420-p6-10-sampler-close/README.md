# EXP420: P6-10 sampler edge closure -- EOS, ignore_eos and adaptive_p

The remaining P6-10 items were `ignore_eos`, the fork-specific adaptive sampler,
deep filter edges and the RNG/grammar boundary. This experiment makes the first
two observable and checks both lines on the same requests.

## Configuration

Two servers, same argv and model: the accepted exp244 runtime and the flint
`build-cuda/bin`. Context 512, temperature/seed as below. One short prompt that
naturally reaches EOS, and one hot story prompt for the adaptive sampler.

## Result

| case | ours | native | tokens / content / probabilities |
|---|---|---|---|
| short prompt, `ignore_eos` false | 3 tokens, `stop_type=eos`, content `02ec2f94...` | same | equal |
| short prompt, `ignore_eos` true | 8 tokens, `stop_type=limit`, content `0545afeb...` | same | equal |
| hot prompt, base chain | 32 tokens, `stop_type=limit`, content `60dc4b79...` | same | equal |
| hot prompt, `samplers=[top_k,top_p,temperature,adaptive_p]`, target 0.5 | 32 tokens, content `7f53a945...` | same | equal |

`ignore_eos` changes the output (3 -> 8 tokens, `eos` -> `limit`), so the
equality is not agreement about nothing. `adaptive_p` also changes the output
(content `7f53a945...` vs `60dc4b79...`), and both lines produce the same changed
output. An earlier attempt without `adaptive_p` in the sampler list changed
nothing on either line; the code explains why: `common/sampling.cpp` adds the
adaptive sampler only when the user explicitly includes it.

## Scope decisions for the remainder

| clause | decision |
|---|---|
| RNG as an object | native identity. Both lines use the same native `std::mt19937` path; the sampler object has zero `gg_function_` symbols, so a differential there would compare one implementation with itself. The seed path is covered by every fixed-seed case. |
| grammar-constrained generation | belongs to P7-09, where the native PEG grammar engine is inventoried. P6-10 owns the sampler chain, not the grammar parser. |
| deep token-filtering edges | covered by EXP384-388 (top-k, top-p, min-p, typical, XTC, DRY, top-n-sigma, dist) and the earlier hot matrix; no new edge was found that this run left untested. |

## Verdict

P6-10 is closed with the sampler matrix above plus EXP384-388. The grammar
engine and RNG primitive are explicitly assigned elsewhere, not counted here.
