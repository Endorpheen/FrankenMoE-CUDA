# EXP351: sampler differential with fixed seeds

Same server, same tiny model, same request, one line at a time; the comparison is the generated `content` string, exactly.

| case | settings | result |
|---|---|---|
| greedy | `temperature 0`, `top_k 1`, seed 42 | SAME |
| seed42 | `temperature 0.8`, `top_k 40`, `top_p 0.9`, seed 42 | SAME |
| seed43 | same sampling, seed 43 | SAME |
| topk1 | `temperature 1.0`, `top_k 1`, seed 7 | SAME |
| minp | `temperature 0.7`, `top_k 0`, `min_p 0.1`, seed 11 | SAME |

Five configurations, sixteen tokens each, zero differences. The two different seeds give two different outputs -- so the test is sensitive to
the seed and the equality is not an artifact of a degenerate distribution -- and greedy agrees with `top_k 1` as it should.

Each case is a direct exercise of the sampler chain over the same logits: if the distributions differed, the seeds would diverge.

## Extended matrix

| case | settings | result |
|---|---|---|
| long | `temperature 0.9`, `top_k 50`, `top_p 0.95`, seed 5, 32 tokens | SAME |
| biassame | `temperature 0.8`, `top_k 40`, seed 42, `logit_bias` penalising `e` by -5 | SAME |
| repeat | identical to seed42, sent again in the same session | SAME -- and equal to seed42 on both lines, so sampling is reproducible within a process |
| grammar | `temperature 0.8`, `top_k 40`, seed 42, GBNF `root ::= [a-z]+ (" " [a-z]+)*` | SAME |

Nine configurations, zero differences. The grammar case is the interesting one: constrained decoding goes through the grammar sampler and
still produces the same tokens, and `logit_bias` demonstrably changes nothing between the lines while changing the distribution.

## Still open in P6-10

Mirostat v1/v2, DRY, XTC, typical sampling, top-n-sigma and the RNG itself as an object (the seed path is covered, not `std::mt19937`
equivalence) are not yet in the matrix. The covered set is the default chain plus grammar and bias.

## Sensitivity check, and a correction to my own earlier reading

Comparing the cases against each other showed that in the 15-case matrix only **six distinct outputs** appeared, grouped as
`{greedy, seed43, topk1, minp, miro2, typical}`, `{seed42, repeat}`, `{biassame, xtc, dry, topsigma}`. So for most of those modes the
equality between lines proved very little: on `stories260K` the next-token distribution is so peaked that different samplers walk the same
path. My earlier statement that "the two seeds produce two different outputs, so the comparison is seed-sensitive" was true but misleading --
seed 43 happened to coincide with greedily decoded text.

A hotter, longer matrix fixes the measurement:

| case | settings | cross-line | changed output vs baseline? |
|---|---|---|---|
| hot_base | temp 1.5, top_k 0, seed 42, 64 tokens | SAME | -- |
| hot_other | same, seed 99 | SAME | yes |
| hot_miro1 | mirostat 1, tau 5, eta 0.1 | SAME | yes |
| hot_miro2 | mirostat 2, tau 5, eta 0.1 | SAME | yes |
| hot_typ | typical_p 0.5 | SAME | yes |
| hot_xtc | xtc_probability 1.0, threshold 0.1 | SAME | yes |
| hot_dry | dry_multiplier 2.0, base 1.75 | SAME | **no** -- equals baseline |
| hot_sigma | top_n_sigma 1.0 | SAME | **no** -- equals baseline |

Six distinct outputs across eight cases, zero cross-line differences. DRY and top-n-sigma are **not demonstrated**: they produced the
baseline text, so agreement there is agreement about nothing. That is the same trap as the unobservable Q5_K mutation earlier in the
project -- a test that cannot fail is not evidence.

## Making DRY and top-n-sigma bite

Both were moved off "not demonstrated" with the right stimulus rather than a louder claim.

- **DRY**: a repetitive prompt (`the the the the the the`) at `temperature 1.2`, `dry_multiplier 3.0`, `dry_allowed_length 1`,
  `dry_penalty_last_n 64` changes the continuation (`rep_dry` differs from `rep_base`, first difference after the opening words), and the
  two lines produce the **same** changed text. So DRY now has a case that can fail and does not.
- **top-n-sigma**: `top_n_sigma 1.0` and `0.0` both leave the baseline text untouched, but `0.5` changes it (`sig_low` differs from
  `sig_base`) and again both lines agree.
- Parameter lesson: `dry_penalty_last_n: -1` and a negative `top_n_sigma` are rejected by the API with HTTP 400 before any sampling happens.
  The probe now uses in-range values.
