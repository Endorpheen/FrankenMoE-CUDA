# EXP-2026-11-19-1374: the ported encoder compared with the native one, and it differs

## Why this comparison had to happen before wiring

P7-06's plan opened with it: the ported BPE encoder exists as a standalone program (`frankenmoe-tokenize`), it has never
been compared with the shipped tokenizer on the model that matters, and the encode decision in the runtime is still
native. A wrong encoder changes every downstream answer while looking plausible, so the comparison comes first.

## The comparison

The native reference is a harness calling `llama_tokenize` with the same flags the CLI uses (specials added and
parsed). Both read the same bytes: the CLI reads all of stdin as one text, so the reference had to do the same, which
took a second mode in the harness and was worth it - comparing line-by-line against a whole-text result compares
different inputs.

Ten prompts as one text: ASCII prose, Python, Cyrillic, Japanese, a long rare word, doubled spaces, a special-token
marker, punctuation, numbers.

```
native 114 ids, goose 118 ids
first difference at 68   native[68]=248045   goose[68]=27
native context [293, 198, 248045, 8678, 198]
goose  context [293, 198, 27, 91, 316]
```

The native side produces **one** token where the port produces **three**, and from there the streams never resynchronise:
45 of the remaining positions differ.

## What this means and does not mean

The shipped runtime is not affected: its encode decision is still native, which is exactly why the plan put the
comparison before the wiring. What it does mean is that the port is **not ready** to serve, and the number that P7-06
was missing now exists - and it is not zero.

## Where the divergence falls

Between the newline tokens at positions 67 and 70, i.e. inside the region the doubled-space and long-word prompts
occupy. Naming the exact input is the next step: bisect by prompt, one CLI run per prompt, since the CLI loads the
vocabulary per invocation and takes about forty seconds each.

## Not done here

No change to any ported source, and no wiring attempt. A difference recorded before the fix is what keeps the fix
honest.
