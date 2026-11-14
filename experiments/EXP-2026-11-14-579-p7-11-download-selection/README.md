# EXP-2026-11-14-579 — download selection (P7-11 S3)

Native counterpart: `common/download.cpp` (file-local helpers for GGUF split
names, quant tags, part lists, best model/sibling, sidecars, repo tags and the
resume decision). Base commit: `039f9e59`.

## What this slice covers

`source/download-selection.goose` (810 lines, 15 exports):

| export | native | notes |
|---|---|---|
| `dl_split_info(name, out, tag_out)` | `get_gguf_split_info` | prefix offsets, tag offsets, index, count; the tag is upper-cased, so its offset is always 0 |
| `dl_quant_bits(name)` | `extract_quant_bits` | first digit run in the tag, stoi-like |
| `dl_part_count` / `dl_part_name` | `common_download_get_all_parts` | `-%05d-of-%05d.gguf` |
| `dl_split_group(blob, off, len, n, sel, out)` | `get_split_files` | same count and prefix bytes |
| `dl_best_model(blob, off, len, n, tag)` | `find_best_model` | tag order, `tag.`/`tag-`, split index 1, empty-tag fallback |
| `dl_best_sibling(..., model, keyword, tag)` | `find_best_sibling` | same directory, keyword, exact tag, closest bits |
| `dl_name_is_sidecar` / `dl_filename_is_model` | same | case-sensitive find, basename rules |
| `dl_sidecar_index` / `dl_sidecar_keyword` | sidecar table | mmproj, mtp, dflash, eagle3, dspark |
| `dl_plan_sidecars(present, out)` | `common_download_plan_sidecars` | table order |
| `dl_split_repo_tag(s, out)` | `common_download_split_repo_tag` | repo before the first `:`, tag after the last, exactly one `/` |
| `dl_resume_action(downloaded, status)` | transcribed from `common_pull_file` | 0 proceed, 1 drop partial then fail, 2 fail and keep |
| `dl_resume_header(downloaded, supports_ranges)` | transcribed | range request only when the offset is non-zero |

Deliberate deviation: `find_best_model` builds a `std::regex(tag + "[.-]")`
natively, so regex metacharacters inside a tag would be interpreted; the port
matches the tag literally. Every real GGUF quant tag (`Q4_K_M`, `Q8_0`, `F16`,
`NVFP4`, `IQ2_XS`, …) is identical under both rules. An adversarial tag would
differ, and the report records that instead of shipping a regex engine.

## Gate

`tests/gen_s3.py` emits the native probe (`tests/probe_s3.cpp`, which includes
`download.cpp` and links nothing but libc) and the Goose driver from one table.
31 cases are compared line by line:

```
31 native lines, 31 Goose lines, diff = 0
```

Coverage: split names (12), part lists (3), split groups (2), best model (4),
best sibling (4), sidecars (3), model naming (1), repo tag (1), resume (1). The
hand-readable expected lines are in `tests/cases_s3.tsv`.

## Mutations

`tests/mutate_s3.py <lo> <hi>` edits the port, regenerates the driver, runs it
with `goose run` and compares with the oracle.

```
python3 tests/mutate_s3.py 0 8    # 8/8 caught
python3 tests/mutate_s3.py 8 16   # 8/8 caught
```

## Recorded limits

- Tag matching is literal, not `std::regex`; only adversarial tags differ.
- `resume_action`/`resume_header` are compared against a transcription of the
  inline branch in `common_pull_file`, because the native code has no callable
  helper for them.
- The S2 representation (flat blob plus offset/length names) is reused, but S2's
  validators are not duplicated here.
- The gate runs `goose run`, not a full C build; no network is touched.
