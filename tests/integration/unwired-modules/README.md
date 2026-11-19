# unwired-modules

**Requirement:** P11-01 and P11-04 - a module only counts when something composes it.
**Produced by:** `tools/check-unwired-modules.py`; permanent since 2026-11-19.
**Tier:** integration - reads the tree, no build, no model, under a second.

## What it proves

Every Goose module that exports a decision is inside a composed unit, or its path says why not
(archive, rejected variant, staged copy, fixture, test). Twice in a row a module owned a decision,
nothing contained it, and a host had grown its own copy of the rule instead (EXP995
shutdown-policy, EXP996 limits/error_404) - both found by accident. This is that accident, made
into a list.

The distinction it keeps matters for the migration count: a module that compiles but is in no unit
is not a ported decision, and the ledger must not count it as one.

## Run

```
bash tests/integration/unwired-modules/run.sh
```

Expected: `PASS no unwired module outside the recorded baseline`, exit 0.
