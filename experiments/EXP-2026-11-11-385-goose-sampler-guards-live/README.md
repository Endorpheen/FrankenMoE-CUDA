# EXP385: sampler early-exit guards are live

## Verdict

**LIVE on the reconstructed tree.**

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | `min_p` is skipped at `p <= 0` or an empty candidate list | `llama_sampler_min_p_apply` |
| 1 | `typical` is skipped at `p >= 1` | `llama_sampler_typical_apply` |
| 2 | `top_p` is skipped at `p >= 1` | `llama_sampler_top_p_apply` |
| 3 | `top_k` is skipped at `k <= 0` | `llama_sampler_top_k_impl` |

Op 0 and op 3 are the ones this profile takes: `min_p = 0` and `top_k = 20` on the agreed request, so
both fire on every token.

## Gate

| check | result |
|---|---|
| 73 draws over p ∈ {−1, 0, 1, 0.5, 0.999999, 1} × sizes 0..2 × four guards | 73 checks / 0 failures |
| mutations | **5/5 caught** |

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| probabilities | equal |
| `FG385S_SAMPLER_GUARDS` | **live, ≈200 calls** |
| `FG384S` / `FG383S` / `FG345W` / `FG241U` | live |

## Note on the scaling

`p` crosses the boundary as `p * 1e6` rounded to an integer, because the bridge carries integers. The
comparison `p >= 1` becomes `>= 1000000`, and the gate tests the exact boundary. A parameter within
1e-6 of the boundary would compare differently from the native float test; that band is documented here
rather than hidden, and it is empty for every parameter this profile uses (0, 0.95, 1.0).
