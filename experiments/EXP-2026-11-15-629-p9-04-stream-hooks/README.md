# EXP-2026-11-15-635 — P9-04: router hooks, row streaming and I/O-compute overlap

Hypothesis: what the router hook DECIDES, what the overlap loop DECIDES about
copying ahead and waiting, and which tables the row policy SELECTS are pure
functions of small integer state and node names. Expressed in Goose, they
reproduce the native decisions exactly on synthetic schedules, with no graph, no
file, no GPU and no thread involved — so
`router_hook.cpp`, `expert_stream_source.cpp` (overlap path) and
`row_stream.cpp` (takeover) keep only the tensor reads, the barriers, the
`pread`s and the rebinding.

Base commit: `9981c403` (`docs: close P7-11 and move its loader items to P7-12`;
runtime tree verified identical at `HEAD`, see `results/canon-compat.log`).
Work was done outside the repository, in
`/tmp/opencode/sub-635/EXP-2026-11-15-635-p9-04-stream-hooks/`, so the shared
tree, the production runtime and other agents' files are untouched. There is no
model and no tok/s number here by construction: P9-04 is a runtime policy layer.

## What was already taken, and what this experiment covers

Two accepted experiments own the neighbouring decisions, and this one does not
repeat them:

| accepted | owns |
|---|---|
| EXP-2026-11-15-620 (`cfg_`/`ar_`/`cc_`) | config validation, recipes, architecture registry, `match_expert`, CLI resolution |
| EXP-2026-11-15-621 (`xl_`/`dw_`/`sp_`) | expert LRU and cache accounting, dense placement, the row-stream **slab plan** (geometry, gather plan, eviction guard, materialize size limit) |
| **this one** (`rh_`/`ov_`/`rs_`) | hook **activation and call order**, **overlap** decisions, row-stream **source selection** |

EXP-621 explicitly left `RouterHook` capture ("which tables are row-gathered"),
the `ReadyFlag` overlap path and `StallUnion` out as native. Those are exactly
what `rh_`, `ov_` and `rs_` cover. The slab plan (`sp_gather`, `sp_evict`,
`sp_materialize_ok`) and the LRU (`xl_*`) are cited, not re-ported.

## Slices

Three independent decision sets, one experiment. Each slice has its own
`.goose` file, its own native transcription and its own gate section; the
prefixes `rh_`, `ov_`, `rs_` collide with no other accepted prefix.

| slice | Goose source | prefix | native counterpart |
|---|---|---|---|
| S1 hook | `source/hook_rules.goose` | `rh_` | `runtime/core/src/moe/router_hook.cpp` |
| S2 overlap | `source/overlap_rules.goose` | `ov_` | `expert_stream_source.cpp` (overlap path) + `stall_union.h` |
| S3 row source | `source/row_select.goose` | `rs_` | `router_hook.cpp` capture/dispatch + `row_stream.cpp` takeover + `session.cpp:1051-1082` |

### S1 — the router hook (`rh_`)

The hook is the only place where the runtime sees a llama.cpp graph, so "what is
this node" and "what am I allowed to do with it" are the whole of its behaviour.

| export | native | decision |
|---|---|---|
| `rh_is_moe_node` | 48-50 | the 8-byte `ffn_moe_` prefix test that guards every other matcher |
| `rh_parse_tail` | 51-63 | digits only to the end of the name, `> 2^20` refused |
| `rh_match_layer_node` | 66-70 | prefix + tail for the topk, the gate matmul, and the softmax-topk variant |
| `rh_match_weights` | 83-100 | the four mutually exclusive weight-chain names; the `'-'` is what rejects `ffn_moe_weights_sum` |
| `rh_drop_armed`, `rh_sub_armed` | 239, 232 | arming: a source, a threshold, decode unless armed for prefill |
| `rh_want_weights` | 1476 | the chain is asked for by a trace or by the drop policy |
| `rh_weights_iso` | 1489 | isolate the chain only while the layer is still learning its terminal |
| `rh_logits_scope` | 1496 | the gate matmul is looked for only for a decode-phase prediction |
| `rh_ctrace_iso` | 1510-1522 | the compute trace's cursor: every node, or one per layer transition |
| `rh_ask_iso` | 1528 | the ask pass's answer, as a disjunction of the four consumers |
| `rh_capture_take` | 1410-1420 | which expert tensor a captured node admits |
| `rh_defer` | 1668 | defer the load from the topk to the terminal weight node |
| `rh_close_layer` | 321-341 | learn the terminal; FORGET it when the deferral was not honoured |
| `rh_observe_weights` | 1547-1551 | last-wins learning, and the deferred layer's decision point |
| `rh_workload` | 1588-1600 | capture/replay, and the shape mismatch that fails closed |
| `rh_topk_plan` | 1553-1699 | the topk callback's call ORDER as a plan |
| `rh_ask_plan` | 1405-1528 | the ask pass's call ORDER, with the capture pass owning the pass |

