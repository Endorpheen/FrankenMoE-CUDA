# P8-12 — two promoted call sites: runtime switches, differential gate, native body removed

Build-and-gate-only increment on top of `2b443d45`. No model was loaded, no
GPU was touched, and nothing in `/home/end0/AI/FrankenMoE-CUDA` was modified or
committed. Everything below lives under `/tmp/opencode/sub-666/`, which is a copy
of `/tmp/opencode/sub-662/` minus its two build directories (their CMake cache
holds absolute paths, so both configurations were configured and built again).

- Base commit: `2b443d45`. Input tree: `/tmp/opencode/sub-662/flint-cpu`
  (the flint CPU copy that built the merged server ports), plus its
  `merged-server-context.cpp`, `server-unit/` and `audit/`.
- Compiler: `gcc-13` / `g++-13` 13.4.0, cmake 4.2.3, ninja, 32 cores.
  Goose: `goose-2026.3-string`.
- Two sites: model status (`server-models.h`) and the Origin localhost test
  (`server-http.cpp`). Both are now runtime decisions that default to the Goose
  module; the model-status native body is deleted, the Origin native body stays
  as the arm its switch selects.
- The composed unit was **not** regenerated: `server.goose` is
  `0564fa83…10fce` and `server.c` is `ee633abd…d275cad1`, both identical to
  sub-662, so the gate measures the same unit that sub-662 linked.

## 1. Results in one line each

| claim | result |
|---|---|
| both sites are runtime switches, default Goose (rule 10) | **yes**, `FG_GOOSE_SERVER_CORS_NATIVE` only disables the Origin site |
| model-status native body deleted | **yes**, the 7-case switch is gone; the OFF path answers `unknown` |
| OFF build / ON build, phase A | **yes / yes**, rc=0, 19,608,544 B / 19,692,888 B |
| OFF build / ON build, phase B | **yes / yes**, rc=0, 19,608,376 B / 19,692,728 B |
| default configure (no flags) has the improvement | **yes**, `FG_GOOSE_SERVER_STATUS=ON`, `FG_GOOSE_SERVER_CORS=ON`, 19,688,608 B, `goose_init` 1 |
| differential gate, no server | **15/15 cases per run, 0 divergences**, phase A 4 runs, phase B 5 runs (incl. default build) |
| mutations | **12/12 killed**, halves 6/6 and 6/6, control PASS, re-run after the deletion 12/12 |
| unit reached with a started runtime | **yes**, first time: sub-662 never called `goose_init` (§4) |
| repository tracked files changed | **0** |
| server started | **no** (one accidental bare invocation, §10) |

## 2. What was switched, and how

Both sites keep their decision **outside** the call site, in one header that the
server and the gate include, so the gate measures the code the server runs and
not a copy of it:

```
tools/server/fg-goose-server/fg-goose-sites.h      the two decision functions
tools/server/fg-goose-server/fg-goose-switches.h   runtime switches and env names
tools/server/fg-goose-server/fg-goose-switches.cpp one cached getenv per site
```

The switch semantics follow rule 10: unset, empty, `0`, `false`, `no`, `off`
(case-insensitive) keep the Goose path; any other value selects the native body.
The value is read once per process (`static const`, function-local), so a request
never pays for `getenv` and a decision cannot change under a live server. No CLI
knob was added: the server's options are a single JSON schema, and a new key for a
diagnostic that only a build or a bisect needs is not worth a schema change.

Two things are worth stating exactly:

1. **The status site has no switch left.** `FG_GOOSE_SERVER_STATUS_NATIVE` existed
   in phase A, gate-green, and disappeared together with the body it could
   restore: with the 7-case switch deleted there is nothing for a flag to select.
   This is step 3 of sub-662's removal order (`§6`), done early for this one site
   because step 2 removed its body in the same increment.
2. **The Origin site keeps a working native arm.** Its switch selects a full
   native equivalent (`host == "localhost" || "127.0.0.1" || "::1"` plus the
   exception arm), so disabling the module there is lossless.

