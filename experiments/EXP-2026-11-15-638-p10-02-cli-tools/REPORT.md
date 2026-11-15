# REPORT — EXP-2026-11-15-637 (P10-02, offline CLI-tool slices)

## Result

| item | value |
|---|---|
| slices | **3** (llama-bench `lb_`, tokenize `tz_`, fit-params `fp_`) |
| gates | **3/3 green**, 0 failures, identical at `-O1` and `-O0` |
| cases / values | 182 / 503 (lb), 40 / 40 (tz), 28 / 28 (fp) |
| mutations | **36/36 caught**: 12 per slice, two halves of 6/6 |
| model / GPU / server | none started |
| repository changes | none (work under `/tmp/opencode/sub-637/`) |
| base commit | `137bc40c`; native files unchanged at `0cb42612` |

## Method

1. **Inventory.** Read `upstream/llama.cpp/tools/**`, `common/arg.cpp`,
   `examples/simple`, `examples/gguf` and `runtime/cli`. For each tool, the
   model-free decisions were separated from the model loop. `gguf-split` was
   dropped because `goose/model/gguf-split.goose` (`gsp_`) already ports it;
   `batched-bench` and `completion` have no model-free decision surface; the
   remaining audited tools need weights. `common/arg.cpp` (P7-10) and
   `runtime/cli` (`cc_`) were checked for overlap and left alone.
2. **Three slices, one module each.** Prefixes `lb_`, `tz_`, `fp_` are free in
   the active tree. Each module is standalone, has no packages, and uses only
   the flat export ABI.
3. **Differential gates.** S1's native arm is the canon `llama-bench.cpp`
   compiled into the fixture (statics callable, `main` dropped, four host facts
   stubbed, `--gc-sections`); the four markdown row helpers and the
   `tensor_split` row are cited transcriptions because the tool exposes no
   function for them. S2/S3 native arms are transcriptions of their `main`
   bodies (both mains load a model) with the native printf formats.
4. **Mutation run.** 12 mutations per slice, split 6/6, every mutated module
   generated at `-O0`; a surviving mutation is treated as a gate hole.

## Gate output

```
FG637_LB cases=182 values=503 failures=0
FG637_TZ cases=40  values=40  failures=0
FG637_FP cases=28  values=28  failures=0
```

Same lines from the `-O0` run. The gates cover: six output formats and their
refusals, split/lazy names, integer and double averages and standard deviations
(including the truncating `stdev<uint64_t>`), the full printed field list for
type/width/display-name/SQL-type, JSON and CSV escaping including bytes above
`0x7f`, markdown size/params/test/t-s rows, `tensor_split`, 25 range strings in
both signed modes, 28 scalar defaults, the six prompt-source precedence cases,
the escape and `add_bos` matrices, 16 report layouts, and 12 fitted lines
(single/multiple devices, trailing-zero trimming, overrides, negative `ngl`).

## Mutations

| half | llama-bench | tokenize | fit-params | total |
|---|---|---|---|---|
| 1 | 6/6 | 6/6 | 6/6 | 18/18 |
| 2 | 6/6 | 6/6 | 6/6 | 18/18 |

0 survived, 0 build failures. One setup error found during development (a
mutation that was not valid Goose) was replaced with a valid one; it is not
counted above.

## Interfaces

S1 exports 23 functions (`lb_out_from_str`, `lb_out_str`, `lb_split_str`,
`lb_lazy_str`, `lb_avg_u64`, `lb_stdev_u64`, `lb_avg_ts`, `lb_stdev_ts`,
`lb_field_type`, `lb_csv_escape`, `lb_json_escape`, `lb_json_value`,
`lb_md_width`, `lb_md_name`, `lb_md_size`, `lb_md_params`, `lb_md_test`,
`lb_md_ts`, `lb_sql_type`, `lb_tensor_split`, `lb_range`, `lb_defaults`,
`lb_default_cpu_mask`). S2 exports 6 (`tz_source`, `tz_escape_needed`,
`tz_add_bos`, `tz_report`, `tz_fmt_ids`, `tz_fmt_piece`). S3 exports 3
(`fp_u32`, `fp_ts_count`, `fp_line`).

