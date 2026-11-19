# EXP-2026-11-19-1307: the doors are called on the real model, counted

Every earlier proof that the samplers run through Goose was measured on candidate arrays built by a harness. The
question this card answers is whether the same doors are reached during **real generation** from the model.

## The evidence, from the server's own log

Three separate lifecycles - the clean-room copy, the archive downloaded from the release, and the run that produced
the baseline - each with the fixed profile and three requests of 512 tokens:

```
                        top_p   dist   counter lines
cleanroom-1305          1500    500    22
downloaded-1305         1500    500    22
hostreg-1298            1500    500    22
```

The counters print on the first call and every hundredth thereafter, so 1500 and 500 mean 1500 top-p applications
and 500 selection applications per lifecycle - not 15 and 5. Identical across three independent runs of the same
binary, which is what a deterministic sampling path should produce.

## What it settles

- the chain is connected to the shipped library, not to a copy: the counters come from the server that generated
  the tokens whose hash matches the baseline;
- the calls happen on real logits: 1500 top-p applications is consistent with 512 generated tokens advancing through
  the sampler chain per request;
- the keys the profile sets (top-p 0.95, top-k 20, temp 1) reach the ported doors and not only the native ones.

## What it does not settle

`penalties` prints no counter in these runs because the fixed profile leaves the repeat penalty at its default, so
that sampler is not added to the chain. Its door is verified on candidate arrays only. Recording the gap is the
point: a counter that never appears is not evidence of absence or presence.

## Why this belongs in the record

The rule that a successful compile is not proof of presence applies to the sampler path as well. A door that is
exported, verified in a harness and never reached by the shipped server would look identical from every other angle
- this is the measurement that distinguishes the two.
