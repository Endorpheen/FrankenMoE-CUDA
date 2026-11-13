# EXP351: Simple and sequential batch splits in Goose

`llama_batch_allocr::split_simple` and `split_seq` (P6-07).

## Verdict

**GATED, NOT INTEGRATED** — both walks, their size-test placement and the `used` state are exact;
the source tree is read-only.

## Scope transferred

Both splits begin by locating the first unused token; finding none means an empty ubatch.

**simple**: take that token and every following token in order, marking each used, until the list is
exhausted or the ubatch reaches its size. The size test runs **after** the token was added, so a full
ubatch is exactly `n_ubatch`, and the walk starts at the first free index — a gap in the middle does
not stop it.

**sequential**: a ubatch holds tokens of one sequence set and its subsets. After each token the scan
advances past used tokens and past tokens whose set is **not** a subset (`(cur & s) != s`), then
adopts the set of the token it landed on. The size test runs **before** that scan, so a reached limit
stops without looking ahead.

The two also differ in what they report as the number of sequences: simple reports one per token,
sequential reports a single sequence.

## Gate

| check | result |
|---|---|
| 6 000 draws over 1–14 tokens, both splits, 1–4 size limit | 75 280 checks / 0 failures |

The gate compares the record stream, the returned count **and** the final `used` array — the pool
state is what the next split reads.

## Mutations

| mutation | failures |
|---|---:|
| used tokens are taken again | 7 660 |
| the simple split stops one token early | 6 842 |
| the simple split runs to the end regardless of size | 15 156 |
| the sequential split accepts non-subsets | 9 582 |
| the sequential split keeps the first set after moving | 1 663 |
| the sequential split ignores the size limit | 2 217 |
| an exhausted batch reports a token | 207 |

All seven are caught. The narrowest (207) is the exhausted-batch case: it only shows when every
token is already used, which the generator reaches rarely but deliberately.

## Files

- `source/host.goose` — both walks.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
