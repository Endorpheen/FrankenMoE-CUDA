# EXP-2026-11-15-703 — the cache outlives the graph, and reuse from n_past

Base: `2038a6fc`. No model run, no GPU.

## 1. Result

```
FG_STATE_LIFE  tokens=6 rebuilt=6 lost=0 stray=0 PASS
FG_STATE_REUSE split_at=2 differs=0 distinguishable=1 PASS
```

Units 6 and 7 of the state ledger, closing it at 8 of 8.

## 2. Unit 6 — a buffer that survives a graph rebuild

Six tokens, each in its **own** ggml context which is freed immediately after the write. The
cache lives in a separate context allocated once. Read back afterwards through a seventh
fresh context, every position still holds its token: `lost=0`.

`stray=0` is the second half of that check and the one worth naming: positions 6 and 7 were
never written and must still be zero. A slot that wrote past its own row would corrupt a
neighbour, and a per-token "did my value survive" check would not notice - the neighbour's
value would still be there, just wrong.

The division of responsibility is deliberate. This gate owns the LIFETIME (one context per
cache, freed once); the offset arithmetic stays in `state_slot` in Goose. No layout
knowledge is duplicated in the host.

## 3. Unit 7 — reuse from n_past

Processing positions [0,4) in one call and [0,2) then [2,4) in two calls gives the same
output at position 3, bit for bit.

`distinguishable=1` is what stops this from being vacuous. The two-call result at position 3
must differ from the single-call result over [0,2), otherwise both arms would agree because
nothing was added, and the check would pass while testing nothing. It does differ.

## 4. What these two do not prove

That a full decode loop produces the reference token sequence. They prove the cache
mechanism the loop will sit on: the buffer persists, the split point is respected, and
reuse does not change the answer.

## 5. State ledger closed

| # | unit | evidence |
|---|---|---|
| 1 | attention KV slot | EXP-701 |
| 2 | KV write/keep/read | EXP-701 |
| 3 | recurrent conv state | EXP-692 |
| 4 | SSM state as an output | EXP-696 |
| 5 | one mechanism for both caches | EXP-702 |
| 6 | buffer survives a graph rebuild | this card |
| 7 | reuse from n_past | this card |
| 8 | comparison vs full recompute | EXP-701, EXP-692 |

## 6. Cost

Gate compiles only. No unit rebuild.