`lb_range` returns the count or `-1`/`-2`/`-3`; `lb_defaults` returns 28 and
fills a 28-slot `i64` array; `fp_line` writes the whole line including the
newline and returns its length; all other text exports return the byte count.

## Exact integration

1. Copy the modules to `goose/cli/`:
   `llama-bench-report.goose`, `tokenize-cli.goose`, `fit-params-line.goose`.
2. Append those three paths to `MODULES` in `tools/build-main-runtime.sh` after
   `goose/cli/cli-protocol.goose`, in that order. No composer rename is needed
   (all internals are prefixed); composition was checked locally: 33 globals,
   0 duplicates, one custom extern.
3. Rename `fg637_fmt2` to `lb_fmt2` in the canon copy and add the shim
   `int64_t lb_fmt2(sl_u8 out, double x)` that calls
   `snprintf(out.data, out.len, "%.2f", x)`, next to `gs_json_fmt_e`.
4. Copy this experiment directory to
   `experiments/EXP-2026-11-15-637-p10-02-cli-tools/`, run
   `tools/sync-goose-tree.py` and `tools/check-goose-tree.py`.
5. Update P10-02 in `docs/goose-migration/tasks.json`: `slices_total = 3`,
   `slices_done = 3`, `remaining` = the still-native tool paths listed below,
   `coverage_doc` = this experiment's README; refresh `status_totals`,
   `updated_at`, `work_front`. P10-02 stays `TODO`.
6. One atomic commit, English subject:
   `EXP-2026-11-15-637: port llama-bench, tokenize and fit-params CLI decisions (P10-02)`.

## Compatibility

- Goose `2026.3-string` (runtime `1e714e61`), gcc/g++ 13.4.0, `-O1` gate and
  `-O0` mutations; no packages fetched.
- Flat ABI only. `u64` native fields are carried as `i64` and must stay below
  `2^63`; the `stdev<uint64_t>` corpus stays below that too (the native uint64
  wrap above it is not modelled).
- `f32` tensor splits are widened to `f64` by the caller before `fp_line`;
  `fp_u32` is defined for `[0, 2^32)`.
- Token ids are `i64` at the ABI boundary (`i32` in the tool); pieces are
  supplied by the caller. The Windows invalid-UTF-8 branch is a flag, POSIX is
  plain `printf`.
- `--help`/`--version`/`exit()` remain with the carrier.

## Risks and open points

- The S1 native arm depends on four stubbed host facts; if llama-bench starts
  using them inside a ported decision, the gate must gain the real values.
- `escape_json` above `0x7f` reproduces a native truncation (`\ufffff`); if
  upstream fixes that bug, the gate and the port change together.
- `parse_int_range` calls `std::stoi`; values beyond `i64` are mapped to `-3`
  rather than aborting, a deliberate carrier difference.
- The markdown row text and the S2/S3 arms are transcriptions, not linked native
  functions: they prove decision drift, not ABI compatibility.
- Wiring the tools to these exports (replacing printf/argv loops) is not part of
  this experiment and must keep the C++ carriers thin.

## Remaining P10-02 inventory

Not covered here: `llama-bench` instance expansion and live printers;
`tokenize`'s `common_tokenize` loop and model-load path; `fit-params`'
`common_fit_params` and `--fit-print`; `batched-bench`, `cli`, `completion`,
`perplexity`, `export-lora`, `cvector-generator`, `tuning`, `quantize`,
`imatrix`, `mtmd` entrypoints. `gguf-split` is complete under `gsp_`.

## Artifact SHA-256

See `results/hashes.txt`. Module hashes:

```
0a3b37cfcba1b4a6985de87c2a9b1851e83e78178dd106af2cf6b9afae6bbed4  source/llama-bench-report.goose
5ca4a50e8e80aa0a81a973441ca66e3b2ff4f0a1131f2af01e5b838aac28fc28  source/tokenize-cli.goose
ca1ee2263d2b97446786d18012572ae031bb4e2ce4d77e28c3fc8371c57b3161  source/fit-params-line.goose
```
