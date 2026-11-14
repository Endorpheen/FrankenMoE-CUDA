# EXP-2026-11-15-620 — P9-03: dense weights and expert streaming/LRU sources

Hypothesis: the *decisions* of the expert cache (`expert_stream_source.cpp`), the
dense-weight placement policy (`dense_weights.cpp`) and the row-stream plan
(`row_stream.cpp`) are pure functions of small integer state. Expressed in
Goose, they reproduce the native decisions exactly on synthetic tables, with no
file, kernel, GPU or thread involved — so the native files keep only the I/O,
the threads and the tensor rebinding.

Base commit: `df3ce73a` (FrankenMoE-CUDA). The three native files are unchanged
at `d0b6e258` (`git diff df3ce73a d0b6e258 -- <file>` is empty), so the line
citations hold against both.

## Inventory: what is a decision and what is a syscall

| native region | verdict | why |
|---|---|---|
| `entry_bytes`, `worst_cycle_bytes`, `account_demand` | pure | arithmetic on the model shape |
| LRU list (`lru_unlink/push_front/push_back`, `evict_tail`) | pure | intrusive list over `(layer, expert)` ids |
| `touch_entry` | pure except the commit | hit test, promotion, residency accounting; `vm_commit` is the only syscall |
| `set_cache_budget` | pure except quiesce | grow/clamp/floor and the eviction loop |
| `retain`, `query_residency` | pure | list moves and a 3-value classification |
| `prefetch` filter, `spec_integrate_done` | pure | candidate set, cold/hot insert, stale-completion guard |
| `load_layer` dedup + post-load eviction | pure | `seen_` pass and the stamp-guarded loop |
| `byte_ranges`, `subtract_range`, `hold_back_oversized`, `take_row_gathered` | pure | interval algebra plus a `size > avail` test; `avail` is MemAvailable |
| `DenseWeights::init` verdict | pure | mode dispatch, with `pinned_max_bytes()` as an input |
| `row_stream`: budget clamp, geometry, gather plan, eviction guard, fetch order, materialize limit | pure | integers over the reservation |
| `FileReader`/O_DIRECT/pread, `vm_reserve/commit/evict`, `madvise`, `mincore`, `/proc/self/maps`, pinned dma-buf | native | kernel and hardware |
| I/O lane pool, `ReadyFlag` spin/wait, `StallUnion`, `IoJob` trace | native | threads and timing |
| `RouterHook` capture (which tables are row-gathered), ggml `tensor->data` rebind | native | graph and ABI |

## The three slices

Prefixes are unique per slice and no helper or constant is shared, so the three
sources compose into one translation unit without renames.

### S1 `source/expert_lru.goose` (prefix `xl_`)

State enters as slices: `lay` (per layer: bound, nb2 of projections 0..2),
`cvalid`, `cspec`, `cprev`, `cnext`, `cstamp`, `ever`, `seen`, `st` (head, tail),
`sacc` (cresident, hits, lookups, rereads, useful), `acc` (token demand, layer
demand, accumulators, last_il).

| export | native anchor | decision |
|---|---|---|
| `xl_entry_bytes` | 741 | one expert = every projection of its layer |
| `xl_worst_cycle` | 697 | `top_k` clamped at `n_expert` per bound layer |
| `xl_unlink_e`, `xl_push_front`, `xl_push_back` | 709/721/732 | the intrusive list |
| `xl_stamp` | 920 | the generation stamp, written before the hit test |
| `xl_touch` | 917 | hit/miss, one-shot useful count, promotion, residency |
| `xl_evict_tail` | 761 | the cold end, `ever_evicted` set |
| `xl_set_budget` | 675 | grow without eviction, clamp at the whole set, 1-byte floor, shrink loop |
| `xl_retain` | 777 | promote residents only; an absent id is prefetch's job |
| `xl_query` | 882 | 0 miss, 1 resident, 2 speculative |
| `xl_filter` | 434 | candidates: in range, not resident, not queued; duplicates kept |
| `xl_integrate` | 812 | `!valid && rem == 0`; cold for a guess, hot under route-ahead |
| `xl_account` | 648 | reset on a non-increasing layer |
| `xl_load_plan` | 1002-1025 | first occurrence stages, repeat promotes, invalid skipped |
| `xl_evict_load` | 1067 | the whole loop stops when the coldest entry is stamped |

### S2 `source/dense_place.goose` (prefix `dw_`)

