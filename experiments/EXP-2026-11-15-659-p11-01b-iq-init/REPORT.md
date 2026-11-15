# P11-01 / M1-b — the IQ-table seam is gone: the unit is a library of its own

Build-only increment. No model was loaded, no server was started, no GPU was
allocated, and nothing in `/home/end0/AI/FrankenMoE-CUDA` was written or
committed. Every artefact is under `/tmp/opencode/sub-661/`.

- Base commit: `2fca7f4a` (EXP-2026-11-15-658, M1)
- Repository HEAD while this ran: `81be234f` (parallel work); the unit is
  pinned to M1's frozen module list, not to HEAD
- Input tree: `/tmp/opencode/sub-659/tree` (copied, builds excluded)
- Input report: `experiments/EXP-2026-11-15-658-p11-01-m1-apex-assembly/REPORT.md`
- Work tree: `/tmp/opencode/sub-661/tree`
- Compiler: `gcc-13` / `g++-13` 13.4.0, cmake 4.2.3, 32 cores
- Unit: unchanged, composed at base `050354e9`, `unit/main.goose`
  `7f29ca5e…9f44967d` — byte-identical to M1's

## 1. Results in one line each

| claim | result |
|---|---|
| unit + adapter compiled outside `ggml-base` | **yes**, `libfg-goose-runtime.a` (4,673,818 B) |
| static-library cycle | **none**: 73-node target graph, 0 cycles, no `--start-group` |
| OFF build + `llama-server` link | **yes**, 17,794,640 B, 0 errors |
| ON-all build + `llama-server` link | **yes**, 22,050,592 B, 0 errors |
| ten accepted gates, OFF | **10/10**, `goose_calls=0` |
| ten accepted gates, ON | **10/10**, accepted counters (30 / 394 / 24+7 / 53 / 182 / 74 / 40,072 / 2,572 / 84 / 376) |
| new IQ seam gate | **PASS**, 15,360 comparisons, 0 failures |
| two-thread init | **1** `goose_init` for 512 calls, descriptor bound |
| `goose_init` definitions | **1** in each of 13 ON binaries, **0** in 11 OFF |
| 55 switch exports | max **1** definition per ON binary, **0** in OFF, 0 violations |
| `op_sqrt` | 0 in both `llama-server` binaries |
| runtime archive's external refs | libc only + **one weak** `fg_goose_iq_tables_native` |
| repository touched | **no** (no modified tracked file, no commit) |

## 2. The init contract

The unit declares 14 IQ entry points (`qd_iq2_grid`, `qz_iq2_qgrid`,
`qz2_iq3_map`, …). Until M1 they were defined inside
`ggml/src/ggml-quants.c`, because the grids live in `ggml-common.h` literals and
the map/neighbour tables are file-static arrays built by `iq2xs_init_impl()` /
`iq3xs_init_impl()`. That single fact is what forced the unit into `ggml-base`.

Now the contract is data, not a link-time definition:

```
host/goose-iq-tables.h
    fg_goose_iq_tables                6 function pointers + provider ctx
      abi, size                       checked at bind time (ABI 1)
      ctx                             provider state, NULL for the CPU tables
      iq2_grid, iq3_grid, iq2_map, iq2_nb, iq3_map, iq3_nb

    int  fg_goose_host_bind_iq_tables(const fg_goose_iq_tables *)
    int  fg_goose_host_init_with_tables(const fg_goose_iq_tables *)
    int  fg_goose_host_iq_tables_bound(void)
    const fg_goose_iq_tables * fg_goose_iq_tables_native(void)   // CPU provider
```

Ordering is the whole point: `fg_goose_host_init_once()` installs the
descriptor first, then calls `goose_init(0, NULL)`. The tables therefore enter
the runtime at init; no module code can reach them at link time.

Three properties make the contract safe rather than merely different:

