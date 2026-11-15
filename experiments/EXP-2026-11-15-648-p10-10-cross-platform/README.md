# EXP-2026-11-15-649 — P10-10: CPU-feature, platform-path and device-selection policy

Three independent Goose slices for the parts of the runtime that decide *where*
code runs: which CPU variant a shared backend offers, which directories and
separators a platform uses, and which device the loader picks. Requirement:
`P10-10` — preserve cross-platform and alternate CPU/device support.

- **Base commit:** `aa9109b5`.
- **Native tree:** `upstream/llama.cpp` at the same commit. The files this slice
  reads are byte-identical in `work`/the live service tree except
  `ggml/src/ggml-backend-reg.cpp`, `common/common.cpp` and
  `ggml/src/ggml-cpu/kleidiai/kleidiai.cpp`; where they differ the subset this
  slice reads is unchanged, and every line reference below is to
  `upstream/llama.cpp`.
- **Production code changed:** none. No model, no GPU and no device was started.
- **Working tree:** `/tmp/opencode/sub-649` (outside the repository).

## Hypothesis

Everything the runtime needs in order to *choose* a platform path, a CPU variant
or a device is a decision over values the caller can supply: hwcap/hwcap2 bits,
sysctl/`IsProcessorFeaturePresent` results, a compile-flag word, environment
strings, a list of registered device names and types, and a list of candidate
shared libraries with their scores. Each block can therefore be replaced by a
Goose export that touches no device, opens no library and reads no global, and
each can be gated on synthetic inputs against the native decision.

The hypothesis is accepted only if, for every slice:

1. the port reproduces the native decision on every case at `-O1` **and** at
   `-O0`, with byte-identical gate output;
2. every half of 6 mutations is caught by the gate at `-O0`;
3. the three modules compose with the canonical unit without a duplicate global
   symbol and without changing an existing interface.

## Slices

| Slice | Module | Native source | Exports | Checks |
|---|---|---|---|---|
| S1 `feat_` | `source/feat-cpu-features.goose` | `ggml/src/ggml-feats.h:95-140`; `ggml-cpu/arch/arm/cpu-feats.cpp:7-44`; `ggml-cpu/arch/x86/cpu-feats.cpp:263-325`; `kleidiai.cpp:75-90,317-321,380-386` | 7 | 228 |
| S2 `plat_` | `source/plat-paths.goose` | `common/common.h:25-29`; `common/common.cpp:232-283,1007-1017,1019-1071,1073-1116,1118-1127,1185-1202` | 8 | 76 |
| S3 `dev_` | `source/dev-backend-select.goose` | `ggml/src/ggml-backend-reg.cpp:186-224,307-315,318-333,344-363,381-390,404-466,468-482,484-560` | 10 | 80 |

### S1 `feat_` — CPU feature decode and backend score

| export | native | scope |
|---|---|---|
| `feat_arch64_linux(hwcap, hwcap2, sve_raw, out)` | `ggml_feats_get_arch64_runtime`, Linux branch | seven feature flags plus `sve_cnt`; fp16 needs FPHP **and** ASIMDHP; a failed `prctl` leaves the length at zero; the length is masked with `PR_SVE_VL_LEN_MASK` |
| `feat_arch64_apple(vals, ok, out)` | Apple branch | a failed `sysctl` leaves the flag false, so the result is the conjunction of "call succeeded" and "value non-zero"; no vector length |
| `feat_arch64_windows(pf, out)` | Windows branch | one `IsProcessorFeaturePresent` id per flag; no vector length |
| `feat_arch64_score(bits, flags)` | `ggml_backend_cpu_aarch64_score` | 0 when a required flag is missing, else `1 + Σ 2^(bit+1)` |
| `feat_x86_score(bits, flags)` | `ggml_backend_cpu_x86_score` | the same shape; the `GGML_AVX512` block requires F, CD, VL, DQ and BW together |
| `feat_kleidiai_mask(bits, sve_cnt, sme_cores)` | `kleidiai.cpp:317-321,380-386` | SVE only when the vector length is exactly `QK8_0` (32); SME/SME2 only with a positive core budget |
| `feat_kleidiai_name(mask, out)` | `cpu_feature_to_string` | branch order NONE, SME2, SME, SVE, I8MM, DOTPROD, else UNKNOWN |

