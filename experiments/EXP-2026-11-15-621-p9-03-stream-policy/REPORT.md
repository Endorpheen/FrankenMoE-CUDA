# REPORT — EXP-2026-11-15-620 (P9-03)

Date: 2026-11-15. Base commit: `df3ce73a` (sources unchanged at `d0b6e258`).
Scope: dense weights and expert streaming/LRU sources — the decisions only.

## Verdict

**READY FOR INTEGRATION** (three slices, gate green, 12/12 mutations caught).
The experiment is an isolated working copy under
`/tmp/opencode/sub-623/EXP-2026-11-15-620-p9-03-stream-policy`; nothing in
`/home/end0/AI/FrankenMoE-CUDA` was modified by this task (concurrent work on
`tools/compose-goose.py` and `HEAD` is noted under compatibility checks).

## Numbers

| slice | file | cases | checks | failures |
|---|---|---:|---:|---:|
| S1 expert LRU/cache policy | `source/expert_lru.goose` | 15 | 80 | 0 |
| S2 dense placement | `source/dense_place.goose` | 37 | 55 | 0 |
| S3 row-stream plan | `source/stream_plan.goose` | 22 | 63 | 0 |
| **total** | `source/stream_policy.goose` (imports only) | **74** | **198** | **0** |

```
FG_P9_03_A      cases=15 checks=80  failures=0
FG_P9_03_B      cases=37 checks=55  failures=0
FG_P9_03_C      cases=22 checks=63  failures=0
FG_P9_03_STREAM cases=74 checks=198 failures=0
```

Mutations: `python3 tests/mutate.py 0 6` → 6/6 caught;
`python3 tests/mutate.py 6 12` → 6/6 caught. Total **12/12**, four per slice,
two halves of six.

Gate command: `bash tests/build.sh` (the module is generated and compiled at
`-O1`, the fixture at `-O1`; mutations run the same path at `-O0`). No model, no
GPU, no disk read, no thread.

## Interfaces the port relies on

These are the canon/native contracts it must be given; none of them is
re-implemented here.

| interface | where | used for |
|---|---|---|
| `MoeRecipe::max_exps == 3` | `runtime/core/include/bmoe/recipe.h` | the `lay` stride and projection loop |
| `IExpertSource` (`load_layer`, `prefetch`, `retain`, `settle_spec`, `query_residency`, `expert_bytes`, `stats`) | `bmoe/expert_source.h` | the call sites the exports replace |
| `IExpertSource::Stats` counters (`cache_hits`, `cache_lookups`, `cache_resident_bytes`, `evictions`, `rereads`, `spec_useful`, `cache_budget_bytes`, `cache_resizes`) | same | the `sacc` array |
| `DenseWeightsMode` `{Mmap, Warmed, Anonymous, Pinned}` | `bmoe/config.h` | `dw_verdict`'s mode argument |
| `DenseTensorRef {tensor, file_off, size, file_idx}` | `moe/dense_weights.h` | `dw_classify`/`dw_subtract` inputs |
| `IRowSource` / `RowSourceStats` | `bmoe/row_source.h` | the row-policy surface whose decisions S3 owns |
| `pio::mem_available_bytes`, `pio::pinned_max_bytes`, `pio::vm_page` | `io/platform_io.h` | the three kernel answers passed as arguments (`avail`, `pinned_ok`, page) |
| `FileReader`, `pio::vm_*` | `io/file_reader.h`, `io/platform_io.h` | stay native; the commit outcome is the `commit_ok` argument |

The port owns the decisions listed in README's per-slice tables; it takes no
`ggml_tensor`, no file descriptor and no mutex.

## Exact integration step

Canon files to add (no production edit was made in this assignment):

```
goose/model/expert-lru.goose
goose/model/dense-place.goose
goose/model/stream-plan.goose
goose/model/stream-policy.goose     # imports the three; no policy inside
```

Order of operations:

1. `python3 tools/check-composition.sh` after adding `stream-policy.goose` to the
   `core` set — confirms the composed unit typechecks.
2. `python3 tools/compose-goose.py OUT.goose goose/model/{expert-lru,dense-place,stream-plan,stream-policy}.goose`
   then `goose check OUT.goose` (the composer drops the resolved local imports).
3. `goose --standalone -o stream_policy.c --header stream_policy.h OUT.goose`,
   compile that C at `-O1`, link it into the runtime binary.
4. `python3 tools/check-goose-symbols.py --update` and commit the recorded
   export set.
