# Staged S2.2 — the speculative-decoding decisions inside the staged runtime build

Date: 2026-11-15
Base commit of the repository: `4ae52726`
Work directory: `/tmp/opencode/sub-642/tree` (the repository was not modified, nothing was committed)
Stage: `docs/goose-migration/RUNTIME-INTEGRATION.md`, S2 item 2
("speculative decoding behind a build flag")
Module: P6-11, `goose/misc/spec-decode-decisions.goose` (`sdp_`, 11 exports)
Models: EXP-2026-11-15-626 (staged S1.1, one Goose module behind a flag),
EXP-2026-11-15-630 (staged S2.1, the config validator behind a flag)

## 1. Goal and result

Goal: switch the per-step policy of the self-speculative decode loop from the native
decision chain to the accepted Goose module `sdp_*` through a build flag, without touching
production, and prove the switch by a model-free differential gate on synthetic tokens.

Result: done for seven decisions — the draft source, the accept rule, the emit bound, the
step budget, the rollback, the advance and the end-of-turn trim.

| check | result |
|---|---|
| composition (one module, no imports) + typecheck | 12 top-level declarations, 13 specialisations |
| `goose --standalone` | one C unit, 182,043 B |
| staged build, `-DFG_GOOSE_SPEC=OFF` (whole tree, 254 compile steps) | built, no warnings |
| staged build, `-DFG_GOOSE_SPEC=ON` (whole tree, 256 compile steps) | built, no warnings |
| both trees from empty build directories | 1 min 52 s wall, 19 min 46 s CPU |
| smoke gate, OFF | 6,146 cases, `failures=0` |
| smoke gate, ON | 6,146 cases, `failures=0` |
| divergence between the two paths | 0 |
| `goose_calls` | 2,572 on ON, 0 on OFF |
| whole-loop transcripts OFF vs ON | identical, 132 lines, SHA-256 equal |
| `sdp_*` definitions | 1 per export in the ON library (11), 0 in the OFF library |
| native bodies (`*_native`, `bmoe::ngram_draft`) | 1 definition each in both libraries |
| negative controls (3 single-site mutations) | 3/3 detected, baseline green again after restore |
| model, server, GPU, KV | not used at all |

Per decision, the gate compares the dispatched result against the native body in the same
process: accept 344 cases, budget 4,167 (plan 600 combinations, rollback, advance, trim),
draft source 112, loop 1,523. The end-to-end section is 33 scripted runs (567 tokens
emitted, 368 drafted, 353 accepted, 150 drafted steps, 223 decodes, 9 end-of-turn trims,
9 runs that hit end-of-generation, 3 runs where the source abstained and the step cost the
plain decode).

## 2. Environment

```
gcc / g++   15.2.0 (Ubuntu 15.2.0-16ubuntu1)
cmake       4.2.3
goose       runtime 1e714e6146bc, toolchain goose-2026.3-string
kernel      7.0.0-43-generic
```

No GPU run, no `llama-server`, no model file, no commit. The tree starts as the staged S1.1
copy (`rsync -a` from `/tmp/opencode/sub-629/tree`), so the S1.1 JSON-schema targets are
present and built with their flag OFF, next to the S2.2 targets.

## 3. What was copied and what was added

| source (repository) | destination (staged) | role |
|---|---|---|
| `goose/misc/spec-decode-decisions.goose` | `goose/misc/` | the module under test |
| `runtime/core/src/engine/ngram_draft.cpp` | `runtime/core/src/engine/` | the shipped draft source, compiled unchanged |
| `runtime/core/include/bmoe/ngram_draft.h` | `runtime/core/include/bmoe/` | its header, unchanged |
| `runtime/core/src/engine/session.cpp` | `reference/` | provenance of the transcription, not compiled |
| `upstream/llama.cpp/common/speculative.cpp` | `reference/` | provenance of the MTP bounds, not compiled |
| `tools/compose-goose.py` | `scripts/` | composition, unchanged |

New files of this stage:

