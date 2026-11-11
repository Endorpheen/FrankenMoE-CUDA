# EXP399: disable contract for every route added by the three items

## Verdict

**CONTRACT HOLDS.**

## Setup

One run with all seven new switches at 0:

    GGML_GOOSE_KV_FIND_SLOT, GGML_GOOSE_KV_META_PLAN, GGML_GOOSE_KV_LAYOUT_SPAN, GGML_GOOSE_KV_READ_META,
    GGML_GOOSE_RECURRENT_PAYLOAD_SPAN, GGML_GOOSE_HYBRID_DELEGATION, GGML_GOOSE_BATCH_CONTAINER_LAYOUT

The accepted runtime's own switches are left alone.

## Result

| check | result |
|---|---|
| completion | `e68df05b…`, equal to the reference |
| all probabilities | equal |
| restore | exact |
| slot file | 387 989 920 bytes, unchanged |
| `FG392S` `FG393S` `FG394S` `FG395S` `FG396S` `FG397S` `FG398S` | **all 0** |
| `FG241U` `FG232R` (accepted) | live |

So every route this line added can be switched off, the answer never depends on them, and none of the
switches disturbs an accepted route. This is what a rollback would rest on if primary review rejects any
of the three items.
