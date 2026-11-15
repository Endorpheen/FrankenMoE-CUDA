# EXP-2026-11-15-649 — P10-12: tallies, prefill plan, trace order, moe-predict report

Verdict: **ACCEPTED candidate** (model-free, differential). The lead's acceptance is the
commit gate. Base commit: `1b3f4a1a` (`docs: STATUS after P10-05/P10-08/P10-10/P10-12`).

Work was done outside the repository, in
`/tmp/opencode/sub-650/EXP-2026-11-15-649-p10-12-tallies-prefill-trace/`. The shared tree,
the production runtime and other agents' files are untouched. No model, GPU, server or
benchmark was started.

## Hypothesis

`experiments/EXP-2026-11-15-647-p10-12-residual-io/REPORT.md` §7 recommends closing the four
model-free residual regions that its audit left open in `engine/session.cpp` and
`cli/main.cpp`: G1 (`GenTally`/`PrefillTally`), G2 (prefill chunk plan), G3 (trace framing
order) and G6 (`moe-predict` report). The hypothesis was that all four are pure arithmetic,
ordering or formatting decisions, so each can be ported as one Goose module and pinned by a
differential gate whose native arm is the carrier's own text.

The hypothesis held. What the gates had to be careful about is not the logic but the
*observation*: `session.cpp` cannot be linked (it pulls llama.cpp, the hooks, the sinks), and
half of the regions live in an anonymous namespace or inside `generate()`. The answer is a
verbatim extractor: `tests/native_extract.py` copies the exact byte ranges out of the base
commit into `build/native_*.{h,inc}`, and each fixture `#include`s them with stubs for the
services the port deliberately does not own. Nothing in the native arm is retyped.

## Slices

| # | slice | prefix | native region | module |
|---|---|---|---|---|
| S1 | generation and prefill accounting | `tl_` | `session.cpp` 157-251, 259-285, 1934-1935, 1945-1952, 1967-1968 | `source/tallies.goose` |
| S2 | prefill chunk plan | `pf_` | `session.cpp` 1541-1542 | `source/prefill-plan.goose` |
| S3 | trace framing order | `tr_` | `session.cpp` 1485-1530 | `source/trace-frame-order.goose` |
| S4 | `moe-predict` report | `mp_` | `cli/main.cpp` 540-588 | `source/moe-predict-report.goose` |

All four prefixes are free in the active tree: no `export fn` in `goose/` starts with `tl_`,
`pf_`, `tr_` or `mp_`, and none of the four file names exists outside `_archive/`. The only
`tr_` anywhere in the active tree is a local `let tr_ref` in
`goose/cuda/flint-kv-read-header.goose`, which is not an export.

## S1 — `tl_`: `GenTally`, `PrefillTally`, the summary averages

`GenTally` is the decode phase's running measurement; its fields are cumulative across a warm
session, so a token's flash figures are deltas against the previous token's sample and the
totals are what the summary averages over `n_gen`. The port takes the cursors and totals as
arguments and returns both the row and the advanced state, one call per token.

`tl_record(...)` → `out_f` (23 values used) and `out_i` (6 values):

| out_f | field | out_i | field |
|---:|---|---:|---|
| 0 | `wall_ms` | 0 | `majflt` |
| 1 | `cpu_ms` | 1 | `turn` |
| 2 | `majflt_mib` | 2 | `read_bytes` delta (signed) |
| 3 | `dense_resident_frac` | 3 | total `majflt` |
| 4 | `cache_budget_mib` | 4 | total `read_bytes` (signed) |
| 5 | `io_ms` | 5 | next `prev_bytes` cursor |
| 6 | `mgmt_ms` | | |
| 7 | `compute_ms` (clamped at 0) | | |
| 8 | `cache_hit_pct` (-1.0 when no lookups) | | |
| 9 | `stall_ms` (overlap only) | | |
| 10 | `drain_ms` | | |
| 11 | `adopt_ms` | | |
| 12-16 | totals `io_s`, `mgmt_s`, `stall_s`, `drain_s`, `adopt_s` | | |
| 17 | total `cpu_s` | | |
| 18-22 | next cursors `io_s`, `mgmt_s`, `stall_s`, `drain_s`, `adopt_s` | | |

