# EXP-2026-11-15-635 — P9-04: router hooks, row streaming and I/O-compute overlap

**Verdict: ACCEPTED** (model-free, differential). Base commit `9981c403`; the
runtime tree is identical at `HEAD` (`results/canon-compat.log`). No model, no
GPU, no tok/s: P9-04 is runtime policy, and this card reports decisions.

## Numbers

| slice | native source | exports | cases | failures |
|---|---|---:|---:|---:|
| S1 `rh_` hook | `router_hook.cpp` | 18 | 302 | 0 |
| S2 `ov_` overlap | `expert_stream_source.cpp` overlap path + `stall_union.h` | 20 | 560 | 0 |
| S3 `rs_` row source | `router_hook.cpp` capture/dispatch + `row_stream.cpp` takeover + `session.cpp:1051-1082` | 12 | 145 | 0 |
| total | | 50 | 1007 | 0 |

- Gate at `-O1`: `FG_P9_04_HOOKS cases=1007 checks=1007 failures=0`.
- Gate at `-O0`: identical byte-for-byte gate lines.
- Mutations: **12/12 caught**, in two halves of six, every one by a gate
  failure (not a build failure).
- Memory impact: none. Nothing here allocates, loads or runs; the ported
  decisions are integer comparisons over state the runtime already holds.

## What is new relative to EXP-620 and EXP-621

EXP-621 declared three regions native and left them out: the `RouterHook`
capture pass, the `ReadyFlag` overlap path and `StallUnion`. This experiment
covers exactly those, plus the hook's activation and call ORDER (which neither
accepted experiment touched) and the session's dense/row split.

Not repeated: the expert LRU (`xl_`), the dense placement (`dw_`), the row-stream
slab plan and the materialize size limit (`sp_`), the recipes/`match_expert`
(`ar_`), config validation (`cfg_`).

## Findings worth carrying into the runtime

1. **The hook's order is a contract, not an accident.** At the topk, substitution
   must run before route-ahead and before the gather, and route-ahead must
   COLLECT before it SUBMITS — the source records that collecting afterwards
   stale-failed nearly every layer. `rh_topk_plan` pins the whole sequence; it is
   the only artifact in the tree that states it in one place.
2. **Forgetting is the self-correcting half of the deferral.** `close_drop_layer`
   learns a layer's terminal weight node, and a deferral the graph did not honour
   makes it FORGET that node (native 321-341). Without the forget, one moved node
   would fault every token; with it, one token is recoverable.
3. **Wave two grows a batch; it does not restart one.** `next_idx_`/`done_cnt_`
   are not reset by the second publish, and a lane parked after wave one is
   woken by `next_idx_ < batch_njobs_`, not by the generation. Mutating either
   half is caught (`ov_worker_growth`, `ov_publish_always_notify`).
4. **The staging window is exactly one layer.** A readiness request for another
   layer is fatal, not a wait (native 1327-1335): the alternative is a compute
   thread blocking forever on a flag nobody will publish.
5. **A table's row verdict is a set difference.** One reference that is not a
   table-position row gather vetoes the table (`rs_table_verdict`), which is why
   a tied embedding used as the output head can never be row-streamed.

## Assumptions

- The native transcription keeps the native state fields and step order and
  reads the same names; tensor reads, `pread` outcomes, commit outcomes and
  timestamps are gate inputs. A kernel answer is never compared.
- Node names are modelled as the native 64-byte `char[64]` zero-filled buffers,
  so the `memcmp`-based matchers see exactly what they see in a graph.
- Expert-suffix matching (`match_expert`, the recipe table) is EXP-620's and is
  used as given, not re-validated.
- `.goose` `i64` vs native `int` differ only above 2^31; every name, count and
  flag in the gate stays below it, and the one place where the native is
  `int`-bounded (the 2^20 suffix ceiling) is tested at and past the bound.

## Limitations (honest)

- The hook lifecycle case drives both sides with ONE shared schedule driver (the
  fixture plays the session). That is fair for comparing the two implementations'
  answers, but it does not independently validate the driver's own ordering.
