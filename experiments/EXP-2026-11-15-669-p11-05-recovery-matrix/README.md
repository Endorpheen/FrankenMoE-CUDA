# EXP-2026-11-15-669 — P11-05: the five failure-recovery checks pass, and rule 12 becomes executable

Base: `35b96308`. Build and gate only. No model, no server, no GPU.

## 1. Two outcomes

**P11-05 failure matrix: 5/5.** The five properties the requirement lists are
now executed against the staged tree instead of being described:

| # | property | result |
|---|---|---|
| 1 | a missing generated unit fails the ON link, and names the file | ok |
| 2 | a wrong stage-flag value is rejected at configure | ok, `must be ON or OFF` |
| 3 | an unbound IQ provider aborts instead of reading NULL | ok, `unbound_aborts=1`, 15 360 comparisons, 0 failures |
| 4 | a partially switched build links, enabled gate uses Goose, disabled one stays native | ok, `mode=goose` / `mode=native` |
| 5 | rollback recovers after the failed ON build, with zero `goose_init` | ok |

**Rule 12 is now a rule and a mechanism.** `AGENTS.md` gained rule 12
(selective re-run with reuse of unchanged results) and `CLAUDE.md` is
synchronised in the same commit, per rule 11.

## 2. Why check 4 failed first, and what that says

The first run of check 4 failed:

```
FG629_SMOKE mode=goose ... | FGQ_SMOKE_RESULT mode=goose ...
```

It configured one stage ON expecting the rest to stay native, but EXP666 made
every stage default ON. The test was written against the old default, not against
the project. The fix names the nine stages the partial build must turn off.

This is worth recording because the same class of error is what rule 12 is meant
to catch cheaply: the failure was in check 4 alone, the other four had already
passed, and re-running only check 4 after the fix is what the rule prescribes.

## 3. Selective re-run, implemented

`tools/check-failure-recovery.sh` takes an optional list of check numbers as its
last argument:

```
bash tools/check-failure-recovery.sh builds/stage-m1 build-on build-off build-part 4
  [4] partially switched build links and runs the native path
    ok: ...
    skipped (not selected): check 1/2/3/5

failure recovery: ok (1 checks, 4 skipped)
```

A check that did not run is reported as **skipped**, never as passing, so a
partial run cannot be mistaken for a full one. The summary line always states
the skipped count.

The same mechanism already exists at the level above: `tools/check.sh` stamps
each level with a content fingerprint of its inputs and reuses the previous
result while the fingerprint is unchanged (`--why` prints the reason, `--force`
overrides). Together these cover the rule at both granularities: whole levels and
individual checks inside a level.

## 4. What still runs in full, and when

Rule 12 does not weaken acceptance:

- a **final acceptance** or a **control point** runs the whole set;
- a change to the *inputs* of a skipped check invalidates it automatically,
  because the fingerprint covers the tracked file hashes;
- the FULL level exists for the clean-build, determinism and recovery matrix and
  is expected before a release, not after every edit.

## 5. Files

- `AGENTS.md`, `CLAUDE.md` — rule 12, and the report template now says twelve
  rules.
- `tools/check-failure-recovery.sh` — selective re-run, with the skipped count
  in the summary.
- `tools/check.sh` — unchanged in this step; its stamping is what the rule
  describes for whole levels.
