# EXP271: server-queue decisions on the live path

Integration half of EXP-270. Two sites in `tools/server/server-queue.cpp` are switched to the Goose module behind
`fg270q_enabled()` (default on; `GGML_GOOSE_SERVER_QUEUE=0` restores the native logic):

- `pop_deferred_task()` builds the `id_slot` array, asks for the index and moves that entry to the front of the main
  queue;
- `cleanup_pending_task()` builds the task-id array for each of the three queues, asks for the removal mask and erases by
  index, keeping the queues and their ownership untouched.

## Build and ABI

`server-queue.cpp` compiled with the flint command; only `fg270q_enabled`, `fg270q_pick_deferred` and
`fg270q_cleanup_mask` are added to its undefined set. The object replaced `server-queue.cpp.o` inside a copy of
`libserver-context.a`, and `libllama-server-impl.so` was relinked from ninja's command with the archive and the bridge.

## Live A/B with concurrency and a cancel

Three clients per arm, same DSO: A sends the accepted 32K request, B sends the same request two seconds later (it
defers while A holds the only slot), C opens a streaming request and disconnects after three events (which posts a
cancel and reaches `cleanup_pending_task`).

| arm | A content | A tokens/probs | B content | B tokens/probs | witness |
|---|---|---|---|---|---|
| `GGML_GOOSE_SERVER_QUEUE=1` | `e68df05b...` | equal | `e68df05b...` | equal | `FG270Q_SERVER_QUEUE pick=2 cleanup=2` |
| `GGML_GOOSE_SERVER_QUEUE=0` | `e68df05b...` | equal | `e68df05b...` | equal | `pick=0 cleanup=0` |

Both deferred pops and both cancels ran through the module, and both clients got byte-identical answers with all saved
probabilities equal. The witness counts also show the switch truly disables the path.

## What is not claimed

No timing claim: three clients share one slot, and one run per arm cannot support a latency verdict. The route is
verified functionally, not measured.
