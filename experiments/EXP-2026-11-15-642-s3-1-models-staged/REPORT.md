# Staged S3.1 — the model-management decisions inside the staged runtime build

Date: 2026-11-15
Repository base commit: `8c33cde8`
Work directory: `/tmp/opencode/sub-643/tree` (the repository was not modified, nothing was committed)
Stage: `docs/goose-migration/RUNTIME-INTEGRATION.md`, S3 item 1
Modules: P8-06, `goose/server/{model-manager,hot-reload,connectors}.goose`
Models: EXP-2026-11-15-626 (staged S1.1, one Goose module behind a flag),
EXP-2026-11-15-630 (staged S2.1), EXP-2026-11-15-640 (staged S2.2, the same shape),
gate of EXP-2026-11-15-631 (P8-06, the three modules and their transcription)

## 1. Goal and result

Goal: switch the model-manager's decision layer from the native code to the accepted Goose
modules `mg_*`/`hr_*`/`cn_*` behind a build flag, without touching production, and prove the
switch with a model-free differential gate on synthetic manager state.

Result: done for six decisions and seven exports — the status predicates, the unload table,
the LRU victim walk, the preset normalisation, the reload's unload set, and the connector
target (binary source plus dial address).

| check | result |
|---|---|
| composition (three modules, one unit) + typecheck | 252 top-level declarations, 206 specialisations |
| `goose --standalone` | one C unit, 371,420 B |
| staged build, `-DFG_GOOSE_MODELS=OFF` (whole tree) | built, 277 compile steps, 0 warnings |
| staged build, `-DFG_GOOSE_MODELS=ON` (whole tree) | built, 280 compile steps, 0 warnings |
| both trees from empty build directories | 1 min 57 s wall, 20 min 36 s CPU |
| smoke gate, OFF | 84 cases, 198 values, `failures=0` |
| smoke gate, ON | 84 cases, 198 values, `failures=0` |
| divergence between the two paths | 0 |
| `goose_calls` | 84 on ON, 0 on OFF |
| dispatched-result transcripts OFF vs ON | identical, 288 lines, SHA-256 equal |
| switched symbols (`mg_pred`, `mg_unload_action`, `mg_victim`, `hr_preset_norm`, `hr_unload_set`, `cn_find_binary`, `cn_upstream_host`) | 1 definition each in the ON library, 0 in the OFF library |
| native bodies (`*_native`) | 1 definition each in both libraries |
| negative controls (3 single-site mutations) | 3/3 detected, baseline green again after restore |
| model, server, child process, GPU, HTTP, SSE | not used at all |

Per decision, the gate compares the dispatched verdict against the native transcription in
the same process: pred 14 cases (4 fields each), unload 14 (3 fields), victim 14 (index and
name), preset 12, unload_set 13 (the emitted rows, their reasons and the count), connector 17
(binary and host). Section 7 lists what each one asserts.

## 2. Environment

```
gcc / g++   15.2.0 (Ubuntu 15.2.0-16ubuntu1)
cmake       4.2.3
goose       runtime 1e714e6146bc, toolchain goose-2026.3-string
kernel      7.0.0-43-generic
```

No GPU run, no `llama-server`, no model file, no child process, no commit. The tree starts as
the staged S1.1 copy (`rsync -a` from `/tmp/opencode/sub-629/tree`), so the S1.1 JSON-schema
targets are present and built with their own flag OFF, next to the S3.1 targets.

## 3. What was copied and what was added

| source (repository) | destination (staged) | role |
|---|---|---|
| `goose/server/model-manager.goose` | `goose/server/` | the module under test (13 exports) |
| `goose/server/hot-reload.goose` | `goose/server/` | the module under test (10 exports) |
| `goose/server/connectors.goose` | `goose/server/` | the module under test (10 exports) |
| `tools/compose-goose.py` | `scripts/` | composition, unchanged |
| `upstream/llama.cpp/tools/server/server-models.{cpp,h}` | `reference/` | provenance of the transcription, not compiled |
| `work/llama.cpp-exp081-kv/tools/server/server-connect.cpp` | `reference/` | provenance of the connectors transcription, not compiled |

