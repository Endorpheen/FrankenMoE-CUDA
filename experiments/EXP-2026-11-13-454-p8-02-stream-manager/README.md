# EXP454: stream session manager and GC in Goose (P8-02 seventh slice)

The stream session manager owns the conversation map: create, replace, lookup,
evict, cancel-evict, the stop path and the TTL sweep. This experiment moves those
decisions into Goose.

## What was ported

`source/manager.goose`, from `server-stream.cpp` (`create_or_replace`, `evict`,
`evict_and_cancel`, `stop_gc`, `gc_loop`):

| operation | native effect | Goose plan |
|---|---|---|
| `create_or_replace` | install a fresh session; retire the previous one by cancel **then** finalize | `replace=1, cancel=1, finalize=1` |
| `evict` | erase; finalize only, producer not cancelled | `cancel=0, finalize=1` |
| `evict_and_cancel` | erase; cancel first so a drain loop stops, then finalize | `cancel=1, finalize=1` |
| `stop_gc` | finalize every session, clear the map, no cancel | `cancel=0, finalize=1` |
| `gc_loop` | drop only sessions with `completed_at != 0 && completed_at <= now - TTL` | `gc_decision` |
| cutoff | `now - STREAM_SESSION_TTL_SECONDS` (300) | `gc_cutoff` |

The map, `shared_mutex`, the GC thread and the condition-variable wake stay
native.

## Gate

`tests/manager_fixture.cpp` simulates both arms over the same operation sequence
with per-session `cancelled`/`finalized` flags and `completed_at`, and compares
map membership and every session's flags after every operation.

| corpus | cases |
|---|---|
| fixed script with repeated create/replace, evict present and absent, cancel-evict, stop | 14 steps |
| GC boundary: alive, exactly at cutoff, one second after, long expired, completed in the future | 1 sweep |
| 40 randomised sequences x 60 operations (create/evict/cancel-evict/stop/GC with advancing time) | 2 400 ops |
| total | **33 823 cases, 71 135 values, 0 failures** |
| ASan + UBSan (C and C++ objects) | 0 failures, rc 0 |

Mutations 7/7 caught: replace stops cancelling the previous session (10 895
failures), evict also cancels (4 822), evict_and_cancel stops cancelling (4 705),
stop-all also cancels (7 175), GC drops live sessions (2 694), GC boundary becomes
exclusive (2), GC cutoff ignores the TTL (1).

The last two mutations are deliberately narrow: they only show up at the exact
boundary and at a single randomised timestamp. Both were caught because the
fixture pins `completed_at == now - TTL` and checks the cutoff value directly
instead of relying on a broad average.

## What stays native

- the conversation map, the shared mutex and the session `shared_ptr`s;
- the GC thread, the wake interval and the 60-second sweep period;
- the SSE HTTP handlers that call these operations.

P8-02 stays IN_PROGRESS on the transport, handler binding, GCP AIP and `/props`.

## Reproduce

    bash experiments/EXP-2026-11-13-454-p8-02-stream-manager/build_fixture.sh
    python3 experiments/EXP-2026-11-13-454-p8-02-stream-manager/mutate.py