1. **One provider per process.** `bind` uses a compare-exchange; the same
   pointer again returns 0, a different descriptor returns `-4`, a bad
   ABI/size/NULL entry returns `-1/-2/-3`. Two owners of the process-global
   tables would be a second copy, which is exactly the seam being removed.
2. **A clear failure instead of a silent NULL read.** Every dispatcher asks
   `iq_tables_required()`, which prints the view name and aborts if nothing is
   bound. `fg-iq-seam` proves this in a forked child: `unbound_aborts=1`,
   signal `SIGABRT`.
3. **No hard dependency on ggml.** The adapter references the CPU provider
   weakly. A host that never links ggml can bind its own tables (device-side
   owner, test double) and link only `fg-goose-runtime`.

The CPU provider lives where the tables live, `ggml-quants.c`: six
`(ctx, group, index)` adapters plus a `static const fg_goose_iq_tables` and the
`fg_goose_iq_tables_native()` accessor. The 14 unit-facing dispatchers moved to
`host/goose_host.c`; their names and signatures are unchanged, so the composed
unit did not have to be recomposed.

## 3. What changed

| file | change |
|---|---|
| `host/goose-iq-tables.h` | **new**, the contract above (ABI 1) |
| `host/goose_host.c` | regenerated: binding, `init_with_tables`, `iq_tables_required`, 14 dispatchers |
| `host/goose-host.h` | regenerated: the three new API entries |
| `host/goose_iq_tables_for_quants.inc` | **deleted**; nothing is appended to `ggml-quants.c` any more |
| `llama.cpp/ggml/src/ggml-quants.c` | −14 wrappers, +6 ctx adapters, + descriptor, + provider accessor |
| `CMakeLists.txt` | new `fg-goose-runtime` static library; `ggml-base` links it PUBLIC; gates link it; new `fg-iq-seam` target |
| `gates/m1/host_smoke.c` | threads call `init_with_tables(fg_goose_iq_tables_native())`, assert `iq_bound=1` |
| `gates/m1/iq_seam.c` | **new** gate: unbound abort, bind rules, all 14 views against the native tables |
| `common/fg-goose-quant-bridge.c` | private `pthread_once` removed; uses `fg_goose_host_ensure_init()` |
| `common/fg-goose-gguf-bridge.cpp` | private `std::call_once` removed; same guard |
| `gen_host.py` (this dir) | emits the dispatchers instead of the `.inc`; `WORK` moved to `sub-661` |

The last two rows close a second start-up path M1 left open: the S1.3 and S1.2
bridges still entered `goose_init()` through their own guards. With a
descriptor installed at init, a private guard would also have skipped the
binding, so both now use the one guard.

## 4. Library graph

`fg-goose-runtime` is `STATIC` and contains exactly two objects,
`goose-gen/main.c` and `host/goose_host.c`. Its out-edges in the configured
graph are `m` and `pthread`; there is no edge to `ggml-base`, `ggml` or
`llama`. `ggml-base` → `fg-goose-runtime` is the only crossing, because
`ggml-base` owns the provider and the S1.2/S1.3 bridges.

Measured on the generated target graph (`audit/graph.dot`, 73 nodes, 143
edges): **0 strongly connected components larger than one node**, i.e. no
cycle anywhere in the project. `--start-group` appears nowhere in the link
line, and the order is the one a single-pass static link needs:

```
… libggml.a  libggml-base.a  libfg-goose-runtime.a …
```

Archive audit (`audit/archive-audit.txt`):

| check | result |
|---|---|
| hard undefined symbols of `libfg-goose-runtime.a` | 34 libc + the unit's 65 adapter entries, all defined by `goose_host.c.o` |
| weak undefined symbols | `fg_goose_iq_tables_native` — one |
| `fg_goose_iq2_grid`/`ggml_*` hard references in the archive | 0 |
| `goose_init` defined in `libggml-base.a` | 0 |
| `goose_init` defined in `libfg-goose-runtime.a` | 1 |

