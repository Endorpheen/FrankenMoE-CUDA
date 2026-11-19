# set-self-contained

**Requirement:** rule 22 - an accepted set must be usable, not merely hash-matching.
**Tier:** fast - `readelf` and a directory listing.

## What it catches

`builds/checkpoint-20261119-EXP1476` holds 23 files while the release holds 24, and the missing one,
`libmoe_mmid_tiled.so`, is required by `libggml-cpu.so.0`. Every hash in that directory is correct and the
set still cannot load. This test reads `DT_NEEDED` from every ELF in a set and asks whether each name is
present in the same directory, so that gap is one command away instead of one accident away.

## Two false results it produced before it worked

1. It passed the checkpoint on its first run because `libssl.so.3` was flagged as missing from the release
   and I widened the system list - correctly, OpenSSL is a system library - but the widening also let
   `libm` match `libmoe_mmid_tiled.so`. The names now need a boundary after them.
2. The same run found `libssl.so.3` as a genuine release dependency that `F1-03-DEPENDENCIES.md` had
   missed, because that report read `ldd` of `libllama.so.0` only. Six ELFs in the set need OpenSSL;
   the report now says so.

## Outcome today

- release set: **ok**, every non-system dependency is present;
- checkpoint: **FAILS**, `libggml-cpu.so*` needs `libmoe_mmid_tiled.so`, which is not there.

The failure is the finding, not a defect of the test.

## Run

```
bash tests/regression/set-self-contained/run.sh              # newest release set
bash tests/regression/set-self-contained/run.sh <set-dir>
```
