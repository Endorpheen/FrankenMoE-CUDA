# Staged S3.2 — the P9-05 session, runtime and draft decisions inside the staged build

Date: 2026-11-15
Base commit of the repository: `8c33cde8`
Work directory: `/tmp/opencode/sub-644/tree` (the repository was not modified, nothing was committed)
Stage: `docs/goose-migration/RUNTIME-INTEGRATION.md`, S3 item 2
("session, runtime and draft source behind a build flag")
Modules: P9-05, `goose/misc/{session-lifecycle,runtime-facade,draft-source}.goose`
(`ss_` 11 exports, `rts_` 6, `nd_` 11 — 13 of them switched here)
Models: EXP-2026-11-15-626 (staged S1.1, one Goose module behind a flag),
EXP-2026-11-15-630 (staged S2.1, the config validator behind a flag),
EXP-2026-11-15-640 (staged S2.2, the per-step spec decisions behind a flag),
EXP-2026-11-15-632 (the accepted P9-05 slices and their gate)

## 1. Goal and result

Goal: switch the open-time decisions of the standalone session from the native decision chain
to the accepted Goose slices through one build flag, without touching production, and prove the
switch by a model-free differential gate on synthetic state.

Result: done for thirteen decisions — the hot-store sizing and its refusal codes, the VRAM
preflight clamp, the post-load probe (margin, budget, action), the two context widths
(`n_batch`, `n_ubatch`), the order of `run()`'s five calls with its two early returns, the MTP
draft-context width and the verify-batch width.

| check | result |
|---|---|
| composition (three modules, one unit) + typecheck | 210 top-level declarations, 87 specialisations |
| `goose --standalone` | one C unit, 189,669 B |
| staged build, `-DFG_GOOSE_SESSION=OFF` | built, no warnings |
| staged build, `-DFG_GOOSE_SESSION=ON` | built, no warnings |
| smoke gate, OFF | 191 cases, 929 checks, `failures=0` |
| smoke gate, ON | 191 cases, 929 checks, `failures=0` |
| divergence between the two paths | 0 |
| `goose_calls` | 376 on ON, 0 on OFF |
| open-plan transcripts OFF vs ON | identical, 24 lines, SHA-256 equal |
| `ss_`/`rts_`/`nd_` definitions | 1 per switched export in the ON library (13), 0 in the OFF library |
| `goose_init` definitions | 1 in the ON library, 0 in the OFF library |
| native transcription | 13 bodies, 1 definition each in both libraries |
| same gate at `FG_GATE_OPT=-O0` | 191/929/0 in both modes, distinct libraries |
| model, server, GPU, KV, gguf file | not used at all |

Per decision, the gate compares the dispatched result against the native transcription in the
same process: hot store 23 cases, refusal codes 23, sentinel MiB 12, preflight clamp 16, probe
margin 10, probe budget 14, resize plan 20, `n_batch` 10, `n_ubatch` 12, `run()` order 12,
draft width 14, verify width 14, sentinels 10, and the composed open plan over 24 states.

## 2. Environment

```
gcc / g++   13.4.0 (Ubuntu 13.4.0-10ubuntu1)
cmake       4.2.3
goose       runtime 1e714e6146bc, toolchain goose-2026.3-string
            binary sha256 664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab
kernel      7.0.0-43-generic
```

No GPU run, no `llama-server`, no model file, no commit. The tree starts as the staged S1.1
copy (`rsync -a` from `/tmp/opencode/sub-629/tree`): the S1.1 JSON-schema target is present and
built with its flag OFF in both modes, next to the S3.2 targets.

## 3. What was copied and what was added

| source (repository) | destination (staged) | role |
|---|---|---|
| `goose/misc/session-lifecycle.goose` | `goose/misc/` | the module under test (`ss_`) |
| `goose/misc/runtime-facade.goose` | `goose/misc/` | the module under test (`rts_`) |
| `goose/misc/draft-source.goose` | `goose/misc/` | the module under test (`nd_`) |
| `tools/compose-goose.py` | `scripts/` | composition, unchanged |
| `runtime/core/src/engine/session.cpp` | `reference/` | provenance of the transcription, not compiled |
| `runtime/core/src/engine/runtime.cpp` | `reference/` | provenance of the transcription, not compiled |
| `runtime/core/include/bmoe/session.h`, `config.h` | `reference/` | the interfaces the decisions live behind |