The unit's extern list is still 99: 34 libc + 51 named helpers + 14 IQ views,
and the adapter defines all 65 non-libc ones (`audit/unit-externs.txt`,
`audit/adapter-defs.txt`).

## 5. Commands

```
rsync -a --exclude build-off --exclude build-on --exclude build-base \
      /tmp/opencode/sub-659/tree/ /tmp/opencode/sub-661/tree/
python3 gen_host.py                      # adapter + seam header

cmake -S tree -B tree/build-off -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build tree/build-off -j 24

cmake -S tree -B tree/build-on -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
      -DFG_GOOSE_JSON_SCHEMA=ON -DFG_GOOSE_GGUF=ON -DFG_GOOSE_QUANT=ON \
      -DFG_GOOSE_METRICS=ON -DFG_GOOSE_TOKENIZER=ON -DFG_GOOSE_STREAM=ON \
      -DFG_GOOSE_CONFIG=ON -DFG_GOOSE_SPEC=ON -DFG_GOOSE_MODELS=ON \
      -DFG_GOOSE_SESSION=ON
cmake --build tree/build-on -j 24

for g in fg-goose-smoke fg-gguf-smoke fg-goose-quant-smoke \
         fg-goose-metrics-smoke fg-goose-tokenizer-smoke fg-goose-stream-gate \
         fg-config-smoke fg-spec-smoke fg-model-smoke fg-session-smoke; do
    tree/build-off/$g /tmp/opencode/sub-661/run-off
    tree/build-on/$g  /tmp/opencode/sub-661/run-on
done
tree/build-on/fg-host-smoke
tree/build-on/fg-iq-seam
```

`LLAMA_BUILD_TOOLS=ON` is still required for the `llama-server` target in this
llama.cpp revision (unchanged from M1).

## 6. Builds and links

| configuration | build | link | binary | size |
|---|---|---|---|---|
| OFF | 0 errors | ok | `build-off/bin/llama-server` | 17,794,640 B |
| ON-all | 0 errors | ok | `build-on/bin/llama-server` | 22,050,592 B |

Warnings are the same two upstream ones in both configurations
(`llama.cpp/tools/mtmd/mtmd-helper.cpp:997,1006`, `-Wsuggest-attribute=noreturn`).
The new library adds none.

The OFF binary is unchanged: identical section sizes
(`.text 9,592,031`, `.rodata 4,090,232`, `.data 6,264`, `.bss 693,832`), the
same 21,735 defined symbols, byte-identical symbol names against M1's OFF
binary. Its SHA-256 differs only because the build path (and therefore the
build-id) differs.

The ON binary is **1,734,064 B larger** than M1's. The cause is measured, not
guessed: M1's unit was compiled at **`-O3` with warnings enabled**, because
`set_source_files_properties()` in the top-level `CMakeLists.txt` does not
reach a source added to `ggml-base`, a target created in
`llama.cpp/ggml/src`. Its compile command (`build-on/compile_commands.json`) had
`-O3` and no `-w`/`-O1`; the object was `.text 2,062,085 B / 3,490 symbols`.
With the unit in its own top-level target the intended `-w;-O1` now applies
(`build.make` shows `-w -O1`), giving `.text 3,722,759 B / 4,191 symbols`. All
ten gates are unaffected: their decisions and counters match the accepted
cards exactly. See D-1.

## 7. Gates

