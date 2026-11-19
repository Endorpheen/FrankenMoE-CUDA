# C1-06: protecting Apex while extending Qwen - what exists, and the one gate that is owed

Checked 2026-11-19 on `main`.

## The mechanical protections exist and run

| guard | what it stops | tier |
|---|---|---|
| `tests/regression/accepted-components` | a rebuild that silently loses an accepted Goose component | fast, seconds |
| `tests/regression/symbol-loss` | a build where something the previous accepted artefact exported has disappeared | fast, seconds |
| `tests/integration/unwired-modules` | a module that claims a decision and is in no unit | integration, sub-second |
| `tests/integration/carrier-symbols` | a link that cannot resolve a Goose symbol | integration, 0.2 s |
| `tools/check-cumulative-baseline.py` | an acceptance measurement taken on a build missing an accepted primitive | pre-measurement |
| CI Goose job | a unit that stops composing, typechecking or compiling | per push, ~4 min |

The last of these already did its job for the Qwen work: run 44597680081 reports success for the
commit that put `qwen4exp`, `q4m`, `q4t` and `q4e` into the unit.

## The gate that is owed, and why it is not a violation

`tools/apex-test.sh` has not run since that commit. The last Apex run is
`rel-exp1476-20261119-212613`, made before the Qwen modules joined the unit.

That does not put an unverified build into production, which is the rule that matters: the shipped
artefact is `v0.10.0-exp1476`, built from the accepted checkpoint, and no later build has been
promoted or released. What is owed is the measurement that would qualify a build carrying the Qwen
modules - and that needs a rebuild plus a model run, in that order:

```
bash tools/build-independent-apex.sh /home/end0/AI/LlamaServer/<new-dir>
python3 tools/check-cumulative-baseline.py /home/end0/AI/LlamaServer/<new-dir>/run
APEX_SERVER_DIR=/home/end0/AI/LlamaServer/<new-dir>/run bash tools/apex-test.sh qwen-unit 512 4096 4096 4992
```

## What the comparison must report

Against the accepted checkpoint (decode 62.86 / prefill 2940.9 median, hash `c2ff42cc1219`, RAM
20 848 620 kB, VRAM 9511 MiB): decode, prefill, RAM, VRAM and the answer hash, as four numbers
plus correctness - rule 19 and rule 20 both apply, and a single response is not aggregate
throughput.

## What is not claimed

That Apex still behaves the same with the Qwen modules in the unit. Nothing has been measured since
they joined, and this document records that rather than assuming it.