| export | native anchor | decision |
|---|---|---|
| `dw_byte_ranges` | 20 | the dense set is the complement of the expert ranges, tail included |
| `dw_subtract` | 98 | remove `[a,b)`, splitting the one range it falls in |
| `dw_classify` | 128 then 172 | oversized → mmap, row-wanted → flash; `avail == 0` holds nothing back |
| `dw_verdict` | 52-79 | empty set checked before the pinned capability |
| `dw_span` | row_stream 57 | the reservation is the tensor size rounded to a page |
| `dw_resident_bytes` | 205-233 | what the warm sweep and the sensor still see |

### S3 `source/stream_plan.goose` (prefix `sp_`)

| export | native anchor | decision |
|---|---|---|
| `sp_round_budget` | 36 | 0 → default, below one slab → one slab |
| `sp_n_slabs`, `sp_slab_len` | 71, 109 | ceiling division; the last slab is short |
| `sp_gather_plan` | 170-200 | gen bump and wrap, invalid ids, straddle, dedup, missing at full slabs |
| `sp_evict` | 148 | oldest-first, stamped slabs skipped, stop when `resident + headroom <= budget` |
| `sp_fetch_order` | 207 | ascending file offsets, resident slabs are no-ops |
| `sp_materialize_ok` | 213 | a table larger than the window is refused |

`source/stream_policy.goose` imports the three. It exists only because the three
generated headers declare the same `sl_i64`/typedef set and cannot be included
side by side; it contains no policy.

## Gate

```
bash tests/build.sh
```

`tests/fixture.cpp` carries a transcription of the native functions (state
fields and step order kept, source line cited at every function) and drives the
same scripted scenarios through the Goose exports. A commit failure, MemAvailable
and takeover success are arguments on both sides, so the comparison is of
decisions, not of kernel answers.

```
FG_P9_03_A      cases=15 checks=80  failures=0
FG_P9_03_B      cases=37 checks=55  failures=0
FG_P9_03_C      cases=22 checks=63  failures=0
FG_P9_03_STREAM cases=74 checks=198 failures=0
```

The corpus:

- **S1, 15 cases**: miss/hit/evict order; the cache-off shared-slot branch; hit
  promotion; grow, clamp at the set size, shrink to the 1-byte floor; a budget
  below one entry; duplicates with promote-on-last; invalid ids; the generation
  guard; the prefetch filter (resident, queued, invalid, duplicate); integrate
  cold, hot and the in-flight skip; a speculative hit counted once; the fused
  layout with an absent projection; demand accounting across a token boundary
  and an unbound layer; a failed commit.
- **S2, 37 cases**: complementary ranges (single, unsorted, overlapping,
  touching, edges, empty, beyond the file, zero-length); subtraction (middle,
  head, tail, disjoint, enclosing, multi-range); placement over 4 to 6 tensors
  with an avail boundary, a failed takeover, an empty resident set and
  `avail == 0`; the verdict matrix (4 modes × resident/empty × pinned
  capability); the reservation span.
- **S3, 22 cases**: empty and invalid gathers, dedup, a 20 000-byte row across
  two slabs, a row whose last byte is the first byte of the next slab, a wide
  row, the short last slab, the generation wrap, a hot gather; eviction that
  fits, takes two, takes all, skips the oldest stamp, skips the newest stamp,
  and a headroom larger than the window; geometry over 8 sizes (slab count,
  short slab, span); 6 budget floors; 3 materialize rows; 2 fetch orders.

`tests/gate.log` is the recorded output of the last run.

## Mutations

```
python3 tests/mutate.py 0 6     # 6/6 caught
python3 tests/mutate.py 6 12    # 6/6 caught
```

Twelve semantic mutations, two halves of six, four per slice. A mutation that
breaks the build counts as caught; the source is restored after each and once
at the end.

| # | mutation | caught by |
|---|---|---|
| 0 | a speculative entry enters the LRU hot (`xl_back` → `xl_front`) | `a_integrate[1]`, `a_integrate[3]` |
| 1 | a hit no longer promotes its entry | `a_promote[3]` |
| 2 | the post-load eviction ignores the generation stamp | `a_gen_guard[0..1]` |
| 3 | `worst_cycle` is not clamped at `n_expert` | `a_*` (10 cases, state line) |
| 4 | an overlapping expert range moves the dense cursor backwards | `b_overlap` |
| 5 | `hold_back` runs without a measurable MemAvailable | `b_plan_avail0` |
| 6 | `subtract_range` emits a zero-length head remainder | `b_sub_head`, `b_plan_fit/tight/avail0` |
| 7 | the pinned capability is checked before the empty set | `verdict_ahwb_n0_p0` |
| 8 | a row ending on a slab boundary loses the next slab | `c_gather_boundary`, `c_gather_boundary2` |
| 9 | a stamped slab is the first eviction target | `c_evict_two/all/stamped/newest_stamped/headroom_big` |
| 10 | a budget below one slab is accepted | `budget_1`, `budget_16383` |
| 11 | a table exactly the window size is refused | `c_mat_equal` |