The refusal/fallback rules are decisions, not errors: a layer whose terminal is
unknown loads undropped at its topk (1489, 1668); a deferral that was never
honoured forgets the terminal so the next graph re-learns it (321-341); a
workload whose shape does not match aborts the graph through the abort callback
rather than tolerating two different workloads (1588-1600).

### S2 — I/O-compute overlap (`ov_`)

Overlap mode never waits for a layer before computing it; it stages the routed
experts onto readiness flags and blocks each compute thread on exactly the slice
it is about to multiply. Four questions follow, and all four are pure.

| export | native | decision |
|---|---|---|
| `ov_p0` | 1163-1167 | the first PRESENT projection of a layer (a fused layout has no slot 0) |
| `ov_two_wave` | 1168 | two-wave publish is refused for `load_all` and without a present projection |
| `ov_commit_only` | 1174 | which projection's pages are committed before the lanes start |
| `ov_staged` | 1119-1135 | the deduplicated, ascending staged set; `load_all` stages the bank by index |
| `ov_stage_verdict` | 1176-1186 | commit failure / hit / miss, and the hit that needs no read at all |
| `ov_wave_jobs`, `ov_wave_list` | 1188-1196 | what "copy ahead" schedules: projection-major, misses only |
| `ov_publish` | 1189-1212, 1266-1280 | the batch snapshot; wave two grows the batch instead of restarting it |
| `ov_drain_done` | 1089-1103 | WHICH BATCH TO WAIT FOR: the previous one, before `jobs_` is reused |
| `ov_worker_wait` | 371-380 | a lane wakes for a new generation OR a batch that grew in the one it drained |
| `ov_drain_more` | 338-340 | a drain belongs to one generation and ends when it is superseded |
| `ov_gate_scope` | 1327-1335 | the staging window is one layer: another layer's request is a fault, not a wait |
| `ov_gate_path` | 1315-1365 | not ours / fault / already ready / stall |
| `ov_publish_mode` | 336-365 | store the flag, and take the mutex only when someone is registered |
| `ov_read_failure`, `ov_commit_failure` | 342-346, 1197-1233 | which failures abort: a read in overlap; a commit after wave one published |
| `ov_quiesce_arg` | 1106 | where the speculative window closes (adopt or cancel) |
| `ov_stall_enter/exit/total` | `stall_union.h` 11-20 | the union of stalled-thread intervals, including an interval still open |

### S3 — row-stream source selection (`rs_`)

Not which slab to read — which TABLE the policy is allowed to own, and what the
hook does when it meets one at run time.

| export | native | decision |
|---|---|---|
| `rs_index_materialized` | 173-193 | a graph input, or a pure view of one, eight hops deep |
| `rs_leaf_admit` | 1425-1426 | a named leaf that is not an expert is a dense candidate |
| `rs_ref_verdict` | 1427-1433 | only the TABLE position of a row gather counts; everything else vetoes |
| `rs_table_verdict` | 195-200 | the answer is the set difference: gathered MINUS disqualified |
| `rs_session_take` | session 1060-1078 | the split: experts are the stream's, CUDA-resident weights cannot be rebound, non-gguf leaves are not file tensors |
| `rs_row_budget` | session 1080 | the window: negative clamped to zero |
| `rs_init_accept` | row_stream 45-61 | an unnamed or sizeless tensor is skipped, a bad shard is fatal, a non-row-shaped tensor is left alone |
| `rs_policy_on` | row_stream 74-78 | nothing qualified is a supported outcome, not a failure |
| `rs_dispatch` | 1446-1462 | gather when the index is a readable I32, otherwise materialize |
| `rs_prep_fatal` | 1463 | one refused table aborts the graph |
| `rs_materialize_need` | row_stream 213-217 | how many slabs a whole-table pull needs |
| `rs_whole_unlink` | row_stream 233-241 | a materialized table leaves the LRU for the rest of the run |

The materialize SIZE LIMIT (`sp_materialize_ok`, native 219) is EXP-621's and is
not re-tested here.

## Interfaces the port relies on (canon)

