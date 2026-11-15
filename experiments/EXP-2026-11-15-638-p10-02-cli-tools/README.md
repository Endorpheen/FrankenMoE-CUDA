# EXP-2026-11-15-637 — P10-02: maintained command-line tools (offline decision slices)

Verdict: **ACCEPTED candidate** (model-free, differential). The lead's acceptance
is the commit gate. Base commit: `137bc40c` (the three native tool files are
byte-identical at `0cb42612`, verified with `git diff --stat`).

Work was done outside the repository, in
`/tmp/opencode/sub-637/EXP-2026-11-15-637-p10-02-cli-tools/`. The shared tree,
the production runtime and other agents' files are untouched. No model, GPU,
server or benchmark was started.

## Hypothesis

The maintained command-line tools keep a large part of their behaviour outside
the model: argument parsing and defaults, mode selection, input checks, metric
aggregation, output formats and units. Those decisions can be ported to Goose
and pinned with a differential gate that never loads a model. On that base the
tool fronts can later call Goose instead of C++.

The hypothesis held: three independent slices, 571 compared values at `-O1`
and `-O0` with 0 failures, and 36/36 mutations caught.

## Inventory and slice choice

Examined: `upstream/llama.cpp/tools/**`, `upstream/llama.cpp/common/arg.cpp`,
`examples/simple`, `examples/gguf`, and `runtime/cli`. The P10-02 ledger lists
ten audited tool directories; the rest of `tools/` was read to avoid a silent
exclusion.

| tool (base commit) | lines | model-free decision surface | choice |
|---|---:|---|---|
| `tools/llama-bench` | 2543 | format selection, `cmd_params_defaults`, `parse_int_range`, `avg`/`stdev`, field types, JSON/CSV/SQL escape, markdown widths/rows/units, tensor_split row | **S1** |
| `tools/tokenize` | 222 | prompt-source precedence, escape application, `add_bos`, ids/pieces output, count footer | **S2** |
| `tools/fit-params` | 78 | the fitted `-c/-ngl/-ts/-ot` line, uint32 cast, split trimming | **S3** |
| `tools/gguf-split` | 609 | CLI parse, sizes, split strategy, shard naming | already in `goose/model/gguf-split.goose` (`gsp_`) — not duplicated |
| `tools/batched-bench` | 262 | none: reads args through `common_params_parse` and prints inside the model loop | rejected |
| `tools/completion` | 956 | interactive chat; no pure output formatting | rejected |
| `tools/cli`, `perplexity`, `export-lora`, `cvector-generator`, `tuning`, `quantize`, `imatrix`, `mtmd` | — | entrypoint needs a model / multimodal weights | not in this experiment |
| `examples/simple`, `examples/gguf` | 223/273 | `simple` is a model loop; `gguf` reads a real file and its decisions are GGUF container work | not in this experiment |
| `common/arg.cpp` | 4767 | `common_params_parse`, owned by P7-10; `common_arg::to_string` is help rendering | outside P10-02 |
| `runtime/cli` | — | config resolution already ported (`cc_`, P9-01 S3) | no overlap |

Prefixes `lb_`, `tz_`, `fp_` are free in the active tree. `tk_` was avoided
because `goose/jinja/parser.goose` already uses `tk_next`/`tk_expect`
internally; `gs_`/`gsp_` are taken by the GGUF work.

## Slices and interfaces

One Goose module per slice. All exports take and return scalars, `u8`/`i64`/
`f64` slices only, per the flat export ABI.

### S1 — `source/llama-bench-report.goose` (`lb_`)

