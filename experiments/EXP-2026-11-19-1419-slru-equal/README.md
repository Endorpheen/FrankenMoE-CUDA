# EXP-2026-11-19-1419: the cache's replacement policy agrees on 61 441 real decisions

## How it got here

EXP-1418 recorded the composition as broken: the module needed `slru_ld_i32` and `slru_st_i32`, and inventing them would
have been a second adapter for symbols that already had one. The adapter exists - `integration/goose-standalone/slru-runtime-shim.c`,
which is the host FFI of exactly this module - so it was **reused verbatim**: the build script copies it into the unit and
the library compiles it beside the chain shim. No new adapter, no duplicated logic.

## The comparison, on the accepted profile

`plan()` snapshots its state, lets the native decision run, then runs the ported door on the snapshot and compares the
fills, the hit count **and their order**:

```
FG_SLRU_COMPARE calls=61441 disagreements=0
decode 62.26 / 62.51 / 62.60   prefill 2697.6 / 2952.8 / 2953.5   hash c2ff42cc1219
```

Sixty-one thousand four hundred and forty-one decisions - every MoE layer of every decode step of the run - and the ported
policy matches the native one exactly, including the order in which experts are filled. The order is the part a
set-comparison would miss and the part that decides whether uploads merge.

## What this means for P9-03

The pure decision of the expert cache is now **proven equal on the real workload**, which is the first of the item's 25
call sites to reach that standard. It is not yet **served**: the native answer is still the one used, and the comparison
runs under a flag.

## What is still open

- serving the door's answer, which needs the equality above plus an acceptance triple without the flag;
- the other call sites the item lists - placement and row streaming - which are not in this file;
- and the observation from EXP-1417 stands: `get_slot_map` is called with one token per decode step, and the policy runs
  inside `copy()`, so the decision is made where the uploads happen.

## Reproduce

```
FG_SLRU_DIAG=1 APEX_SERVER_DIR=$PWD/builds/flint-acc/bin bash tools/apex-test.sh slru-real 512 4096 4096 4992
grep FG_SLRU_COMPARE results/apex-runs/slru-real-*/server.log | tail -1
```