- `bmoe::MoeRecipe` — `max_exps = 3`, `exps_suffix[3]`
  (`runtime/core/include/bmoe/recipe.h`); the expert suffix table is EXP-620's
  `ar_` slice and is not re-ported.
- `RouterHook` (`runtime/core/src/moe/router_hook.{h,cpp}`): `on_eval`, the
  `set_*` arming entry points, `captured_weights()`, `row_gathered_weights()`.
  No ABI change is proposed: every `rh_`/`rs_` export replaces a decision that
  is already `static` or a private member, and the state vectors are the
  existing fields (`term_variant_`, `chain_last_`, `pending_`, `drop_`).
- `IExpertSource` / `IRowSource` (`runtime/core/include/bmoe/expert_source.h`,
  `bmoe/row_source.h`): `load_layer`, `prefetch`, `settle_spec`,
  `query_residency`, `stats`; `serves`, `gather`, `materialize`. The `ov_`/`rs_`
  exports sit *behind* those interfaces, so nothing in the interface changes.
- `StallUnionState`/`StallUnion` (`runtime/core/src/moe/stall_union.h`): three
  integers and two transitions; the mutex and the clock stay native.
- `ggml_tensor` (`ggml.h`): `name[64]`, `op`, `type`, `nb[]`, `ne[]`, `data`,
  `src[GGML_MAX_SRC]`. Read-only here.

## Exact integration step

Each export replaces a decision that is already a local expression in the
native file; the integration is a call plus the state it needs, not a redesign.

1. `rh_` — in `router_hook.cpp`, `on_eval` keeps its structure and calls the
   ported predicates:
   - 1410-1420 → `rh_capture_take(p, il >= 0 && il < n_layer_, src->ne[2])`;
   - 1489-1490 → `rh_weights_iso(weight_variant, wl, term_variant_)`;
   - 1510-1522 → `rh_ctrace_iso(ctrace_on_, ctrace_layers_, node_layer(t->name), &ctrace_ask_layer_)`;
   - 1547-1551 → `rh_observe_weights(t->data && t->type == GGML_TYPE_F32, wl, weight_variant, drop_.layer, drop_.deferred, term_variant_, &chain_last_)`;
   - 321-341 → the body of `close_drop_layer` becomes `rh_close_layer(term_variant_, D.layer, chain_last_, D.deferred)` plus the load it already performs.
   The two plans (`rh_topk_plan`, `rh_ask_plan`) are an ORDER contract: if they
   are adopted, `on_eval` dispatches through the same op codes and the fixture
   becomes the order regression test. They can also be adopted as documentation
   only, which is what the current runtime would need to do first.
2. `ov_` — `load_layer_async` and the readiness path:
   - 1089-1103 → `ov_drain_done(done_cnt_, batch_njobs_, io_stop_)`;
   - 1163-1194 → `ov_p0`/`ov_two_wave`/`ov_commit_only` replace the lambda and the two booleans;
   - 1197-1233 → `ov_commit_failure(published)` decides `return false` vs `fatal_` + wake;
   - 336-365 → `ov_publish_mode(j.flag >= 0, ready_waiters_.load())`;
   - 371-380 → `ov_worker_wait(...)` as the `io_cv_.wait` predicate;
   - 1315-1365 → `ov_gate_scope`/`ov_gate_path` before the spin.
   `stall_union.h` keeps `StallUnion`; `StallUnionState::enter_at/exit_at/total_at`
   become `ov_stall_*` over the same three integers.
3. `rs_` — `session.cpp:1051-1082` calls `rs_session_take` per captured leaf and
   `rs_row_budget(row_stream_mb)`; `RowStream::init` calls `rs_init_accept` per
   candidate; `router_hook.cpp:1446-1462` calls `rs_dispatch` and `rs_prep_fatal`;
   `gather`/`materialize` take `rs_materialize_need` and `rs_whole_unlink`.

The generated header is one translation unit (`source/stream_hooks.goose`), so
the integration is a single `#include "stream_hooks.h"`-shaped addition; the
`#include` name is the lead's call.

## Gate

```
bash tests/build.sh          # -O1
bash tests/build.sh mutate   # -O0
python3 tests/mutate.py 0 6  # half 1
python3 tests/mutate.py 6 12 # half 2
```

`tests/fixture.cpp` carries a transcription of the native functions (state
fields and step order kept, source line cited at every function) and drives the
same scripted scenarios through the Goose exports. A commit outcome, an I/O
outcome, a timestamp and a tensor shape are arguments on both sides, so the
comparison is of decisions, not of kernel answers.