`server-connect.cpp` is absent from `upstream/llama.cpp` at this commit and
`runtime/core/src/engine/model_router.cpp` does not exist; the model manager in this tree has
no engine-side carrier of its own, exactly as the P8-06 report found ("the router *is* the
manager"). The stage therefore builds its own carrier, as S2.2 did.

New files of this stage:

| file | purpose |
|---|---|
| `common/fg-model-decide.h` | the carrier's types (pred / unload / row / model / unload_set / connector) and the two-armed entry points |
| `common/fg-model-decide.cpp` | the native transcriptions (always compiled) and the dispatch |
| `common/fg-goose-model-bridge.h/.cpp` | containers -> name tables and slices, one export per decision, witness counter |
| `tests/fg_model_smoke.cpp` | the gate: six decisions against the native transcriptions |
| `scripts/models-gate.sh` | the exact command sequence of section 4, writes `evidence/` |
| `patches/s31-staged-cmake.diff` | the staged `CMakeLists.txt` delta (60 lines, no llama.cpp file touched) |
| `evidence/` | logs, smoke output, dumps, `nm`, SHA-256, sizes, negative controls |

Deliverable: sources, script, patch, report and `evidence/`. `goose-gen/models.goose`,
`goose-gen/models.c`, `goose-gen/models.h`, every `build-model-*` directory and every binary
in them are reproducible from the three module files plus `scripts/models-gate.sh` and must
not be committed.

## 4. Exact commands

```bash
cd /tmp/opencode/sub-643/tree

# 1. Composition (three modules, one unit), typecheck, one standalone C unit.
python3 scripts/compose-goose.py goose-gen/models.goose \
    goose/server/model-manager.goose \
    goose/server/hot-reload.goose \
    goose/server/connectors.goose
# -> composed 3 module(s) -> goose-gen/models.goose
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose check goose-gen/models.goose
# -> parsed ok: 252 top-level declarations
#    typechecked ok: 206 specialization(s), 2 struct/0 enum instance(s)
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose --standalone \
    -o goose-gen/models.c --header goose-gen/models.h goose-gen/models.goose
# -> wrote goose-gen/models.c (371420 bytes)

# 2. Both builds of the whole staged tree; the flag selects the decision path.
cmake -S . -B build-model-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_MODELS=OFF
cmake --build build-model-off -j 32        # -> 277 compile steps, no warnings
cmake -S . -B build-model-on  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_MODELS=ON
cmake --build build-model-on  -j 32        # -> 280 compile steps, no warnings

# 3. Gate and transcript parity.
./build-model-off/fg-model-smoke
./build-model-on/fg-model-smoke
./build-model-off/fg-model-smoke --dump > evidence/dump-off.txt
./build-model-on/fg-model-smoke  --dump > evidence/dump-on.txt
diff <(head -n -1 evidence/dump-off.txt) <(head -n -1 evidence/dump-on.txt)

# 4. Symbols.
nm -D --defined-only build-model-on/libfg-model-core.so  | grep ' T mg_'
nm -D --defined-only build-model-off/libfg-model-core.so | grep ' T mg_'   # empty
```

`scripts/models-gate.sh` runs the same sequence in one shot (1 min 57 s from empty build
directories, both modes, whole tree) and writes every file in `evidence/`.

## 5. How the flag is wired

The flag adds one option, one library, one object and one executable to the staged project.
No llama.cpp file is edited (section 3: `diff -rq` against the S1.1 tree is empty) and no
existing target changes behaviour.

```cmake
option(FG_GOOSE_MODELS
       "Select the Goose model-management decisions (mg_/hr_/cn_) instead of the native ones"
       OFF)

add_library(fg-model-core SHARED common/fg-model-decide.cpp)
target_compile_options(fg-model-core PRIVATE ${FG_GATE_OPT})

if(FG_GOOSE_MODELS)
    add_library(fg-goose-models OBJECT goose-gen/models.c)
    target_compile_options(fg-goose-models PRIVATE ${FG_GATE_OPT} -w)
    target_sources(fg-model-core PRIVATE common/fg-goose-model-bridge.cpp)
    target_compile_definitions(fg-model-core PRIVATE FG_GOOSE_MODELS=1)
    target_link_libraries(fg-model-core PRIVATE fg-goose-models)
endif()
```

The dispatch is one `#ifdef` per decision, in one place (`common/fg-model-decide.cpp`):

```cpp
fg_model::unload_t unload_action(int mapped, int status) {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::unload_action(mapped, status);
#else
    return unload_action_native(mapped, status);
#endif
}
```

Properties that matter for the production rule ("a new feature's flag exists only to
disable it"):

- OFF is the default; the OFF path is the native transcription, which is compiled in both
  configurations (`nm` counts `*_native` 1/1, `evidence/nm-native.txt`);
- nothing native is deleted and no run-time branch exists — the choice is made once, per
  translation unit, by the compiler;
- the Goose object is a separate translation unit with `-w`, compiled at the same `-O1` as
  the gate (the Release `-O3` comes earlier on the command line and loses, as in S2.1/S2.2);
- the library links `libstdc++`, `libgcc_s`, `libc`, `libm` only — no llama.cpp, no ggml, no
  CUDA, no server (`evidence/ldd-smoke-on.txt`).

## 6. The main seam

The native manager holds its state under a mutex, wakes on condition variables and owns the
child processes; the module owns the verdicts as pure integer and string functions. The seam
between them is this stage's real content, and it has five parts:

1. **Every decision takes a flattened snapshot.** `mg_victim` takes a name table plus six
   int64 arrays; `hr_unload_set` takes two name tables, two status/offset/length triples and
   two option texts. The bridge builds all of it per call from `std::vector` rows, so a call
   is a copy, not a pointer into the mapping. Correct only if the caller's snapshot is
   consistent — which is exactly what the native mutex gives for free and the carrier does
   not. On a request path this needs a pooled buffer and a snapshot taken under the same lock.
2. **The option map is serialised by the host, and its order had to be canonicalised.**
   Natively a preset is a `std::map<option>`, so `preset_options_for_compare` always sees
   keys in byte order. The module walks the entries in the order it is given. The first gate
   run found this: `LLAMA_ARG_NGL=99\nLLAMA_ARG_CTX=4096` normalised to itself on the Goose
   path and to `CTX`,`NGL` natively. `canon_options()` in the bridge sorts the entries before
   the call, which is a flattening, not a policy (keys and values pass through untouched), and
   it restores native order-insensitivity: the corpus now contains both orders and the
   verdicts agree. A caller that bypasses the bridge can still reintroduce the difference.
3. **The unload table models the monitor thread.** Native `unload()` on a LOADED or SLEEPING
   model does not write the exit command; it inserts the name into `stopping_models`,
   notifies `cv_stop`, and a per-instance `stopping_thread` writes
   `CMD_ROUTER_TO_CHILD_EXIT` to the child's stdin (`server-models.cpp:1085-1105`). The
   module's `send_exit` is that verdict. The command, the wait and the join stay native; the
   gate pins the verdict, not the write.
4. **Row indices travel instead of names.** `mg_victim` and `hr_unload_set` return indices into
   the row order the caller passed, and the gate checks both the index and the resolved name,
   so an off-by-one in the bridge cannot hide behind an agreeing transcription.
5. **The transcriptions are the oracle, so their provenance is pinned.** `server-models.cpp`
   cannot be compiled here (it needs the server, `mtmd`, the preset parser and libcurl), so
   the six decision bodies are transcribed into `*_native` with the lines they copy cited in
   `fg-model-decide.h`, and `reference/` carries the two native files unchanged.

The connector decision is the one place where two native functions with two different return
conventions (`find_binary`'s empty string, `upstream_host`'s string) become one carrier value:
`connector_t { binary, host }`, where `binary` mirrors the module's `-1 / 0 / 1+k` and `host`
is the dial address. The filesystem walk behind `find_binary` is reduced to two flags —
"sibling exists" and "the k-th PATH entry exists" — because the carrier must not touch the
filesystem.

## 7. Gate: what is compared

Two builds, one binary each, one corpus. In both builds the oracle is the `*_native` body in
the same library.

| | OFF | ON |
|---|---|---|
| `fg_model::*` body | `*_native` | bridge -> `mg_`/`hr_`/`cn_` -> struct |
| `FG643_MODEL` | `mode=native cases=84 values=198 failures=0 goose_calls=0` | `mode=goose cases=84 values=198 failures=0 goose_calls=84` |
| dispatched-result transcripts (288 lines) | `56ff2c81…` | `56ff2c81…` (identical) |

Per decision:

| section | cases | values | what it asserts |
|---|---|---|---|
| pred | 14 | 56 | `is_ready`, `is_running`, `is_ready_or_sleep`, `is_failed` over the seven statuses, a non-zero exit code on an unloaded row, a status that does not exist (9), and a loaded row carrying a stale exit code |
| unload | 14 | 42 | action, `send_exit` and `terminate` for unmapped and mapped rows over the seven statuses plus an unknown one; the LOADING force-kill and the SLEEPING graceful stop are separate cases |
| victim | 14 | 28 | the victim index and its resolved name: empty mapping, one row, two rows given newest-first, a tie on `last_used`, busy / stopping / queued / not-ready rows, negative and zero stamps, sleeping rows, a mapping given out of name order, a wide eight-row mapping |
| preset | 12 | 12 | the normalised text: empty, each stripped key alone, all four stripped, kept keys in order, kept keys out of order, a value containing `=`, a long model path |
| unload_set | 13 | 26 | the emitted rows and their reasons plus the count: empty mapping, no change, everything removed, only HOST/PORT changed (must not unload), only ALIAS/LOG_COLORS changed (must not unload), a real option change, a new model, non-running rows, name-order emission, a mixed case, LOADING and SLEEPING rows |
| connector | 17 | 34 | the binary verdict and the dial host: sibling present, PATH hit at 0/1/2, no hit, no directories, a unix socket only, a socket before a wildcard, an IPv6 literal, a bracketed literal, an empty host list, a plain v4, a host name, and a PATH miss with a socket bind |

Coverage notes, stated plainly:

- In the OFF build the per-decision comparison is a structural identity (the dispatch
  forwards to the body being compared against); the number that carries the claim is the ON
  column, where every case crosses the slice boundary.
- The corpus is synthetic manager state; no clock, no mutex, no child. Re-running reproduces
  every hash.
- Names are unique in every victim and reload case (section 12, item 2).

## 8. Negative controls

Three single-site mutations, each applied in the scratch build `build-model-negctl`
(`FG_GOOSE_MODELS=ON`, `-O0`), gate run, source restored, generated C regenerated and compared
byte for byte (`cmp`) with the pristine copy, then the baseline re-gated:

| control | site | mutation | reaction |
|---|---|---|---|
| A | module | `mg_is_ready`: LOADING counted as ready | 2 failures, first `[FAIL] pred_loading/ready: want 0 got 1` |
| B | module | `hr_preset_norm` no longer strips `LLAMA_ARG_PORT` | 4 failures, first `[FAIL] preset_port_only/text: want "" got "LLAMA_ARG_PORT=8080"`, and `set_host_port_only` turns into a spurious unload `want "0:" got "1:0r2"` |
| C | bridge | `canon_options` sorts descending | 4 failures, first `[FAIL] preset_keep_two/text: want "LLAMA_ARG_MODEL=…|LLAMA_ARG_NGL=99" got "LLAMA_ARG_NGL=99|LLAMA_ARG_MODEL=…"` |

```
after restore: generated C sha256 2cf4dc17… MATCHES pristine
baseline: FG643_MODEL mode=goose cases=84 values=198 failures=0 goose_calls=84
```

Control B is the one worth keeping: it shows the gate sees not only a wrong string but the
manager doing the wrong thing with it — unloading a healthy model because an injected option
was compared.

Logs: `evidence/negctl-a-pred.log`, `-b-port.log`, `-c-bridge.log`.

## 9. Sizes

| artefact | bytes |
|---|---|
| `goose/server/model-manager.goose` + `hot-reload.goose` + `connectors.goose` (input) | 53,918 |
| composed unit `goose-gen/models.goose` | 54,070 |
| `goose-gen/models.c` (generated) | 371,420 |
| `goose-gen/models.h` (generated) | 6,142 |
| Goose object `models.c.o` | 53,248 |
| `common/fg-model-decide.cpp` (native + dispatch) | 7,748 |
| `common/fg-goose-model-bridge.cpp` | 7,167 |
| `tests/fg_model_smoke.cpp` | 15,987 |
| `libfg-model-core.so` OFF | 42,976 |
| `libfg-model-core.so` ON | 123,424 (+80,448, +187 %) |
| `fg-model-smoke` OFF / ON | 63,536 / 63,536 (only the mode string differs) |

The ON growth is the whole composed unit — all 33 exports, including the 26 that are composed
but unwired.

## 10. SHA-256

Inputs and stage files:

```
4a3eb02d2a30ab564f6998d9a27e5762a454894c0895d0418ba42262ca6915ff  goose/server/model-manager.goose
5b9a6d710679a4fedc3ead6355d05005596b8f9c36989251d550e675d6f8b57c  goose/server/hot-reload.goose
e2e151e3ee62782dedf5611bd1fa1fba348ccedad0791f44c6658c6bd4cb5f4b  goose/server/connectors.goose
7c14f2495653bee93ef7cb13451262f7105c076e138675f90d3c65ac4478e7d7  goose-gen/models.goose
2cf4dc172f3eb5bc2d1492de6ccddbbcd745459f522a2dd64c51e66ff8fab33d  goose-gen/models.c
0fab22a338d52ab9cb8d69e2c1a1096635a0bd4e084440b125c0c5a37b45b88a  goose-gen/models.h
950facc393e14a2daaab522236939b4419e4ff0e17a1f25756f480217cef0ea7  common/fg-model-decide.h
1ec0322e97580bfe8d17483399833ea6259568d1282d1446cc324e41782fc3b1  common/fg-model-decide.cpp
7c93230713d5eb2c4030b17bf6c6f3c8ab9e946f794cd8c76e0d097b7400aec3  common/fg-goose-model-bridge.h
d2493cccc3c2b52ba42d724052c35915758ff01843e27c94ef408f7b16a8aacc  common/fg-goose-model-bridge.cpp
056df2dc1b31ba751f91110d485a088d81561f39cafb99f749e3244de4bf1e7a  tests/fg_model_smoke.cpp
968b23378a08099dfb6a7c9a06711e3d865befb3454fd87198e509cf348ae92d  scripts/models-gate.sh
7471f43d4fb6eecbecf5c25e2d83c542b896ea1d49efcb85847db285603e15bc  CMakeLists.txt
```

Artefacts:

```
b1f19378fcf166971fd12be1ce7f8964df7adffc2a659dbe7dd4f7af2c1887a7  build-model-off/libfg-model-core.so
b702aa371e0c09b02fafafd2f2b0f5ec93bd9ff361676f2a1b3f0b0359942b83  build-model-on/libfg-model-core.so
8c6267fcad503d8243b0b6de0a42ec55df8dfc378f1969c2b2f481479cac13e6  build-model-off/fg-model-smoke
4590a6462d9bc724586573d2f53ff6c5a9a7a839942cb00d4cf432ce99df7eef  build-model-on/fg-model-smoke
4883eef265e9c12da62f78c7fcbc7c1f3b26030e11d723c853a1c812fe3b91ae  build-model-on/CMakeFiles/fg-goose-models.dir/goose-gen/models.c.o
56ff2c812136d561384008e24f2fb257b5eff99e4aa2ecaa744b732f6ee9d087  evidence/dump-off.body
56ff2c812136d561384008e24f2fb257b5eff99e4aa2ecaa744b732f6ee9d087  evidence/dump-on.body
```

The three module hashes are the values EXP-2026-11-15-631 recorded for the accepted sources
(`4a3eb02d…`, `5b9a6d71…`, `e2e151e3…`), so this stage builds the accepted modules and not a
mutation of them. Regeneration is deterministic: after three in-place mutation rounds the same
command reproduced `2cf4dc17…` byte for byte.

This stage wrote nothing into the repository: every file above lives under
`/tmp/opencode/sub-643/tree`. The repository moved past the assigned base commit while this
stage ran (`8c33cde8` -> `9cd9bad3`, other sessions' commits) and shows one tracked
modification from those sessions (`tools/build-main-runtime.sh`); none of it is from here.

## 11. Raw evidence

```
evidence/model-generate.log        composition, typecheck, the generated C byte count
evidence/model-{off,on}-configure.log
evidence/model-{off,on}-build.log  whole staged tree, 277 / 280 compile steps, no warnings
evidence/model-smoke-off.txt       FG643_MODEL mode=native cases=84 values=198 failures=0 goose_calls=0
evidence/model-smoke-on.txt        FG643_MODEL mode=goose  cases=84 values=198 failures=0 goose_calls=84
evidence/model-dump-{off,on}.txt   every dispatched result; the bodies are identical
evidence/model-dump-{off,on}.body  the same without the summary line, for the diff
evidence/model-nm-switched.txt     the 7 switched exports: ON=1, OFF=0 each
evidence/model-nm-native.txt       the 6 native bodies: 1 definition each in both libraries
evidence/model-nm-{on,off}.txt     the full defined-symbol dumps
evidence/model-ldd-smoke-on.txt    no llama.cpp / ggml / CUDA / server on the link line
evidence/negctl-*.log              the three negative controls, 2 / 4 / 4 failures
evidence/model-sizes.txt, model-sha-sources.txt, model-sha-artifacts.txt
```

Every file in `evidence/` produced by this stage carries the `model-` or `negctl-` prefix.
The remaining entries (`base-*`, `dump-base.txt`, `nm-c.txt`, `nm-export-counts.txt`,
`sha-base.txt`, `final-sha.txt`, `script-run.log`) are inherited from the S1.1 stage in this
tree and belong to the JSON-schema gate, not to this slice.

## 12. What is still open

1. **26 of the 33 exports are composed but unwired.** Switched: `mg_pred`,
   `mg_unload_action`, `mg_victim`, `hr_preset_norm`, `hr_unload_set`, `cn_find_binary`,
   `cn_upstream_host`. Still native: `mg_status_str`, `mg_source_str`, `mg_remove_gate`,
   `mg_stop_gate`, `mg_wait_pred`, `mg_update_fields`, `mg_arch_take`, `mg_capacity`,
   `mg_tick`, `mg_lru_log`, the hot-reload alias/tags/startup/hidden family, and the
   connectors share-code/HF-repo/sidecar family. `mg_tick` is the notable one: it mutates the
   `stopping` array as it emits victims, so it needs the carrier to own a writable snapshot.
2. **Duplicate names are outside the contract.** The native mapping is
   `std::map<std::string, instance_t>`, so it cannot hold two rows with one name; the module
   walks rows. The first gate corpus contained `{"a", last=20}` and `{"a", last=10}` and the
   two paths disagreed (native 0, Goose 1, the lower stamp). The case was removed and the
   assumption is recorded here rather than papered over: rows must have unique names.
3. **Option text is a host serialisation.** `model_t.options` is `KEY=VALUE` lines; the
   native preset is a `common_preset` object. The bridge canonicalises order (section 6,
   part 2) but does not model option *types* (an `int` option and the string `"99"` normalise
   the same here, as they did in P8-06) or duplicate keys.
4. **No locking, no waiting, no process.** The mutex, the condition variables, the monitor
   thread, the three waits (`mg_wait_pred`) and the actual exit command stay native. This
   stage switches verdicts over a snapshot; a wrong snapshot is invisible to the gate.
5. **The scheduler is not switched end to end.** `mg_capacity` and `mg_tick` stay native, so
   "evict until there is room" is still the native loop; only the victim verdict is Goose's.
6. **SSE and HTTP are untouched.** `mg_update_fields` (what the SSE data carries) and every
   handler in `server-models.cpp` stay native, so the P8-06 findings about the stale progress
   object and the unmapped-name SSE send are still live in production.
7. **The connector is not spawned.** `cn_unavailable`, `cn_resolve_code`, `cn_format_code` and
   the log-tag policy stay native; the llama-connect child is never started here, and
   `find_binary`'s filesystem walk is two test flags.
8. **Gate level.** The gate and the Goose object run at `-O1` (Release `-O3` first on the
   command line, `-O1` last, so `-O1` wins — same convention as S2.1/S2.2). The three
   controls ran at `-O0` in a scratch directory and their logs are the evidence.
9. **No model-level parity.** No server, no model, no GPU. Nothing here claims a step toward
   the S3 acceptance criterion of identical behaviour with real models loaded.
10. **Three staged slices share one project.** `FG_GOOSE_JSON_SCHEMA` (S1.1) and
    `FG_GOOSE_MODELS` (S3.1) are independent; this stage built with the first OFF in both
    modes. Their combination is untested.
