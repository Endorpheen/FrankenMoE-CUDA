# Inventory of the performance modules — what runs on Goose, what does not, and why

Base: `0db34264`. Every number here is measured or read from a binary, not estimated.

## 1. How the counts were taken

The FG reports are **cumulative progress lines**, not per-event logs: a module prints
`calls=1000`, then `calls=2000`, and so on. Summing the lines inflates everything by orders of
magnitude, which is why the first comparison showed `FG401S_DETOK_DECISIONS` at 75 million. The
correct figure is the **maximum** `calls=` value per `module[mode]`.

A second correction: reports are per **mode**, not per module. `FG149_POOL mode=6` at 316 164
and `FG149_POOL mode=16` at 45 274 are unrelated counters; comparing module totals hides that.

## 2. The modules that matter, by measured call count

| module[mode] | reference | ours | what it is |
|---|---:|---:|---|
| `FG148_QUANT[9]` | 482 400 | **0** | quantised dot product (Q4_K/Q4_0 row kernels) |
| `FG149_POOL[6]` | 316 164 | **612** | thread/job pool dispatch |
| `FG147_BACKEND[30]` | 180 626 | **274** | backend buffer/op dispatch |
| `FG150_EXPERT` | 135 360 | **0** | expert selection / MoE routing path |
| `FG149_POOL[16]` | 45 274 | 290 | pool, second mode |
| `FG143_CPU[100]` | 45 120 | **0** | CPU executor |
| `FG149_POOL[15]` | 45 120 | **0** | pool, third mode |
| `FG147_BACKEND[31]` | 22 404 | 612 | backend, second hot mode |

Everything else matches within a factor of two, or is newer Goose routes that only our build
has (FG375-FG408 family, 113 extra symbols in `libllama`).

## 3. Why FG148, FG149 and FG150 are silent — the actual finding

The four hot entry points are **exported but never referenced inside the accepted library**:

```
fg148_row_q4_0      internal_calls=0  addr_refs=0   (T, exported)
fg148_encode        internal_calls=0  addr_refs=0
fg150_expert_config internal_calls=0  addr_refs=0   (t, local)
fg149_pool_control  internal_calls=0  addr_refs=0   (T, exported)
```

No `call` instruction, no address reference, no entry in a data table. They are not gated by a
flag - they are **unreachable from inside that library**. The only way they can run is if
someone outside calls the exported ones, and nothing in our tree does.

So the accepted `libggml-cpu` in stage-run is not the build that produced the reference
counters. The reference loaded its CPU library from `runtime/exp160/run/`, which no longer
exists; only `runtime/exp160/build/*.o` remains, and none of those four object files references
`fg148_row_q4_0` either.

This is the single most important fact about the decode gap: **the exact binary that produced
2228.76 / 41.62 is not on this machine.** What we have is a build with the same hash as the one
the reference *recorded*, but whose hot paths are wired differently - the hash match is on the
file, and the file's internal wiring does not produce those counters.

## 4. What that means for restoring the speed

Restoring decode to 40+ tok/s by re-enabling these paths is not a configuration change. Three
options, honestly stated:

1. **Rebuild the CPU backend from its Goose sources** with the quantised-dot, pool and expert
   routes wired in, i.e. do the migration properly: take the Goose modules
   (`goose/cpu/goose-cpu-q4k-dot*.goose`, `goose-cpu-pool`, `goose/cpu/goose-moe-control`) and
   generate the CPU library from them instead of using the prebuilt binary. This is the path
   the mandate actually asks for - every module ends up implemented in Goose, with no C++
   dependency - and it is also the only one that guarantees the hot paths exist.
2. **Find the missing exp160 library.** It is not in the tree, not in checkpoints, and its
   `.so` cannot be reconstructed from the four `.o` files left behind. Not viable.
3. **Accept 34.95 decode.** Explicitly rejected: the mandate sets 40+ as mandatory.

Option 1 is the work. It is also the work that removes the last C++ dependency from the hot
path, which is the migration goal itself.

## 5. Language and dependency of each hot module

| module | implementation | currently needed | migrated to Goose? |
|---|---|---|---|
| `FG148_QUANT` | Goose sources exist (`goose-cpu-q4k-dot`, `q6k-dot`, `quant-batch`) | yes, hot in decode | **sources yes, wiring no** |
| `FG150_EXPERT` | Goose sources exist (`goose-moe-control`, `goose-quantized-moe`) | yes, hot in decode | sources yes, wiring no |
| `FG149_POOL` | Goose sources exist (`goose-cpu-pool`, `goose-cpu-job-pool`) | yes, hot in prefill | sources yes, wiring no |
| `FG160` fusion | called by the accepted build; provided by base libs | yes | partially - 7 call sites exist |
| `FG147_BACKEND` | Goose graph builders | yes | provenance of binding, fine |

The pattern is consistent: **the Goose source for every missing hot path already exists in this
repository.** What is missing is the generated CPU library that calls them.

## 6. Next concrete step

Build `libggml-cpu` from `goose/cpu/*` with the quantised-dot, pool and expert routes wired,
then measure. The acceptance test is unchanged: content sha256 must stay
`9d90a010...1ba7560`, prefill must not drop below 2222, and decode must reach 40+.

This is a build-from-Goose task, not a configuration task, and it removes C++ rather than
adding it - so it satisfies the "no new C++ dependency" criterion by construction.