| stage | gate | OFF | ON |
|---|---|---|---|
| S1.1 | `fg-goose-smoke` | 30 cases / 182 rules, 0 fail, `goose_calls=0` | same, `goose_calls=30` |
| S1.2 | `fg-gguf-smoke` | 14 cases / 41 checks, 0 fail, `goose_calls=0` | 14 / 55, `goose_calls=394` (13/14/14/347/6) |
| S1.3 | `fg-goose-quant-smoke` | 46 cases, 0 fail, `row=0 blck=0` | 58 cases, 0 fail, `row=24 blck=7` |
| S1.4 | `fg-goose-metrics-smoke` | 53 / 91, `goose_calls=0` | 53 / 91, `goose_calls=53` |
| S1.5 | `fg-goose-tokenizer-smoke` | 182/182, `goose_calls=0 pins=0` | 182/182, `goose_calls=182 pins=2` |
| S1.6 | `fg-goose-stream-gate` | 71 cases, `goose_calls=0` | 71 cases, `goose_calls=74` |
| S2.1 | `fg-config-smoke` | 66 + 40,000 fuzz, 48/48, `goose_calls=0` | same, `goose_calls=40,072` |
| S2.2 | `fg-spec-smoke` | 6,146 cases, `goose_calls=0` | 6,146 cases, `goose_calls=2,572` |
| S3.1 | `fg-model-smoke` | 84 / 198, `goose_calls=0` | 84 / 198, `goose_calls=84` |
| S3.2 | `fg-session-smoke` | 191 / 929, `goose_calls=0` | 191 / 929, `goose_calls=376` |
| M1 | `fg-host-smoke` | n/a | `threads=2 calls=512 goose_init_runs=1 iq_bound=1 bind_errors=0` PASS |
| M1 | `fg-iq-seam` | n/a | `unbound_aborts=1 bind_rc=0 iq_bound=1 goose_init_runs=1 rebind_rc=0 replace_rc=-4 checks=15,360 failures=0` PASS |

Raw outputs: `run-off/`, `run-on/`, `audit/gates.tsv`.

`fg-iq-seam` compares every grid entry of both families through all three
spellings (11,520 + 2,304 comparisons), then 64 slots of every map and
neighbour table through `qz_` and `qz2_` (1,536), against the native accessors.
15,360 checks, 0 failures: the seam moved the data path, not the values.

## 8. Symbol audit

13 ON binaries (llama-server, the 10 stage gates, host-smoke, iq-seam) and 11
OFF binaries (llama-server + the 10 stage gates); `audit/nm-audit.txt` has the
row-by-row table.