CMake defaults moved with the decision: `FG_GOOSE_SERVER_STATUS` and
`FG_GOOSE_SERVER_CORS` are `ON` by default, `TOOLS`, `MIME` and `CACHE` stay `OFF`
until their own runtime gate exists. `FG_GOOSE_SERVER_STATUS=OFF` still builds,
but it is a **unit-isolation build**, not a product one: the native vocabulary is
gone and the site answers `unknown`. The comment above the options says so.

The old preprocessor switches at both sites (`#if FG_GOOSE_SERVER_STATUS`,
`#if FG_GOOSE_SERVER_CORS`) are gone; the runtime switch is the only selector and
the compile-time option now only decides whether the unit is linked at all.

An integer contract replaces the enum on the way into the unit: `server-models.h`
carries seven `static_assert`s that `SERVER_MODEL_STATUS_*` are still `0..6`, so a
reordered or extended enum fails the build instead of renaming a status at run
time.

## 3. The composed unit was never started (found and fixed)

The gate's first run failed with

```
goose runtime error: goose_init must be called before exported functions
```

Every unit export panics until `goose_init` ran, and nothing in the sub-662 tree
called it: `nm` shows the definition and the merge never added a caller. The
sub-662 ON build therefore could not have answered a single switched decision —
it was never exercised, which is why the defect survived.

Fix, minimal and in the bridge:

- `fgs_init_early(argc, argv)` — exported, idempotent (`std::call_once`), called
  at the top of `llama_server()` in `tools/server/server.cpp` before anything else
  runs there, i.e. on the thread the runtime owns and with the real process
  arguments;
- every wrapper calls the same `goose_start(0, nullptr)` first, so a consumer with
  no startup hook (`llama-cli` links the merged queue too) still works.

The gate covers both paths: one run calls `fgs_init_early` like the server, one
run sets `FG_GATE_NO_EARLY=1` and measures the lazy fallback. Both are green.

## 4. Commands

```
# baseline reproduction of sub-662 (step 1 of this task)
cd /tmp/opencode/sub-666
./build.sh off
./build.sh on -DFG_GOOSE_SERVER_STATUS=ON -DFG_GOOSE_SERVER_TOOLS=ON \
              -DFG_GOOSE_SERVER_MIME=ON -DFG_GOOSE_SERVER_CACHE=ON \
              -DFG_GOOSE_SERVER_CORS=ON
# → 19,607,808 B and 19,687,864 B: the exact sizes sub-662 recorded

# phase A: runtime switch, both native bodies present
cd flint-cpu
cmake -S . -B build-off <same options> -DFG_GOOSE_SERVER_*=OFF
cmake --build build-off -j 24
cmake -S . -B build-on  <same options> -DFG_GOOSE_SERVER_*=ON
cmake --build build-on -j 24
../gate/run.sh A            # 4 gate runs
python3 ../gate/mutate.py   # 12 mutations, control first

# phase B: status body deleted
cmake --build build-off -j 24 && cmake --build build-on -j 24
cmake -S . -B build-default ... # no FG_GOOSE_* flag at all
cmake --build build-default -j 24
../gate/run.sh B            # 5 gate runs, incl. the default build
python3 ../gate/mutate.py   # same 12 mutations, re-run

./check.sh A | B            # symbol table of both binaries
```

`build.sh` holds the configure line (Release, `GGML_CUDA=OFF`,
`BUILD_SHARED_LIBS=OFF`, `LLAMA_SUBPROCESS=ON`, `LLAMA_OPENSSL=OFF`,
`LLAMA_CURL=OFF`, `LLAMA_USE_PREBUILT_UI=OFF`). The 70 embedded UI assets were
copied from `build-cuda/tools/ui/dist` into `flint-cpu/tools/ui/dist`, so no
network is used and both configurations embed the same table.

## 5. OFF / ON evidence

`audit/symbols-A.tsv`, `audit/symbols-B.tsv`:

