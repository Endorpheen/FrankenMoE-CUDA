# EXP-2026-11-19-1353: the sequence detokeniser is a sample now, not a single case

## The harness

`harness.c` loads the model once (CPU, mmap - the vocabulary does not need the GPU) and calls `llama_detokenize`
directly over:

- **seven lengths**: 1, 2, 5, 17, 64, 257, 1025 tokens;
- **three shapes**: low ids (which include specials), the tail of the vocabulary, and a spread across all 248 320;
- **every flag pair**: `remove_special` x `unparse_special`, both values.

84 calls, one model load. The comparison itself runs inside the library at the public entry
(`FG_TK_DETOK_DIAG`), so the harness only drives calls and the library reports.

## Result

```
FG_TK_SEQ_PROGRESS calls=81 disagreements=0
diffs=0
```

No unexpected negative return either - the door and the native agree on the short-buffer case as well, at least for
the buffer the harness provides.

## What changed from the previous card

EXP-2026-11-19-1352 had one comparison: a 32 768-token prompt, equal. That was the largest single case and **not a
sample**. Now the door is exercised across lengths, shapes and flags, which is what every other door in this report
has and this one lacked.

## What it still does not establish

The conversion. The door is measured, not used: `llama_detokenize` still returns the native bytes. Converting it is a
separate change, and the case to handle is the same one the buffer form had - a short caller buffer, where the native
returns `-needed`.

## Not in the sweep

The harness needs a model load (~4 minutes), so it is run deliberately rather than on every sweep. `verify.sh` is
self-contained when it is run.
