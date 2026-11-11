# EXP384: sampler softmax runs in Goose, bit for bit

## Verdict

**LIVE on the reconstructed tree, bit-identical to the native normaliser.**

## Why this one was possible

My earlier note claimed the softmax family could not be ported because Goose has no `exp`. That was
wrong: `exp` lives in `stdlib/math.goose` as `extern "gs_expf"` aliased onto libm, so a Goose module can
call the same function the native loop calls. With the same function and the same order of operations,
the result is identical by construction, not by tolerance.

## Transferred

`llama_sampler_softmax_impl`, normaliser only:

1. the maximum — taken from the first element when the array is already sorted, otherwise scanned
2. `p[i] = exp(logit[i] - max)` accumulated left to right
3. one division per element

`do_sort` is untouched: when the caller asks for a sort, llama sorts first and the module is called with
`sorted = true`, exactly as the native code sees it.

## Gate

| check | result |
|---|---|
| 243 sets (n from 1 to 64, 40 trials each, sorted and unsorted, plus all-equal, extreme spread, single element) | 3 769 comparisons |
| comparison | **bit for bit**, 0 failures |
| mutations | **6/6 caught** |

Mutations: max only from the first element, max is the minimum, exp without subtracting the max, sum
keeps only the last term, divide by a shifted sum, scan starts at the second element.

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| all 128 probabilities | equal |
| `FG384S_SOFTMAX` | **live, 3 report lines (≈200 calls)** |
| `FG383S` / `FG382S`×2 / `FG381S` / `FG373S` / `FG345W` / `FG241U` | live |

An identical answer with a different implementation of the normaliser is the strongest result this
profile can give: one bit of difference would eventually change a sampled token.

## Still native in this chain

`dist` (the selection itself) uses `std::discrete_distribution` with `std::mt19937`. Reproducing it means
reproducing a libstdc++-specific algorithm (normalisation, `generate_canonical`, `lower_bound`), which
pins the port to one standard-library implementation. That is a decision to take deliberately, not a
step to slip in, so it stays out of this experiment.
