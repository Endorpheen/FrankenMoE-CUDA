# EXP382: recurrent state I/O plan is live (APEX.RECURRENT_MEMORY remainder)

## Verdict

**LIVE on the reconstructed tree**, next to the accepted `FG232R`.

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | a layer with no R tensor writes nothing | `llama_memory_recurrent::state_write_data` |
| 1 | the S pass covers exactly the layers that own an S tensor | same |

Ordering itself is unchanged and now explicit in the module's comment: every R tensor of every layer
first, then every S tensor — the stream ordering the format requires.

## Gate

| check | result |
|---|---|
| 34 draws over layers -5..5, including a cross-check that the two plans disagree on every layer | 34 checks / 0 failures |
| mutations | **4/4 caught** |

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both, probabilities equal |
| restore | succeeds |
| `FG382S_RECURRENT_IO_PLAN` | **live, 2 calls** (the R pass and the S pass) |
| `FG232R` (accepted recurrent route) | live |
| `FG381S` / `FG373S` / `FG345W` / `FG241U` | live |

## Honest boundary

The payload bytes still move inside llama: the bridge carries scalars and flat arrays only, so tensor
ownership cannot cross by design. What is now Goose-owned on the used path is the plan — which layers
are written, in which pass, and which ranges (already `FG232R`). The tensor *contents* remain llama's,
and the format still interleaves metadata with payload, which is the same wall recorded in EXP366.
