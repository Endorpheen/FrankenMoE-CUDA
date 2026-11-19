# EXP-2026-11-18-965: the evidence check was wrong about three modules

Status: tool corrected; the previous experiment's three findings were false.

## Correction first

EXP964 reported three modules as having no gate:

    goose/json/ast-helpers.goose
    goose/json/peg-invalid.goose
    goose/misc/edge0-pack-index.goose

**That was wrong.** Each has a real gate that calls its exported functions without
ever writing the module's path:

| module | gate |
| --- | --- |
| `edge0-pack-index` | `EXP-2026-11-13-511-edge0-pack-index/tests/pack_fixture.cpp` |
| `json/peg-invalid` | `EXP-2026-11-13-485-p7-09-peg-invalid/tests/peg_inv_fixture.cpp` |
| `json/ast-helpers` | `EXP-2026-11-13-497-p7-09-ast-helpers/tests/ast_fixture.cpp` |

I found this by looking at the module's exports and searching for calls, which is
what the tool should have done. The lesson is the one this series keeps repeating:
a checker that searches for one shape of evidence reports a false absence for every
other shape, and a false absence is worse than no check, because it looks like a
finding.

## What was done

`tools/check-unit-evidence.py` now builds a **reverse call index**: one pass over
the corpus mapping every called identifier to the files that call it, then each
module's exports are looked up in it.

The naive version - per module, scan every file with a regex - took minutes and hit
a 180 s timeout. Building the index once and looking names up took 13 s, which is
the difference between a check nobody runs and one that is cheap enough to keep.

## Result

| check | EXP964 (wrong) | now |
| --- | --- | --- |
| modules with a gate | 137 | **140** |
| prose-only | 3 | **0** |
| runtime | 10 s | 13 s |
| evidence kinds searched | path, basename, prose | **plus export calls** |

The three modules now resolve to `strong` with their gate listed, which was
verified by running `--module` on each and reading the answer against the file.

## Not claimed

The tool still measures **naming and calls, not execution**: a gate that mentions
`index_stats` is not proven to run it successfully, and a module listed in an
evidence file is not proven live. That distinction is why the tool is not called a
coverage checker.

`--strict` now exits 0, which is the honest result rather than the target: the
previous exit 1 came from the tool's blind spot, not from three missing gates.

## Reproduction

    python3 tools/check-unit-evidence.py
    python3 tools/check-unit-evidence.py --module goose/misc/edge0-pack-index.goose
    python3 tools/check-unit-evidence.py --strict; echo $?    # 0
    # the gate that falsifies EXP964's claim:
    grep -n 'index_stats' \
        experiments/EXP-2026-11-13-511-edge0-pack-index/tests/pack_fixture.cpp