| file | purpose |
|---|---|
| `common/fg-spec-decide.h` | the carrier's types: accept / plan / rollback / advance / trim, the loop |
| `common/fg-spec-decide.cpp` | the native bodies (always compiled), the dispatch, the loop model |
| `common/fg-goose-spec-bridge.h/.cpp` | containers -> slices, one `sdp_*` call per decision, witness counter |
| `tests/fg_spec_smoke.cpp` | the gate: seven decisions and the whole loop against the native bodies |
| `tests/` `scripts/spec-gate.sh` | the exact command sequence of section 4, writes `evidence/` |
| `scripts/spec-anchors.py` | the provenance check: the anchors of the transcription in `reference/session.cpp` |
| `patches/s22-staged-cmake.diff` | the staged `CMakeLists.txt` delta |
| `patches/s22-session-proposal.diff` | the same seven switches as they would land in `session.cpp` (proposal, not compiled) |
| `evidence/` | logs, smoke output, dumps, `nm`, SHA-256, sizes, negative-control logs |

Deliverable: source, scripts, patches, report and `evidence/`. `goose-gen/spec-decide.c`,
`goose-gen/spec-decide.h`, every `build-spec-*` directory and every binary in them are
reproducible from `goose/misc/spec-decode-decisions.goose` plus `scripts/spec-gate.sh` and
must not be committed.

## 4. Exact commands

```bash
cd /tmp/opencode/sub-642/tree

# 1. Composition (one module, no imports), typecheck, one standalone C unit.
python3 scripts/compose-goose.py goose-gen/spec-decide.goose \
    goose/misc/spec-decode-decisions.goose
# -> composed 1 module(s)
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose check goose-gen/spec-decide.goose
# -> parsed ok: 12 top-level declarations
#    typechecked ok: 13 specialization(s), 0 struct/0 enum instance(s)
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose --standalone \
    -o goose-gen/spec-decide.c --header goose-gen/spec-decide.h goose-gen/spec-decide.goose
# -> wrote goose-gen/spec-decide.c (182043 bytes)

# 2. Provenance of the transcription (the anchors listed in fg-spec-decide.cpp).
python3 scripts/spec-anchors.py
# -> anchors=10 missing=0

# 3. Both builds of the whole staged tree; the flag selects the decision path.
cmake -S . -B build-spec-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_SPEC=OFF
cmake --build build-spec-off -j 32        # -> 254 compile steps, no warnings
cmake -S . -B build-spec-on  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_SPEC=ON
cmake --build build-spec-on  -j 32        # -> 256 compile steps, no warnings

# 4. Gate and whole-loop transcript parity.
./build-spec-off/fg-spec-smoke
./build-spec-on/fg-spec-smoke
./build-spec-off/fg-spec-smoke --dump > evidence/dump-off.txt
./build-spec-on/fg-spec-smoke  --dump > evidence/dump-on.txt
diff -q evidence/dump-off.txt evidence/dump-on.txt

# 5. Symbols.
nm -D --defined-only build-spec-on/libfg-spec-core.so | grep ' sdp_'
```

`scripts/spec-gate.sh` runs steps 1-5 in one shot (1 min 52 s from empty build directories,
both modes, whole tree) and writes every file in `evidence/`.

## 5. How the flag is wired

The flag adds one option, one library, one object and one executable to the staged project.
No llama.cpp file is edited and no existing target changes behaviour.

```cmake
option(FG_GOOSE_SPEC
       "Select the Goose speculative-decoding decisions (sdp_*) instead of the native ones"
       OFF)
set(FG_GATE_OPT "-O1" CACHE STRING "Optimization level of the spec gate and the Goose object")

add_library(fg-spec-core SHARED
    common/fg-spec-decide.cpp
    runtime/core/src/engine/ngram_draft.cpp)      # the shipped file, unmodified
target_compile_options(fg-spec-core PRIVATE ${FG_GATE_OPT})

if(FG_GOOSE_SPEC)
    add_library(fg-goose-spec OBJECT goose-gen/spec-decide.c)
    target_compile_options(fg-goose-spec PRIVATE ${FG_GATE_OPT} -w)
    target_sources(fg-spec-core PRIVATE common/fg-goose-spec-bridge.cpp)
    target_compile_definitions(fg-spec-core PRIVATE FG_GOOSE_SPEC=1)
    target_link_libraries(fg-spec-core PRIVATE fg-goose-spec)
endif()
```

The dispatch is one `#ifdef` per decision, in one place (`common/fg-spec-decide.cpp`):