| phase | config | bytes | goose_init | unit exports | fgs_* defs | status calls | cors calls |
|---|---|---|---|---|---|---|---|
| A | OFF | 19,608,544 | 0 | 0 | 6 | 2 | 2 |
| A | ON | 19,692,888 | 1 | 6 | 6 | 2 | 2 |
| B | OFF | 19,608,376 | 0 | 0 | 6 | 4 | 2 |
| B | ON | 19,692,728 | 1 | 6 | 6 | 4 | 2 |

Compared with sub-662, the OFF row changed on purpose: the old compile-time `#if`
removed the Goose block entirely (`fgs_*` refs 0), while a runtime switch keeps
the bridge reachable in both builds and lets the OFF build take the native arm
through the constant `-1`. The unit is still absent from OFF: `goose_init` 0, no
`mg_status_str`, no `cors_is_localhost` definition.

`nm` on the promoted switches: `fg_goose_cors_native` and `fgs_init_early` are
defined in both builds; `fg_goose_status_native` does not exist in either, because
the phase-B tree no longer has it.

The default configure (no `FG_GOOSE_*` argument at all) yields
`FG_GOOSE_SERVER_STATUS=ON`, `FG_GOOSE_SERVER_CORS=ON`, `TOOLS/MIME/CACHE=OFF`,
builds clean, is 19,688,608 B and carries `goose_init` once — the improvement is
what a plain `cmake -B build` gets.

## 6. The differential gate

`gate/gate.cpp`, no server, no model:

- links the shipped decision functions (the same header the server includes), the
  shipped bridge and switches objects from the build directories, and the shipped
  unit object — nothing is re-compiled from server sources;
- compares every answer with a **frozen copy** of the native bodies at
  `2b443d45`: `server_model_status_to_string` (`server-models.h:55`) and
  `origin_is_localhost` (`server-http.cpp:77`), including the parser call and its
  exception arm;
- 9 status cases: ids `0..6` plus `-1` and `7`, which must answer `unknown`;
- 6 Origin cases — **3 localhost hosts and 3 refusals**:
  `http://localhost:8080`, `https://127.0.0.1`, `http://[::1]:9000`;
  `https://example.com`, `http://127.0.0.1.evil.com` (suffix trap), and
  `ftp://localhost`, which the native parser rejects, so it reaches the
  `parsed = 0` arm;
- 15 cases per run, TSV per line, exit code 1 on any divergence.

| phase | run | cases | divergences |
|---|---|---|---|
| A | ON, Goose arm, explicit `goose_init` | 15 | 0 |
| A | ON, Goose arm, bridge lazy `goose_init` | 15 | 0 |
| A | ON, native arm (`FG_GOOSE_SERVER_*_NATIVE=1`) | 15 | 0 |
| A | OFF bridge (native arm is the only arm) | 15 | 0 |
| B | ON, Goose arm, explicit `goose_init` | 15 | 0 |
| B | ON, Goose arm, bridge lazy `goose_init` | 15 | 0 |
| B | ON, Origin native arm (status has none) | 15 | 0 |
| B | OFF bridge (status must answer `unknown`) | 15 | 0 |
| B | default configure, Goose arm | 15 | 0 |

Logs: `audit/gate-A.log`, `audit/gate-B.log`.

## 7. Mutations

`gate/mutate.py`, two halves of six, applied to a copy of the composed unit and
rebuilt with `goose --standalone` + `gcc-13 -O1`; composition is a concatenation,
so `mg_status_str_body` and `cors_is_localhost` there are byte copies of
`model-manager.goose:159-186` and `cors-policy.goose:96-112`. A control build with
no edit must pass first, otherwise every "kill" would be noise.

| id | half | mutation | divergences | verdict |
|---|---|---|---|---|
| C0 | control | no edit | 0 | PASS |
| A1 | status | id 0 `downloading` loses its last byte | 1 | KILLED |
| A2 | status | id 1 `incomplete` loses its last byte | 1 | KILLED |
| A3 | status | id 2 answers the prefix `download` | 1 | KILLED |
| A4 | status | id 4 becomes `Loading` | 1 | KILLED |
| A5 | status | id 5 gains a trailing space | 1 | KILLED |
| A6 | status | the out-of-range fallback gains a trailing space (only ids 7 and -1 hit it) | 2 | KILLED |
| B1 | cors | `127.0.0.1` → `127.0.0.0` | 1 | KILLED |
| B2 | cors | `::1` → `::2` | 1 | KILLED |
| B3 | cors | `localhost` → `Localhost` | 1 | KILLED |
| B4 | cors | a parse failure is accepted as local | 1 | KILLED |
| B5 | cors | every host falls through as local | 2 | KILLED |
| B6 | cors | any non-empty host is local | 2 | KILLED |