```
FG_P9_04_A cases=302 checks=302 failures=0      # hook
FG_P9_04_B cases=560 checks=560 failures=0      # overlap
FG_P9_04_C cases=145 checks=145 failures=0      # row source
FG_P9_04_HOOKS cases=1007 checks=1007 failures=0
```

Identical at `-O1` and `-O0` (`results/gate-o1.log`, `results/gate-o0.log`).
A "case" here is one compared decision instance: a node name, a flag vector, an
event in a schedule or a grid point. Each slice has far more than the ten
required, and the scripted schedules (16 topk callbacks, 8 ask passes, a 26-event
hook lifecycle, a 13-event stall script) are the ones that carry order.

## Mutations

Twelve semantic changes to the port, two halves of six, each rebuilt at `-O0`
and compared against the gate's own baseline: **12/12 caught**, every one by a
gate failure rather than by a build failure.

| # | mutation | failures |
|---|---|---|
| 1 | `rh_tail_ceiling`: the 2^20 layer-suffix ceiling is lifted | 2 |
| 2 | `rh_weights_dash`: a chain name is matched by prefix, no `'-'` required | 6 |
| 3 | `rh_weights_iso_always`: every weight node isolated, learned or not | 2 |
| 4 | `rh_plan_second_collect`: route-ahead's post-load collect dropped | 1 |
| 5 | `ov_worker_growth`: a lane is not woken for a batch that grew | 3 |
| 6 | `ov_two_wave_load_all`: two-wave publish used for `load_all` | 14 |
| 7 | `ov_publish_always_notify`: the mutex + wake on every slice | 3 |
| 8 | `ov_stall_every_exit`: the union accumulates on every exit | 5 |
| 9 | `ov_gate_order`: the scope fault checked after readiness | 2 |
| 10 | `rs_index_nine_hops`: the index walk follows nine hops | 1 |
| 11 | `rs_ref_any_slot`: a gather at any source slot qualifies a table | 3 |
| 12 | `rs_shard_silent`: a bad shard index silently skips the table | 2 |

`results/mutate-1.log`, `results/mutate-2.log`.

## Compatibility and conflicts

Honest list of what adopting this changes:

- **Types.** No new structs, no ABI change. The exports take `i64`/`f64`/slices
  and return `i64`; the C header uses the same `sl_i64`/`sl_u8` shapes as the
  accepted EXP-620/621 headers. If all three experiments are integrated into one
  runtime build, their generated headers declare the same typedefs and must be
  `--standalone`-combined (as `source/stream_hooks.goose` does) or included from
  separate translation units.
- **Containers.** `term_variant_` is `std::vector<int8_t>` in the native file
  and `i64[:]` in the port: an adapter is needed (a widened copy, or a
  `int8`-typed slice export). `staged_`/`drop_w_`/`drop_mask_` stay native.
  `stall_union.h`'s three integers (`stalled_`, `open_ns_`, `total_ns_`) become
  an `i64[3]` window; the mutex and the clock stay in the wrapper.
- **Order contract.** `rh_topk_plan`/`rh_ask_plan` are a *contract*: adopting
  them means `on_eval` dispatches by op code. Adopting `rh_topk_plan` before the
  route-ahead/prefetch call sites are refactored will not work — the plan names
  them. That is a rewrite of the callback's body, not a swap of one predicate,
  and it should be its own step.
- **Two-wave publish.** `ov_publish` models wave two as *growing* the batch
  (`next_idx_`/`done_cnt_` untouched). The native already does this; the port
  only makes it explicit, so integrating it is a no-op behaviourally — which is
  the point: it is now pinned by the gate.
- **No assumptions about architecture names.** The port reads no recipe table
  (EXP-620 owns it) and assumes nothing about which gating a model uses: the
  weight chain is learned from the graph, as the native does.

## Artifacts

- `source/hook_rules.goose`, `source/overlap_rules.goose`,
  `source/row_select.goose`, `source/stream_hooks.goose`
- `tests/fixture.cpp`, `tests/build.sh`, `tests/mutate.py`
- `results/gate-o1.log`, `results/gate-o0.log`, `results/mutate-1.log`,
  `results/mutate-2.log`, `results/canon-compat.log`, `results/hashes.txt`
- `REPORT.md`

Generated `build/g.c`, `build/g.h`, `build/g.o` and the gate binaries are
reproducible from the Goose sources and are not part of the evidence.
