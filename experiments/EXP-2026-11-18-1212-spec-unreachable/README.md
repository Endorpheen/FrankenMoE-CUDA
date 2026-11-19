# EXP-2026-11-18-1212: the speculative acceptance block is not entered at all

The synth decision is ported and gated (2006 cases), its door is exported, and a diagnostic was placed
at the top of the accept dispatch:

```
FG_ACCEPT_BRANCH synth=%d replay=%d reject=%d draft=%zu
```

It never printed. Configurations tried:

| configuration | branch print | synth counter |
|---|---|---|
| `--spec-type ngram-simple`, temp 0.8, 48 tokens | no | no |
| `--spec-type ngram-simple`, temp 0.0, 48 tokens | no | no |
| `--spec-type ngram-simple`, `cache_prompt`, 3 requests | no | no |
| `--spec-type ngram-simple --spec-ngram-simple-size-n 3 --spec-ngram-simple-size-m 8` | no | no |

So the whole acceptance dispatch - not just my chosen branch - is unreached: the speculative
verification block above it is not executed for these requests, and the earlier `FG_SP_PREFIX calls=1`
came from a different route again.

## What this means for P6-11

Three decisions are gated synthetically and one (`n_max`) is verified live; the acceptance decisions
cannot be measured live until this block runs. Finding out why it does not run is a **control-flow
investigation in the server**, and it is now the single blocker for every remaining live number in
P6-11. Further flag guessing is not the way: the next step is reading where `slot.spec_draft` is filled
and under which conditions the verify block is reached.

## Recorded so it cannot be mistaken for success

The door, the host hook and the counters are all in the tree and compiled. None of them has produced a
single live number. This card exists so that the presence of the code is not read as evidence.
