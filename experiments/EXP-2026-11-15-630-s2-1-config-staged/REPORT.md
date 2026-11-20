# Staged S2.1 — the Goose config validator inside the staged runtime build

Date: 2026-11-15
Base commit of the repository: `b18249e0`
Work directory: `/tmp/opencode/sub-632/tree` (the repository was not modified, nothing was committed)
Stage: `docs/goose-migration/RUNTIME-INTEGRATION.md`, S2 item 1
("Goose validation of the configuration, behind a build flag")
Model: EXP-2026-11-15-626 (staged S1.1, one Goose module behind a flag)

## 1. Goal and result

Goal: switch `bmoe::validate()` from the native decision chain to the accepted
Goose module `cfg_validate` through a build flag, without touching production,
and prove the switch by a model-free differential gate.

Result: done for `goose/cli/config-validate.goose` (48 rules, 32 + 5 flat inputs).

| check | result |
|---|---|
| composed unit (`arch-registry` + `config-validate` + `config-resolve`) | typechecked, 95 top-level declarations |
| `goose --standalone` | one C unit, 228,895 B |
| staged build, `-DFG_GOOSE_CONFIG=OFF` | built (whole staged tree, 12 targets) |
| staged build, `-DFG_GOOSE_CONFIG=ON` | built (whole staged tree, 13 targets) |
| smoke gate, OFF (66 cases + 6 order pins + 40,000 fuzz) | 0 failures, rules 48/48 |
| smoke gate, ON (same corpus) | 0 failures, rules 48/48 |
| accept/reject agreement | 40,072 / 40,072 in both builds |
| error-text agreement | 40,072 / 40,072 on ON (every message, byte for byte) |
| `cfg_validate` definitions | 1 in the ON library, 0 in the OFF library |
| `validate_native` definitions | 1 in both libraries (the fallback stays compiled) |
| `goose_calls` | 40,072 on ON (one per `validate()`), 0 on OFF |
| mutation gate, 12 mutants in two halves | half A 6/6, half B 6/6, 12/12 caught |
| baseline after restoring every mutant | green again: 72/72, `failures=0` |
| model, server, GPU | not used at all |

## 2. Environment

```
gcc / g++   15.2.0 (Ubuntu 15.2.0-16ubuntu1)
cmake       4.2.3
goose       runtime 1e714e6146bc, toolchain goose-2026.3-string
kernel      7.0.0-43-generic
```

No GPU run, no `llama-server`, no model file, no commit.

## 3. What was copied and what was added

The tree started as the staged S1.1 copy (`rsync -a` from
`/tmp/opencode/sub-629/tree`). Added for this stage:

| source (repository) | destination (staged) | role |
|---|---|---|
| `goose/cli/config-validate.goose` | `goose/cli/` | the module under test |
| `goose/cli/config-resolve.goose` | `goose/cli/` | composed with it (P9-01 set) |
| `goose/model/arch-registry.goose` | `goose/model/` | composed with it (P9-01 set) |
| `runtime/core/src/config.cpp` | `runtime/core/src/` | the carrier: native body + dispatch |
| `runtime/core/include/bmoe/config.h` | `runtime/core/include/bmoe/` | the structs, unchanged |
| `tools/compose-goose.py` | `scripts/` | composition, unchanged |

New files of this stage:

| file | purpose |
|---|---|
| `common/fg-config-native.h` | declares `bmoe::validate_native`, the pre-integration body |
| `common/fg-goose-config-bridge.h/.cpp` | `RunConfig` -> `iv[32]`/`fv[5]`, the call, the witness counter |
| `tests/fg_config_smoke.cpp` | the gate: two paths, one corpus |
| `tests/config_mutate.py` | the mutation gate: 12 mutants, two halves, restore and re-gate |
| `scripts/config-gate.sh` | the exact command sequence of section 4 |
| `patches/s21-config-validate.diff` | `config.cpp` against the repository copy |
| `patches/s21-staged-cmake.diff` | the staged `CMakeLists.txt` delta |
| `evidence/` | logs, smoke output, `nm`, `ldd`, SHA-256, `mutations.txt` |