New files of this stage:

| file | purpose |
|---|---|
| `common/fg-session-decide.h` | the carrier's shape: the 13 decisions, the `native::` oracle, `OpenState`/`OpenPlan` |
| `common/fg-session-decide.cpp` | the transcribed native bodies (always compiled), the dispatch, the composed open plan |
| `common/fg-goose-session-bridge.h/.cpp` | state -> scalars/`sl_i64`, one export per decision, witness counter |
| `tests/fg_session_smoke.cpp` | the gate: 13 decisions plus the composed open plan against the transcription |
| `scripts/session-gate.sh` | the exact command sequence of section 4, writes `evidence/` |
| `scripts/session-anchors.py` | the provenance check: 28 anchors in `reference/{session,runtime}.cpp` |
| `patches/s32-staged-cmake.diff` | the staged `CMakeLists.txt` delta |
| `patches/s32-session-proposal.diff` | the same 13 switches as they would land in `session.cpp`/`runtime.cpp` (proposal, not compiled, `patch -p1 --dry-run` clean against `reference/`) |
| `evidence/` | logs, smoke output, dump, `nm`, SHA-256, sizes, `ldd` |

Deliverable: source, scripts, patches, report and `evidence/`. `goose-gen/session.goose`,
`goose-gen/session.c`, `goose-gen/session.h`, every `build-sess-*` directory and every binary in
them are reproducible from the three module files plus `scripts/session-gate.sh` and must not be
committed.

## 4. Exact commands

```bash
cd /tmp/opencode/sub-644/tree

# 1. Composition (three modules, one unit), typecheck, one standalone C unit.
python3 scripts/compose-goose.py goose-gen/session.goose \
    goose/misc/session-lifecycle.goose goose/misc/runtime-facade.goose goose/misc/draft-source.goose
# -> composed 3 module(s) -> goose-gen/session.goose
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose check goose-gen/session.goose
# -> parsed ok: 210 top-level declarations
#    typechecked ok: 87 specialization(s), 2 struct/0 enum instance(s)
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose --standalone \
    -o goose-gen/session.c --header goose-gen/session.h goose-gen/session.goose
# -> wrote goose-gen/session.c (189669 bytes)

# 2. Provenance of the transcription (the anchors quoted in fg-session-decide.cpp).
python3 scripts/session-anchors.py
# -> anchors=28 missing=0

# 3. Both builds of the staged tree; the flag selects the decision path.
cmake -S . -B build-sess-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_SESSION=OFF
cmake --build build-sess-off -j 32 --target fg-session-smoke
cmake -S . -B build-sess-on  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_SESSION=ON
cmake --build build-sess-on  -j 32 --target fg-session-smoke

# 4. Gate and open-plan transcript parity.
./build-sess-off/fg-session-smoke   # FG644_SESSION mode=native cases=191 checks=929 failures=0 goose_calls=0
./build-sess-on/fg-session-smoke    # FG644_SESSION mode=goose  cases=191 checks=929 failures=0 goose_calls=376
./build-sess-off/fg-session-smoke --dump 2>/dev/null > evidence/dump-off.txt
./build-sess-on/fg-session-smoke  --dump 2>/dev/null > evidence/dump-on.txt
diff -q evidence/dump-off.txt evidence/dump-on.txt

# 5. Symbols.
nm -D --defined-only build-sess-on/libfg-session-core.so | grep -E ' (ss_|rts_|nd_)'
nm -D --defined-only build-sess-off/libfg-session-core.so | grep -cE ' (ss_|rts_|nd_)'
```

`scripts/session-gate.sh` runs steps 1-5 plus the `-O0` rebuild in one shot (19 s wall from
empty build directories) and writes every file in `evidence/`.

## 5. How the flag is wired

The flag adds one option, one library, one object and one executable to the staged project. No
llama.cpp file is edited and no existing target changes behaviour.

