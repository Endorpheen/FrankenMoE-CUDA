# P7-06 and P7-07 are stuck on a carrier choice, not on a missing run

Measured 2026-11-20, after the grammar witness showed that a working port can be inert simply because the
compiled carrier does not call it.

## The fact

| copy of `llama-vocab.cpp` | references to Goose/port doors |
|---|---|
| `runtime/reconstructed/src/llama-vocab.cpp` - **the one `build.py` compiles** | **0** |
| `vendor/flint/src/llama-vocab.cpp` | **112** |

`build.py` compiles `SRC/<name>`, and `SRC` is `runtime/reconstructed/src`. So the tokenizer doors
(`bt_bpe`, `bt_token_to_piece`, the `fg_*` family) are **linked** - they are symbols in the artefact, which
is why `unit-carry` counts them - but the compiled carrier never calls them. The shipped tokenizer and
detokenizer are the native ones.

That is the same shape as everything else found today: a module that is present is not a module that runs,
and a port that was proven correct against 248 320 tokens in an experiment says nothing about the artefact
being shipped if that artefact compiles the other copy.

## Why the earlier evidence did not catch it

`experiments/EXP-...-tokenizer` compared the port against the native path in a build that compiled the
**vendor** copy. The comparison was real; the artefact it described was not the one that ships. That is the
exact confusion rule 33 names, and it is now recorded for this pair of requirements.

## What would close them

One decision, then one gate:

1. decide which carrier copy the shipped build compiles. If the vendor copy is the right one - it carries
   the wiring - then `build.py`'s carrier list is what must change, and the staged copy's purpose (host
   wiring that the vendor source does not have) has to be re-examined for this file;
2. rebuild and prove the doors are called, not merely linked: a marker on first use in `bt_bpe` and
   `bt_token_to_piece`, in the same way `FG171_TOP_P` proves the sampler and `FG171_GRAMMAR_SIZES` proves
   the grammar hook;
3. then the acceptance run, and only then deletion of the native bodies.

None of this is a measurement problem. It is a build-composition problem, and it has been invisible because
both copies exist in the tree and both look plausible.
