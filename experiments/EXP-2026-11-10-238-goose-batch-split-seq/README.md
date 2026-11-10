# EXP238: `split_seq` selection in Goose — ported, gated, NOT exercised by the Apex profile

## What it preserves

`llama_batch_allocr::split_seq` (llama-batch.cpp:790-829) walks from the first unused token and appends
further tokens **only while their sequence set is a SUBSET of the set being carved out**.

That subset test is the whole point. Written the other way round — "any overlap" instead of "subset" — it
accepts a token belonging to a sequence outside the ubatch. That corrupts the batch split without changing
any token's value, so token-equality cannot see it.

`seq_set_t` is `std::bitset<LLAMA_MAX_SEQ>`; the module receives it as two 64-bit words, enough for
LLAMA_MAX_SEQ up to 128, which keeps the subset test a pair of bitwise ops.

## Gate

    61059 checks, 0 failures

6 named cases plus 20000 random. The named ones pin: all-used → 0 selections, `n_ubatch == 1` → a single
token, no subset match → single token, a subset chain that continues, and the trap case where a **superset**
must not match.

Mutation caught: subset test replaced by "any overlap" — **16708 failures**.

## Not exercised

    FG237S_BATCH_SPLIT     calls=146
    FG238Q_BATCH_SPLIT_SEQ calls=0
    prefill 2479.0  decode 24.76
    tokens True  probabilities True

Zero calls: `split_seq` is the `embd_all` path, and the fixed 32K/128 Apex profile does not take it. Same
category as EXP218, EXP230, EXP231 — correct, gated, unreachable here.

## The regression I caused and caught

The first build of this stage compiled from the **pristine** `llama-batch.cpp`, which silently dropped
EXP237's route. `check_witnesses.py --baseline exp237` reported `MISSING - FG237S / verdict: REGRESSION`, and
`nm -u` on the object showed only `fg238q_`. Rebuilt on EXP237's **cumulative** file; the object now carries
both, and the run reports `verdict: OK`.

This is the third time in this project that the same mistake has appeared — pristine-source rebuild losing the
previous stage — and the first time a witness check caught it before the commit. The check is only useful
with `--baseline` and only if it is run before committing rather than after.
