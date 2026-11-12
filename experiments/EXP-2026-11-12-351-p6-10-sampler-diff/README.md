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
