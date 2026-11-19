# carrier-symbols

**Requirement:** the guard for every accepted stage (rule 31/33) - the shipped link must resolve.
**Produced by:** the carrier divergence found on 2026-11-19 (P9-03 check).
**Tier:** integration - reads sources only, 0.2 s, no build, no model, no GPU.

## What it proves

Every Goose symbol that a compiled carrier references is defined by some compiled source. That is
the only question the link cares about, and it is answered before any compilation.

It exists because a file-level rule cannot be right here: the staged and vendor copies of
`llama-model-loader.cpp` are not interchangeable (the staged copy calls 51 doors but does not
define the `fg_port_*` family that vendor's `llama-impl.cpp` needs; the staged `llama-moe-cache.cpp`
calls `fg262a`/`fg263c`/`fg756s` while the vendor copy calls none). Picking either file wholesale
loses what the other has. `runtime/reconstructed/check_carriers.py` states the difference instead
of choosing, and `build.py` refuses to compile when it fails.

Current result: `200 source(s), 176 symbols defined, 176 referenced, PASS`.

## Run

```
bash tests/integration/carrier-symbols/run.sh
python3 runtime/reconstructed/check_carriers.py        # the same check, directly
```

Exit 0 when nothing is unresolved; 1 with the symbol list otherwise.