Half A 6/6, half B 6/6, total **12/12**, control PASS — both in phase A and, as a
re-run, in phase B. A6 is the one that pays for the out-of-range cases: without
ids `-1` and `7` the gate would have survived it. Raw data:
`audit/mutations-A.tsv`, `audit/mutations-B.tsv`, per-mutation gate output in
`gate/mut/<id>.gate.txt`.

## 8. Deletion of `server_model_status_to_string`

Removed: the 7-case switch, i.e. the last native copy of the status vocabulary.
What remains is the module call and one documented fallback:

```cpp
static inline std::string fg_goose_model_status_text(int64_t status) {
    char buf[32];
    const int64_t n = fgs_model_status_text(status, buf, (int64_t) sizeof(buf));
    if (n >= 0) {
        return std::string(buf, (size_t) n);
    }
    // Only a build without the unit reaches this arm; it has no vocabulary left.
    return "unknown";
}
```

Three independent proofs that the OFF path no longer carries the body:

1. **Source**: `audit/diff-tools-server.patch` shows the seven `case` lines and
   the `default` deleted from `fg-goose-sites.h` (the call-site function in
   `server-models.h` is now a one-line delegation).
2. **Behaviour**: in phase B `gate-off` is run with `gate-off unknown ref` —
   all 9 status cases must answer `unknown`, and they do. The same binary passes all 6 Origin cases against the reference,
   because that native body is still there.
3. **Code**: in `build-off/bin/llama-server` the call site is
   `call fgs_model_status_text; test %rax,%rax; js 18a660`, and `18a660` loads
   the literal at `0xb0ffb1` — the `unknown` tail of the merged string — and
   returns it (`audit/off-body.txt`). There is no case dispatch anywhere on the
   path, and the bridge in that build is compiled to the constant `-1`.

The ON path is unchanged and still answers the reference vocabulary: the ON gate
run is 15/15 with 0 divergences, and the unit object is byte-identical to
sub-662's.

## 9. SHA-256

```
build-off/bin/llama-server      02514d6115277941491dc33d104ee17eacc13bf80af0212bd05ae0fd0d4c48d3
build-on/bin/llama-server       a9f558453dc83a39cc3999a6fdfa9ceca523125ee56a8c1a9ecfea85183b8424
build-default/bin/llama-server  aff56d762cd90e110c9a7d7463f8e0f7515fa27c41cd2d7b582365f52a6c81e8
gate/gate-on                    6a7bdb7712bbb442e76bda5c9378877ff662865980b9e59e58a6bdf2aac668f0
gate/gate-off                   c938aa74e81b354734f81d4ce5cbcaefb166fc436152c35e7cc4cb0b49a24bb8
gate/gate.cpp                   47fcaf71f33f9edf1ed78e6d163fab49b1ce77f3314e66ddbc93b9200aec0eeb
gate/mutate.py                  c12da5a8778412c8e884f81bdbad2468bf36792701a78731cfdec07a66e79c8a
fg-goose-sites.h                9d3a0c407c1f773b02d6b8e7b50dd2028bb96fcda025a5585a4d04f2f39c2631
fg-goose-switches.h             7ae03d8e13f9b2a00657aab076ad12f0fc28e6ac267ad5eda1a69e3fbbe6d292
fg-goose-switches.cpp           c20781361437c1f3045326a210a1ccf363bfc8c032d11619d97ba4800c232318
fg-goose-server.cpp             5e15a02c025abccbc8928621d45d607b4c469c78b00abdc8d1a605d802d9a8a7
fg-goose-server.h               8cb98c25de73ffcc41cca334486a99c25bb7528829269fd6b6a6fd33fc34e979
server-models.h                 beba22102ea7a128258fe67f2c42b7879e6f7826da269df0a9badc4631d29e13
server-http.cpp                 8b6244d74224fd9a582dd4243579253008cb5595ff1064fae093f877c2048b34
server.cpp                      03431f3931923bb843518d76189d87cbd8ba965af9debe41be05191da7652145
tools/server/CMakeLists.txt     18dae4241d8850ea1b7d29d75356b24783af14fc6a487cc39dbfa46697f1c17a
CMakeLists.txt                  fc84de9186fb9c496ff67f9290c366ebba4f5a4670e7623b006212980f27e634
server.goose (unchanged)        0564fa8311f81e9eb6a4e635cf589a2ae9dbb1422b6f78fcc2ebaff039e10fce
server.c     (unchanged)        ee633abd756f8a23e45df7dab879513b949d431444449f3579c33fe3d275cad1
```

