# EXP-2026-11-19-1319: the end-of-generation door agrees on all 248 320 tokens

## The measurement

`bt_is_eog` is a membership test over the end-of-generation set, so the whole vocabulary can be compared on the first
call. That is what the hook does, behind `FG_TK_DETOK_DIAG`:

```
FG_TK_EOG_TOTAL tokens=248320 eog=5 disagreements=0
```

Every token of the model against a five-token EOG set, through the shipped library, with no disagreement.

## How this attempt differed from the one that failed

The previous attempt moved an existing helper and cut a brace, producing errors against unrelated functions
(EXP-2026-11-19-1318). This one is two pure insertions: a separate namespace next to the existing hook helpers, and
the sweep inside `is_eog`. Nothing was moved, and the build was run after each of the two edits rather than after
all of them.

The intermediate errors were still mine - a member name that does not exist (`n_tokens` for `id_to_token.size()`) and
a helper declared on a class whose declaration is not visible where I put it - but each was a compile error with a
clear message, which is what a single-point edit buys.

## Acceptance with the sweep compiled in and its flag off

```
pre-flight   11 of 11 components
decode       62.37 / 63.16 / 63.11     median 63.11
prefill      2697.8 / 2946.3 / 2945.9  median 2945.9
hash         c2ff42cc1219
```

## What this establishes

The ported end-of-generation decision matches the native one for every token of this model, measured by the shipped
library.

## What it does not establish

As with the detokenizer: the runtime still takes the native decision, and the door is exercised only under the
diagnostic flag. The two remaining detokenizer doors - `bt_detokenize` over sequences and the special-order
helper - are still unexercised, and they are the next items rather than completed ones.