## Exact integration step

Canon paths (new files, no production edit in this assignment):

```
goose/model/expert-lru.goose
goose/model/dense-place.goose
goose/model/stream-plan.goose
goose/model/stream-policy.goose      # imports the three; composition unit
```

Build glue, in this order:

1. add `goose/model/stream-policy.goose` to the `core` set in
   `tools/check-composition.sh` and run `python3 tools/check-composition.sh`;
2. `python3 tools/compose-goose.py OUT.goose goose/model/{expert-lru,dense-place,stream-plan,stream-policy}.goose`
   (or let the composer concatenate; the imports resolve as siblings) and
   `goose check OUT.goose`;
3. `goose --standalone -o stream_policy.c --header stream_policy.h OUT.goose`,
   compile the C at `-O1` and link it with the runtime;
4. `python3 tools/check-goose-symbols.py --update` and commit the new export
   list (the guard fails on a canonical symbol that disappears, not on new
   ones, but the recorded set must match the tree).

Replaced call sites, with the parameters and the order they must be called in.
The state arrays are the existing native fields widened to `int64_t`; `lay` is
built once at init as `bound, nb2[0..2]` per layer.

| # | native site | becomes | order / parameters |
|---|---|---|---|
| 1 | `ExpertStreamSource::lru_unlink/push_front/push_back` (709/721/732) | `xl_unlink_e`, `xl_push_front`, `xl_push_back` | `(cprev_, cnext_, lru_st_, id)`; `lru_st_[2] = {chead_, ctail_}` replaces the two scalars and is written back by every call |
| 2 | `touch_entry` (917) | `xl_stamp` then `xl_touch` | stamp first (native writes it before the hit test); then, when `cvalid_[id] == 0`, call `commit_proj_pages` for the projections the caller owns; then `xl_touch(lay, cvalid_, cspec_, cprev_, cnext_, ever_, lru_st_, sacc_, n_expert_, il, e, promote, commit_only_proj, commit_ok)`. `commit_ok` is the commit result: the port cannot call `vm_commit`. Serial path `promote = 1`; overlap two-wave `promote = 0` with `commit_only_proj = p` |
| 3 | `evict_tail` (761) | `xl_evict_tail` | `(lay, cvalid_, cspec_, cprev_, cnext_, ever_, lru_st_, sacc_, n_expert_)`; `-1` means empty |
| 4 | `set_cache_budget` (675) | `quiesce_spec()` on the shrink arm, then `xl_set_budget` | `(…, bytes, cache_max_, total_expert_bytes_, cache_max_ == 0, n_expert_)`; returns the budget to assign to `cache_max_`. The grow arm needs no quiesce, and the adapter must keep not calling one |
| 5 | `retain` (777) | `xl_retain` | after the caller's `!active_ || cache_max_ == 0 || !bound` guard |
| 6 | `query_residency` (882) | `xl_query` | `(cvalid_, cspec_, il, ids, n_ids, n_expert_, cache_max_ == 0, scratch)`; the native `uint8_t*` is filled from an int64 scratch of `n_ids` |
| 7 | `prefetch` filter (434-441) | `xl_filter` | inside the `io_mtx_` block, replacing the filter loop; `out` is `spec_cand_` widened. `spec_remaining_` is an argument |
| 8 | `spec_integrate_done` (812) | `xl_integrate` | after the under-lock eligibility snapshot; `(…, done, ndone, spec_remaining_, n_expert_, spec_adopt_)` |
| 9 | `load_layer` dedup (1002-1025) | `xl_load_plan` | fill `kind[i]` first, then walk `ids` in order: 1 → stage, 2 → promote (cache on only), 0 → skip; `n_unique = out[0]` |
| 10 | post-load eviction (1065-1071) | `xl_evict_load` | `(…, cstamp_, cgen_, cache_max_, n_expert_)` after `read_*_ns_` is accounted, as natively |
| 11 | `account_demand` (648) | `xl_account` | `(acc, il, n_unique, entry_bytes(il))`, then assign `acc` back; called after staging, before the reads |
| 12 | `worst_cycle_bytes` (697) | `xl_worst_cycle` | `(lay, n_layer_, n_expert_, top_k)` at init and in the warning |
| 13 | `DenseWeights::byte_ranges` (20) | `dw_byte_ranges` | per shard; input copies are sorted in place by the port, so pass the unsorted pair arrays |
| 14 | `subtract_range` (98) | `dw_subtract` | one call per removed `[off, off+size)`, in tensor order (hold-back pass first, takeover second) |
| 15 | `hold_back_oversized` (128) + `take_row_gathered` (172) | `dw_classify` once | call after `RowStream::init` has settled `takeover_ok`, then apply: code 1 → `mapped_` + `dw_subtract`, code 2 → row stream + `dw_subtract`. Both `fprintf` lines stay in the adapter |
| 16 | `init` verdict (52-79) | `dw_verdict` | `(mode, tensors_.size(), pio::pinned_max_bytes() != 0)`: 0 → return false with the existing platform message, 1 → `read_anonymous`, 2 → `warm`, 3/4 → `advise_random_mapped` |
| 17 | reservation span (row_stream 57) | `dw_span` | `(size, pio::vm_page())` |
| 18 | resident bytes in the takeover log (205-208) | `dw_resident_bytes` | `(sizes, codes, n)` |
| 19 | `RowStream::init` budget (36) | `sp_round_budget` | `(budget_bytes, slab_bytes, default_budget_bytes)` |
| 20 | `t.n_slabs` (71) | `sp_n_slabs` | `(size, slab_bytes)` |
| 21 | `slab_len` (109) | `sp_slab_len` | `(size, s, slab_bytes)` |
| 22 | `gather` plan (179-200) | `sp_gather_plan` | `(n_rows, row_bytes, ref.size, idx, n, n_slabs, slab_bytes, stamp, gen_, resident, need_, out)`; then `gen_ = out[2]`, `evict_to_budget(out[1])`, fetch `need_[0..out[0])` |
| 23 | `evict_to_budget` (148) | `sp_evict` | `(resident, stamp, gen_, budget_bytes_, resident_bytes_, headroom, order, n_order, bytes, keep, out)`; replaces the `std::list` with a flat `order_` array and returns the surviving order |
| 24 | fetch order (207) | `sp_fetch_order` | `(need, n, resident, out)` before the `fetch_slab` loop |
| 25 | `materialize` limit (213) | `sp_materialize_ok` | `(ref.size, budget_bytes_)`; 0 → the existing refusal and `io_errors_` bump |

