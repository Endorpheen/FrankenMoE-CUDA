# EXP-2026-11-14-585 — HF cache naming (P7-11 S2)

Native counterpart: `common/hf-cache.cpp` (file-local validators, repo/folder
mapping, refs selection, snapshot and blob path assembly). Base commit:
`4dac50f2`.

## What this slice covers

`source/hf-cache-naming.goose` (485 lines, 12 exports):

| export | native | notes |
|---|---|---|
| `hf_repo_valid` | `is_valid_repo_id` | 1..256 bytes, base `[A-Za-z0-9_]`, `[/.-]` must be surrounded by base bytes, exactly one `/` |
| `hf_token_valid` | `is_valid_hf_token` | `hf_` + 34..253 alnum, length 37..256 |
| `hf_commit_valid` | `is_valid_commit` | exactly 40 hex bytes |
| `hf_oid_valid` | `is_valid_oid` | 40 or 64 hex bytes |
| `hf_repo_folder` | `repo_to_folder_name` | `models--` prefix, every `/` becomes `--` |
| `hf_folder_repo` | `folder_name_to_repo` | `--` becomes `/`; `-1` for a missing prefix, otherwise the length (0 for an empty id) |
| `hf_is_abs` | `fs::path::is_absolute` | empty or `/`-leading rejection input |
| `hf_path_norm` | `(base / sub).lexically_normal()` | `.` dropped, `..` pops and is clamped at the root, empty components collapse, a trailing `/` survives after an empty, `.` or `..` component |
| `hf_subpath_ok` | `is_valid_subpath` | rejects an absolute subpath, then checks the normalized join as a component prefix of the base |
| `hf_file_paths` | `get_repo_files` path block | `snapshots/<commit>/<sub>` and the `blobs/<oid>` redirect when the oid is known and the snapshot is absent |
| `hf_refs_pick` | `get_cached_ref` | regular file, non-empty first line, valid commit, `main` wins, first valid entry is the fallback |
| `hf_finalize_action` | `finalize_file` head | 0 already in place, 1 nothing to move, 2 link or move |

Bodies are private `h_*` functions and the exported `hf_*` names are thin
wrappers. Goose allows one concrete signature per export, so the gate driver
calls the private bodies, the same convention S3 uses.

## Deliberate deviations and limits

- The cache root is a caller-supplied prefix; `HF_HOME`, `HF_HUB_CACHE`,
  `HUGGINGFACE_HUB_CACHE`, `XDG_CACHE_HOME`, `LLAMA_CACHE` and `HOME`/`getpwuid`
  resolution stay native. This removes the environment dependency the inventory
  flags for `hf-cache.cpp`.
- Lexical normalization is modelled for absolute POSIX paths whose base is
  already normalized and has no trailing separator. Windows path rules are not
  modelled.
- `get_cached_ref` and `finalize_file` need a real directory or real files, so
  their selection and branch order are transcribed into the probe next to the
  native definitions. The compared strings and action codes are the ones the
  native code returns; the directory iteration order is supplied by the caller.
- An unreadable ref file and a non-regular ref entry both collapse into the
  `reg = 0` column; a multi-line ref file is modelled by its first line only.
- The case corpus never contains the `|` byte, which is the table delimiter, or
  an embedded newline.
- `string_replace_all` from `common.cpp` is copied verbatim into the probe. It
  is the only external helper the two mapping functions reach and is a replace
  loop, not a decision under test; linking all of `common.cpp` would drag in
  httplib and the log queue.
- The gate never calls `get_repo_files`, `remove_cached_repo` or any HTTP path.

## Gate

`tests/gen_s2.py` emits the native probe (`tests/probe_s2.cpp`, which includes
`hf-cache.cpp` and calls its file-local helpers) and the Goose driver
(`build/gate_s2.goose`) from one table, so the inputs cannot drift.

```
tests/build_probe.sh                       # build/probe_s2 + build/native_s2.out
goose run build/gate_s2.goose > build/goose_s2.out
diff build/native_s2.out build/goose_s2.out
```

Result: 1556 cases, 1556 native lines, 1556 Goose lines, diff = 0.

| kind | cases | what it compares |
|---|---|---|
| repo | 1475 | validity, folder name, reverse name; every 1..3-byte string over `aZ9-_/.@:`, every 4-byte string over `a0-/_`, length 255/256/257 |
| fold | 10 | reverse mapping alone, missing prefix, empty id, repeated separators |
| token | 12 | length 36/37/256/257, prefix case, non-alnum byte |
| commit | 10 | 40/64 hex, upper case, wrong length, non-hex byte |
| subpath | 25 | `.`, `..`, escapes, trailing `/`, `//`, absolute, `..`-prefixed names |
| paths | 8 | blob redirect versus final path, empty subpath, nested subpath, 64-byte oid |
| refs | 10 | `main` preference, invalid commit, empty line, non-regular entry, empty table |
| finalize | 6 | all eight flag combinations that reach the three branches |

The oracle is the native translation unit, not a rewrite: `--gc-sections` drops
the linked-but-uncalled network and filesystem functions, so the probe opens no
socket and writes no file.

## Mutations

`tests/mutate.py <lo> <hi>` edits the port, regenerates the driver, runs it and
compares with the oracle; a differing line, a run failure or missing output all
count as caught.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Twelve mutations in two halves: validators and the name mapping first, paths,
subpaths, refs and finalize second. The table with per-mutation diff counts is
in `REPORT.md`.

## Files

```
source/hf-cache-naming.goose   port
tests/gen_s2.py                one table -> probe + driver
tests/probe_s2.cpp             generated native oracle
tests/cases_s2.tsv             human-readable case list
tests/mutate.py                mutation run
tests/build_probe.sh           probe build and oracle capture
build/gate_s2.goose            generated driver (port + cases)
build/native_s2.out            oracle output, 1556 lines
build/goose_s2.out             port output, 1556 lines
```
