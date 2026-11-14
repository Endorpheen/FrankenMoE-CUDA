# EXP-2026-11-14-596 — imatrix loader, remap and prepare (P7-04 S2)

Native counterparts:

- `common/imatrix-loader.cpp` — `common_imatrix_load_legacy`, `common_imatrix_load`
- `src/llama-quant.cpp` — `remap_layer`, `remap_imatrix`
- `tools/quantize/quantize.cpp` — `load_imatrix`, `prepare_imatrix`

Base commit: `a640bac9`. Slice: P7-04 S2, see `docs/goose-migration/P7-04-COVERAGE.md`.

## What this slice covers

`source/imatrix.goose` (1069 lines, 7 exports) owns every decision of the three
files above. No function opens a file: a caller serialises the bytes the native
code would have read and passes them in.

| export | native | scope |
|---|---|---|
| `im_load` | `common_imatrix_load` | try the GGUF path, fall back to the legacy path on any parse failure |
| `im_gguf` | the GGUF half alone | header, KV list, tensor infos, data offsets, the `.in_sum2`/`.counts` pairing, `imatrix.datasets`/`chunk_count`/`chunk_size`, the F32 rule |
| `im_legacy` | `common_imatrix_load_legacy` | `int32` counts, per-entry `ncall`/`nval`, the NUL-truncated name, the optional trailing chunk count and dataset name |
| `im_normalize` | `load_imatrix` | the `!is_legacy && !has_metadata` rejection and the per-entry scaling: legacy divides by `ncall`, GGUF divides `ncounts` expert groups by their counts and leaves the partial tail zero |
| `im_prepare` | `prepare_imatrix` | excluded then included substring filters over the entry names, plus the `m_last_call` return value |
| `rl_run` | `remap_layer` | the stateful prune remap over a tensor sequence: `mapped`, `next_id`, dropped tensors, the `blk\.(\d+)\.` match |
| `ri_run` | `remap_imatrix` | the inverse lookup on the remapped name; -1 where native calls `GGML_ABORT` |

Semantics that the fixtures pin down:

- entries behave like `std::map<std::string, ...>`: byte-lexicographic order,
  last write wins for a repeated name, and the GGUF loader pairs tensors by
  suffix, so `x.in_sum2.counts` pairs on `x.in_sum2`, not on `x`;
- `has_metadata` is key presence only, regardless of the value types, while a
  non-string-array `imatrix.datasets` contributes nothing;
- `remap_layer` sets `next_id` only in the `blk < prune.front()` branch, so a
  sequence that starts above the first pruned layer keeps the caller's value;
- `remap_imatrix` runs on the **remapped** name, not the original
  (`llama-quant.cpp` stores the new name in the tensor before the lookup). The
  first driver version used the original name and the gate caught it;
- through that pipeline a miss is unreachable: every produced name carries a
  mapped value. The two `ri_extra` cases call `remap_imatrix` on names the
  prune pass never saw, which is the only way to reach the abort.

## Gate

`tests/gen_imx.py` builds one fixture table and emits both sides:

- `tests/probe.cpp` — the native oracle. It includes `imatrix-loader.cpp`, the
  real `quantize.cpp` (with `main` renamed) and, in a second translation unit,
  `llama-quant.cpp`. Every fixture is a synthetic byte buffer handed to the
  loaders through `memfd_create` and `/proc/self/fd/N`, so no test file is
  written to disk. Each case runs in a child process with its stdout and stderr
  on `/dev/null`, and its lines are passed back through a pipe, so a native
  `exit(1)` or `GGML_ABORT` becomes a `term` line instead of killing the probe.
- `build/gt.goose` — the port plus a driver over the same bytes, the same
  counters and the same line format.

`tests/build.sh` builds the llama libraries of the base commit into
`build/llama.cpp` on the first run (`GGML_CUDA=OFF`, no tools, no server), links
the probe, runs both sides and diffs:

```
[diff] gate ok: 277 lines identical
```

36 loader fixtures (153 lines), 12 prepare cases (20 lines), 15 remap cases (79
lines) and 4 native terminations. Coverage includes a valid file of each format,
the trailing-metadata variants of the legacy format, repeated and NUL-containing
names, every GGUF rejection the loader depends on (missing sums/counts, non-F32,
zero tensors, bad alignment, truncated data), the metadata rule, four rounding
cases, NaN/Inf/subnormal/negative-zero sums, GGUF v2 and v3, a 16-byte alignment
and an unrelated tensor in the same file.

## Mutations

`tests/mutate.py --half 1|2` holds 12 textual mutations in two halves. Each
mutant must still pass `goose check`, is run at `-O0` and must differ from
`build/native.out`.

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

## Reproduce

```
tests/build.sh
python3 tests/mutate.py --half 1
python3 tests/mutate.py --half 2
```

`REPORT.md` carries the recorded output, the mutation table and the SHA-256 of
every input and generated artifact. Build products (`build/llama.cpp`,
`build/native.out`, `build/cases.tsv`, `build/gt.goose`, `tests/probe.cpp`) are
reproducible and not part of the source change; `source/imatrix.goose`,
`tests/gen_imx.py`, `tests/mutate.py`, `tests/build.sh`, the three native shims
and `README.md` are.

## Recorded limits

- **Failure comparison is the native boolean.** The port returns distinct
  status codes that the native loader cannot report (it returns `bool`), and on
  failure both sides leave the outputs unspecified; only `ok`/`fail` is gated.
  The status codes exist for the caller's log.
- **Partial trailing reads.** A legacy file with 1–3 trailing bytes is emulated
  byte-wise: libstdc++ copies the available bytes into the zeroed `int32` and
  sets `failbit`. The fixture `legacy_short_tail` pins the result, but it rests
  on that stream behaviour rather than on a documented guarantee.
- **Negative length fields are undefined in native**: `std::vector<char>(len + 1)`
  with a negative `len` and a write at index `len` (`name_as_vec[len] = 0`), or
  `push_back(dataset.data())` on an empty vector. The port rejects them and no
  fixture exercises them.
- **GGUF duplicate KV keys** are rejected by `gguf.cpp`'s parser; the port does
  not check them. That parser is the P7-02 module; this slice reproduces the
  header, KV, tensor-info and data-length checks the loader depends on, not the
  whole of `gguf_init_from_file`.
- **Buffer capacities.** The port writes into caller arrays and returns
  `IM_ERR_CAP` when one is too small; native uses dynamic containers. The
  fixtures stay within 16 entries, 64 sums, 32 remap names.
- **GGML_ASSERT paths.** A wrongly typed `imatrix.chunk_count`/`chunk_size`
  reaches `gguf_get_val_u32`, which asserts. The port returns `IM_ERR_ASSERT`
  and the driver prints the same `term|sig6` line the probe observes; the
  assert message and backtrace are not compared.
- **Type tables.** `blck_of`/`tsize_of` transcribe all 43 `ggml_type` entries,
  but only F32 and F16 appear in the fixtures; other types only affect the
  offset validation.
- **Linux only.** The probe uses `memfd_create` and `fork`; there is no test
  file and no model on disk.
- **No weights, no real imatrix files.** Every input is a synthetic buffer built
  by `tests/gen_imx.py`.