`runtime/reconstructed/`, the S1.1 JSON-schema stage in the same staged project
and the llama.cpp targets are untouched; they are built by the same command and
stand next to the config targets, not on their link line.

The seven archived `.cpp` inputs under `runtime/reconstructed/{src,server-merge}`
matched the corresponding files in [the S1.1 staging snapshot](../EXP-2026-11-15-626-s1-1-apex-staging/runtime/reconstructed)
byte for byte. That snapshot holds the shared source copies. The full historical
directory is also available at commit `f3ac1c0646908671700933e5420564b4786d21c5`.

Deliverable and artefacts: source, scripts, patches, report and`evidence/` are
the deliverable. `goose-gen/config-slice.c`, `goose-gen/config-slice.h`, every
`build-*` directory and every binary in them are reproducible from
`goose/cli/config-validate.goose` plus `scripts/config-gate.sh` and must not be
committed.

## 4. Exact commands

```bash
cd /tmp/opencode/sub-632/tree

# 1. Compose the P9-01 set, typecheck, and generate one standalone C unit.
python3 scripts/compose-goose.py goose-gen/config-slice.goose \
    goose/model/arch-registry.goose \
    goose/cli/config-validate.goose \
    goose/cli/config-resolve.goose
# -> composed 3 module(s)
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose check goose-gen/config-slice.goose
# -> parsed ok: 95 top-level declarations
#    typechecked ok: 40 specialization(s), 0 struct/0 enum instance(s)
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose --standalone \
    -o goose-gen/config-slice.c --header goose-gen/config-slice.h goose-gen/config-slice.goose
# -> wrote goose-gen/config-slice.c (228895 bytes)

# 2. Staged build, flag OFF. Builds the whole staged tree (llama.cpp included).
cmake -S . -B build-config-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_CONFIG=OFF
cmake --build build-config-off -j 32

# 3. Staged build, flag ON.
cmake -S . -B build-config-on -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_CONFIG=ON
cmake --build build-config-on -j 32

# 4. Gate.
./build-config-off/fg-config-smoke
./build-config-on/fg-config-smoke

# 5. Mutation gate: 12 mutants, two halves, restore and re-gate (19 s).
python3 tests/config_mutate.py
```

`scripts/config-gate.sh` runs steps 1-4 in one shot (2 min 25 s from empty build
directories, both modes, whole tree) and writes `evidence/`.
`tests/config_mutate.py` runs step 5 and writes `evidence/mutations.txt`.
Re-running both from scratch reproduces every artefact hash in section 10.

## 5. How the flag is wired

The flag touches one existing file and adds three. The complete diff is in
`patches/`.

`runtime/core/src/config.cpp` — the native body is renamed, not edited:

```cpp
ValidationResult validate_native(const RunConfig & cfg) { ...original body... }

#ifdef FG_GOOSE_CONFIG
ValidationResult validate(const RunConfig & cfg) {
    int64_t iv[32]; double fv[5];
    fg_config_to_flat(cfg, iv, fv);
    const long long rule = fg_config_validate_flat(iv, fv);
    switch (rule) {
    case 0:  return r;                                        // CFG_OK
    case 1:  return fail("model_path is required");
    ...
    case 48: return fail("moe.substitute_lambda must be in [0, 1] ...");
    default: return fail("config validation failed inside the Goose module (rule " +
                         std::to_string(rule) + ")");
    }
}
#else
ValidationResult validate(const RunConfig & cfg) { return validate_native(cfg); }
#endif
```

`common/fg-goose-config-bridge.cpp` — the flat fill, and the precision rule:

```cpp
inline double narrow(float v) { return static_cast<double>(v); }   // float -> double
static_assert(is_float32<decltype(std::declval<RunConfig>().moe.drop_cold_frac)>(), "...");
fv[3] = narrow(cfg.moe.drop_cold_frac);                            // never double -> double
```

The narrowing is the point EXP-620 recorded: the native comparisons are
single-precision (`> 0.0f`, `<= 1.0f`), so a double in `fv[]` can disagree at
the `[0, 1]` boundaries. `RunConfig`'s five float fields are `float` today; the
`static_assert`s make the bridge fail to compile if one of them is ever widened,
instead of silently changing precision.