```cmake
option(FG_GOOSE_SESSION
       "Select the Goose session/runtime/draft-source decisions instead of the native ones"
       OFF)
set(FG_GATE_OPT "-O1" CACHE STRING "Optimization level of the session gate and its Goose object")

add_library(fg-session-core SHARED common/fg-session-decide.cpp)
target_compile_options(fg-session-core PRIVATE ${FG_GATE_OPT})

if(FG_GOOSE_SESSION)
    add_library(fg-goose-session OBJECT goose-gen/session.c)
    target_compile_options(fg-goose-session PRIVATE ${FG_GATE_OPT} -w)
    target_sources(fg-session-core PRIVATE common/fg-goose-session-bridge.cpp)
    target_compile_definitions(fg-session-core PRIVATE FG_GOOSE_SESSION=1)
    target_link_libraries(fg-session-core PRIVATE fg-goose-session)
endif()
```

The dispatch is one `#ifdef` per decision, in one place (`common/fg-session-decide.cpp`):

```cpp
int64_t hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                  int64_t top_k) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::hot_slots(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
#else
    return native::hot_slots(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
#endif
}
```

Properties that matter for the production rule ("a new feature's flag exists only to disable
it"):

- OFF is the default; the OFF path is the native transcription, which is compiled in both
  configurations (`nm` counts `fg_session::native::*` 13/13, `evidence/nm-off.txt`);
- nothing native is deleted and no run-time branch exists — the choice is made once, per
  translation unit, by the compiler;
- the Goose object is a separate translation unit with `-w`, compiled at the same `-O1` as the
  gate (`-O3` from Release comes first on the command line and loses, as in S2.1/S2.2 —
  `evidence/flags.txt` records it; `FG_GATE_OPT=-O0` gives a 295,560 B ON library against
  43,504 B at `-O1`);
- the library links `libstdc++`, `libgcc_s`, `libc`, `libm` only — no llama.cpp, no ggml, no
  CUDA (`evidence/ldd-smoke-on.txt`).

Composition instead of three objects: every standalone module emits its own `goose_init`, and
three of them in one target would collide. Composing the three modules into one unit keeps one
`goose_init` and one object, so the `objcopy --redefine-sym` step EXP-632 anticipated is not
needed here; `nm` shows one `goose_init` (ON) and zero (OFF).

## 6. The main seam

`session.cpp` owns these decisions inside a function that loads a model, parses a gguf and
allocates device memory. The module owns them as pure integer functions. Everything between the
two is this seam, and it has four parts:

1. **`expert_bytes_per_slot` is an input, as EXP-632 records.** The native code derives it with
   a `std::regex` sum over the GGUF tensor names
   (`^blk\.[0-9]+\.ffn_(up|down|gate|gate_up)_(ch|)exps\.weight$`), dividing each match by
   `n_expert`. That regex is not ported and stays in the carrier: `OpenState.bytes_per_slot`
   and the `hot_slots`/`resize_plan` signatures take the number. The proposal patch hoists it
   into one `expert_bytes_per_slot(const GgufMeta &)` so the open-time sizing and the probe
   cannot feed the module two different values.
2. **The plan is a sequence, not a set of independent calls.** `plan_open` runs the native
   order — preflight clamp, then hot store, then the probe on the *post-probe* free-VRAM
   reading, then the widths, then `run()`'s trace — because the probe's input is the state the
   hot store produced, not the requested budget. The gate compares the derived plan, not just
   the individual decisions.
3. **The transcription is the oracle, so its provenance is pinned.** `session.cpp` cannot be
   compiled here (engine, llama.cpp, GPU), so its bodies are transcribed into `native::*` with
   the lines quoted above each; `scripts/session-anchors.py` checks 28 anchors by exact line
   and substring (`anchors=28 missing=0`). A transcription that drifts from the file it claims
   to reproduce passes a differential against itself, which is exactly what the anchors exist
   to catch.
4. **The bridge translates, and does not decide.** `ss_resize_plan` writes a flat `out[3]`;
   the bridge copies it into `ResizePlan`. `rts_plan` writes step codes into an `sl_i64`; the
   bridge passes the caller's array and its cap. No policy lives in the bridge.

## 7. Gate: what is compared

Two builds, one binary each, one corpus. In both builds the oracle is `native::*` in the same
library.

| | OFF | ON |
|---|---|---|
| plain name body | `native::*` | bridge -> `ss_`/`rts_`/`nd_` |
| `FG644_SESSION` | `mode=native cases=191 checks=929 failures=0 goose_calls=0` | `mode=goose cases=191 checks=929 failures=0 goose_calls=376` |
| open-plan transcripts (24 lines) | `f1c701fc…` | `f1c701fc…` (identical) |

Per decision:

| decision | export | cases | what it asserts |
|---|---|---|---|
| sentinels | `ss_sentinels` | 10 | `max(8, top_k)` on negative, zero, below, at and above the floor |
| hot store | `ss_hot_slots` | 23 | budget off/negative, missing metadata, zero expert count, no tensors, exact sentinel boundary, sentinel+1, top-k 2/8/12, expert-count clamp, 4 MiB slots, floor division, huge budget |
| refusal code | `ss_hot_reason` | 23 | which native message the same rows select (0/1/2/3) |
| sentinel MiB | `ss_sentinel_mib` | 12 | the ceiling figure the too-small message quotes, including the 1-byte and 3 MB rows |
| preflight clamp | `ss_vram_clamp` | 16 | budget off, no GPU layers, free exactly 4096 (inclusive), 4097, below reserve, fits, clamped, huge free |
| probe margin | `ss_probe_margin` | 10 | 256 base, +512 for MTP, every other code keeps the base |
| probe budget | `ss_probe_budget` | 14 | free above/at/below the margin, both margins |
| resize plan | `ss_resize_plan` | 20 | all three actions with both outputs: keep (fits, exact fit, 1-slot plan), resize (one MiB less, half budget, MTP margin, floor, clamp, 4 MiB slots, top-k 12), disable (budget 0, 255 free, huge slot, no metadata, no tensors) |
| `n_batch` | `rts_n_batch` | 10 | `n_ctx` mirrored, including 0, 1 and 65536 |
| `n_ubatch` | `rts_ubatch` | 12 | explicit, zero, negative, wider than `n_batch`, zero context |
| `run()` order | `rts_plan` | 12 | validate/map/open/request/generate, both early returns, caps 5/4/3/2/1/0, negative `validate_ok` |
| draft width | `nd_draft_ubatch` | 14 | floor 32, `draft_max+1` between 32 and the target, clamp to the target, target 0 |
| verify width | `nd_verify_batch` | 14 | `max(n_batch, draft_max+1)`, `draft_max` 0 to 4095, `n_batch` 0 |
| composed plan | all of the above | 24 | `ok`, effective budget, slots, refusal code, sentinel MiB, probe action/budget/slots, both context widths, both spec widths, `run()` trace — field by field, every state |

Coverage notes, stated plainly:

- In the OFF build the per-decision comparison is a structural identity (the dispatch forwards
  to the body being compared against); the number that carries the claim is the ON column, where
  every case crosses the slice boundary.
- Every decision has its own pinned literals, written by hand, so a transcription mistake
  cannot hide behind a matching port. Five of them were corrected while writing this stage
  (floor division clamped by `n_expert`, the sentinel MiB for 4 MiB slots, the target-width row
  of `nd_draft_ubatch`, two preflight/uppper-bound rows) — the table is hand-checked, not
  generated from the code it compares against.
- The composed plan is a real differential in both modes: it compares `plan_open` against
  `plan_open_native` inside one build, and the `--dump` diff adds the cross-build parity of the
  same 24 transcripts.
- The corpus is synthetic: MiB figures, expert counts, slot sizes, widths. No device is
  queried, no gguf is opened; the scripts are deterministic and re-running reproduces every
  hash (`goose --standalone` reproduces `189669` bytes byte for byte, `cmp` clean).

## 8. Sizes

| artefact | bytes |
|---|---|
| `goose/misc/session-lifecycle.goose` (input) | 9,222 |
| `goose/misc/runtime-facade.goose` (input) | 6,654 |
| `goose/misc/draft-source.goose` (input) | 5,290 |
| composed unit `goose-gen/session.goose` | 21,322 |
| `goose-gen/session.c` (generated) | 189,669 |
| `goose-gen/session.h` (generated) | 3,312 |
| Goose object `session.c.o` | 15,704 |
| `common/fg-session-decide.h` | 7,073 |
| `common/fg-session-decide.cpp` (native + dispatch + plan) | 13,495 |
| `common/fg-goose-session-bridge.h` | 2,106 |
| `common/fg-goose-session-bridge.cpp` | 3,466 |
| `tests/fg_session_smoke.cpp` | 24,294 |
| `libfg-session-core.so` OFF | 17,464 |
| `libfg-session-core.so` ON | 43,504 (+26,040, +149.1 %) |
| `libfg-session-core.so` ON at `-O0` | 295,560 |
| `fg-session-smoke` OFF / ON | 43,496 / 43,536 |

The ON growth is the whole composed unit — all 28 exports, including the eleven `ss_`/`nd_`
ones this stage does not switch (teardown contract, MTP admission, streamed span, `n_rs_seq`,
the `RunInfo` report). The native bodies are present in both libraries.

## 9. SHA-256

Inputs and stage files:

```
ae013aef00c8d37e742d369373c70b1e924b46057c8837c620ebedf94f3e537c  goose/misc/session-lifecycle.goose
f69307f4579e80d19572bdda8304b38243a8b9a172b5da231ca3957ae1c3caf7  goose/misc/runtime-facade.goose
15ef8fef31fd23421b7b33058c83a26460460941648482584367c5af45502d79  goose/misc/draft-source.goose
f869a8001e666877afadae6a60012a6d09324f692560935f8c68c354732cec08  goose-gen/session.goose
5cd499f5991d7eff552da322ccb2d34382b48b394903f9bbaa9fe0abc976477c  common/fg-session-decide.h
354010aff4750ef43a402452d479969d84f685b1dc691ad13423a6f998dfa4f5  common/fg-session-decide.cpp
29cb23d29c937edbe6f96847f751d3944cf6f370b833c05c79e1c9f1ffff72f4  common/fg-goose-session-bridge.h
76a5a6cba46d33db6b62bc73b163f1dc01948647f55b9215c7cbd9935835742e  common/fg-goose-session-bridge.cpp
458cb7f255a4cd45e324f080fada3542d7a1e197621bf296bb52dd3e999caf4d  tests/fg_session_smoke.cpp
76818036a42575405a987e56d2b58cc9ae3ae1a3d97bc813b150733b0abdb789  scripts/session-gate.sh
50c7a84c186bff61fef048666f117644533c217de7b4d0f25855e69d17b4a292  scripts/session-anchors.py
528e31fc164a99a400d680b903816e526b70fad5216c1e0e0073dd7518f5f2a3  CMakeLists.txt
```

Artefacts:

```
847501e7b32e5ee1008848743e8f105d4712e94128558a55f3ab38025ab5eb50  build-sess-off/libfg-session-core.so
41ec7d5c7e99953f6feb074b65de2c04d28b6874a901c38a2456d0067210bfed  build-sess-on/libfg-session-core.so
f844fe0baa8dfb9b7bf81258e839e778e401d550262271f502024ed17b4036d1  build-sess-off/fg-session-smoke
8645526eaa6f49be0b405289f3d7fd635449799d20fa3716e32fb2d8b0344288  build-sess-on/fg-session-smoke
ebeb930bdd6b56416713453b772fb3996fe14f70f233adbfe34daa4d251b8eac  build-sess-on/CMakeFiles/fg-goose-session.dir/goose-gen/session.c.o
f1c701fc1bd039c8a99939a94b81bf19e4cebe3310b05d91e0a33c59068bf819  evidence/dump-off.txt
f1c701fc1bd039c8a99939a94b81bf19e4cebe3310b05d91e0a33c59068bf819  evidence/dump-on.txt
```

The three module files hash to `ae013aef…`, `f69307f4…` and `15ef8fef…`, the values EXP-632
recorded for the accepted sources, so this stage builds the accepted modules and not a mutation
of them. Regeneration is deterministic: the same `goose --standalone` command reproduced
`189669` bytes byte for byte (`cmp` against the saved pristine copy).

This stage wrote nothing into the repository: every file above lives under
`/tmp/opencode/sub-644/tree`.

## 10. Raw evidence

```
evidence/compose.log               composed 3 module(s)
evidence/goose-check.log           parsed 210 declarations, 87 specialisations
evidence/generate.log              the generated C byte count
evidence/anchors.txt               the 28 anchors, missing=0
evidence/off-configure.log, off-build.log
evidence/on-configure.log,  on-build.log    the staged tree, both modes, no warnings
evidence/smoke-off.txt             FG644_SESSION mode=native cases=191 checks=929 failures=0 goose_calls=0
evidence/smoke-on.txt              FG644_SESSION mode=goose  cases=191 checks=929 failures=0 goose_calls=376
evidence/smoke-o0.txt              the same numbers with FG_GATE_OPT=-O0
evidence/dump-off.txt, dump-on.txt the 24 open-plan transcripts, identical
evidence/nm-export-counts.txt      one definition per switched export on ON (13), 0 on OFF
evidence/nm-goose-init.txt         1 (ON); the OFF count is recorded in nm-off.txt
evidence/nm-on.txt, nm-off.txt     the full defined-symbol dumps
evidence/ldd-smoke-on.txt          no llama.cpp / ggml / CUDA on the link line
evidence/sizes.txt, sha256.txt
evidence/o0-*-configure.log, o0-*-build.log
```

## 11. Boundaries this stage does not cross

1. **The tensor-name regex.** `expert_bytes_per_slot` is an input. The `std::regex` over
   `size_by_name` stays native; a wrong value changes the slot count on both paths equally,
   because both take the same input. Porting it is a GGUF slice, not this one.
2. **Teardown.** `ss_teardown_rank` / `_before` / `_check` are a contract pin, not a call site:
   a destructor cannot ask a pure function which member to destroy. The eleven `ss_` exports are
   composed into the unit, but only the eight sizing/probe ones are switched; the teardown trio
   keeps its gate in EXP-632 and the comment in `~Impl` is what keeps the body from drifting.
3. **`n_rs_seq`.** `nd_rs_seq` takes the context's current value as its third argument, because
   the native code never writes the field when speculation is off. Switching it would need the
   carrier to read back `cparams.n_rs_seq` after `llama_context_default_params()`; the decision
   is left native here and the proposal patch shows the call with that default. Inventing a zero
   would be a port artefact, not a native decision.
4. **The draft context's existence.** `plan_open` computes `draft_ubatch` only for MTP and
   `verify_batch` only when a source is on, matching `session.cpp:909` and `:978`. But the
   *allocations* (`llama_init_from_model`, `llama_batch_init`) stay native; this stage decides
   the width, not the object.
5. **`cfg.expert_hot_slots`.** The native code skips the sizing entirely when the launcher pins
   a slot count. The plan models the derived path only; `OpenState` has no pinned-slots field,
   so the pinned branch stays native. Stated here so a reader does not assume full coverage.
6. **The error strings.** `ss_hot_reason` returns a code; the carrier keeps the message text.
   The proposal patch maps the code back to the three native strings, including the MiB figure
   from `ss_sentinel_mib`, but that mapping is not compiled or gated here.
7. **`session.cpp` was not compiled.** The proposal patch (`patches/s32-session-proposal.diff`,
   72 added / 39 removed lines) is the integration step, checked by `patch -p1 --dry-run` against
   `reference/`, not by a build: the engine needs llama.cpp, a KV cache and a GPU. Until it is
   compiled, "the flag switches production" is a claim about this carrier, not about the engine.

## 12. What is still open

1. **The engine-side build.** The carrier compiles the decisions; `session.cpp`/`runtime.cpp`
   themselves were not built with the flag. That is the first integration step and needs the
   production build and Igor's approval (hard rule 1).
2. **The pinned-slots branch and the MTP admission refuse path.** `cfg.expert_hot_slots > 0`
   bypasses the hot-store arithmetic natively; `plan_open` does not model it. The admission
   (`nd_mtp_admit`) and the streamed span (`nd_stream_layers`) are composed and gated in EXP-632
   but not switched here.
3. **`RunInfo`.** `nd_source_name`, `nd_ri_draft_max`, `nd_ri_p_min`, `nd_ri_ngram_min` are not
   wired into this carrier: they belong to the report a session hands back, which does not exist
   in a model-free build.
4. **No negative control in this stage.** EXP-640 ran three single-site mutations; this stage
   reuses EXP-632's 36/36 mutation result for the modules and adds none of its own. A mutation of
   the bridge would be the missing control.
5. **Two staged slices share one project.** `FG_GOOSE_JSON_SCHEMA` (S1.1) and `FG_GOOSE_SESSION`
   (S3.2) are independent; this stage built with the first OFF in both modes. Their combination
   is untested, and two composed units mean two `goose_init` symbols if both flags are ever ON in
   one target — they are separate libraries here, so it works, but it is worth a note before a
   third slice joins.
6. **No model-level parity.** No server, no model, no GPU: the transcript parity is against a
   transcription of the open path, not against `llama-server` opening the same model. S3
   acceptance still needs the GPU runtime and Igor's explicit approval.
