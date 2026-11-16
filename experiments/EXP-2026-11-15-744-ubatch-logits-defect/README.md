# EXP-2026-11-15-744 — proven: `-ub` changes the logits, not the sampling

Base: `b4ad51dd`. EXP-743 found different answers per ubatch and could not say whether that was numeric
reordering or a defect. This settles it with greedy decoding, where sampling cannot be the cause.

## 1. The test

`temperature = 0` (deterministic argmax), same prompt, same seed, 64 tokens, three ubatch values:

| ub | cache | answer sha | first 8 tokens |
|---:|---:|---|---|
| 4096 | 4352 | `39cff5a2` | 11, 198, 248069, 271, 2, 5194, 2743, 35059 |
| 3072 | 5376 | `4b59bacb` | 11, 1716, 58558, 13, 198, 760, 6951, 26817 |
| 2048 | 5376 | `471fc44a` | 11, 1156, 579, 1622, 369, 310, 8290, 264 |

**Three ubatch values, three different deterministic outputs.** The first token is identical in all
three; the second differs.

## 2. What that proves

With `temperature = 0` there is no sampling: the token emitted is the argmax of the logit vector. If
the logits were the same and only their summation order differed by 1e-6, the argmax would be the same
token except in a near-tie. It is not - the three arms pick **198, 1716 and 1156**, three unrelated
tokens.

**So the logit vectors differ substantially, not marginally.** A 1e-6 reordering cannot turn `\n` into
` check`; a 1e-1 difference - or a different computation entirely - can.

This rules out the "batch-dependent kernel summation order" explanation as the *sufficient* cause. It
may still contribute, but something larger is wrong in the smaller-batch path.

## 3. What is affected

| configuration | reference-equivalent? |
|---|---|
| ub 4096 | **yes** - reproduces the historical hash `c2ff42cc` at temp 1.0 and is the only one that does |
| ub 3072 | no |
| ub 2048 | no |

Every speed figure measured at ub 3072 (43.05) and ub 2048 (42.35) describes a configuration that
computes something else. They are excluded as reference-equivalent configurations until fixed.

## 4. Why this is the most important finding of the session

It retroactively reframes most of the speed work. EXP-733 through EXP-742 reported a progression from
39.86 to 43.05 decode, achieved by lowering `--ubatch-size`. **That progression was bought by changing
the computation**, not by making the reference computation faster. The 43.05 is real for what it
computes; it is not "the reference answer, faster".

This is precisely the trap the project's own rules exist to catch, and it was caught only because the
correctness check was run after the speed work rather than before. The lesson is procedural: **the
output hash belongs in the A/B loop, not in a separate later pass.** A configuration whose speed is
being measured should be checked against the reference hash in the same run.

## 5. Operational consequence

- `run-apex.sh` default: reverted to `--ub 4096 --cache 4352` (done in EXP-743).
- `BEST-CONFIG.json`: non-4096 configurations marked `reference_equivalent: false`.
- Recommendation: take the hash with every benchmark row. `tools/bench-log.py` already records
  `content_sha256`; the missing discipline was comparing it instead of just storing it.

## 6. Next, specifically

Find *which* computation differs. The candidates, now narrowed:

1. **The CPU MoE path selects kernels by ubatch.** With `--cpu-moe`, expert tensors live in RAM and the
   copy/matmul route depends on batch size. If a smaller batch selects a different expert kernel whose
   accumulation is not equivalent, that is the defect.
2. **The KV or recurrent state path** treated differently per batch size - the model is 30/41 recurrent
   layers, where batch size changes the state update shape.
3. **A bug in the Goose bridge** for one of these paths, which a differential gate against the native
   implementation would catch.

Test 1 is the cheapest: run the same prompt at ub 4096 and 2048 with a single-token generation and
compare the logit vectors directly, rather than comparing the sampled text.

## 7. Cost

Three server runs, one request each.
