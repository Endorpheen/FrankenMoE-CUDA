# EXP-2026-11-19-1369: every exported door is traceable to a record

## Why the audit exists

`sp_reject_accept` was written, wired into the native route, exported - and had **no evidence of any kind**. It was
found by asking a different question, not by any check. That is the failure this audit exists to prevent.

## What it reports

The tool lists every exported Goose symbol in the shipped library and asks whether a card or a document mentions it, or
mentions its **feature**:

```
exported doors 49: named 35, feature-covered 14, no record 0
```

The two columns are kept apart on purpose. `fg_sampler_top_p_call` is never named in the report, because the report
describes what was measured - top-p - rather than the wrapper that was called. Counting that as "no evidence" would be
wrong; counting it as "named" would be wrong in the other direction.

## What the audit says today

- 35 doors are named directly in a card;
- 14 are covered only under their feature name (`typical`, `xtc`, `top-p`, `dist`, `mirostat`, `adaptive-p`,
  `penalties`, `temp_ext`, `grammar`, `synthetic accept`, `rejection accept`);
- **0 have no record.**

## What it does not prove

That a door is *used*. A record of a measurement is not a record of a call site, and the second question is the one the
migration's coverage rule asks. The audit answers only "can this port be found in the evidence", which is the question
that failed for `sp_reject_accept`.

The gap branch is unexercised because there are no gaps; it is there for the next port that arrives without a card.

## Usage

```
python3 tools/audit-door-evidence.py                 # report
python3 tools/audit-door-evidence.py --fail-on-gap   # exit 1 when any door has no record
```
