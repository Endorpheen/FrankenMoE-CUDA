# accepted-components

**Requirement:** the guard for every accepted stage (rule 31) - no requirement id of its own.
**Produced by:** `tools/check-accepted-components.py`, the release pre-flight, made permanent.
**Tier:** fast - seconds, no model, no GPU.

## What it proves

The shipped library still carries every component that was accepted earlier: the host
registration, the Goose sampler chain, the Q4_K kernel and its shim marker, the grammar and
speculative doors, the dry-limit pair, the bulk and SLRU bridges, the GGUF reader call path, the
bench/fit/CUDA/HF decision doors and the expert cache. A component that disappears in a rebuild
fails downstream only much later, with a fallback its caller cannot distinguish from a decision.

This is the check that caught the cumulative candidate compiled from the wrong `llama-model.cpp`
in EXP-2026-11-19-1298; it is the reason that comparison is not repeated.

## Run

```
bash tests/regression/accepted-components/run.sh            # builds/flint-acc/bin
bash tests/regression/accepted-components/run.sh <dir>      # a release set or a checkpoint
```

Expected: `check-accepted-components: ok, every accepted component is in the shipped library`,
exit 0. Exit 77 when the directory has no `libllama`. The log records the directory and the source
revision.