| export | native (llama-bench.cpp at the base commit) |
|---|---|
| `lb_out_from_str(s) -> i64` | `output_format_from_str`, 240-252: `none/csv/json/jsonl/md/sql`, exact and lower case, `-1` when unknown |
| `lb_out_str(v, out) -> i64` | `output_format_str`, 221-238 |
| `lb_split_str(v, out)`, `lb_lazy_str(v, out)` | `split_mode_str` 254-270, `lazy_mode_str` 272-287 |
| `lb_avg_u64(vals)`, `lb_stdev_u64(vals)` | `avg<uint64_t>`/`stdev<uint64_t>`, 102-121, integer arithmetic, `sqrt` truncated |
| `lb_avg_ts(samples, ntok) -> f64`, `lb_stdev_ts(...)` | `test::get_ts` 1579-1588 + `avg<double>`/`stdev<double>`, 102-121 |
| `lb_field_type(field) -> i64` | `test::get_field_type`, 1631-1650 (0 STRING, 1 BOOL, 2 INT, 3 FLOAT) |
| `lb_json_escape(s, out)`, `lb_json_value(field, value, out)` | `escape_json` 1784-1800, `format_json_value` 1802-1817 |
| `lb_csv_escape(s, out)` | `csv_printer::escape_csv`, 1759-1774 |
| `lb_md_width(field)`, `lb_md_name(field, out)` | `markdown_printer::get_field_width` 1867-1920, `get_field_display_name` 1922-1972 |
| `lb_md_size(bytes, out)`, `lb_md_params(n, out)`, `lb_md_test(np, ng, nd, out)`, `lb_md_ts(avg, sd, out)` | `print_test` rows, 2065-2105 (`%.2f MiB/GiB`, `%.2f M/B`, `pp/tg/@ d`, `%.2f ± %.2f`) |
| `lb_sql_type(field, out)` | `sql_printer::get_sql_field_type`, 2113-2127 |
| `lb_tensor_split(vals, ndev, out)` | the `tensor_split` row, 1652-1668 (`%.2f` joined by `/`, trailing zeros kept) |
| `lb_range(s, allow_neg, out) -> i64` | `parse_int_range`, 293-335: `first[-last[(+|*)step]][,...]`; returns the count, `-1` invalid format, `-2` invalid range, `-3` `std::stoi` out of range |
| `lb_defaults(out) -> i64` | scalar fields of `cmd_params_defaults`, 380-423 (28 values, indexes named in the module) |
| `lb_default_cpu_mask(out)` | `cmd_params_defaults.cpu_mask` (`"0x0"`) |

Exact native quirks that are part of the port: `no_kv_offload` is INT because
the integer list is checked first although the name also appears in the boolean
list; `f16_kv` is BOOL although no printed field uses it; a byte above `0x7f`
in `escape_json` takes the control branch, the `\u%04x` of the signed value is
truncated by the 8-byte buffer, and the output is the seven bytes `\ufffff`;
`parse_int_range` accepts `|` as an operator in the regex class and then refuses
it, accepts a reversed range as an empty list, refuses a step that does not
advance, and returns an empty list for an empty string.

### S2 — `source/tokenize-cli.goose` (`tz_`)

| export | native (tokenize.cpp at the base commit) |
|---|---|
| `tz_source(stdin, file, prompt) -> i64` | 106-115: stdin (0) > file (1) > prompt (2), `-1` when none |
| `tz_escape_needed(source, escape_param) -> i64` | 131/175: file and stdin are processed locally, `--prompt` already was |
| `tz_add_bos(model_wants, no_bos) -> i64` | 180-182 |
| `tz_report(tokens, pieces, offs, lens, invalid, ids, count, out)` | 187-215: `[%d, %d]`, `%6d -> '<piece>'`, the Windows invalid-UTF-8 suffix, `Total number of tokens: %zu` |
| `tz_fmt_ids(tokens, out)`, `tz_fmt_piece(token, piece, invalid, out)` | the two loop bodies separately |

A file prompt is not stripped of a trailing newline; `-p` is not escape
processed twice; the count line follows the list.

### S3 — `source/fit-params-line.goose` (`fp_`)

| export | native (fit-params.cpp at the base commit) |
|---|---|
| `fp_u32(x) -> i64` | `uint32_t(tensor_split[id])`, truncation |
| `fp_ts_count(splits, ndev) -> i64` | 47-49, trailing zeros dropped while `nd > 1` |
| `fp_line(n_ctx, ngl, splits, ndev, pats, poff, plen, bufts, boff, blen, n_override, out)` | 45-71, the whole printed line including the newline |