`tl_record` arguments, in order: `overlap, prev_bytes, prev_io_s, prev_mgmt_s, prev_stall_s,
prev_drain_s, prev_adopt_s, wall, faults, cpu_s, turn, st_on, st_read_bytes, st_read_s,
st_mgmt_s, st_stall_s, st_cache_hits, st_cache_lookups, st_drain_s, st_adopt_s,
st_dense_frac, st_cache_budget_bytes, fault_bytes, acc_majflt, acc_cpu_s, acc_read_bytes,
acc_io_s, acc_mgmt_s, acc_stall_s, acc_drain_s, acc_adopt_s, out_f, out_i`.

`tl_prefill(moe_on, cpu0, cpu_now, pre_*, post_*, out[5])` closes the prompt bracket:
`cpu_seconds`, `read_mib`, `io_seconds`, `stall_seconds`, `mgmt_seconds`. The CPU delta is
measured even when streaming is off, as the native `end()` does.

`tl_summary(moe_on, n_gen, majflt, cpu_seconds, read_bytes, io_s, mgmt_s, stall_s, drain_s,
adopt_s, s_per_token, overlap, out[9])` → `majflt_per_token`, `cpu_s_per_token`,
`moe_read_mib`, `moe_io_s_per_token`, `moe_mgmt_s_per_token`, `moe_stall_s_per_token`,
`moe_compute_s_per_token` (clamped), `moe_drain_s_per_token`, `moe_adopt_s_per_token`; the
moe block is zeroed when `moe_on == 0`, matching the native `if (moe.enabled)`.

Not owned, stays with the host: the `/proc` and device samples (RSS, swap, `mem_available` —
`pr_` already owns the text grammar), `process_cpu_seconds()` itself, and `fault_bytes()`.

## S2 — `pf_`: the prefill chunk plan

`pf_plan(n_common, n_prompt, n_batch, out)` writes `(i, chunk)` pairs, `out[2k] = i` and
`out[2k+1] = chunk`, and returns the pair count. Positions are absolute, so the first pair's
`i` is the reused prefix and the frame's `base_pos`. No chunk is planned when
`n_common >= n_prompt`.

`n_batch >= 1` is a precondition the session's config validator already enforces; the module
repeats it as a guard so a caller that skips validation cannot hang, and the gate never
exercises it because the native loop could not terminate.

## S3 — `tr_`: the trace framing order

`tr_begin(route, compute, base_pos, n_tokens, phase, turn, out)` and
`tr_flush(route, compute, io, route_rows, compute_rows, io_rows, phase, step, turn, out)`
return an ordered plan; events are five `i64` slots (`op, a0, a1, a2, a3`):

| op | event | arguments |
|---:|---|---|
| 1 | `SET_BATCH_PHASE` | phase |
| 2 | `BEGIN_DETERMINISTIC` | base, n, phase |
| 3 | `BEGIN_TRACE_BATCH` | base, n, phase, turn |
| 4 | `BEGIN_COMPUTE_BATCH` | base+n-1, phase, turn |
| 5 | `END_COMPUTE_BATCH` | — |
| 6 | `END_TRACE_BATCH` | — |
| 7 | `ROUTE_ROWS` | count (absent when the frame is empty) |
| 8 | `COMPUTE_ROWS` | count |
| 9 | `IO_TAKE_ROWS` | count (always, empty or not) |
| 10 | `IO_STAMP_ROWS` | count, turn, phase, step |
| 11 | `IO_ROWS` | count |

`tr_stamp(base_pos, n_tokens, phase, out[2])` returns the frame's `phase` and `step` — the
`trace_phase`/`trace_step` the I/O rows carry. The two ordering decisions the native comments
state are the ones the plan keeps: the phase reaches the drop policy before any trace call,
and the compute interval closes before anything is handed over.

## S4 — `mp_`: the `moe-predict` report

`mp_report(head[12], stale2[4], unscored, n_layers, per_layer[12*n], out)` writes the report
into `out` and returns its length, newline-terminated. `head` is `rows, slots, hits, exact`
for the stale gate, then prev-token, then the fresh-gate control; `stale2` is the same four
for the two-layer horizon; `per_layer` is the same twelve per layer.

