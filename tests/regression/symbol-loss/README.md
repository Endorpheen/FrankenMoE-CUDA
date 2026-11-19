# symbol-loss

**Requirement:** rule 33 - one cumulative build, nothing accepted disappears from it.
**Produced by:** `tools/check-no-symbol-loss.py`; permanent since 2026-11-19.
**Tier:** fast - reads two libraries, under a second.

## What it proves

No exported symbol and no diagnostic string of the previous accepted build is missing from the
current one. A maintained list of components is a promise; a symbol diff is a fact about two
artefacts, and it catches the case the list cannot: a component present by name but rebuilt from a
different source.

Argument, `APEX_SERVER_DIR`-free by design: the current library is the first argument, otherwise
the newest release set or checkpoint under `builds/`. Exit 77 when there is nothing to compare.

## Run

```
bash tests/regression/symbol-loss/run.sh                # newest set
bash tests/regression/symbol-loss/run.sh <libllama.so.0>
```

Expected: `check-no-symbol-loss: ok, no exported symbol disappeared`, exit 0.