```cpp
fg_spec::accept_t accept(const std::vector<int32_t> & drafts, const std::vector<int32_t> & target,
                         const std::vector<int32_t> & eog, int32_t tok,
                         std::vector<int32_t> * confirmed) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::accept(drafts, target, eog, tok, confirmed);
#else
    return accept_native(drafts, target, eog, tok, confirmed);
#endif
}
```

Properties that matter for the production rule ("a new feature's flag exists only to
disable it"):

- OFF is the default; the OFF path is the native code, which is compiled in both
  configurations (`nm` counts `*_native` 1/1, `evidence/nm-native-counts.txt`);
- nothing native is deleted and no run-time branch exists — the choice is made once, per
  translation unit, by the compiler;
- the Goose object is a separate translation unit with `-w`, compiled at the same `-O1` as
  the gate (the Release `-O3` comes earlier on the command line and loses, as in S2.1);
- the library links `libstdc++`, `libgcc_s`, `libc`, `libm` only — no llama.cpp, no ggml,
  no CUDA (`evidence/ldd-smoke-on.txt`).

## 6. The main seam

`session.cpp` owns the decisions inside a loop that needs the engine, a KV cache and
llama.cpp. The module owns them as pure integer functions. Everything between the two is
this seam, and it has four parts:

1. **The verify rows are precomputed.** `sdp_accept` takes `target[i]` (the target's own
   argmax at verify row `i`) and `eog[i]`. The native loop computes `argmax` lazily and
   stops at the first mismatch; the switched path must fill both arrays for all `n_draft`
   rows before the decision. That is up to `n_draft - n_acc` extra `argmax` +
   `llama_vocab_is_eog` pairs per wide step — in the gate corpus 15 rejected drafts over
   150 wide steps, so about 0.1 extra row per step, but it is a real cost of this ABI and
   the reason `argmax` and `is_eog` are explicitly *not* switched (a callback into the
   module would remove it, at the price of a much wider interface).
2. **The confirmed vector is host-reconstructed.** The module returns counts
   (`n_acc`, `confirmed` length, `eog` hit, stop index); the carrier rebuilds the token
   vector as `tok` followed by the first `confirmed - 1` drafts. Correct because acceptance
   means the target's token equals the draft, and covered by the gate's `acc_eog_*` cases
   and by negative control C, but the elements come from the drafts, not from the module.
3. **The step plan is called twice per step.** The module folds "room and the cap that goes
   in before drafting", "the defensive resize after the source returns" and "the verify
   width" into one export with `n_draft_in`. The carrier calls it with `n_draft_in = 0`
   before drafting (room, `n_max`, call) and again with the actual proposal (resize, `wide`,
   `width`). The loop model and the proposal patch do the same; the values agree with the
   native arithmetic in all 600 combinations of the grid.
4. **The transcription is the oracle, so its provenance is pinned.** `session.cpp` cannot be
   compiled here, so its decision bodies are transcribed into `*_native`
   (`scripts/spec-anchors.py` checks the 9 anchors in `reference/session.cpp` by line range
   and substring, `anchors=10 missing=0`). The draft source is the exception: it is *not*
   transcribed, `runtime/core/src/engine/ngram_draft.cpp` is compiled as shipped, so the
   OFF path for that decision is production code byte for byte.

The loop model is the fifth part: `fg_spec::run` and `fg_spec::run_native` execute the same
statements over a scripted token function `T(pos)`, so acceptance is the real rule (the
draft is compared against what the target would produce at that position) and the emitted
sequence must equal plain greedy decoding. That is what makes the slice observable without
a model; it is also its limit, see section 12.

## 7. Gate: what is compared

Two builds, one binary each, one corpus. In both builds the oracle is the `*_native` body in
the same library.

| | OFF | ON |
|---|---|---|
| `fg_spec::*` body | `*_native` | bridge -> `sdp_*` -> struct |
| `FG642_SPEC` | `mode=native cases=6146 failures=0 goose_calls=0` | `mode=goose cases=6146 failures=0 goose_calls=2572` |
| loop transcripts (132 lines) | `14a4954a…` | `14a4954a…` (identical) |

Per decision:

| section | cases | what it asserts |
|---|---|---|
| ngram | 112 | the drafted count and the drafted tokens, over 20 named corpora (empty, one token, run, copy, tie, cap, `n_max = 0`, `min_match = 0`, `max_match < min_match`, 12-token block) and 36 deterministic random corpora |
| accept | 344 | `n_acc`, `confirmed` length, `eog` hit, stop index, `n_draft` and the confirmed *vector*, over 15 named cases (all accepted, mismatch at first/middle/last, empty, accepted `eog` at first/second/last, `eog` past a mismatch, target slice shorter than the drafts, an 8-wide group) and 40 random groups; plus the emit bound (14 cases) |
| budget | 4167 | the plan on 600 combinations of `n_predict` / `n_gen` / `draft_max` / `spec_on` / `n_draft_in` (room, `n_max`, call, wide, `n_draft`, width), the rollback on every `n_acc <= n_draft <= 4` (needed, keep, rejected), the advance (next `n_past`, logits row), the trim over `n_prompt` x `n_gen` x `kv_end` (emitted end, removed rows) |
| loop | 1523 | 33 whole runs: emitted sequence, per-step trace (n_draft, n_acc, wide, keep, row), `n_past`, `kv_end`, drafted/accepted/steps/decodes/trims/eog counters; plus plain-vs-speculation equality and the abstaining-step floor |

Coverage notes, stated plainly:

- In the OFF build the per-decision comparison is a structural identity (the dispatch
  forwards to the body being compared against); the number that carries the claim is the ON
  column, where every case crosses the slice boundary.
- The loop section compares `run_native` against `run` inside *one* build, so it is a real
  differential in both modes, and the `--dump` diff adds the cross-build parity of the same
  transcripts.
- The corpus is synthetic: token ids, positions and widths, no confidences (see section 12,
  MTP probabilities). The scripts are deterministic; re-running reproduces every hash.

## 8. Negative controls

Three single-site mutations, each applied in the scratch build `build-spec-negctl`
(`FG_GOOSE_SPEC=ON`, `-O0`), gate run, source restored, generated C regenerated and compared
byte for byte (`cmp`) with the pristine copy, then the baseline re-gated:

| control | site | mutation | reaction |
|---|---|---|---|
| A | module | `sdp_rollback`: `keep = n_past + 1 + n_acc` -> `n_past + n_acc` | 274 failures, first `[FAIL] rb_keep: want 1 got 0` |
| B | module | `sdp_accept`: the accepted `eog` token counted after the break, not before | 59 failures, first `[FAIL] acc_eog_first: want 1 got 0` |
| C | bridge | the confirmed vector rebuilt from `n_acc` instead of `confirmed - 1`, so it carries the accepted `eog` token | 5 failures, first `[FAIL] acc_eog_first: want [9] got [9, 10]` |

```
after restore: generated C sha256 1c848105… MATCHES pristine
baseline: FG642_SPEC mode=goose cases=6146 failures=0 goose_calls=2572
```

Logs: `evidence/negctl-a-rollback.log`, `-b-accept-order.log`, `-c-bridge-confirmed.log`.
A mutation of the generated C itself was not run; the three controls cover the module's
policy, its loop order, and the bridge — the three places this slice can drift.

## 9. Sizes

| artefact | bytes |
|---|---|
| `goose/misc/spec-decode-decisions.goose` (input) | 17,729 |
| composed unit `goose-gen/spec-decide.goose` | 18,233 |
| `goose-gen/spec-decide.c` (generated) | 182,043 |
| `goose-gen/spec-decide.h` (generated) | 2,050 |
| Goose object `spec-decide.c.o` | 12,904 |
| `common/fg-spec-decide.cpp` (native + dispatch + loop) | 15,087 |
| `common/fg-goose-spec-bridge.cpp` | 4,027 |
| `tests/fg_spec_smoke.cpp` | 20,253 |
| `libfg-spec-core.so` OFF | 30,840 |
| `libfg-spec-core.so` ON | 51,216 (+20,376, +66.1 %) |
| `fg-spec-smoke` OFF / ON | 62,576 / 62,576 (only the mode string differs) |

The ON growth is the whole module — all 11 exports, including the four MTP ones that are
composed but unwired.

## 10. SHA-256

Inputs and stage files:

```
e9f93b67252381b78e3df63934cda589efe55c57aa913ea496a4b6bc70f033e6  goose/misc/spec-decode-decisions.goose
5d9c1ffef2b6f08a2653353f89b837acc880b545d19e8880addd03b098517695  goose-gen/spec-decide.goose
1c84810570b59a8ab8de25b06e4967aa19ad0b56a282e75abb67445c4f7dd6ca  goose-gen/spec-decide.c
3ee967132565ade77b8da260d08a76058c1472bff53ced25311af99261fe994b  goose-gen/spec-decide.h
22e9494884d652d0eeb0197c3616b4d25d921f0162679f988d82209bcd40b51e  common/fg-spec-decide.h
93b4595aa3769da127a55a309df8f78aa08c2e7fd6def7cec153577757365e52  common/fg-spec-decide.cpp
2ff0e0bd5f7cd72dccced78f7ec38581f72b3d33b7b33e6224243739cee74d2d  common/fg-goose-spec-bridge.h
abc213dfbb07c5f44245a6b7d102f71e4171c82f061ba178b25baa10b2c7989b  common/fg-goose-spec-bridge.cpp
2d3466883ddbf32be34fd11eeaff01c75e61b4f8d1019ac50d00204f280f0113  runtime/core/src/engine/ngram_draft.cpp
79eca1ecf4557dac6c4fc3e89f90d74f2c7717756665d8c18b06da445366b600  runtime/core/include/bmoe/ngram_draft.h
df744d2d220040a071c9c09c60e7d08d881b1a11647b7daed041122e28a07933  tests/fg_spec_smoke.cpp
328852119af1c5e02fa705d4ab475aed3d4836a0d8107b03773afd069ba3e897  scripts/spec-gate.sh
aec5636b4d926128d4416c90a48647d617f27a682e1cdc4edcb401c3e0a32a04  scripts/spec-anchors.py
632830ea3eb96d8625d4196e352ca8e159ed3ec7419698b19d024ffb41c2c7e6  CMakeLists.txt
```

Artefacts:

```
69e6af3c753fe4cc9324b7214fb1bf01268e245dc68cbe98f45a7453205b54d8  build-spec-off/libfg-spec-core.so
6ff68ca97f96df74394b665a48c79157e09ba9e09e678a7d1395b3c7f467a2d7  build-spec-on/libfg-spec-core.so
7b33f6620e40d002c6acc4e68693a8253b05c2e435317dcb00d37eb88bb49943  build-spec-off/fg-spec-smoke
1c708a155a33290d69df718011d1fa0630f1e35f1df522552cb719c8df9a40c5  build-spec-on/fg-spec-smoke
d2bd52675bc0619ffe98dbec9734df7cec55c39b37189742403ead0fcfed0490  build-spec-on/CMakeFiles/fg-goose-spec.dir/goose-gen/spec-decide.c.o
14a4954a53257891d07bf0d4ed2828c88e6217a004a6357e04d9f9cfb3d60c12  evidence/dump-off.txt
14a4954a53257891d07bf0d4ed2828c88e6217a004a6357e04d9f9cfb3d60c12  evidence/dump-on.txt
```

`goose/misc/spec-decode-decisions.goose` hashes to `e9f93b67…`, the value the file has in
the repository at the time of writing, so this stage builds the accepted module and not a
mutation of it. Regeneration is deterministic: after three in-place mutation rounds the same
command reproduced `1c848105…` byte for byte (`cmp` against the saved pristine copy).

This stage wrote nothing into the repository: every file above lives under
`/tmp/opencode/sub-642/tree`. The repository has moved past the assigned base commit
(`4ae52726` -> `d58c38bc`, other sessions' commits) and shows tracked modifications from
those sessions; none of them is from this stage.

## 11. Raw evidence

```
evidence/generate.log             composition, typecheck, the generated C byte count
evidence/anchors.txt              the 10 anchors, missing=0
evidence/spec-{off,on}-configure.log
evidence/spec-{off,on}-build.log  whole staged tree, 254 / 256 compile steps, no warnings
evidence/smoke-off.txt            FG642_SPEC mode=native cases=6146 failures=0 goose_calls=0
evidence/smoke-on.txt             FG642_SPEC mode=goose  cases=6146 failures=0 goose_calls=2572
evidence/dump-off.txt, dump-on.txt  the 33 loop transcripts, identical
evidence/nm-sdp-counts.txt        one definition per export on ON (11), 0 on OFF
evidence/nm-native-counts.txt     8 native bodies, 1 definition each in both libraries
evidence/nm-spec-on-sdp.txt       the 11 sdp_ exports found in the ON library
evidence/nm-spec-{on,off}.txt     the full defined-symbol dumps
evidence/ldd-smoke-on.txt         no llama.cpp / ggml / CUDA on the link line
evidence/negctl-*.log             the three negative controls, 274 / 59 / 5 failures
evidence/sizes.txt, sha256.txt
evidence/gate-run.log             the full reproduction, 1 min 52 s
```

The files without the `spec-`/`sdp-` prefix in `evidence/` (`base-*`, `dump-base.txt`,
`off-*`, `on-*`, `nm-c.txt`, `nm-export-counts.txt`, `sha-base.txt`, `sha-artifacts.txt`,
`final-sha.txt`, `script-run.log`) are inherited
from the S1.1 stage in this tree and belong to the JSON-schema gate, not to this slice.

## 12. What is still open

1. **Batched verify is not bit-identical, and this gate cannot see it.** The accept rule
   compares the target's own `argmax` against a draft, and that `argmax` comes from a
   `llama_decode` of a wide batch, whose arithmetic differs from a single-token pass at
   near-ties (`docs/mtp.md`). The module only compares numbers it is given; a divergence
   introduced by the batch itself is invisible here and stays a model-level question.
2. **MTP probabilities are unwired.** `sdp_mtp_width`, `sdp_mtp_head_clamp`,
   `sdp_mtp_rewind` and `sdp_mtp_catchup` are composed into the unit (11 exports) and not
   called. They consume the MTP head's own confidences, so they cannot be exercised on
   synthetic tokens; the native bodies (`common/speculative.cpp:1445-1470`, `1700-1800`;
   `session.cpp:1700-1706`, `1812-1830`) stay in place. The MTP arm's `dp.n_max =
   std::min(draft_max, room)` is likewise untouched in the proposal.
3. **The draft KV is modelled, not switched.** `sdp_mtp_rewind`'s rewind and every
   `llama_memory_seq_rm` / `llama_memory_clear` / bounded-rollback snapshot stay native. This
   stage carries KV as the pair (`n_past`, `kv_end`) and decides only the keep point; a
   wrong keep point is visible in the transcript, a broken KV call is not.
4. **`session.cpp` was not compiled.** The proposal patch
   (`patches/s22-session-proposal.diff`, 69 changed lines) is the integration step, checked
   against the anchors by hand, not by a build: `session.cpp` needs the engine, llama.cpp and
   a GPU build. Until it is compiled, "the flag switches production" is a claim about this
   carrier, not about the engine.
5. **Extra `argmax` cost.** Section 6, part 1: up to `n_draft - n_acc` extra `argmax` and
   `is_eog` calls per wide step. Measured in the gate corpus at ~0.1 row per step; on a real
   vocabulary of 150k that is a per-step cost the native loop does not pay.
6. **`-1` and short-slice paths.** `sdp_accept`'s "no row" branch and the module's
   `out.len` guards are covered by `acc_short_target` / `acc_no_target` / `acc_no_eog`, but
   no carrier path produces them: the bridge always passes full slices.
7. **The transcription lives twice.** `*_native` and `reference/session.cpp` must move
   together; `scripts/spec-anchors.py` catches a missing anchor, not a changed one that keeps
   the substring. A semantic diff against the engine's own loop is not written.
8. **Gate level.** The gate and the Goose object run at `-O1` (Release `-O3` first on the
   command line, `-O1` last, so `-O1` wins — same convention as S2.1). No mutation build with
   `FG_GATE_OPT=-O0` was delivered; the three controls ran in a scratch directory and their
   logs are the evidence.
9. **No model-level parity.** No server, no model, no GPU: the emitted sequence parity is
   against plain greedy decoding of a scripted token function, not against
   `llama-server --spec`. S2 acceptance still needs the GPU runtime and Igor's explicit
   approval.
10. **Two staged slices share one project.** `FG_GOOSE_JSON_SCHEMA` (S1.1) and
    `FG_GOOSE_SPEC` (S2.2) are independent; this stage built with the first OFF in both
    modes. Their combination is untested.