One conversion stays libc: `%.1f`, through the `mp_fmt_f1` extern, the same concession
`mt_fmt_g6` makes in `metrics/metrics-traces.goose`. The shim writes at an offset inside `out`
and returns the characters written; the port owns the six-column padding, the `-` rule, the
row layout and the em dash in the "scored" line.

## Gate

Four gates, one per slice, at `-O1` and `-O0`; the summary is identical.

```
FG649_TL cases= 28 values= 847 failures=0      GenTally / PrefillTally / summary
FG649_PF cases= 16 values= 432 failures=0      prefill chunk plan
FG649_TR cases= 42 values= 433 failures=0      trace framing order
FG649_MP cases= 15 values=1064 failures=0      moe-predict report
```

101 cases / 2776 compared values, 0 failures, identical at both optimisation levels. Double
columns are compared bit-exactly (`memcmp`), because both arms execute the same IEEE-754
operations in the same order; integer columns are compared exactly.

The native arm is the base-commit text, extracted at build time:

| extracted file | native lines | SHA-256 |
|---|---|---|
| `build/native_gen_tally.h` | `session.cpp` 157-251 | `08fb1c202e0575cb43d2e9ff55b90d5ebcfba50bdc808d6237f1a7b833b4a742` |
| `build/native_prefill_tally.h` | `session.cpp` 259-285 | `e647497c903be8be202229179cb458dc05240b8deaca626f75fb59e1ca523ce6` |
| `build/native_prefill_loop.inc` | `session.cpp` 1541-1542 | `96dd7030c1abfd1fcc591cd71a92da8eb1157388be8fca4f56d20447d6b9be84` |
| `build/native_trace_begin.inc` | `session.cpp` 1485-1496 | `148db4f7c674f2cc17272a8c7759ba071a574c1f417e591bf50b84d956093f3b` |
| `build/native_trace_flush.inc` | `session.cpp` 1497-1530 | `ce41bd2aaddf271d4a7730e579cb2e553b5c413134742feb644dbef8dfc47f1e` |
| `build/native_predict_report.inc` | `cli/main.cpp` 540-588 | `a5ad4bfbbcba0d258a055a6604941cbdd33a3755779984a547c55d4cbd72449c` |
| `build/native_summary_a.inc` | `session.cpp` 1934-1935 | `3838e1134ea93b3343b001b21e7f3ddf47d15a582bbc4cc6a81398437fd9d650` |
| `build/native_summary_b.inc` | `session.cpp` 1945-1952 | `de6c34a385d40bfaf9ed69298811e8a70b714f6f95fca51c2b7d9d564c0c2671` |
| `build/native_summary_c.inc` | `session.cpp` 1967-1968 | `32c7058d85ed5fa5e4f7185369a3d86a4db5ee87eea90e84fafee24df0c99da0` |

Native files at the base commit: `session.cpp`
`24ea1ce4f796a49efa3a65d9f2cae7cc85f83abd781190451823cc090fb33593`, `cli/main.cpp`
`baa36d47bcee47b7a212407a71faf1168d4877071d49369a1bc7936ef55b4e72`.

The extractor anchors on an exact line prefix and fails when an anchor is missing or appears
twice, so a rebase that moves these regions is a loud build error rather than a silently
empty oracle.

What each gate stubs is exactly what the port does not own:

1. `tl`: `pio::process_memory`/`device_memory` return false (the RSS/swap columns belong to
   `pr_` and are not compared), `pio::process_cpu_seconds` reads a settable clock so
   `PrefillTally`'s CPU delta is reproducible, and `pio::fault_bytes` is the host page size —
   the gate asserts the stub equals `sysconf(_SC_PAGESIZE)`, so the constant is still pinned
   to the platform.
2. `pf`: nothing is stubbed; the loop body that records `(i, chunk)` is the only fixture code.
3. `tr`: the hook and the three sinks are recorders that push the same event codes; the I/O
   stamp is read back from the row the native loop stamped.