The decode is checked against the **real** `ggml-feats.h`: the fixture defines
`getauxval`/`prctl` as injected stubs, defines `__aarch64__` for the include and
then compares the native record field by field. The Apple and Windows branches
cannot be compiled here (`sysctl.h`, `windows.h`), so those two are
transcriptions. The two score functions are transcriptions with the compile-time
flag word lifted to a parameter; `tests/x86-score-check.sh` closes that gap for
x86 by including the real `cpu-feats.cpp`, asking the real CPUID for the host
feature word and requiring `port == real` for eight flag words.

### S2 `plat_` — paths, environment, priority, terminal

| export | native | scope |
|---|---|---|
| `plat_sep(platform)` | `DIRECTORY_SEPARATOR` | 92 on Windows, 47 elsewhere |
| `plat_cache_dir(platform, LLAMA_CACHE, XDG_CACHE_HOME, HOME, pw_dir, LOCALAPPDATA, out)` | `fs_get_cache_directory` | `LLAMA_CACHE` wins and gets only a trailing separator; otherwise XDG, `$HOME/.cache/`, the Linux `getpwuid` fallback, `$HOME/Library/Caches/`, `%LOCALAPPDATA%`, and the `llama.cpp` suffix appended only to the computed root |
| `plat_config_dir(platform, XDG_CONFIG_HOME, HOME, pw_dir, APPDATA, out)` | `fs_get_config_directory` | Apple shares the XDG branch; the `getpwuid` fallback is Linux-only |
| `plat_cache_file(...)` | `fs_get_cache_file` | the separator assertion in the file name, then root + name; directory creation stays with the caller |
| `plat_set_env(platform, value_empty)` | `common_set_env` | empty value unsets on POSIX, `_putenv_s` on Windows |
| `plat_priority(platform, prio)`, `plat_priority_noop(prio)` | `set_process_priority` | nice `5/0/-5/-10/-20` or the five `SetPriorityClass` constants; `NORMAL` is a no-op success |
| `plat_tty_colors(NO_COLOR, TERM, out_tty, err_tty)` | `tty_can_use_colors` | non-empty `NO_COLOR` first, `TERM=dumb` second, then `out || err` |

Return convention: a non-negative value is the number of bytes written, `-1`
"failed to find `$HOME`/`%LOCALAPPDATA%`", `-2` "not implemented on this
platform" (Emscripten), `-3` the file-name separator assertion.

### S3 `dev_` — device and backend selection

| export | native | scope |
|---|---|---|
| `dev_by_name(blob, off, len, n, name)`, `dev_reg_by_name(...)` | `ggml_backend_dev_by_name`, `ggml_backend_reg_by_name` | ASCII case-insensitive equality, first registration wins |
| `dev_by_type(types, n, want)` | `ggml_backend_dev_by_type` | first device of the type in registration order |
| `dev_init_best(types, n)` | `ggml_backend_init_best` | GPU, then IGPU, then CPU, never ACCEL/META |
| `dev_best_score(has_score, scores, n)` | the selection loop of `ggml_backend_load_best` | strictly greater wins, the first of equal scores stays, zero best means "no candidate" |
| `dev_backend_filename(platform, name, out)` | `backend_filename_prefix/_extension` | `libggml-<name>.so` / `ggml-<name>.dll` |
| `dev_match_candidate(platform, name, filename)` | the candidate test in `load_best` | the trailing dash belongs to the prefix, so `libggml-cpu.so` is not a variant candidate; `libggml-cpu-armv8.so.1` is rejected by the extension |
| `dev_exe_prefix(platform, path, resolved, out)` | `get_executable_path` | executable directory plus separator; `./` on a failed Linux `readlink`; empty for a failed `GetModuleFileNameW` |
| `dev_search_paths(...)` | the search-path list | the user path alone, or `GGML_BACKEND_DIR`, the executable directory and the current directory, with a failed `current_path()` dropping the entry |
| `dev_registered_ids(...)` | `register_backend`/`register_device` | a repeated backend id is skipped with all of its devices, a repeated device id on its own |

