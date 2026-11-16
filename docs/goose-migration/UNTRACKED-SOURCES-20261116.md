# Seven files the runtime is built from are not in the repository

Found 2026-11-16 while checking whether the tokenizer bridge was wired. It is a reproducibility problem of
the same kind as BRIDGE-BUILD-GAP, but larger: not one build step, seven source files.

## What is untracked

`runtime/reconstructed/build.py` compiles thirteen "changed" files. Six are tracked; **seven are not**:

| file | tracked | differs from flint source |
|---|---|---|
| llama-batch.cpp | yes | |
| llama-context.cpp | yes | |
| **llama-graph.cpp** | **no** | yes, 38 lines |
| **llama-hparams.cpp** | **no** | |
| **llama-kv-cache.cpp** | **no** | |
| llama-model.cpp | yes | |
| **llama-memory-recurrent.cpp** | **no** | |
| **llama-memory-hybrid.cpp** | **no** | |
| llama-model-loader.cpp | yes | |
| **llama-sampler.cpp** | **no** | yes |
| **llama-vocab.cpp** | **no** | **yes, 157 lines** |
| llama-moe-cache.cpp | yes | |
| **unicode.cpp** | **no** | yes |

They are not gitignored - `git check-ignore` returns nothing for them. They were simply never added.

## Why this is worse than it looks

The untracked diffs are **Goose integrations, not formatting**. From `llama-vocab.cpp` against flint:

```
// Goose tokenizer decisions (EXP202). GGML_GOOSE_TOKENIZER=0 restores every branch.
// Goose special-token partition decisions (EXP390). GGML_GOOSE_VOCAB_PARTITION=0 restores the native tests.
extern "C" int fg390s_enabled(void);
extern "C" int64_t fg390s_decide(int64_t op, int64_t a, int64_t b);
// Goose byte-fallback decisions (EXP402). GGML_GOOSE_BYTE_FALLBACK=0 restores the native path.
extern "C" int fg402s_enabled(void);
```

So the calls that make the tokenizer decisions reachable - the actual migration work, not just the modules -
live in a file that `git clone` would not restore. A fresh checkout would build a runtime with those calls
gone, and nothing in the repository would report the loss.

That is also why the S1.5 gate's `grep -rn FG_GOOSE_TOKENIZER` found only the gate: the runtime side of the
same flag is in the untracked file.

## What this explains

`migration-status.sh` counts these files' functions as "own C++ still needed" and reports them from the
working tree. That accounting is correct for the machine it runs on and unverifiable anywhere else.

## Proposed fix

1. Commit the seven files verbatim. They are source, not generated output - `check-generated-files.py`
   only rejects generated C over 1 MiB, and these are `.cpp`/`.h`.
2. Before committing, record for each what differs from its flint source, so the diff is reviewable as the
   set of Goose integrations it is. `llama-vocab.cpp`'s 157 lines are four bridge families (EXP202, EXP390,
   EXP402 and the partition), each already gated and each with its own experiment.
3. Add a check to the pre-commit hook: every file `build.py` compiles must be tracked. That is one
   `git ls-files` comparison and would have caught all seven.

## Not done here

Nothing was committed. Adding 5000 lines of source under a time-boxed session without per-file review of
the diffs is not a safe change, and the review is the point: the whole value is knowing what those 157
lines do.
