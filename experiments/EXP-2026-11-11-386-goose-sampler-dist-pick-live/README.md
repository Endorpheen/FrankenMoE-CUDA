# EXP386: token selection runs in Goose, bit for bit

## Verdict

**LIVE on the reconstructed tree; the completion is unchanged.**

## The algorithm that is actually used

My first attempt ported `std::discrete_distribution`. Reading the line showed this server does not use
it: `llama_sampler_dist_apply` takes the `#if 1` branch -- one uniform draw, `target = sum_cum * rnd`,
then the first index whose running sum of float probabilities reaches the target, normalising `p` in the
same pass. That is what was ported.

| step | where |
|---|---|
| the uniform draw | stays native (RNG primitive) |
| maximum, `exp(logit-max)`, `sum_cum` as double | Goose |
| `target = sum_cum * rnd`, first index with `run_sum >= target` | Goose |
| `p[i] /= sum_cum` in the same pass | Goose |

## Gate

| check | result |
|---|---|
| 70 sets, n from 1 to 300, sorted and unsorted, 5 draws each, **index and every p compared bit for bit** | 24 086 comparisons, 0 failures |
| boundary draws landing exactly on a running-sum threshold | covered, 0 failures |
| mutations | **7/7 caught** |

Mutations: strict threshold, normaliser is the running sum, selection never stops, target ignores the
draw, maximum from the first element only, sum starts at one, probabilities not stored.

## Run

| check | result |
|---|---|
| completion | `e68df05b…`, equal to the reference |
| all 128 probabilities | equal |
| `FG386S_DIST_PICK` | **live, ≈128 calls (one per token)** |
| `FG385S` / `FG384S` / `FG383S` / `FG345W` / `FG241U` | live |

With softmax (EXP384) and selection (this one) both bit-identical, the sampler's probability path is
Goose-owned end to end. The RNG and the distribution primitive remain native by design.
