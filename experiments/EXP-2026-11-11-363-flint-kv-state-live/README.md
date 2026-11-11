# EXP363: three call sites live, KV state-write exercised by a save

Batch selection, `ubatch_add` and the KV state-write range selection running in one stage, with the
state path reached through the server's own slot save.

## Verdict

**LIVE, EXACT** — all three witnesses fire, the completion is byte-identical, and the save produced a
file.

## Stage

44 flint objects with `llama-batch.cpp.o` and `llama-kv-cache.cpp.o` replaced by integrated copies,
three bridges linked: 5 297 008 bytes, ABI complete, all three library names substituted.

## Run (32K/128, one run, no parallel work)

| check | result |
|---|---|
| completion: tokens / probabilities / hash | equal to the control (`e68df05b…`) |
| `FG350S_SPLIT_SELECT` / `FG349S_UBATCH_ADD` | live |
| `FG345W_KV_STATE_WRITE` | **live, 1 call** — from the save |
| save | `save_ok`, slot file 387 989 920 bytes |
| prefill / decode | 1621.2 / 26.09 tok/s |

## What this settles

The KV state-write route was never on the completion path, which is why earlier probes saw zero calls.
It is on the save path, and a save is an ordinary server request: `POST /slots/0?action=save`. The
selection Goose made for that section produced a file the server accepted.

## Not covered

`state_read` (restore of that file), the meta/data halves as separate live routes, and the `state_clear`
paths. The natural next check is a restore of the file just written and a comparison of the state
afterwards.