- `rh_topk_plan` transcribes a straight-line C++ sequence of calls. Its native
  oracle is the same sequence written as guarded appends; the mutation table is
  what makes the comparison non-circular (four of the twelve mutations are order
  and guard changes in that region).
- Only `-O1`/`-O0` and this host's gcc-13 are measured. The gate compares
  decisions, so it makes no claim about timing, thread interleaving or I/O.

## Integration caveats (honest)

- **Container widening.** `term_variant_` is `std::vector<int8_t>` natively and
  `i64[:]` in the port; adopting the port needs an adapter, not just a call.
  `stall_union.h`'s three integers become an `i64[3]` window, with the mutex and
  the clock left in the wrapper.
- **The order plan is a rewrite, not a swap.** Adopting `rh_topk_plan` means
  `on_eval` dispatches by op code and the route-ahead/prefetch call sites are
  refactored. It should be its own integration step; the predicates around it
  (`rh_weights_iso`, `rh_defer`, `rh_close_layer`, `rh_observe_weights`) can be
  adopted one at a time.
- **Header composition.** EXP-620, EXP-621 and this experiment each generate a
  standalone header with the same `sl_i64`/`sl_u8` typedefs. One combined
  translation unit (as `source/stream_hooks.goose` does) or three separate
  translation units — the lead's choice; two headers in one TU will not compile.
- **No ABI, no interface change is proposed.** Every export replaces a `static`
  function or a private member decision, and `IExpertSource`/`IRowSource` stay
  as they are.

## Artifacts

| artefact | SHA-256 |
|---|---|
| `source/hook_rules.goose` | `0a2de5e66c08857be3b8d221b3dc7941c75fd9be7a1759f29c0e34adcc3f4ef8` |
| `source/overlap_rules.goose` | `f47d0d44dd15a188473d8ee6ee685f751e001e3ba5f4854e9f438cf857481507` |
| `source/row_select.goose` | `f418a174e79c47ce1d2240db836fdfadc3b99f6ba0fd1e42d20660b39f241fd6` |
| `source/stream_hooks.goose` | `0b69be7af9c9ba494ccbccde48897720516de99d40a907e746ea0efd5a2da01c` |
| `tests/fixture.cpp` | `66f2b6a31463e2638ae2bb8767f18f7b5a323edf8beb1f92c41bae1844ccf486` |
| `tests/build.sh` | `2be77cf43cc9720db5e51116f1d5a8ccf4f01a31a1a7e6e8414cc1a87ef16296` |
| `tests/mutate.py` | `f6cd8f05cff11483e0a8006c39e131617a143c501ffbb7c844f600e69c92f84e` |
| `results/gate-o1.log` | `4f73c290104a009928503681056b5508d014f517337448b29112a9d33257f54d` |
| `results/gate-o0.log` | `39a613249ba70bed4f218e7b0173d62de7f5da33fe9bc798be7367930b3399e2` |
| `results/mutate-1.log` | `a0f078c69a95f032d62fe3070732daaa6f47ec835d3bfddf4405333cadda9ee2` |
| `results/mutate-2.log` | `13ef61f9f70a2b7f19daa680cf18ffac566f60b6a78c3dba10f33a807a772b9d` |

Toolchain: goose runtime `1e714e6146bc` (goose-2026.3-string), gcc-13/g++-13
13.4.0, Linux x86_64. Generated `build/g.c`, `build/g.h`, `build/g.o` and the
gate binaries are reproducible and not part of the evidence.

## Reproduce

```
cd /tmp/opencode/sub-635/EXP-2026-11-15-635-p9-04-stream-hooks
bash tests/build.sh          # -O1 gate
bash tests/build.sh mutate   # -O0 gate
python3 tests/mutate.py 0 6
python3 tests/mutate.py 6 12
```

Expected: three slice lines and `FG_P9_04_HOOKS cases=1007 checks=1007
failures=0` if the sources are unmodified, and `mutations 6/6 caught` twice.
