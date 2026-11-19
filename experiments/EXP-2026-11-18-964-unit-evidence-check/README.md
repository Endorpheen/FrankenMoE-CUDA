# EXP-2026-11-18-964: a check for whether unit modules have gates

Status: tool accepted, and it found three real gaps on its first honest run.

## Hypothesis fixed before changes

EXP962 and EXP963 were the same finding found twice by hand: a module gated in an
experiment but absent from the canonical unit. Both times the check was a manual
grep over five names. That does not scale to 140 modules, and the reverse
question - "which modules in the unit have no gate at all?" - had no answer.

`tools/decision-coverage.py` measures something else: whether C++ decision
predicates route through Goose. This needed a different instrument.

## What was done

`tools/check-unit-evidence.py` reads the module list from
`tools/build-main-runtime.sh` itself - never typed - and, for each module, asks
which tracked files name it. Two rounds of self-correction were needed:

1. the first run reported **140/140 evidenced**, which was obviously wrong. Cause:
   `tools/build-main-runtime.sh` lists every module, so it "evidences" every
   module. `SELF_LISTING` now excludes files that name the unit by construction;
2. the second run still reported 140/140. Cause: a bare README mention counted.
   A module whose only witness is prose is exactly as unverified as one with no
   file - so evidence is now classified **strong** (a test, gate, report or
   executable check names it) against **weak** (only prose does).

`--module` explains one entry, `--strict` exits 1 when any module is prose-only.

## Result

| check | value |
| --- | --- |
| modules in the unit | 140 |
| named by a test, gate or report | **137** |
| named only by prose | **3** |
| runtime | 10 s over 2663 candidate files |

The three, each verified by hand afterwards with a search for any `.py`/`.cpp`
naming it - none exists:

    goose/json/ast-helpers.goose
    goose/json/peg-invalid.goose
    goose/misc/edge0-pack-index.goose

Their only witnesses are `EXP-2026-11-15-658/evidence/modules-base.txt` and
`docs/goose-migration/BUILD-SET.md`, both of which list the unit rather than test
it. So the tool's first honest report is that three modules travel with the
runtime and nothing checks them.

## Not claimed

This does not fix the three; it makes them visible, which is what a check is for.
Whether each deserves a gate or is trivially correct is a separate judgement and
is not asserted here.

The tool measures naming, not execution: a module named in a gate is not proven to
run in it. That distinction is the same one the project already draws between
"linked" and "live", and the tool is deliberately named for what it measures.

## Reproduction

    python3 tools/check-unit-evidence.py
    python3 tools/check-unit-evidence.py --strict      # exit 1 today
    python3 tools/check-unit-evidence.py --module goose/misc/sampler-dry.goose
    python3 tools/check-unit-evidence.py --module goose/json/peg-invalid.goose