4. `mp`: only `mp_fmt_f1` (`%.1f`); the real `bmoe/predict_stats.h` supplies
   `hit_frac()`/`exact_frac()`, and the report is captured from the carrier's own `printf`
   sequence through a temporary file descriptor.

## Mutations

12 per slice, two halves of six, module regenerated at `-O0` (gate code stays at `-O2`):

| half | tl | pf | tr | mp | total |
|---|---:|---:|---:|---:|---:|
| 1 | 6/6 | 6/6 | 6/6 | 6/6 | 24/24 |
| 2 | 6/6 | 6/6 | 6/6 | 6/6 | 24/24 |

48/48 caught, 0 survived, 0 build failures. The table is `tests/mutate.py`
(`python3 tests/mutate.py 1|2 --jobs 4`). Examples: the millisecond conversion becomes
centiseconds, the fault bytes drop out of the MiB column, the compute clamp stops firing, the
no-cache sentinel becomes 0.0, the overlap residual subtracts io, the drain delta is taken
against the stall cursor, the prefill total is in KiB, the plan skips the reused prefix, the
loop advances two batches, the tail chunk is one token long, the frame sets the phase after
the trace calls, the compute interval is not closed, an empty frame still hands rows over, the
io stamp loses its phase/turn, the table column is five wide, a dense layer prints a row, the
control warning threshold drops to 0.998.

Three first-round findings, all fixed rather than relabelled:

1. `mp_pct` originally divided two `i64` operands: this language's `/` on integers is integer
   division, so the row column read 0.0 wherever the aggregate read 75.0. The gate caught it;
   both operands are now declared `f64` explicitly (same for `mp_hit`/`mp_exact`).
2. The `pf` mutation "advance one short of the batch" hangs the port for `n_batch == 1`, which
   is an infinite loop, not a caught mutation; it was replaced by "advance two batches".
3. The `mp` threshold mutation survived at first because the corpus only had 0.999 and 1.0;
   a 0.9985 case was added, which is the interval between the two predicates.

## Exact integration step

1. Copy the four modules next to the other engine slices:

   ```
   cp source/tallies.goose            goose/misc/tallies.goose
   cp source/prefill-plan.goose       goose/misc/prefill-plan.goose
   cp source/trace-frame-order.goose  goose/misc/trace-frame-order.goose
   cp source/moe-predict-report.goose goose/misc/moe-predict-report.goose
   ```

2. In `tools/build-main-runtime.sh`, append to `MODULES` after
   `goose/misc/kv-turn-state.goose`:

   ```
   goose/misc/tallies.goose
   goose/misc/prefill-plan.goose
   goose/misc/trace-frame-order.goose
   goose/misc/moe-predict-report.goose
   ```

   No `RENAMES` entry is needed in `tools/compose-goose.py`: every helper and constant is
   prefixed (`tl_`/`TL_`, `pf_`/`PF`, `tr_`/`TR_`, `mp_`/`MP_`). A composed single-unit check
   of the four files together passes with 35 top-level declarations and no duplicate symbol.

3. In `runtime/cli/main.cpp`, `print_predict_report` keeps its signature and fills the flat
   arrays from `RunSummary`, then writes the port's bytes to `stdout` in one `fwrite`; the
   `mp_fmt_f1` shim is the existing fixed-point helper or a three-line `snprintf` wrapper.

4. In `runtime/core/src/engine/session.cpp`, `GenTally::record` becomes a call into
   `tl_record` that unpacks `out_f`/`out_i` into `m` and the member cursors (the dense-only
   branch is `st_on == 0`); `PrefillTally::begin/end` become `tl_prefill`; the summary block
   calls `tl_summary`; the chunk loop uses `pf_plan` and keeps `llama_batch`/hook calls; the
   two trace lambdas become a loop over the `tr_begin`/`tr_flush` plan with a switch on the
   event op. None of the `llama_*` or syscall code moves.

5. Copy this experiment directory to
   `experiments/EXP-2026-11-15-649-p10-12-tallies-prefill-trace/`, then run
   `python3 tools/sync-goose-tree.py` and `python3 tools/check-goose-tree.py` so
   `goose/MANIFEST.json` records the four files, this experiment and the gates.

