# EXP-2026-11-15-645 — P10-05: owned executable infrastructure and generators

Status: **ACCEPTED candidate** (model-free, differential). The lead's acceptance
is the commit gate. Base commit: `792293ad23292d790b74a0f248887ec681d344e0`.
The seven native inputs of this experiment were verified byte-identical at
`792293ad` and again at `c344b309` (`git diff --stat` over those paths is empty).

Work was done outside the repository, in
`/tmp/opencode/sub-647/EXP-2026-11-15-645-p10-05-owned-infra/`. The shared tree,
the production runtime and other agents' files were not touched. No model, GPU,
server, build of llama.cpp or benchmark was started.

## Hypothesis

The owned build/release/packaging infrastructure keeps a large decision surface
outside any model and outside the C++ engine: tag and asset admission, the
dist manifest, the generated-artifact policy, the configure preflight, the
pinned-upstream table and the telemetry-to-JSON conversion. Those decisions can
be ported to Goose and pinned by a gate that executes the *native scripts
themselves* (bash, `cmake -P`, and the scripts' own python) as the oracle.

The hypothesis held: three independent slices, 171 compared values at `-O1` and
`-O0` with 0 failures, and 36/36 mutations caught.

## Inventory

Examined at the base commit: `scripts/**`, `cmake/` (absent: the build declares
nothing outside `runtime/CMakeLists.txt`), `tools/**` (build, packaging,
checks, generator and hygiene scripts), `runtime/CMakeLists.txt`,
`runtime/cli`, `tools/experiment-template`, `docs/goose-migration/tasks.json`
(P10-05 row and its source list).

| candidate | lines | model-free decision surface | choice |
|---|---:|---|---|
| `scripts/make-release.sh` | 129 | version ERE, prerelease glob, runtime-asset class, 2 GiB and 100 MB limits, retention guards | **S1** |
| `tools/package-main-runtime.sh` | 51 | dist file list, `SHA256SUMS` format, `manifest.json` key order, module count via grep | **S1** |
| `tools/check-generated-files.py` | 116 | generated-C marker test, 512-byte head, MiB limit conversion, fail/warn branch | **S3** |
| `tools/check-goose-tree.py` | 133 | required-snippet walk, manifest problem order, orphan pass, export scan, exit rule | **S3** |
| `scripts/build.sh` | 20 | `BUILD_DIR`/`JOBS` defaults, exact cmake argv and order | **S2** |
| `scripts/prepare_upstreams.sh` | 67 | pinned table, three-way patch state, ten integration markers, symlink guard | **S2** |
| `runtime/CMakeLists.txt` | 72 | build-type default, `option()` table, subdirectory composition | **S2** |
| `tools/compose-goose.py` | 200+ | import closure and `RENAMES`; pure Go-side, already exercised by every main build | not ported (it *is* the composer the ports ride on) |
| `tools/build-main-runtime.sh` | 133 | module list, determinism gate | lead-owned build; `--repro` already gates it |
| `tools/check-composition.sh`, `check-goose-symbols.py`, `sync-goose-tree.py` | 54/…/… | tree and symbol verification over the real repository state | analysed, needs the repository index; not a pure decision set |
| `scripts/update_goose_migration_plan.py` | 91 | ledger validation and plan rendering | rejected: ~200 lines of one-off f-string layout over one real JSON, no synthetic input domain |
| `scripts/make-tiny-moe.py` | 257 | GGUF fixture generation | rejected: writes model-shaped binaries, outside a model-free gate |
| `scripts/aggregate_baseline.py`, `check_acceptance.py`, `metrics_to_json.py` | 130/140/67 | baseline aggregation, acceptance floors, telemetry JSON | rejected for this experiment: measurement policy (P10 acceptance is about build/release tooling), and the baseline aggregator reads five real run directories |
| `tools/repo-hygiene/*`, `prune-local-builds.sh` | 31/… | deletion and link hygiene | rejected: destructive filesystem operations, not a decision set to port |
| `cmake/` | 0 | no such directory | — |

Prefixes `pkg_`, `env_`, `art_` for exports and `rlp_`, `ev_`, `gap_` for the
internal helpers are free in the current tree; the helper prefixes were renamed
after a collision check against the 66 modules of
`tools/build-main-runtime.sh`, so the ports compose into the main unit without a
`compose-goose.py` `RENAMES` entry.

## Slices and interfaces

One Goose module per slice. Every export takes and returns scalars, `u8`/`i64`
slices only, per the flat export ABI; text goes to caller-sized buffers and the
return value is the exact byte count.

### S1 — `source/release-plan.goose` (`pkg_`)

| export | native |
|---|---|
| `pkg_version_ok(s)` | `make-release.sh:40`, the bash ERE `^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[0-9]+)?$` |
| `pkg_prerelease(s)` | `:98` `[[ "$version" == *-rc.* ]]` |
| `pkg_basename(s, out)` | GNU `basename` as used at `:118` |
| `pkg_runtime_asset(name)` | `:118` `case "$(basename "$asset")" in goose-runtime-*)` |
| `pkg_asset_selected(name)` | `:120-122` the jq selector `startswith("goose-runtime-")` on the raw name |
| `pkg_asset_over_limit(size)` | `:82` `[ "$size" -gt 2147483648 ]` |
| `pkg_tree_over_limit(size)` | `:86` `awk '$4 > 100000000 {print $5}'` |
| `pkg_keep_asset(has_runtime, keep_old, tag_present, same_version, name)` | `:117-129` the retention guard chain |
| `pkg_dist_files(out)` | `package-main-runtime.sh:26-27` |
| `pkg_sha256sums(...)` | `:36-38` `"\n".join(f"{sha}  {name}") + "\n"` |
| `pkg_manifest_json(...)` | `:40-49` `json.dumps(manifest, indent=2) + "\n"`, insertion order preserved |
| `pkg_module_count(text)` | `:31` `grep -cE '^\s+goose/.*\.goose\s*$'` |
| `pkg_version_line(text, out)` | `:34` `$(goose version \| tail -1)` |

### S2 — `source/build-env.goose` (`env_`)

| export | native |
|---|---|
| `env_use_default(set, val)` | `build.sh:9-10`, bash `${VAR:-default}` on unset **and** empty |
| `env_build_dir(root, set, val, out)` | `build.sh:9` |
| `env_jobs(set, val, out)` | `build.sh:10` (text; `cmake --build -j` owns the numeric parse) |
| `env_cmake_args(...)` | `build.sh:16-30`, the 14 configure words in script order |
| `env_build_type(set, val, multi, out)` | `runtime/CMakeLists.txt:28-30` |
| `env_subdirs(llama, cli, tests, tools)` | `:44-72`, bit mask core/cli/tests/tools |
| `env_option_table(text, out)` | `:32-36`, `option(NAME "desc" DEFAULT)` in file order |
| `env_pin_ok(expected, actual)` | `prepare_upstreams.sh:27` |
| `env_integration_state(reverse_ok, markers)` | `:63-70`, 1 applied / 2 preserved / 3 apply |
| `env_link_guard(exists, is_symlink)` | `:76-80` |
| `env_markers_all(text, snip, off, len, n)` | `:42-61`, `integration_present()`'s ten `grep -Fq` markers |
| `env_upstream_pins(text, out)` | `:38-40`, the `clone_at url dir sha branch` table |

### S3 — `source/generated-artifact-policy.goose` (`art_`)

| export | native |
|---|---|
| `art_is_generated(path, head)` | `check-generated-files.py:49-59` |
| `art_limit(mb_milli)` | `:92` `int(args.mb * 1024 * 1024)` |
| `art_gate(all_flag, size, limit)` | `:96-104`, 2 fail / 1 warn |
| `art_required_split(line, rel, snip)` | `check-goose-tree.py:49-59`, `strip` + `partition("\t")` + comment skip |
| `art_required_kind(line, file_present, snippet_found, rel, snip)` | the same walk with the filesystem answer applied |
| `art_manifest_mask(file_present, hash_match, exp_present)` | `:86-99`, problem bits 1/2/4 with the `continue` |
| `art_orphan(in_manifest)` | `:101-106` |
| `art_verdict(problems, warnings)` | `:128-133` |
| `art_export_names(text, off, len, names)` | `:52` `re.findall(r"(?m)^export fn (\w+)")` |

## Gate design

`tests/build.sh <slice|all> [final|mutate]` builds the generated C at `-O1`
(final) or `-O0` (mutate), links the fixture against it and runs it. The fixtures
never re-type the native rules: each calls `tests/oracle_<slice>.py`, which
extracts the rule text from the byte-identical copies in `tests/native/` and runs
it with the native interpreter.

- Shell rules: `[[ =~ ]]`, `case`, `[ -gt ]`, `basename`, `grep -cE`, `awk`,
  `tail -1` — extracted from the script line by line and executed by `bash`/GNU
  tools, including a synthetic `git ls-tree -r -l` line for the size rule.
- Packaging: the script's own `<<'PY' … PY` block is executed as the script
  executes it, on a synthetic dist directory, and its `SHA256SUMS` and
  `manifest.json` bytes are compared with the port byte for byte.
- CMake rules: the native `if(NOT CMAKE_BUILD_TYPE …)` block, the `option()`
  lines and the `if(BMOE_BUILD_…)` conditions are evaluated by `cmake -P`.
- Python rules: `check-generated-files.py` and `check-goose-tree.py` are imported
  as modules; `is_generated_goose_c()` and `check_required()` are called
  directly, the size-limit and gate expressions are `eval`ed from the copied
  text, and the export regex is taken from the copied text.
- The pinned-upstream table and the `option()` table are generated by both arms
  from the same native text, so a drift in either script changes both the port
  and the gate expectation on the next run.

Case tables live in `tests/cases/*.tsv` and are the single input of both arms;
`\n`/`\t` escapes are decoded identically, and `~512~` is 512 filler bytes for
the head-truncation case.

## Gate numbers

| slice | cases | values | failures `-O1` | failures `-O0` |
|---|---:|---:|---:|---:|
| `pkg_` | 73 | 73 | 0 | 0 |
| `env_` | 44 | 44 | 0 | 0 |
| `art_` | 54 | 54 | 0 | 0 |

`results/gate-o1.log`, `results/gate-o0.log`.

## Mutations

`python3 tests/mutate.py 1|2` — twelve mutations per slice, two halves of six;
each rewrites one decision, regenerates that module at `-O0`, relinks the
fixture and reruns the gate. Both halves: 18/18 caught (36/36 total).
`results/mutate-1.log`, `results/mutate-2.log`.

## Integration step

1. Copy the three modules into the canonical tree:
   `source/release-plan.goose` → `goose/misc/release-plan.goose`,
   `source/build-env.goose` → `goose/misc/build-env.goose`,
   `source/generated-artifact-policy.goose` → `goose/misc/artifact-policy.goose`.
2. Append those three paths to `MODULES` in `tools/build-main-runtime.sh`. They
   import `std` only and define no helper name that another module in that list
   defines (`rlp_`, `ev_`, `gap_` families checked), so no `RENAMES` entry is
   needed.
3. Run `bash tools/build-main-runtime.sh`: expect the symbol check to report
   0 duplicates and the three modules' exports to appear in `main.h`.
4. Register the three files in `goose/MANIFEST.json` through the normal sync and
   run `python3 tools/check-goose-tree.py`.
5. The shell and python front-ends then call these exports instead of computing
   the decisions themselves; per the P10-05 acceptance criterion the declarative
   build declarations and launch wrappers stay as they are.

## Compatibility

- Exports are additive; no existing module, symbol, header or ABI is changed.
- No global state, no allocation, no I/O: every export is a pure function of its
  arguments, so both optimisation levels and any call order produce the same
  bytes.
- The gate needs `bash`, GNU `awk`/`grep`/`basename`, `cmake` and `python3`; it
  needs no GPU, model, network or repository index.
- Native copies are inputs, never patched; the oracle re-extracts from them on
  every run, so a change in the scripts surfaces as a gate failure rather than a
  silent divergence.

## Assumptions and recorded approximations

1. `pkg_module_count` models GNU `\s` as `[[:space:]]` (32, 9, 11, 12, 13) on a
   line that cannot contain `\n`.
2. `pkg_version_line` models `$(… | tail -1)` including command-substitution
   newline trimming; the line's own trailing spaces survive.
3. `pkg_basename` is POSIX: trailing slashes drop, all-slashes gives `/`, empty
   gives empty (GNU `basename` prints nothing).
4. `pkg_manifest_json` reproduces `json.dumps(indent=2, ensure_ascii=True)` for
   this fixed shape. Bytes above `0x7f` become `\u00NN`; a non-ASCII compiler
   version would need UTF-8 aware escaping and is out of scope (the gate covers
   quote, backslash and tab in the version string).
5. `pkg_keep_asset` takes `tag_present` and `same_version` from the caller; the
   native `[ -n "$old" ]` / `[ "$old" = "$version" ]` guards and the outer
   `has_runtime`/`keep_old` guard are one decision chain, the message texts are
   not ported.
6. `pkg_asset_selected` (raw name, jq `startswith`) and `pkg_runtime_asset`
   (`case` on the basename) are two different native rules and intentionally
   disagree for a name with a directory component.
7. `env_cmake_args` emits one argv word per line; no native word contains a
   newline.
8. `env_jobs` returns the resolved text: `${JOBS:-8}` performs no numeric
   validation, `cmake --build -j` owns that.
9. `env_option_table` takes the token after the last quote of an `option()` line
   as the default; the canonical file keeps that shape.
10. `env_integration_state` and `env_markers_all` receive the answers of the two
    native callees (`git apply --check`, `integration_present`); the if/elif
    chain and the marker list are the ported decisions.
11. `art_limit` is integer milli-MiB: `mb_milli * 1048576 / 1000` equals
    `int(mb * 1048576)` for every value in the case table (0.001 … 100 MiB and
    half/quarter steps). A non-representable MiB value is out of contract.
12. `art_is_generated` expects the 512-byte head prefix the native `index_head()`
    hands the predicate; the fixture truncates identically.
13. `art_export_names` approximates Python's `\w` as ASCII `[A-Za-z0-9_]`; Goose
    identifiers are ASCII, so the two agree on canonical sources.
14. `art_manifest_mask`, `art_orphan` and `art_verdict` return decision codes;
    the problem sentences stay in the native tool, which is presentation.

## Conflicts and overlaps

- No overlap with the existing canonical modules: `gguf-split` (`gsp_`) owns
  shard naming, `hf-cache-naming` (`hf_`) owns cache paths, `edge0-pack-index`
  owns pack tables, `config-validate`/`config-resolve` own runtime config. This
  experiment owns *build/release/packaging* decisions only.
- `tools/compose-goose.py` and `tools/build-main-runtime.sh` are lead-owned; the
  ports need no change in either beyond the module-list append.
- `docs/goose-migration/tasks.json` P10-05 remains TODO until the lead accepts
  and integrates; its `slices_total`/`slices_done`/`remaining` mirror is the
  lead's step.

## SHA-256

Full list with the native copies: `results/hashes.txt`. Headline values:

| file | sha256 |
|---|---|
| `source/release-plan.goose` | `d970ab3e4113b49e419ba4d2cd4db86229274ee03548cb69379f9850f64eb4c2` |
| `source/build-env.goose` | `aba4a46c1be8ecf5beb46ed15f8d6613e64ee2679cef9c7345c2755799538de3` |
| `source/generated-artifact-policy.goose` | `a7966bab0d1ce2c0a02552b9af5efce7d34fd5d3a5060a7476a58b105e177f1d` |
| `tests/cases/pkg.tsv` | `7413b2235344f7a3425f03605395c055b97c1a1da875612f65b9e50537a31864` |
| `tests/cases/env.tsv` | `84c54888a8894516f7c915523fd3ccbbfe1fe3972f5d73536f6992b36fd0ddc6` |
| `tests/cases/art.tsv` | `b427d5dd8a6b171bb5b4d462ef350e1070d3891e1aa2aa3fdadb5e6fa051f202` |
| `tests/native/make-release.sh` | `31b64e6241a6ee9e942515e6ee0405f18fed635a4b0b94192a9e4bc160a1b4f6` |
| `tests/native/package-main-runtime.sh` | `8c6fef9cc8fe07ba5ee73978058c80f16dcfb473c742ca669e701efe45f9bdf7` |
| `tests/native/check-generated-files.py` | `dbba03117aa29a6adaa4b82a98fde04268ba08888a477481091c8cddfc7d938c` |
| `tests/native/check-goose-tree.py` | `34b8eb443cf5cb9715d020551d78361fae4018b6acc251c2928ca0e8983a5cd7` |
| `tests/native/build.sh` | `2065706ef0b04b1fba37bb6c6933a3f515d23bb0e5e141ef3742ec05d6581c52` |
| `tests/native/prepare_upstreams.sh` | `30ce6c13c3b2d37da87a4058283fa6bf39961fed8a6de3f6499e9065697734c2` |
| `tests/native/runtime-CMakeLists.txt` | `1783219a570bfeebb73d91bbba428518658e2025bbac3959a6229aede0a2eaea` |

Toolchain: goose `1e714e6146bc`, `gcc-13 (Ubuntu 13.4.0-10ubuntu1)`,
`cmake 4.2.3`, `Python 3.14.4`.