The name and filename tests use the real `std::filesystem::path` in the oracle,
so extension edge cases (`.`, `..`, a leading dot, a versioned `.so.0`) are
compared against the standard library rather than retyped.

## Gate

```
bash tests/build.sh all
[-O1] feat: 228 checks, 0 failures
[-O1] plat:  76 checks, 0 failures
[-O1] dev:   80 checks, 0 failures
[-O0] feat: 228 checks, 0 failures
[-O0] plat:  76 checks, 0 failures
[-O0] dev:   80 checks, 0 failures
[diff] feat: -O1 and -O0 identical
[diff] plat: -O1 and -O0 identical
[diff] dev:  -O1 and -O0 identical
```

384 checks, 0 failures, identical output at both optimisation levels.

```
bash tests/x86-score-check.sh
x86-score host bits=63 flags=0    real=1    port=1    OK
x86-score host bits=63 flags=3    real=4    port=4    OK
x86-score host bits=63 flags=63   real=64   port=64   OK
x86-score host bits=63 flags=175  real=0    port=0    OK
x86-score host bits=63 flags=1920 real=0    port=0    OK
x86-score host bits=63 flags=112  real=0    port=0    OK
x86-score host bits=63 flags=3200 real=0    port=0    OK
x86-score host bits=63 flags=4095 real=0    port=0    OK
```

## Mutations

```
python3 tests/mutate.py --module all
mutations: 36/36 caught
```

12 mutations per slice, six in each half; each mutant passes `goose check`, is
built at `-O0` and must change the gate output or its exit code.

- `feat_` half 1: fp16 conjunction weakened, SVE length unmasked, failed `prctl`
  accepted, dotprod hwcap bit, sve2 hwcap2 bit, Apple result without the
  success conjunction. Half 2: Windows SME2 bit dropped, dotprod weight, AVX512
  four-bit requirement, AMX_INT8 weight, "any length ≥ 32" for SVE, kleidiai
  name order.
- `plat_` half 1: `LLAMA_CACHE` trailing separator, BSD `getpwuid`, missing
  passwd accepted, Apple `Caches` spelling, missing `%LOCALAPPDATA%` accepted,
  Emscripten status. Half 2: Apple leaves the XDG branch, config `getpwuid`
  fallback moved to BSD, separator assertion dropped, medium priority value,
  `TERM=dumb`, Windows environment write.
- `dev_` half 1: case-sensitive name match, type lookup inverted, IGPU step
  dropped, accelerator fallback, equal scores, missing score symbol. Half 2:
  Windows prefix, extension check relaxed, optional trailing dash, failed
  `readlink` default, trailing separator, failed `current_path()`.

## Integration

The three modules are drop-in canonical modules. Exact step:

```
cp source/feat-cpu-features.goose  goose/cpu/
cp source/plat-paths.goose         goose/misc/
cp source/dev-backend-select.goose goose/server/
```

then append to `MODULES` in `tools/build-main-runtime.sh`:

```
    goose/cpu/feat-cpu-features.goose
    goose/misc/plat-paths.goose
    goose/server/dev-backend-select.goose
```

and run `bash tools/build-main-runtime.sh --repro`. The script composes the
unit, typechecks it, generates C, compiles at `-O1` and fails on a duplicate
global symbol. That check was run out of tree from this working directory with
the canonical list plus these three modules: **typecheck ok, 696 global symbols,
0 duplicates** (75 modules before the addition).

Host wiring is a later stage: the exports are pure C functions with no
`extern` dependency, so the caller keeps every platform call
(`getauxval`/`prctl`/`sysctl`/`IsProcessorFeaturePresent`, `getenv`,
`getpwuid`, `setpriority`, `dlopen`, directory iteration) and only the decisions
move. The generated header is the ABI (`sl_i64`, `sl_u8`).

## Compatibility and conflicts

- **Prefixes:** `feat_`, `plat_`, `dev_` are free in the canon (checked over
  every `export fn` under `goose/`, including `_archive`); no export was
  renamed.
