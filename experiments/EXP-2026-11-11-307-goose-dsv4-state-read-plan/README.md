# EXP307: DSV4 compressed-state restore plan in Goose

`llama_dsv4_comp_state::state_read` — the header, the destination stream range,
and the per-layer call order of the compressed-state restore. The tensor records
themselves are EXP306. P5-03.

## Verdict

**GATED, NOT INTEGRATED** — the decision sequence is exact; the source tree is
read-only. No performance claim.

## Scope transferred

The restore reads a six-field header and validates it in the native order:
version, then ratio/state size/embedding size, then layer count. It then resolves
the destination stream range (a single sequence needs `ns == 1`; a full restore
needs `ns == n_stream`; a sequence id outside the stream range is rejected), and
for each layer checks the layer id and performs two tensor-stream reads, kv then
score.

The rejection codes are part of the contract: 1 version, 2 metadata, 3 layer
count, 4 destination range, 5 layer id — native throws distinct messages, and the
FFI form is a distinct negative number. The gate compares the code, and on success
the number of tensor reads, which is what makes a skipped or duplicated read
visible.

## Gate

| check | result |
|---|---|
| 5 000 header/range/layer combinations with one corruption each | 5 000 checks / 0 failures |

Corruptions cover both directions where the predicate is an inequality: version
zero and version three, a layer count above *and* below the truth, a layer id
above and below its index.

## Mutations

| mutation | failures |
|---|---:|
| version zero is accepted | 318 |
| embedding size is not compared | 874 |
| smaller layer counts pass | 733 |
| destination accepts a stream count below one | 339 |
| destination accepts the first id past the range | 18 |
| layer ids below the expected one pass | 21 |
| each layer performs one read | 129 |
| full-state restore accepts a partial stream count | 48 |

All eight are caught.

One of them was invisible on the first run: the fixture only ever raised the layer
count, so `!=` weakened to `>` still rejected everything the gate produced. That is
the second time a one-directional corruption hid half of a predicate (EXP306's
metadata check was the first). **Every inequality rejection needs a probe on both
sides.**

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
