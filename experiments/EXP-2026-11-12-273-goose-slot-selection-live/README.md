# EXP273: slot-selection decisions on the live path

Integration half of EXP-272. Two sites in `tools/server/server-context.cpp`, both behind `fg272s_enabled()` (default on;
`GGML_GOOSE_SLOT_SELECT=0` restores the native logic):

- the LCP-similarity choice in `get_available_slot()`: build eligible/lcp arrays, ask for the index, set `ret` and
  `f_sim_best` from it. The native loop is kept and skipped while the switch is on;
- the single free-slot case of `get_free_slots()` (`n_slots_needed == 1`, no exclusion): build idle/empty/t_last arrays
  and take the LRU index. Other callers, including the parent/child path, keep the native `stable_sort`.

## Build and ABI

`server-context.cpp` compiled with the flint command; only `fg272s_enabled`, `fg272s_pick_similar`, `fg272s_pick_lru` are
added to its undefined set. The object replaced the archive member and `libllama-server-impl.so` was relinked from
ninja's command with the archive and the bridge.

## Live A/B with concurrency

Same harness as EXP-271 (A: accepted 32K request; B: same request two seconds later, which defers; C: streaming client
that disconnects and posts a cancel):

| arm | A content | A tokens/probs | B content | B tokens/probs | witness |
|---|---|---|---|---|---|
| `GGML_GOOSE_SLOT_SELECT=1` | `e68df05b...` | equal | `e68df05b...` | equal | `FG272S_SLOT_SELECT similar=5 lru=3` |
| `GGML_GOOSE_SLOT_SELECT=0` | `e68df05b...` | equal | `e68df05b...` | equal | `similar=0 lru=0` |

Both decisions ran (five similarity selections, three LRU picks), the clients got byte-identical answers with all saved
probabilities equal, and the switch truly disables the path.

## Not claimed

No timing verdict: the run has three clients on one slot, and one pass per arm cannot support latency numbers.
