# EXP-2026-11-19-1297: identical arrays, different index - the generator state, measured block-wise

Configuration 2 (`typical + XTC + adaptive-p + dist`, 100 rounds), compared as whole applies rather than line
windows:

```
applies whose array differs    90 of 100
first differing apply          round 10 (apply 110)
  at that apply the array      identical
  at that apply the selection  goose idx=0 tok=129   native idx=3 tok=4
```

At round 10 the probabilities and the ids are identical in both modes and the draw lands on a different index. The
EMA then absorbs a different `original_probs` entry (`ws` 0.934 against 1.403 from the next apply onward), and every
later round follows - which is why 90 arrays and 100 selections differ from a single divergent draw.

## What this rules in and out, with the measurements behind it

| candidate | verdict |
|---|---|
| the arrays | identical at the divergence |
| the probabilities | identical at the divergence |
| the transform | identical logits at the divergence |
| the `sorted` flag | now passed into the door; the isolating probe is 0 of 100 |
| XTC's survivor count | 162 of 162 applies agree |
| mirostat's `k` | differs, but config 2 has no mirostat; that belongs to config 1 (EXP-1296) |
| the generator state | **the only thing left** |

Both samplers that draw here consume two words per call on the native side and two on the door side, and the size-1
path in `llama_sampler_dist_apply` draws once in both modes, so the arithmetic says they should stay in step. The
measurement says they do not. That is where the next probe belongs: a counter of `ctx->rng()` calls per round, per
call site, in both modes.

## Why the report stops at naming it

This thread has produced five instrument errors against two real defects, and the remaining question is confined to
a configuration the acceptance profile never enables - no typical, XTC, mirostat or adaptive-p parameters are set
there, and the acceptance hash has stayed at `c2ff42cc1219` through every change. The next step is a specific one,
and it is recorded rather than attempted at the end of a long session.