| check | result |
|---|---|
| `goose_init` definitions | **1** in every ON binary (13/13), **0** in every OFF binary (0/11) |
| switch exports (55 distinct) | **1** definition each in every ON binary, **0** in any OFF binary; 0 violations in 55 × 24 rows |
| `op_sqrt` | 0 symbol in both `llama-server` and in `main.c`; the four textual hits in `main.goose` are the `extern "sqrt" fn op_sqrt` declaration and its three calls, which compile to libc `sqrt` (M1's report said 0 in `main.goose`; that was wrong) |
| unit globals in the ON server | 854 of 854 present, no duplicate |
| unit symbols in OFF | 0 (`qz_ld_f32`, `gs_json_strtod`, `json_schema_convert` all absent) |
| provider `fg_goose_iq_tables_native` | 1 in the ON server, 0 in the OFF server, 1 in `libggml-base.a` |

## 9. SHA-256

| artefact | SHA-256 |
|---|---|
| `host/goose-iq-tables.h` | `bfff01dc 3d89bf7f 13667c57 c182c3df b34355b9 c2f43643 429b2459 956a7dfc` |
| `host/goose-host.h` | `5d3dbd9c 1c50756d 91db86b4 06097a8f 93d54fb1 f2d97cd0 e806cf00 f3a45d60` |
| `host/goose_host.c` | `b4f6f69b 831b6041 e30f9dd3 b251b95b 5d6df313 30bea3c7 ec19f9ce 13f1184b` |
| `gen_host.py` | `a050a91c 85a65bf0 77c60803 d952bc58 7d02c217 cab41413 af6493aa 69b20755` |
| `CMakeLists.txt` | `858a12da 764f2511 f6238ccc 06af02dd 3f1cd13d 73564acb 91a1e555 7b9c8365` |
| `ggml/src/ggml-quants.c` | `7f555ed9 cc5a0f57 428afe68 6affa764 8530fd2d 10dc920b 15ec5412 0d3642b2` |
| `gates/m1/iq_seam.c` | `e28a189c 3911f7fe 18b0b4a1 799f209d 025bd30f 1ece81c8 5bfad5c3 157e7dfb` |
| `gates/m1/host_smoke.c` | `56e60246 fb3397c1 765639a5 d53e342d da216ce2 af00f3d7 653dec60 b8166090` |
| `common/fg-goose-quant-bridge.c` | `78079826 73b0ff28 9d303160 a5a4fabb 0e50a94e e3e00dbc c7ffc63d 7220b77b` |
| `common/fg-goose-gguf-bridge.cpp` | `2f157e56 eca39dd7 7e0a7ebe c0605121 439fa615 7dbec473 21cdf26c f40643ec` |
| `unit/main.goose` | `7f29ca5e 8fb9916f 5bd084d5 56e3239e 9a34332a 51598d33 0c22f6bc 9f44967d` |
| `unit/main.c` | `30b32ecf 31179038 d9b83999 e4bc5b8f 43fadee9 14bb8fc3 0b7ad3f0 21b1f782` |
| `libfg-goose-runtime.a` | `3f78bbff 4a23a4cf 24286ac3 49465fbd a0cfc417 7cdd6439 40e2cc1b 6d8ad380` |
| OFF `llama-server` | `ba0208d0 fbb402f5 4c05ff48 e63b410a fc68d3f4 70f91abb a249f5ac afed0b03` |
| ON `llama-server` | `54cf30a4 863a9b6e 629d0525 6f55538f e32e4b2f 22c6deb6 c69f18e7 3ae9bb1b` |
| `fg-host-smoke` | `8b754cab 6903d55c 86e505f4 5cf64fad 442563f2 b140d2c9 0bd3b08f ee48fcf3` |
| `fg-iq-seam` | `106d25e1 a544424e 041d9bd3 1ee0c7eb abee0058 d882fc4c 8cf8ca0c 567f8549` |

Space-separated for readability; `audit/sha-sources.txt` and
`audit/sha-binaries.txt` hold the plain form. The unit hashes are identical to
M1's, so the seam moved without recomposing the unit.

## 10. Deviations, and why

- **D-1. The unit is now compiled at `-O1`, as M1 intended, not at `-O3`.**
  M1's top-level `set_source_files_properties()` did not apply to `ggml-base`
  (a target created in a subdirectory), so the unit was built at release `-O3`
  with warnings on. This is visible in M1's own CMake comment ("compiled at
  -O1") and contradicted by its compile command. The separate target fixes it.
  Cost: +1,734,064 B in the ON server and +701 unit symbols. Gate results are
  unaffected. To keep M1's size instead, add `-O3` to the unit's compile
  options, or (better for size) add `-ffunction-sections -fdata-sections` plus
  `--gc-sections`; neither is done here, because this increment is about the
  seam.
- **D-2. The provider is referenced weakly.** A strong reference would have
  been a hard edge `fg-goose-runtime → ggml-base`, recreating the cycle. The
  explicit entry point `fg_goose_host_init_with_tables()` exists for hosts that
  want the dependency visible and for embedders with their own tables.
- **D-3. Two bridges lost a private guard.** The S1.2 and S1.3 bridges kept
  their own `std::call_once`/`pthread_once` in M1. They now use the one guard.
  Without this, the quant bridge could have entered `goose_init()` before the
  descriptor was installed.
- **D-4. `host/goose_iq_tables_for_quants.inc` is deleted**, not kept unused:
  the generator no longer produces it and `ggml-quants.c` no longer appends
  generated code.
- **D-5. The unit is still linked, whole, into every ON binary.** Moving it to
  its own library does not split it; a static archive member is all-or-nothing
  and the unit is one translation unit. Only dead-code elimination could trim
  it (see D-1).

## 11. What remains

1. **Step 4 of the P11-01 plan** is untouched: the six side libraries (S1.4,
   S1.6, S2.1, S2.2, S3.1, S3.2) are still side libraries, linked only into
   their gates; S3.1 has no engine carrier. The ten flags are still not
   switches in the engine.
2. **A non-CPU table owner.** The contract now allows it, and nothing else in
   the unit has to change; a device-side provider is unimplemented.
3. **`ggml-base` still carries the S1.2/S1.3 bridges**, which is why it still
   links the runtime. Moving those two carriers out (their call sites are in
   `gguf.cpp` and `ggml.c`) would make the edge `llama-common → runtime` only,
   and `ggml-base` would be free of the runtime.
4. **Size / dead code.** The ON server carries the whole unit (4,191 symbols
   after `-O1`) rather than only what its bridges call. Function sections plus
   `--gc-sections`, or a per-stage split of the unit, is the next size lever;
   neither changes behaviour, so it is a separate measurement.
5. **Per-flag ON in isolation** was still not run (M1 recorded the same gap).
6. **Bookkeeping drift** is unchanged: 87 in `RUNTIME-INTEGRATION.md`, 98
   modules at `050354e9`, 107 at HEAD `9f5e2adb`, `dist/manifest.json` at
   84/765, and this unit's 99/854/99/65. Four sources, still not reconciled.
7. **No performance number**, by construction. The unit is on the decode path
   (S1.3); its cost belongs to P11-02 with the A/B protocol, which needs Igor's
   approval.

## 12. Risks, updated

| # | risk | state after M1-b |
|---|---|---|
| R-1 | init/threading | **closed for the merged unit**: one guard, `goose_init_runs=1` under 2 threads × 256 calls, descriptor bound. llama-server's own threads still never started. |
| R-2 | symbol collisions | **not hit**: one `goose_init`, 854 globals once, 55 switch exports once, 0 duplicate definitions. |
| R-3 | two units, one process | **unchanged, still closed**: 13/13 ON binaries carry exactly one unit. |
| R-4 | side-library flags are not switches | **unchanged and visible**: four carriers in the server, six side libraries in their gates only. |
| R-5 | quant on the decode path | **untested**: the IQ views are now proven value-identical, but no tok/s was measured. |
| R-6 | bookkeeping drift | **real**: see §11.6. |
| R-7 | benchmark validity | untouched: no model, baseline 2228.76 / 41.62 tok/s still unmeasured against this build. |
| R-8 | the weak provider | **new**: a host that links `fg-goose-runtime` without `ggml-base` and never binds gets a deliberate abort at the first IQ access, not a wrong value. Covered by `fg-iq-seam`'s forked negative control. |

## 13. Reproduction

```
# 1. the tree and the adapter
rsync -a --exclude build-off --exclude build-on --exclude build-base \
      /tmp/opencode/sub-659/tree/ /tmp/opencode/sub-661/tree/
cd /tmp/opencode/sub-661 && python3 gen_host.py
#    expect: extern 99 = libc 34 + helpers 65 (adapter 51, iq views 14)

# 2. both configurations and the links (see §5)
# 3. the gates (see §5); expect the counters of §7
# 4. the graph and the archives
cmake -S tree -B tree/build-on --graphviz=audit/graph.dot -DFG_GOOSE_*...
#    audit/cycle-check.txt is the SCC result: 0 cycles, no runtime -> ggml-base
nm --defined-only tree/build-on/libfg-goose-runtime.a | grep -w goose_init
nm -u tree/build-on/libfg-goose-runtime.a | grep -E "fg_goose_iq|ggml_"
#    expect: one weak fg_goose_iq_tables_native, nothing hard
```

Nothing was committed and no tracked file was written. `git status` in
`/home/end0/AI/FrankenMoE-CUDA` shows no modified tracked file from this
session.