`plen[i] < 0` is the native null pattern and ends the override list. The line is
`-c <u32> -ngl <i32>[ -ts <u32>,...][ -ot "<pat>=<buft>,..."][\n]`.

## Gate

Three gates, one per slice. Run at `-O1` and at `-O0`; the summary is identical.

```
FG637_LB cases=182 values=503 failures=0
FG637_TZ cases=40  values=40  failures=0
FG637_FP cases=28  values=28  failures=0
```

`cases` counts distinct inputs, `values` counts compared fields. The native arm
of S1 is the tool's own translation unit (`tests/native_lb.cpp` includes
`tools/llama-bench/llama-bench.cpp` from the base-commit canon; `main` is
renamed and dropped by `--gc-sections`, four host facts are stubbed:
`llama_commit`, `llama_build_number`, `common_cpu_get_num_math`,
`llama_max_devices`). The markdown row helpers and `tensor_split` row have no
callable function in the tool, so those lines are transcriptions inside the
native arm, cited. S2 and S3 native arms are transcriptions of their `main`
bodies with the native printf formats, because both mains load a model; the
provenance is stated in each fixture.

`-O0` and `-O1` produce the same 182/40/28 cases and the same 0 failures, which
is the required optimisation-independence check.

## Mutations

12 per slice, two halves of six, module generated at `-O0`:

| half | llama-bench | tokenize | fit-params | total |
|---|---|---|---|---|
| 1 | 6/6 | 6/6 | 6/6 | 18/18 |
| 2 | 6/6 | 6/6 | 6/6 | 18/18 |

36/36 caught, 0 survived, 0 build failures. The table is in `tests/mutate.py`
(`python3 tests/mutate.py 1|2`); each mutation rewrites one decision, for
example: upper-case `MD` accepted, stdev divided by `n`, `no_kv_offload`
reclassified, JSON backslashes not doubled, 1 GiB boundary moved, `*` treated
as `+`, stdin/file precedence swapped, escapes applied to `--prompt` again,
`%6d` narrowed, count off by one, `-ts` separator changed, `uint32` rounding
instead of truncation, `-ot` quote always closed.

## Exact integration step

1. Copy the three modules into the canonical CLI group:

   ```
   cp source/llama-bench-report.goose goose/cli/llama-bench-report.goose
   cp source/tokenize-cli.goose       goose/cli/tokenize-cli.goose
   cp source/fit-params-line.goose    goose/cli/fit-params-line.goose
   ```

2. In `tools/build-main-runtime.sh`, append to `MODULES` after
   `goose/cli/cli-protocol.goose`, in this order:

   ```
   goose/cli/llama-bench-report.goose
   goose/cli/tokenize-cli.goose
   goose/cli/fit-params-line.goose
   ```

   No `RENAMES` entry is needed in `tools/compose-goose.py`: every internal
   name is prefixed. Composition was verified locally (33 globals, 0 duplicates,
   one custom extern, `lb_fmt2`/`fg637_fmt2`).

3. Rename the one borrowed primitive in the canon copy:

   ```
   sed -i 's/fg637_fmt2/lb_fmt2/g' goose/cli/llama-bench-report.goose
   ```

   and provide the host shim next to `gs_json_fmt_e` (the same arrangement the
   accepted JSON/Jinja modules use):

   ```c
   /* correctly rounded "%.2f"; decimal rounding is libc, not a tool decision */
   int64_t lb_fmt2(sl_u8 out, double x) {
       return snprintf((char *) out.data, (size_t) out.len, "%.2f", x);
   }
   ```

4. Copy this experiment directory to
   `experiments/EXP-2026-11-15-637-p10-02-cli-tools/`, then run
   `python3 tools/sync-goose-tree.py` and `python3 tools/check-goose-tree.py`
   so `goose/MANIFEST.json` records the three files, this experiment and the
   gate.

