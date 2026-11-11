# EXP364: the state Goose wrote restores and reproduces the answer

A save-restore round trip on the same stage that carries the Goose state-write selection.

## Verdict

**ROUND TRIP EXACT** — the file written from the Goose-chosen ranges restores and the repeated
completion is byte-identical to the first.

## Procedure (one server, one run)

1. completion on the fixed 32K/128 request → `first`;
2. `POST /slots/0?action=save` with `--slot-save-path`;
3. `POST /slots/0?action=restore`;
4. the same completion again → `second`.

## Result

| check | result |
|---|---|
| save | `n_saved` reported, slot file 387 989 920 bytes |
| restore | `n_restored` reported |
| first vs second: tokens | equal |
| first vs second: all 128 probabilities | equal |
| first vs second: content hash | equal (`e68df05b…`) |
| `FG345W_KV_STATE_WRITE` | 1 call (the save; restore goes through the read path) |
| batch witnesses | both live |

So the ranges Goose selected produce a state file that the native reader accepts and that reproduces
the original continuation exactly. This is a correctness result for the **write** selection, not yet
a live witness for the read half.

## Next

Wire `state_read_meta` so the restore path also has a Goose call site, then repeat this same round
trip and expect the read witness alongside the write one.