Plain `sha256sum` form: `audit/sha-artifacts.txt`, `audit/sha-sources.txt`.
Source patches against sub-662: `audit/diff-tools-server.patch`,
`audit/diff-root-cmake.patch` (7 files changed, 3 added, nothing else).

## 10. Boundaries, assumptions, and one disclosure

- **Not run:** no model, no GPU, no request, no benchmark. Throughput and
  end-to-end server behaviour are still unmeasured by construction.
- **Disclosure:** while collecting disassembly evidence one command line
  accidentally contained the binary itself (`build-off/bin/llama-server` with no
  arguments); it started, printed nothing to the log, and was killed by the tool
  timeout after 120 s. It loaded no model (none was given), answered no request,
  and touched no GPU. No process survived it, and the unrelated listener on port 8080
  belongs to a process this session does not own. Everything else in this report
  comes from builds and from the gate, which never starts a server.
- **Fixed defect:** the composed unit needed `goose_init`, and sub-662 never
  called it (§3). Any conclusion about sub-662's ON build being "wired" is
  therefore only true now.
- **Pre-existing warning:** `build-off` reports one `-Wunused-parameter` for
  `policy` in the static-asset lambda (`server-http.cpp`), because the
  Cache-Control site is compiled out in that configuration. It is present in the
  sub-662 OFF build as well (`audit/build-off.log:242`) and is out of scope here.
- **One new seam:** the status contract is now an integer plus seven
  `static_assert`s. A third-party build that reorders the enum fails the build
  instead of answering the wrong name.
- **Lazy `goose_init`:** if a consumer other than `llama-server` uses a switched
  decision before any explicit init, the runtime starts on that caller's thread
  with no arguments. The gate measures that path; the server itself always takes
  the explicit one.
- **Scope held:** `server-tools.cpp`, the MIME and Cache-Control lambdas,
  `server-schema.cpp`, `server-decision.cpp` and the MCP files were not touched.

## 11. Next step

The two cheapest sites are closed: a runtime gate exists, the status body is
deleted, and the Origin body waits for its release cycle before removal. Next, in
sub-662's order (smallest blast radius first):

1. **Asset MIME** — `mime-table.goose` (`mime_of` + `mime_text`). The gate is the
   same shape: every asset name the embedded table can carry (70 real names plus
   one name per extension and the empty name), Goose vs `a->type` from
   `llama_ui_find_asset`, 0 divergences; then delete the `a->type` fallback and
   keep the compile-time option as the unit-isolation build.
2. **Cache-Control** — `web-assets.goose` (`web_cache_text`). Two policies plus
   one out-of-range id; the native literals `cache_immutable` and
   `cache_revalidate` are the frozen reference. Cheapest mutation half: swap the
   two literals.
3. **Tool dispatch, last** — `tools-pure.goose` (`tool_index`). It is the only one
   that also serves MCP tools appended at run time, so its gate needs the native
   `tools` vector as well: built-in order, a reordered table, an MCP name that is
   not in the built-ins, and an empty name.

Each step: runtime switch default ON, gate green, delete that one body, rebuild
OFF/ON, re-run the mutations.