6. Ledger (`docs/goose-migration/tasks.json`, P9-08 and P10-12): move remainders 1-3 of P9-08
   and gaps G1/G2/G3/G6 of P10-12 to closed; P9-08's `remaining` keeps the MTP warm-up, the
   sampler chain and the CLI driver loop, P10-12's keeps G4, G5, G7-G15.

## Compatibility

- `goose check` per module: 3, 1, 4 and 12 specializations; no errors, no warnings.
- Composed check of all four sources concatenated: `typechecked ok`, 20 specializations, no
  duplicate declaration.
- No prefix collision and no file-name collision in the active tree (see above).
- The extracted regions depend on the base-commit text through exact anchors; rebasing onto a
  changed `session.cpp` rebuilds the oracle, and a moved anchor fails the gate build on
  purpose.

## Assumptions and possible conflicts

1. **Byte deltas stay below 2^63.** The native stores `read_bytes` deltas in `uint64_t` after
   a signed subtraction; the port keeps the signed difference. For every total below 2^63
   (every model this carrier can map) the bit pattern and the printed value are identical;
   the wrap case is in the corpus as a signed value, not as a `uint64_t` huge number.
2. **Integer division is written as `f64` division explicitly.** `hits / slots` on two `i64`
   is integer division in Goose, so the port declares both operands `f64`. This is stated in
   the module because it silently changed the row column once.
3. **`n_batch >= 1`** is assumed, as the config validator guarantees; the module guards it
   and the gate cannot compare against a native loop that would not terminate.
4. **The buffer passed to `mp_report` must be sized by the caller** (≈40 bytes per row); a
   short buffer is filled to its end and the returned length stays truthful.
5. **The report's `%.1f` is libc** through `mp_fmt_f1`; byte equality depends on the same
   libc rounding as the native `printf`, which the gate pins on this host.
6. **Nothing here replaces a call site yet.** `session.cpp` and `cli/main.cpp` still compile
   into the production build until the integration step above lands; that is the same
   limitation EXP-2026-11-15-647 records for its three slices.

## Artefacts

| path | SHA-256 |
|---|---|
| `source/tallies.goose` | `8d47331880c75262580c9bf77406072b3c22b4d7847fab9465843ec85d5764ce` |
| `source/prefill-plan.goose` | `d40e29910f193a37f5df9622bdc7572fcfd9e55c658c1b7b972929e1c1d342a5` |
| `source/trace-frame-order.goose` | `241a2d39240a6a6187a28bcddf5f0f7a57fd651969376abc7738bc6c07708014` |
| `source/moe-predict-report.goose` | `b71f9527841b76ca3724907152d632507c7443dc0e2fa2af5a31d08f36480342` |
| `tests/build.sh` | `fd60909757b716be6897df6d5b82c01e2df995aaa27ce00e1143319383642048` |
| `tests/native_extract.py` | `b236fff839a075a6f2bbcc534430fc0df3e6d874d3aae7da0d61d6a3696ed6d0` |
| `tests/tl_fixture.cpp` | `85bd482b3383c52729653d1b6ef008cac6e1132221d058bacb0b19070d320b87` |
| `tests/pf_fixture.cpp` | `5614d342f0ff41bf2cf81e9f8fe9f717fb2a538a62189f555ccdc2e8c199d92f` |
| `tests/tr_fixture.cpp` | `8533224f429f1377b96afeb4959f173aba8c700ce6af4166b37285cc15d08d16` |
| `tests/mp_fixture.cpp` | `bbd774622114bbd5b0f68f6b32041adeadf6a57607df99be6c5fcf5a9ed41c2b` |
| `tests/mutate.py` | `bc8836daa615eb6ddddf908231ebbaa45f50a32fa11db8a816030e6c3efde40d` |
| `results/gate-o1.log` | gate transcript at `-O1` |
| `results/gate-o0.log` | gate transcript at `-O0` |
| `results/mutate-1.log` | half 1, 24/24 |
| `results/mutate-2.log` | half 2, 24/24 |

Build artefacts (generated C, headers, objects, gate binaries, per-mutation copies) stay out
of the repository; `tests/build.sh` and `tests/native_extract.py` reproduce them from the four
Goose files and the base commit.