5. Replace the 25 call sites of README's integration table. The load-bearing
   ones, in call order:
   - `init`: `xl_worst_cycle`, `dw_byte_ranges`, `dw_classify`, `dw_verdict`,
     `dw_span`, `sp_round_budget`, `sp_n_slabs`;
   - per load: `xl_load_plan` → per id `xl_stamp`, the native
     `commit_proj_pages`, `xl_touch` → `xl_account` → `xl_evict_load`;
   - prefetch settle: `xl_filter`, `xl_integrate`;
   - policy: `xl_retain`, `xl_query`, `xl_evict_tail`, `xl_set_budget`
     (shrink arm after `quiesce_spec()`), `xl_worst_cycle`;
   - row stream: `sp_gather_plan` → `sp_evict` → `sp_fetch_order`,
     `sp_materialize_ok`, `sp_slab_len`;
   - dense: `dw_subtract`, `dw_resident_bytes`.

Parameters, accepted/taken types and the required ordering are in README
("Exact integration step", rows 1-25). The two contract changes that must not be
lost: `xl_touch` takes the commit *outcome* (commit before the call), and
`xl_set_budget` *returns* the budget (assign it) with the quiesce kept in the
caller on the shrink arm.

## Compatibility checks

| check | result |
|---|---|
| `goose check` per slice (3 files) | ok |
| `goose check` on the composed unit | ok |
| duplicate exports in the generated header | none (`xl_*`, `dw_*`, `sp_*` disjoint) |
| differential gate | 74 cases / 198 checks / 0 failures |
| mutation run, two halves | 6/6 and 6/6 |
| production tree | untouched by this experiment. Concurrent work modified `tools/compose-goose.py` (a `RENAMES` entry, 01:38:47) and advanced `HEAD` to `480e4d77`; not this task |
| files staged by the experiment | 8 tracked artifacts, no generated `.c`/`.o` |

## Assumptions and possible conflicts

- Type widening at the boundary: `size_t`/`uint32_t` → `int64_t`;
  `cvalid_`/`cspec_`/`ever_evicted_`/`seen_` widen 1 → 8 bytes per entry
  (~256 KiB at 40×128). The overlap hook reads `cvalid_` as bytes and must be
  adapted, or a `u8` variant added.
- `sp_evict` replaces `std::list<pair<size_t,uint64_t>>` + `spot` with a flat
  order array; this is the one structural change.
- `lru_st_[2]` replaces `chead_`/`ctail_`; every list call writes it back.
- `cgen_` stays 32-bit and is masked; widening it requires widening the stamp
  array with it.
- Three modules, one unit: separate generated units collide on the header
  typedefs, so the runtime links the composed unit.
- The gate's oracle is a transcription, not the linked native binary; the
  198 checks and 12 mutations bound that risk, not remove it.
- No interaction with P9-04 (router hooks, I/O-compute overlap) is assumed
  beyond the `IExpertSource` surface: the hook, the readiness flags and the lane
  pool remain native and are called exactly where they are called today.

## What remains native (honest list)

- file: `FileReader`, `O_DIRECT`/`F_NOCACHE` decisions, `pread`, bounce buffers,
  shard dispatch;
- kernel: `vm_reserve/commit/evict`, `madvise`, `mincore`, `/proc/self/maps`,
  `drop_mmap_copies`, pinned dma-buf, `pinned_max_bytes`;
- threads: the I/O lane pool, `ReadyFlag` publication and waits, `StallUnion`,
  drain/adopt waits;
- graph/ABI: `RouterHook` capture, ggml `tensor->data` rebinds, `mul_mat_id`,
  every CUDA copy;
- telemetry: `IoJob` trace attribution, `mgmt_ns_`/`read_ns_` timers, the
  throttled dense-residency sample;
- commit/evict byte arithmetic (only `dw_span`/`sp_slab_len` are modelled).

## Artifacts and SHA-256

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

Command to reproduce:

```
bash tests/build.sh
python3 tests/mutate.py 0 6
python3 tests/mutate.py 6 12
```

## Next step

Integrate S1 into `expert_stream_source.cpp` first (it is the largest decision
surface and the only one with an ordering contract), then S2 and S3, running
`tools/check-composition.sh`, `tools/check-goose-symbols.py` and the correctness
gates before any performance run. P9-04 (row streaming, router hooks and
I/O-compute overlap) then owns the parts this experiment left native by design.