Preservation requirements at acceptance: the serial and overlap paths must keep
their own job emission (the port shares only the accounting), `chits_` must not
be incremented by `xl_retain`, the stamp must be written before any commit, and
`quiesce_spec()` must stay on the shrink arm of the budget change.

## Compatibility checks performed

1. `goose check` on each slice and on the composed unit — typechecks.
2. No duplicate export: the build greps the generated header and fails on a
   repeated `xl_`/`dw_`/`sp_` name.
3. The differential gate: 74 cases, 198 checks, 0 failures.
4. The mutation run: 12/12 caught in two halves.
5. Production untouched by this experiment: no tracked file under `runtime/`,
   `goose/` or `tools/` was edited here, and the experiment lives in this
   directory. During the run a concurrent agent modified `tools/compose-goose.py`
   (a `RENAMES` entry, timestamp 01:38:47) and advanced `HEAD` to `480e4d77`;
   that change is not part of this work.

## Assumptions and conflicts

- **Types at the boundary.** `size_t`/`uint32_t` fields become `int64_t` at the
  call. `cvalid_`/`cspec_`/`ever_evicted_` widen from one byte to eight; at 40
  layers × 128 experts that is ~256 KiB more, and the overlap hook's byte reads
  must be adapted. If that matters, add a `u8` export variant rather than
  keeping a copy of the state.
- **`xl_touch` does not commit.** It takes the outcome. Integration must commit
  between the stamp and the call, which is a reordering of the native body
  (stamp → hit test → commit → residency) into (stamp → hit test → commit →
  `xl_touch` → residency). The hit test is repeated once (`cvalid_[id]`); the
  gate pins the resulting state.
- **`xl_set_budget` returns the budget** instead of assigning the field, and
  assumes the caller ran the quiesce on the shrink arm.
- **`xl_query` writes int64 codes**; the native `uint8_t*` needs a scratch or a
  narrow variant.
- **`sp_evict` replaces the container.** The native LRU is
  `std::list<std::pair<size_t, uint64_t>>` plus `spot` iterators; the port walks
  a flat oldest-first array and returns the survivors. Integration either adopts
  the flat array (recommended; the gate compares order, resident bytes and the
  stamp guard) or adapts the list on every call.
