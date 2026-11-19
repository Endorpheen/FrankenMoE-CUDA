# first-set

**Requirement:** P6-10 (sampling, grammar, constrained generation).
**Produced by:** the grammar work of EXP-2026-11-18-1110 and EXP-2026-11-19-1491; entered the
permanent tree on 2026-11-19 with the centralised test system.
**Tier:** fast - no model, no GPU, no Apex lock.

## What it proves

`fg_grammar_first_set` reports the allowed first codepoints at the current stacks as inclusive
ranges, and the sampler filter uses that to skip the full automaton. The test compares the port
against `llama_grammar_match_char` on 31 probe codepoints, so the union the port reports must be
exactly the set the native matcher allows across all stacks.

| case | shape | expected |
|------|-------|----------|
| `one-range` | `[a-c]` | agrees on all probes |
| `two-stacks` | `[a-c]` and `[0-9]` | agrees, the union of both |
| `char-any` | `LLAMA_GRETYPE_CHAR_ANY` | agrees, every probe allowed |
| `class-alt` | one class with a `CHAR_ALT` part, `'a' \| 'x'` | agrees |
| `char-not` | `[^a]` | declines |
| `rule-ref-top` | a rule reference at the top | declines |

A decline is the required answer for the last two: an inverse class is not a positive range and a
rule reference has to be expanded first. The port must say so rather than report a range that
would allow a token the grammar rejects.

## Oracle

`fixture.cpp` includes `vendor/flint/src/llama-grammar.cpp`; `llama_grammar_match_char` is the
native function called directly on the same rule elements the flat arrays describe. Logging, abort
and three vocabulary accessors are stubbed because no first-set path reaches them.

## Run

```
bash tests/grammar/first-set/run.sh
```

Expected: `gate: cases=6 fails=0`, exit 0. Log in `build/run.log`.

## Coverage note

`g_advance`, `g_match_char` and `g_match_partial` are not exported by the module, and exporting
them only for a test would change the artefact under test. They are exercised through the two
exported decisions: `reject-alternates` drives the descent and the partial-UTF-8 path, and this
test drives class matching and `match_end`. A direct gate for them stays in
`experiments/EXP-2026-11-18-1110-grammar-advance/tests/` as the record of that experiment.
