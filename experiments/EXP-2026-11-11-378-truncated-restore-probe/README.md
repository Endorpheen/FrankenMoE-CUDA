# EXP378: truncated-restore probe -- the recurrent half cannot be failed alone

## Question

EXP375's failed-read branch (clear the attention half when the recurrent read throws) needs a restore
that passes the attention half and fails in the recurrent half. Is such a truncation reachable?

## Method

`run_trunc.py` saves the slot, truncates the file to a chosen length, then restores. Only the length
changes between runs.

| truncated to | result | witnesses |
|---:|---|---|
| 360 000 000 | restore **succeeds**, probabilities equal | `FG373S` live, `FG375S` 0 |
| 387 900 000 | restore **succeeds**, probabilities equal | `FG373S` live, `FG375S` 0 |
| 1 000 000 | restore fails: `failed to restore kv cache` | `FG373S` live, `FG375S` 0 |

## Conclusion

Truncation either passes entirely or fails inside the **attention** half. The recurrent section is
written with an empty cell set on this profile (`seq_id = -1`), so there are no recurrent payload bytes
to cut; the last 89 920 bytes are never read.

So the branch is unreachable by truncation, and EXP375 keeps its `NOT VALIDATED` verdict. This is a
measured dead end, not an untried idea: the harness is kept so the question does not have to be
re-investigated.