5. Ledger (`docs/goose-migration/tasks.json`, P10-02, still `TODO`):
   `slices_total = 3`, `slices_done = 3`,
   `remaining = ["llama-bench: get_cmd_params_instances and the live printers",
   "tokenize: the common_tokenize loop and the model-load path",
   "fit-params: common_fit_params and --fit-print; batched-bench, cli,
   completion, perplexity, export-lora, cvector-generator, tuning, quantize,
   imatrix, mtmd entrypoints"]`,
   `coverage_doc = "experiments/EXP-2026-11-15-637-p10-02-cli-tools/README.md"`.
   Refresh `status_totals`, `updated_at` and `work_front`. P10-02 does not close
   with this change.

6. Commit subject:
   `EXP-2026-11-15-637: port llama-bench, tokenize and fit-params CLI decisions (P10-02)`.

## Compatibility and assumptions

- Toolchain: Goose `2026.3-string` (runtime `1e714e61`), `gcc-13`/`g++-13`.
  `import math` supplies `sqrt`; no packages are fetched.
- ABI: scalars and `u8`/`i64`/`f64` slices only; `goose_init` is emitted by
  every module and deduplicated by the composer.
- `lb_*` take `i64` where the native takes `uint64_t` (`model_size`,
  `model_n_params`, `samples_ns`). Values must stay below `2^63` at the ABI
  boundary; the native `%zu` domain above that is not representable.
- `lb_stdev_u64` reproduces the native integer expression, which is unsigned
  arithmetic in C++. The gate's corpus keeps every intermediate below `2^63`,
  which is the domain the tool measures in; explicit uint64 wrap above that is
  not modelled.
- `lb_range` maps `std::stoi` `out_of_range` to `-3`; `parse_int_range` never
  returns an error for a reversed range or an empty string (native behaviour).
- `tz_report` takes token ids as `i64` because the flat ABI has no `i32` slice;
  the tokenizer produces `int32` values, so the domains coincide. Pieces are
  supplied by the caller (`common_token_to_piece`), and the Windows invalid-UTF-8
  branch is represented by the `invalid[i]` flag; on POSIX the writer is plain
  `printf`.
- `fp_line` takes `f64` splits while the native stores `f32`; the caller must
  widen each `float` (`(double) tensor_split[i]`), which the gate does.
  `fp_u32` is defined for `[0, 2^32)`; outside it the C++ cast is undefined and
  no case covers it.
- `--help`/`--version` and the `exit()` paths are not ported: the usage text
  stays with the carrier, and only the decision functions are replaced.
- No model, tokenizer, GGUF file, backend or thread is created by the gate.

## Overlaps and conflicts

- `goose/model/gguf-split.goose` (`gsp_`) already owns `gguf-split`'s CLI,
  sizes, strategy and shard naming; this experiment deliberately does not port
  it again.
- `goose/cli/config-resolve.goose` (`cc_`, P9-01 S3) owns `runtime/cli` flag and
  environment resolution; no symbol or decision is duplicated.
- `common/arg.cpp` (`common_params_parse`) is P7-10's; the tokenize slice starts
  after parsing (source selection and output), so a later P7-10 rewrite does not
  collide.
- The only custom extern is the `%.2f` shim, prefixed per experiment until
  integration renames it to `lb_fmt2`.

## Artifacts

```
source/llama-bench-report.goose   S1 module
source/tokenize-cli.goose         S2 module
source/fit-params-line.goose      S3 module
tests/native_lb.cpp               S1 native arm (canon llama-bench TU + 4 stubs)
tests/lb_fixture.cpp              S1 gate
tests/tz_fixture.cpp              S2 gate (transcription native arm)
tests/fp_fixture.cpp              S3 gate (transcription native arm)
tests/build.sh                    build and gate runner
tests/mutate.py                   36 mutations, two halves
results/gate-*.log                gate output at -O1 and -O0
results/mutate-1.log              18/18 caught
results/mutate-2.log              18/18 caught
results/hashes.txt                SHA-256 of every committed artifact
```

Reproduce (from this directory):

```
bash tests/build.sh all           # -O1
bash tests/build.sh all mutate    # -O0
python3 tests/mutate.py 1
python3 tests/mutate.py 2
```

`build/` holds generated C, objects and gate binaries and is not part of the
evidence set.