- **Interfaces used:** none beyond the Goose standard library; no native type,
  no global and no existing module is referenced. `goose/misc/common-strings.goose`
  and `goose/server/hf-cache-naming.goose` are the neighbours: `hf_*`
  deliberately excludes the `LLAMA_CACHE`/`XDG_*` reading that `plat_` owns, so
  the two do not overlap. `ggml_backend_dev_type` values stay with the caller as
  integers 0-4, in the native enum order.
- **Behaviour preserved:** the score functions still return 0 for an
  unsatisfied flag word; the loader still rejects a zero score. The path
  functions still append the trailing separator in the same places. The device
  lookup still prefers the first registration. Nothing in the slice changes a
  default; there is no new environment variable and no opt-in flag.
- **Possible conflicts:** if another pending slice also ports
  `fs_get_cache_directory` (for example a download-cache slice), the two must be
  merged rather than both added; `ggml_backend_load_best`'s search-path list is
  also touched by the live tree's own patch (it added an
  `fs::exists`/`skip_permission_denied` guard), so a rebase on a newer canon must
  re-read `:489-503` before the gate is trusted.

## Recorded limits

- The Linux ARM64 decode is executed natively; the Apple and Windows decodes are
  transcriptions, because this host has neither header. They are compared only
  against the transcription, and the slice claims no ARM64 or Windows execution.
- The x86 real-function cross-check runs on this host's CPU, which reports
  `bits=63` (no AVX-VNNI, no AVX512, no AMX): the positive branch is exercised
  for FMA/F16C/SSE42/BMI2/AVX/AVX2, and the AVX512/AVX-VNNI/AMX blocks are
  exercised only through their reject path. The synthesised gate covers the
  accept path for those bits.
- `striequals` is modelled as ASCII-only; the native `std::tolower` on a signed
  `char` with a non-ASCII byte is undefined behaviour in the original, so the
  port defines the sane behaviour instead of copying the UB.
- `std::filesystem::path::extension` and the platform separator rules are
  modelled for the byte strings the loader actually sees; a Windows path with a
  drive-relative form is not modelled.
- `fs_get_cache_file`'s directory creation is a filesystem side effect and stays
  with the caller; only the name rule is owned here.
- The native throws where the port returns `-1`/`-2`; the caller must map the
  status back to its own exception type.

## SHA-256

```
b073599a4ac4881615e5ca04d57ba891922991009a5da79c388ff64e3f68a3b7  source/dev-backend-select.goose
c0a6a8b042a47d8041bcb2eaae4a98eb580ea74ccfbc117c0a506a1431722d56  source/feat-cpu-features.goose
93bf74ca41974c0c62ea823ac45fff0d9df67df909caef50080e69a79fdb881e  source/plat-paths.goose
770704feb52ef1875692a5ed93eac73d10de7286c2b9ae6ab80db4e8e7e31a33  tests/fixture_dev.cpp
9949997f0608720eddc99ab12d8bd777e890739022f27c14a94e4219911d8533  tests/fixture_feat.cpp
8046f36fb9b9f10befe1d68b2231ca18b5b709ae9c1d293fe213b9b8717ce0b3  tests/fixture_plat.cpp
64d7291e2ba20a3c69003d8268589449dc16e106dade14bce335d85ba01a2504  tests/x86-score-check.cpp
7d854a85b0e7f664ee227c040b40c72c1f766aaf860ffc5144233aa5d9d9cd99  tests/mutate.py
a995e77f975520472926c2471069b4847262dcdfccf16fcd63529923948f7b92  tests/build.sh
1ac62fda4f373d3694baaba71a7fe1963307b6baf29b7d93b61a49212441e165  tests/x86-score-check.sh
```

## Reproduction

```
bash tests/build.sh all          # 384 checks, -O1 and -O0 identical
bash tests/x86-score-check.sh    # 8/8 flag words agree with the real function
python3 tests/mutate.py --module all   # 36/36 caught
python3 tests/mutate.py --module feat --half 1   # single half
```

Optional overrides: `GOOSE_BIN`, `CC`, `CXX`, `FG_UPSTREAM`, `OUT_DIR`,
`FG_ONLY`. `tests/build.sh` derives the root from its own location and stores no
absolute scratch path; generated C, objects and gate binaries live under
`build/` and are not part of the delivery.
