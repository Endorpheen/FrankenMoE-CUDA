# EXP361: split_equal selection live on the accepted line

The second Goose block proved to execute on the real Apex path, and the first whose route is entered
on every batch.

## Verdict

**LIVE, EXACT** — the participating sequence sets and their starting cursors are chosen by Goose, and
the answer is unchanged.

## What runs

- Stage: flint objects with `llama-batch.cpp.o` replaced by the integrated copy and the EXP360 bridge
  linked in; ABI complete; all three library names substituted.
- The bridge passes the used flags, per-row set masks and first sequence ids, the ubatch limit and the
  mask→rows map; it returns the accepted rows and the cursors. `cur_seq_set` is rebuilt from
  `seq_set[row]`, so the type never crosses the boundary.

## Bridge gate

| check | result |
|---|---|
| 4 000 draws, rows + cursors + n_seqs | 18 453 checks / 0 failures |

## Run (32K/128, one run)

| check | result |
|---|---|
| status | RESPONSE_RECEIVED, 128 tokens |
| tokens / probabilities / content hash | equal to the control (`e68df05b…`) |
| `FG350S_SPLIT_SELECT` | **live** — first call, `calls=100`, `calls=200` |
| prefill / decode | 1599.2 / 25.22 tok/s |

Decode clears its floor even with one module in the stage; prefill does not, and that is expected:
the floor describes the full accepted runtime.

## Note on the rest of the splitter

The keep-tail block and `split_simple` remain dormant on this profile (EXP354/EXP355), so wiring them
would add no live witness. What Apex actually runs from `llama-batch.cpp` is this selection and
`ubatch_add` (EXP359) — both are now live.