- **One unit, not three.** Both `goose` and C reject two of the three headers in
  one translation unit (identical typedefs), so the runtime must link the
  composed unit, not three independently generated ones.
- **`cgen_` is 32-bit.** The port masks to 32 bits; an integration that widens
  the generation must widen the stamp array and the mask together.
- **The transcription is a replica.** The gate links no llama.cpp and no CUDA,
  so it compares the port against a transcription of the native decisions, not
  against the native binary. The 12 mutations and 198 checks bound the risk;
  they do not remove it.
- **No performance claim.** Nothing here measures tok/s, RSS or I/O. The port
  adds a few array walks per call; whether the composed unit is faster or slower
  is a separate measurement and is not asserted.

## What stays native

- the file: `FileReader`, `O_DIRECT`/`FCNOCACHE` open decisions, `pread`,
  bounce buffers, shard dispatch;
- the kernel: `vm_reserve`, `vm_commit`, `vm_evict`, `madvise`, `mincore`,
  `/proc/self/maps` resolution, `drop_mmap_copies`, the pinned dma-buf allocator
  and `pinned_max_bytes`;
- the threads: the I/O lane pool, `ReadyFlag` publication and the spin/wait in
  `on_expert_ready`, `StallUnion`, the drain/adopt waits and their counters;
- the graph and ABI: `RouterHook`'s capture pass (which tables qualify as
  row-gathered), the ggml `tensor->data` rebinds, `mul_mat_id` and every CUDA
  copy;
- telemetry plumbing: `IoJob` attribution and `IoTrRow`, `maybe_sample_dense`
  throttling, `mgmt_ns_`/`read_ns_` timers;
- the commit/evict bytes arithmetic (`round_up(len, page)` vs `span`) is modelled
  only through `dw_span`/`sp_slab_len`; the actual calls stay native.

## Recorded limits

- The corpus is synthetic and table-driven; it is not a capture of a real
  router trace from a model.
- `sp_evict` cases use one table of four slabs plus a multi-order case; the flat
  id space covers multi-table order by construction, but the fixture does not
  build two tables at once.
- Sorting in the port is a stable insertion sort; the native `std::sort` is not
  stable, but it sorts `pair` values, so equal keys are indistinguishable and the
  results agree.
- `dw_classify` merges two native passes into one call; the *decision* order
  (hold-back before takeover) is preserved inside it, but the native log lines
  and the `RowStream::init` call between the passes stay in the adapter.
- No alignment/O_DIRECT block-size effect is modelled: `dw_span`/`sp_slab_len`
  take the page/slab as arguments.
- Large-address-space arithmetic is `int64_t`; models with a tensor above 8 EiB
  are out of scope.

## Files

| path | what |
|---|---|
| `source/expert_lru.goose` | S1, the expert cache policy |
| `source/dense_place.goose` | S2, dense placement and source selection |
| `source/stream_plan.goose` | S3, the row-stream plan |
| `source/stream_policy.goose` | the composition unit (imports only) |
| `tests/fixture.cpp` | the native transcription and the corpus |
| `tests/build.sh` | combined build at -O1 (mutations at -O0), gate, export check |
| `tests/mutate.py` | the twelve mutations, `mutate.py [lo hi]` |
| `tests/gate.log` | recorded gate output |
| `REPORT.md` | result, numbers, hashes |

`build/` and the generated `g.c`/`g.h` are reproducible and not part of the
experiment.

## SHA-256

```
cfdf41c3e97a899aa055a5f7d84e201dfa2272430b06f078d46403f61b028212  source/dense_place.goose
72eae3cac7c4fa704947bea734d40d8b60fe49be759cc94a40cc2e73bff26010  source/expert_lru.goose
12abe5892f594862d133a7d48528d98fbef5d0fb9e7ee8baafe19e12fdedbb2a  source/stream_plan.goose
ad5cd245bd49adc9907f603d32a5df13f2db335986ed39993acb804242283e6e  source/stream_policy.goose
a54be101318eb794591e6973f38a9840dc1ccfb29b68ab76facdedb67f3c0b59  tests/fixture.cpp
12d6d4e1f5f84ec9d57ac70b9a934885d920a6afd6c56f108a8756b571c77771  tests/mutate.py
61273e3fbf508fbef19a5863d9fa607505a3624d327c741ae7cc9e39a7cbc797  tests/build.sh
1b3f91cc338d040c0e98598be3b85750c083a5565812512651cb3e3bda0171a6  tests/gate.log
```
