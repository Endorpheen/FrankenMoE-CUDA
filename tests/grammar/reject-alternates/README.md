# reject-alternates

**Requirement:** P6-10 (sampling, grammar, constrained generation).
**Produced by:** EXP-2026-11-19-1491, which moved here once its result was accepted.
**Tier:** fast - no model, no GPU, no Apex lock.

## What it proves

The port's candidate rejection agrees with the native automaton, candidate by candidate, on every
shape where the port used to disagree:

| case | shape | expected |
|------|-------|----------|
| `single-alternate` | `'{' REF(space) '"'`, `space ::= " "` | agrees, 5 pieces |
| `multi-alternate-empty` | `space ::= " " \| <empty>` - the EXP-1471 shape | agrees, 5 pieces |
| `two-alternates` | `space ::= "a" \| "b"` | agrees, 4 pieces |
| `nested-reference` | `REF(REF(" " \| <empty>))` | agrees, 4 pieces |
| `char-not` | `[^a]` | agrees, 4 pieces |
| `partial-lead` | an incomplete UTF-8 lead byte next to `'a'` | agrees, 2 pieces |
| `deep-nesting` | five nested alternations, deeper than the pool | declines `-2` |

## Oracle

`fixture.cpp` includes `vendor/flint/src/llama-grammar.cpp`, so `llama_grammar_reject_candidates`
is the real native function, not a reimplementation. The port is reached through the header the
Goose compiler emits for the live `goose/sampling/sampler-chain-full.goose`.

Logging, abort and three vocabulary accessors are stubbed in the fixture: no reject path reaches
them, and linking the shipped library instead would put a second copy of the non-static grammar
symbols into the process.

## Run

```
bash tests/grammar/reject-alternates/run.sh
```

Expected: `gate: cases=7 fails=0`, exit 0. The build log is `build/run.log`; `gen.o` is reused
while the module source, the optimisation flag and the compiler are unchanged.

## What it does not cover

The real-vocabulary disagreement and decline rates, which need the model, the rebuilt
sampler-chain library and the Apex lock. Those runs are recorded in the experiment cards, not
here.