`CMakeLists.txt` (staged project):

```cmake
option(FG_GOOSE_CONFIG "Select the Goose config validator (cfg_validate) instead of the native validate()" OFF)
# -O1 for the gate, -O0 for the mutation runs; a mutant never edits this file.
set(FG_GATE_OPT "-O1" CACHE STRING "Optimization level of the config gate and the Goose object")

add_library(fg-config-core SHARED runtime/core/src/config.cpp)
target_compile_options(fg-config-core PRIVATE ${FG_GATE_OPT})   # gate convention

if(FG_GOOSE_CONFIG)
    add_library(fg-goose-config OBJECT goose-gen/config-slice.c)
    set_target_properties(fg-goose-config PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
    target_compile_options(fg-goose-config PRIVATE ${FG_GATE_OPT} -w)
    target_sources(fg-config-core PRIVATE common/fg-goose-config-bridge.cpp)
    target_compile_definitions(fg-config-core PRIVATE FG_GOOSE_CONFIG=1)
    target_link_libraries(fg-config-core PRIVATE fg-goose-config)
endif()
```

Properties that matter for the production rule ("a new feature's flag exists
only to disable it"):

- OFF is the default and runs the untouched native chain;
- the native chain is compiled in both configurations (`validate_native` has one
  definition in each library, see `evidence/nm-counts.txt`), so switching the
  flag off is a rebuild, not a revert;
- no native call is deleted and no runtime conditional exists: the choice is
  made at compile time, once per translation unit.

## 6. The main seam

`cfg_validate` takes two flat slices (`iv[32]`, `fv[5]`) because the export ABI
passes scalars and slices only; the native function takes a `RunConfig` by
reference. The seam is therefore `fg_config_to_flat()` — a hand-maintained
mirror of the struct in header order, 37 assignments plus the `DraftSource`
mapping. `RunConfig` is 5 structs deep (`ReportConfig`/`SamplingConfig`/
`MoeStreamConfig`/`SpecConfig`), and the module re-declares the header's
constants (`cache_min_mb = 1500`, `io_threads_max = 8`, ...) because it must
build standalone.

Consequences recorded for the next slice:

1. A field added to `RunConfig` needs three coordinated edits: the bridge, the
   module's documented index list, and (for a new rule) the carrier's switch.
   Nothing in the toolchain catches a missed edit; the gate only catches value
   shifts the corpus reaches, and it caught exactly one such gap here (below).
2. The module's own guard (`cfg_need`: `len < 32` or `len < 5` -> `-1`) is the
   only protection against a truncated layout. The bridge always passes full
   slices, so `-1` is unreachable from the carrier and lands in the `default`
   branch as a named failure rather than a silent "valid".
3. `DraftSource` is mapped by name, not by `static_cast`: `none/mtp/ngram` ->
   `0/1/2` in a `switch`. Reordering the enum cannot silently renumber the
   module's `CFG_SRC_*`.

## 7. Gate: what is compared

Two builds, one binary, one corpus. The oracle is `validate_native()` in the
same translation unit — the real native chain, not a transcription.

| | OFF | ON |
|---|---|---|
| `validate()` body | `validate_native()` | bridge -> `cfg_validate` -> switch |
| `FG632_SMOKE` | `mode=native ... failures=0 rules=48/48` | `mode=goose ... failures=0 rules=48/48` |
| `goose_calls` | 0 (symbol not linked) | 40,072 (one per call) |

Per case the gate asserts four things: accept/reject equality, byte-identical
error text, the same rule number (derived from the native message through a
one-needle-per-rule table that fails if a message maps to zero or two rules),
and the pinned rule, so a change in the native source cannot move both arms
together and still pass.

Corpus: 66 table cases (every one of the 48 rules at least once, plus valid
boundaries such as `n_ubatch == n_ctx`, `draft_max == 8`, the band with
`force_cache`, `drop_cold_frac == 1.0`), 6 order pins where two rules fail at
once and the earlier must win, and 40,000 deterministic fuzz configs with
boundary-heavy pools (7/8/9 around the four `*_max` limits, 63/64/65 around
`ngram_match_limit`, 1450..1501 around `cache_min_mb`, NaN injected into all
five float fields).

Coverage notes, stated plainly:

- In the OFF build the first comparison is a structural identity
  (`validate()` forwards to `validate_native()`); the number that carries the
  claim is the ON column.
- Text equality on ON is a real transcription check: the carrier re-creates all
  48 messages, including the 10 that embed values (`n_ubatch=`, `n_ctx=`,
  `draft_max`, `ngram_max_match`, `cache_mb`, `cache_min_mb` twice, the three
  `*_max` limits). Any drift is a failure, not a nuance.
- The fuzz covers value combinations, not the CLI that produces them.

### Two ad-hoc controls that preceded the mutation set

Before the full set existed, two single mutations were run by hand outside the
delivered tree, and both were caught:

| control | mutation | gate reaction |
|---|---|---|
| A | module: `CFG_CACHE_MIN_MB` 1500 -> 1400 | 12 failures: accept/reject at `cache_mb=1499`, text, rule 31, and fuzz divergences at `cache_mb=1498` |
| B | carrier: `"n_ctx must be positive"` -> `"n_ctx must be >= 1"` | 3 failures: the message maps to 0 rules, rule 4 pin broken, `rules=47/48` |

Control A found a gap in the first version of the gate: the fuzz pool held
1499/1500/1501 but nothing in `[1400, 1499)`, so a downward shift of
`cache_min_mb` was invisible. The pool now straddles every module constant on
both sides (`1450`, `1498`, `63..65`, `7..9`). Those two scripts were scratch;
the reproducible set that replaces them is section 8, and their logs are kept
as `evidence/negctl-a-module.log`, `evidence/negctl-b-text.log`.

## 8. Mutations

`tests/config_mutate.py` runs 12 mutants, six per half, and re-gates the
baseline after restoring all of them. Each mutant is one textual change to
exactly one artefact (module, bridge or carrier); the harness refuses an anchor
that does not occur exactly once, so a shifted source cannot mutate the wrong
line. Per mutant: restore pristine -> apply -> rebuild at `-O0` -> run the gate
-> DETECTED/UNDETECTED -> restore. A mutant is DETECTED when the gate exits
non-zero, i.e. when any of its four checks disagrees.

Half A mutates the **shape of the decision chain** (boundaries, rule order,
NaN). Half B mutates the **seam and the limits** (source mapping, precision,
text, rule number, limit constants).

| id | half | target | what is broken | expected catch | caught by |
|---|---|---|---|---|---|
| A1 | A | module | `cache_min_mb` 1500 -> 1400 | case `cache in pathological band`, fuzz 1498 | `[FAIL] cache in pathological band: accept/reject native=0 selected=1` |
| A2 | A | module | `n_ubatch > n_ctx` -> `>=` (equality rejected) | case `ubatch equal ctx is valid` | `[FAIL] ubatch equal ctx is valid: accept/reject native=1 selected=0` |
| A3 | A | module | `n_predict <= 0` -> `< 0` (zero accepted) | case `n_predict zero` | `[FAIL] n_predict zero: accept/reject native=0 selected=1` |
| A4 | A | module | order: `CFG_R_CACHE_AUTO_BOTH` before `CFG_R_CACHE_BAND` | order pin `cache band before auto conflict` | `[FAIL] order: cache band before auto conflict: text differs` |
| A5 | A | module | order: `CFG_R_DROP_RANGE` before `CFG_R_DROP_CACHE` | order pin `drop cache before drop range` | `[FAIL] order: drop cache before drop range: text differs` |
| A6 | A | module | drop range negated -> positive range (NaN passes) | case `drop NaN` | `[FAIL] drop NaN: accept/reject native=0 selected=1` |
| B1 | B | bridge | `DraftSource::ngram` mapped to 1 (MTP), not 2 | cases `ngram rejects p_min`, `ngram min zero` | `[FAIL] ngram rejects p_min: accept/reject native=0 selected=1` |
| B2 | B | bridge | `drop_cold_frac` not narrowed: float plus 1e-9 in double | case `drop at one` (1.0 becomes 1.000000001) | `[FAIL] drop at one: accept/reject native=1 selected=0` |
| B3 | B | carrier | text of rule 4 changed | case `n_ctx zero` | `[FAIL] message maps to 0 rules: n_ctx must be >= 1` |
| B4 | B | module | rule number: slots returns 11 (`VRAM_NEG`) instead of 12 | case `slots negative` (number and text) | `[FAIL] slots negative: text differs` |
| B5 | B | module | `io_threads_max` 8 -> 7 | fuzz with `io_threads=8` | `[FAIL] fuzz 756: native.ok=1 selected.ok=0` |
| B6 | B | module | `route_ahead_max` 8 -> 9 | case `route_ahead range` (9 accepted) | `[FAIL] route_ahead range: accept/reject native=0 selected=1` |

```
half A: caught 6/6
half B: caught 6/6
counted: caught 12/12
extra X1: DETECTED (prefetch_layers_max 8 -> 7)
generated C sha256 3c177364... MATCHES pristine
baseline after restore: FG632_SMOKE mode=goose cases=66 pins=6 fuzz=40000 failures=0 rules=48/48 goose_calls=40072
72/72 (66 cases + 6 order pins): PASS
```

Notes, stated plainly:

- B4 is one mutation that breaks two mechanisms at once by construction: the
  module returns number 11 for an `expert_hot_slots` violation, so the carrier
  prints the vram text. The gate reports it as a text difference and as a rule
  number difference; both checks would have to be disabled to miss it.
- The limit group (`io_threads` / `prefetch` / `route_ahead`) has three members
  and two slots in a 6/6 split, because two slots are reserved for the required
  order pins. B5 and B6 carry `io_threads` and `route_ahead` (the latter has a
  named-case catch); `prefetch_layers_max` is run as **X1, an extra probe
  outside the counted 12**, and is detected by fuzz case 3708. It is reported
  separately rather than merged into another mutant, so no counted mutant is
  compound.
- The first version of the harness was wrong and the run proved it: `copy2`
  restores a pristine file with its old mtime, so `make` kept the previous
  mutant object and mutants B4-B6/X1 were "caught" by B3's leftover text. The
  baseline after restore failed too (`failures=12`). `restore_sources()` now
  bumps the mtime of every restored file; the run above is the corrected one,
  and each mutant is caught by its own mechanism. Same class of bug as a stale
  generated C, which is why `config-gate.sh` touches the generated unit too.
- 12 mutants, 19 s, no model and no GPU: a mutant is a scratch build, not an
  experiment run.

## 9. Sizes

| artefact | bytes |
|---|---|
| `goose/cli/config-validate.goose` (input) | 10,177 |
| composed unit `goose-gen/config-slice.goose` | 27,337 |
| `goose-gen/config-slice.c` (generated) | 228,895 |
| `goose-gen/config-slice.h` (generated) | 1,845 |
| Goose object `config-slice.c.o` | 21,040 |
| `runtime/core/src/config.cpp` (native + dispatch) | 25,632 |
| `common/fg-goose-config-bridge.cpp` | 5,006 |
| `tests/fg_config_smoke.cpp` | 26,020 |
| `tests/config_mutate.py` | 16,180 |
| `evidence/mutations.txt` | 8,641 |
| `libfg-config-core.so` OFF | 42,576 |
| `libfg-config-core.so` ON | 88,712 (+46,136, +108 %) |
| `fg-config-smoke` OFF / ON | 43,360 / 43,416 |

The ON library growth is the whole composed P9-01 set (three modules, 17
exports: 1 `cfg_`, 6 `ar_`, 10 `cc_`), not just `cfg_validate`; the slice is
shared with the registry and CLI stages, which will link the same object.

`ldd build-config-on/fg-config-smoke` lists `libfg-config-core.so`,
`libstdc++`, `libgcc_s`, `libc`, `libm` — no llama.cpp, no ggml, no CUDA.

## 10. SHA-256

Inputs and stage files:

```
bf61ad2fa42c863380d078df24cf5217d42433c39227f941c89b4f9c0066d054  goose/cli/config-validate.goose
01dae1c99e143cf2fd3f97918246e9d4659682f624dc0d76ecd4092eba218a91  goose/cli/config-resolve.goose
b39540dc868f73b32e4840c3221593646e933e0c156d062576c6f4f2bf2130bb  goose/model/arch-registry.goose
29a2e424d26358018904b86f94c3e4090e6401cb548be5a0a2fe98a41d4128c8  goose-gen/config-slice.goose
3c177364c2d89e02cb91ddca938e7b0883d664dc1e2ee49e21331bfba4d5ab13  goose-gen/config-slice.c
a7641d318cb3eb5e275917edaaa1c77ac775a5dfc86b05194753ca27094a5c86  goose-gen/config-slice.h
aa16f8eb79eb60bbb0c70c02e03df157f707c23799e344d6b7da1ac0253f1a7d  runtime/core/src/config.cpp
f6a05f3f2ea84d95dce33c19956320c4ea6df338b0df88771f5cda06b782a83d  runtime/core/include/bmoe/config.h
33012c9752d6d4e422ce3e67f14f8db4ef2e6538ad0a40a3b4ba7b6bf5dd0e9b  common/fg-config-native.h
148bd931c2a546d225e0dae1a682db82a7fddd4d08f8a65bd9541e249e9ee1c6  common/fg-goose-config-bridge.h
84a94e20083ff90825e5fef37b1c665be1120f33af05635c51213c21815e148f  common/fg-goose-config-bridge.cpp
b89a3705c08e9018d9182bda8270ced381c70b2177c065953f9fc8cb2e1c8464  tests/fg_config_smoke.cpp
093cfc1b128c19f84a4a8e352bfa15f298c585efc0cb2bbbbbc05a21b42e013e  tests/config_mutate.py
4a5bea8d0849ec48167bd9850ec448d46fa899da1242218d8c885084c1e1772f  CMakeLists.txt
b1f0fbcd8eda0dd5376f0d54c5d744c57b39b36250d9b55800b0d745b7165314  scripts/config-gate.sh
e5674474ea38b9aff43cfa4bcaaaf3cd554461e4ea13853a6853a6b2b652bb9f  evidence/mutations.txt
```

Artefacts:

```
c29d3d841e7f88643baae99e28d87deb56cd0d5a2f6571eed2a93e7b35af057f  build-config-off/libfg-config-core.so
31bb7528e41595259ce84559c0101a7838864a4a5090c3189842e0c4824b527d  build-config-on/libfg-config-core.so
313b04e78824b42825fc9de5ac751e40e561664e87d7639fb74cd40fecd79671  build-config-off/fg-config-smoke
30cef87d53f2dcae54418d96f02727399733c605b9fbacc34e4e582b885d8450  build-config-on/fg-config-smoke
cbc6d18ce10338200e4efe48c90f854382d9beb9716c281724799c52b261582b  build-config-on/CMakeFiles/fg-goose-config.dir/goose-gen/config-slice.c.o
```

`goose/cli/config-validate.goose` hashes to `bf61ad2f…`, so this stage builds
the file accepted by EXP-620 and not a mutation of it. Regeneration is
deterministic: after an accidental in-place overwrite of the generated C during
the ad-hoc control setup (a hardlink copy truncated the shared inode), the same
command reproduced `29a2e424…` / `3c177364…` byte for byte. The generated C
embeds the unit's path in `gs_file0`, so the hash is only comparable when the
source path is the same; the relative path used here is `goose-gen/`.

This stage wrote nothing into the repository: every file above lives under
`/tmp/opencode/sub-632/tree`. At the time of writing `git status` in the
repository shows one tracked modification, `docs/goose-migration/
RUNTIME-INTEGRATION.md`, made by another concurrent session (mtime 03:20, a note
on composer determinism); it is unrelated to this stage and was not touched
here.

## 11. Raw evidence

```
evidence/compose.log              3 modules, in composition order
evidence/check.log                typecheck of the composed unit
evidence/standalone.log           the generated C, byte count
evidence/config-{off,on}-configure.log
evidence/config-{off,on}-build.log    whole staged tree, including llama.cpp
evidence/smoke-off.txt            FG632_SMOKE mode=native cases=66 pins=6 fuzz=40000 failures=0 rules=48/48 goose_calls=0
evidence/smoke-on.txt             FG632_SMOKE mode=goose  cases=66 pins=6 fuzz=40000 failures=0 rules=48/48 goose_calls=40072
evidence/opt-level.txt            -O3 and -O1; -O1 is last on the command line and wins
evidence/ldd-smoke-on.txt         no llama.cpp / ggml / CUDA on the link line
evidence/nm-counts.txt            cfg_validate 1/0, validate_native 1/1
evidence/nm-on.txt, nm-off.txt    17 exports on, 0 on off
evidence/mutations.txt            12 mutants, verdicts, per-mutant catch line, restore PASS
evidence/negctl-a-module.log      ad-hoc control A: 12 failures
evidence/negctl-b-text.log        ad-hoc control B: 3 failures
evidence/sha256.txt               the list of section 10
```

`build-mut/` is the mutation build directory (`FG_GOOSE_CONFIG=ON`,
`FG_GATE_OPT=-O0`) with its pristine snapshot under `build-mut/pristine/`; it is
an artefact, like the other `build-*` directories.

## 12. What is still open

1. **`atoi` overflow is outside this slice.** `validate()` consumes `int`
   fields; the overflowing `atoi` happens in `runtime/cli/main.cpp` before the
   struct exists. Both arms of this gate see the same post-parse integer, so no
   corpus here can detect a parse difference. `cc_atoi` / `cc_env_int` are
   composed into the same unit and are ready for the CLI stage, which needs
   `main.cpp` (not buildable without the engine) and its own oracle.
2. **The texts live twice.** The native body keeps its messages and the ON
   switch repeats them, because that is what the integration step prescribes
   (the module returns numbers, presentation stays in the carrier). Text parity
   is gated for every reachable rule, but a future text edit must touch both
   places; a compiler cannot see the duplication.
3. **Ten messages embed values.** They are compared for the values the corpus
   produces; arbitrary values agree because both format with `std::to_string`
   on the same `int`. A change of formatting (`%d`, locale, thousands
   separators) on one side would not be caught for values outside the corpus.
4. **The flat layout is hand-maintained.** 32 + 5 indices with no generator and
   no toolchain check. A new `RunConfig` field can desynchronise the bridge,
   the module and the switch without any build error — the failure would appear
   only where the gate corpus reaches the new field.
5. **`-1` (short layout) is unreachable from the carrier.** The bridge always
   passes full slices, so the module's caller-error path is only exercised by
   the module's own gate, not from here.
6. **No CLI, no `main.cpp`, no env resolution.** `cc_` is present in the unit
   (17 exports) but unwired; the staged build has no host that would call it.
7. **No model-level check.** `validate()` runs once per process start and this
   stage claims nothing about throughput or model output. S2 acceptance will
   need the GPU runtime and Igor's explicit approval.
8. **Gate cost.** 40,072 configs through two `validate()` calls each: 0.01 s
   on this host. Nothing here is on a measured path.
9. **Two staged experiments share one project.** `FG_GOOSE_JSON_SCHEMA` (S1.1)
   and `FG_GOOSE_CONFIG` (S2.1) are independent; this stage built with the
   first OFF in both modes. Their combination is untested and unnecessary
   today, but a future single staged build will carry both.
10. **Mutants are single-site and textual.** A compound regression (two
   coordinated changes that cancel out) or a mutation of the generated C is not
   covered. The two mutations that detach from a case name (B5, X1) are found by
   the fuzz only, so their detection depends on the pool containing the
   boundary value; the pool is maintained by hand next to the module constants.
11. **Mutation cost is not free of the same trap that was already hit.** The
    harness rebuilds, so a restore that does not change the mtime can leave a
    previous mutant object in place; this is fixed in `restore_sources()`, but
    any future tool that edits sources in place inherits the obligation.
