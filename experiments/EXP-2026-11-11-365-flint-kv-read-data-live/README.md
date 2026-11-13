# EXP365: KV state-read header decisions, bridge gated

The Goose module and bridge that decide the read path's header: which rejections fire and which
tensors are transferred.

## Verdict

**BRIDGE GATED, NOT WIRED YET.** The decisions match the native header logic; the call site in
`state_read_data` is the next step.

## What the module decides

1 layer count, 2 capacity, 3 a transposed state — then per layer the key type and row size, the PLE
row size and the value type and row size, each only for tensors that exist. It returns the first
rejection code, or zero with three per-layer read masks (keys, PLE rows, values), so the caller
performs exactly the transfers the native code would.

The transfers themselves (`io.read_tensor`) stay native. One difference is unavoidable and recorded:
the caller reads every type and row size before asking, so on a rejection the IO stream has advanced
past the point where native would have stopped. The restore aborts on a rejection either way, so the
observable state is the same; only the consumed bytes differ.

## Gate

| check | result |
|---|---|
| 5 000 draws, codes 1–8 reachable, masks compared | 11 595 checks / 0 failures |

## Next

Wire it into the flint `state_read_data` and repeat the save/restore round trip of EXP364, expecting
`FG365S_KV_READ_DATA` next to `FG345W_KV_STATE_WRITE`.
