# P9-05 integration package — sessions, runtime facade, draft source

Status: three slices ported and gated, production **unchanged**. This file is
the hand-off for whoever integrates them into the one Goose runtime.

- Base: `e920c612`; sources read at `6aec5a2f` (identical for the five files
  involved: `git diff e920c612 6aec5a2f -- runtime/core/src/engine/{session,runtime,ngram_draft}.cpp runtime/core/include/bmoe/{session,config}.h`
  is empty).
- Compiler: `goose-2026.3-string`,
  sha256 `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`
  (matches `runtime/compiler/current.json`), runtime `1e714e6146bc`.
- Host C: `gcc-13 (Ubuntu 13.4.0-10ubuntu1)`; host C++: `g++-13 13.4.0`.
- No model, no GPU, no benchmark was run.

## 1. Modules and their place in the canon

| Slice | Deliverable | Proposed canon path | Native target recorded in the ledger |
|---|---|---|---|
| S1 | `source/ss-session-lifecycle.goose` | `goose/chat/session-lifecycle.goose` | `runtime/core/src/engine/session.cpp` (open admission, hot-store sizing, teardown order) |
| S2 | `source/rts-runtime-facade.goose` | `goose/cli/runtime-facade.goose` | `runtime/core/src/engine/runtime.cpp` (one-shot path, config mapping) |
| S3 | `source/nd-draft-source.goose` | `goose/misc/spec-source.goose` | `runtime/core/src/engine/session.cpp` (spec wiring at open) |

Why those directories: `session.cpp` slices already live in `goose/chat/`
(`session-loop.goose`, `thinking-control.goose`), `runtime.cpp` is the
entry point and sits beside `goose/cli/config-*.goose`, and
`spec-decode-decisions.goose` (EXP619) lives in `goose/misc/` — S3 is its
open-time half and belongs next to it.

`tools/sync-goose-tree.py` copies experiment slices by ledger entry, so the
integration commit must first add to `docs/goose-migration/tasks.json` under
P9-05: `slices_total` 3, `slices_done` 3, `remaining` empty for this
experiment, `coverage_doc` pointing at this experiment's README, plus the
MANIFEST entry written by the sync script. The suggested experiment id is
`EXP-2026-11-15-636-p9-05-session-runtime-draft`.

## 2. Interfaces the port relies on (all already in the base commit)

Session side (`runtime/core/include/bmoe/session.h`,
`runtime/core/include/bmoe/config.h`):

```cpp
struct SessionConfig { std::string model_path; int n_threads, n_ctx, n_batch,
    n_gpu_layers, vram_cache_mb, expert_hot_slots, n_ubatch; bool chatml;
    int n_expert_used; bool compute_trace_layers; SamplingConfig sampling;
    MoeStreamConfig moe; SpecConfig spec; };
struct RunConfig { /* model_path, prompt, n_predict, n_threads, n_ctx,
    n_gpu_layers, vram_cache_mb, expert_hot_slots, n_ubatch, chatml, progress,
    think, n_expert_used, compute_trace_layers, workload_*, sampling, moe, spec */ };
SessionConfig session_config_from(const RunConfig &);
struct SpecConfig { enum class DraftSource { none, mtp, ngram } source;
    int draft_max; float draft_p_min; int ngram_min_match, ngram_max_match;
    bool enabled() const; bool is_mtp() const; bool is_ngram() const; };
```

Session internals the port replaces or is fed by (`session.cpp`, all in the
anonymous namespace or inside `Session::open`): `hot_slots_from_budget`,
`sentinels`, `accelerator_free_mib`, `bytes_per_slot` (the `std::regex` sum),
`effective_vram_cache_mb`, `expert_hot_slots`, `cparams.n_ubatch`,
`cparams.n_rs_seq`, `dparams.n_ubatch`, `im.mtp_batch`, `im.info` (RunInfo),
and `struct Session::Impl` for the teardown order. No header changes are
needed: every input the port needs is already an `int`, `bool`, `float` or a
`const i64[:]`/`u8[:]` view over caller-owned storage.

Export ABI (produced by `goose --standalone --header`): `int64_t`/`double`
scalars, `sl_u8 {uint8_t *data; int64_t len;}` for byte buffers, `sl_i64` for
`i64[:]`. `goose_init(0, nullptr)` must be called once per program object before
any export.

## 3. Exact integration step

### 3.1 A shared precondition in `Session::open`

Right after `Impl & im = *self->impl_; im.cfg = cfg;` (`session.cpp:641-642`)
add, once:

```cpp
// Draft source as the integer code the Goose slice takes (0 none, 1 mtp, 2 ngram).
const int64_t src_code = cfg.spec.is_mtp() ? 1 : cfg.spec.is_ngram() ? 2 : 0;
```

and hoist the per-slot expert size out of `hot_slots_from_budget` into one
helper so both call sites feed the port the same number:

```cpp
uint64_t expert_bytes_per_slot(const GgufMeta & meta); // the existing regex sum
```

This refactor is **required** and must land in the same commit as S1: the port
takes `bytes_per_slot` as an input, and the probe path (`:874`) would otherwise
compute it a second time.

### 3.2 S1 — `session.cpp`

| Native site | Replace with | Order |
|---|---|---|
| `:669-685` preflight clamp | `const int64_t clamped = ss_vram_clamp((int64_t) accelerator_free_mib(), cfg.vram_cache_mb, cfg.n_gpu_layers);` then `clamped < 0` → the existing 673-676 message, else `effective_vram_cache_mb = (int) clamped` and keep the 679-684 clamp message when `clamped != cfg.vram_cache_mb` | before the model load |
| `hot_slots_from_budget` body `:49-81` | `ss_hot_slots(budget_mb, meta.ok, meta.info.n_expert, (int64_t) expert_bytes_per_slot(meta), top_k)`; on `-1`, pick the message by `ss_hot_reason(...)`; keep the MiB figure from `ss_sentinel_mib(bytes, top_k)` verbatim | at `:691` and `:874` |
| `:856-902` probe | `int64_t plan[3] = {-1,-1,-1}; int64_t act = ss_resize_plan(expert_hot_slots, effective_vram_cache_mb, cfg.n_gpu_layers, (int64_t) free_mib, cfg.spec.is_mtp() ? 1 : 0, gguf().ok ? 1 : 0, info.n_expert, (int64_t) bytes_per_slot, top_k, {plan, 3});` — `act == 1` → `expert_hot_slots = (int) plan[2]; effective_vram_cache_mb = (int) plan[1]; cparams.expert_hot_s = (int32_t) plan[2];` and the 876-881 message with `(int) plan[2]`; `act == 2` → the 889-897 disable block, message text from `ss_hot_reason`; `act == 0` → fall through | after the throwaway `probe` context is freed, before `llama_init_from_model` |
| `~Impl` `:415-431` | no call. Keep the body and replace the prose sentence with `// teardown order pinned by ss_teardown_rank / ss_teardown_check (goose/chat/session-lifecycle.goose)`. Optionally add a debug-only `assert` over the stage ids | last |

`ss_teardown_*` is a contract pin, not a call site: the destructor cannot ask a
pure function which member to destroy. Its gate is what keeps the comment and
the body from drifting.

### 3.3 S2 — `session.cpp:817` and `runtime.cpp`

| Native site | Replace with |
|---|---|
| `session.cpp:817` `cparams.n_ubatch = cfg.n_ubatch > 0 ? (uint32_t) cfg.n_ubatch : (uint32_t) cfg.n_batch;` | `cparams.n_ubatch = (uint32_t) rts_ubatch(cfg.n_ubatch, cfg.n_batch);` |
| `runtime.cpp:14` `sc.n_batch = cfg.n_ctx;` | `sc.n_batch = (int) rts_n_batch(cfg.n_ctx);` |
| `runtime.cpp:59,63` `req.clear_kv = true; req.render_text = cfg.progress;` | keep as-is; `rts_request` pins the four fields in the gate. Optional: build them through `rts_request(...)` if the lead prefers one source of truth, at the cost of an `int64_t rq[4]` per call |
| `runtime.cpp:38-67` call order | no call site: `rts_plan` is the order contract, gated against the transcription |

**Constraint, stated plainly:** `rts_config` cannot replace
`session_config_from`. Its projection covers the integer/boolean fields; the
string, `SamplingConfig`, `MoeStreamConfig` and `SpecConfig` members are copied
by the carrier and have no representation in the slice. Do not "integrate" it
by deleting the struct copy — the mapping would silently drop the whole MoE and
spec configuration. Its role is drift detection for the ten scalar fields.

### 3.4 S3 — `session.cpp` spec wiring

| Native site | Replace with |
|---|---|
| `:728` `mparams.load_mtp = cfg.spec.is_mtp();` | `mparams.load_mtp = nd_load_mtp(src_code) != 0;` |
| `:765-770` the head admission | `if (!nd_mtp_admit(src_code, im.n_layer_nextn)) return fail(<the existing message>);` |
| `:802` `n_layer_streamed` | `const int n_layer_streamed = (int) nd_stream_layers(im.n_layer, src_code, im.n_layer_nextn);` |
| `:843` `cparams.n_rs_seq` | `if (nd_enabled(src_code)) cparams.n_rs_seq = (uint32_t) nd_rs_seq(src_code, cfg.spec.draft_max, (int64_t) cparams.n_rs_seq);` — the `default` argument is the context's current value, so "off" keeps whatever `llama_context_default_params()` set |
| `:926-927` `dparams.n_ubatch` | `dparams.n_ubatch = (uint32_t) nd_draft_ubatch(cfg.spec.draft_max, (int64_t) cparams.n_ubatch);` |
| `:983` `llama_batch_init(max(n_batch, draft_max+1))` | `llama_batch_init((int) nd_verify_batch(cfg.n_batch, cfg.spec.draft_max), 0, 1)` |
| `:1169-1172` `RunInfo` | `char spec[8]; const int n = (int) nd_source_name(src_code, {reinterpret_cast<uint8_t *>(spec), 8}); ri.spec.assign(spec, n);` then `ri.spec_draft_max = (int) nd_ri_draft_max(src_code, cfg.spec.draft_max); ri.mtp_p_min = (float) nd_ri_p_min(src_code, cfg.spec.draft_p_min); ri.ngram_min_match = (int) nd_ri_ngram_min(src_code, cfg.spec.ngram_min_match);` |

Order matters in one place only: the admission check at `:765` must run before
`:802` builds the hook, otherwise a nextn-less model gets a streamer bound past
its trunk before it is refused. The port keeps that order; the integration must
not reorder the two blocks.

### 3.5 Build wiring

```bash
G=…/goose-2026.3-string/goose
for m in chat/session-lifecycle cli/runtime-facade misc/spec-source; do
  n=$(basename "$m")
  "$G" --standalone -o "runtime/core/src/goose/$n-gen.c" \
                 --header "runtime/core/src/goose/$n-gen.h" "goose/$m.goose"
done
```

Then add the three `.c` files to `runtime/core/CMakeLists.txt` as C sources of
the `bmoe` target. Two constraints learned from the accepted migration:

1. **One `goose_init` per binary.** Every standalone module emits its own
   `goose_init`; linking three into one target gives duplicate symbols. The
   established fix is `objcopy --redefine-sym goose_init=goose_init_<slice>` on
   two of the three objects and one explicit call to each renamed initialiser at
   start-up. Verify with `nm` that exactly one `goose_init` survives per module.
2. No accepted slice has been wired into `runtime/CMakeLists.txt` yet (the
   current tree contains no `*-gen.c` under `runtime/`), so this commit would be
   the first such link. Treat it as its own integration change with the lead's
   review, not as part of the port acceptance.

Generated C: 172858 / 165703 / 166327 bytes for S1/S2/S3 at `-O1`, well under
the 1 MiB threshold `tools/check-generated-files.py` enforces; still, the
accepted convention is to keep the Goose source as the artifact and regenerate.

## 4. Compatibility checks performed

| Check | Result |
|---|---|
| `goose check` on each module | ok: 166/161/149 top-level declarations, 0 errors |
| Gate at `-O1` | 17/11/36 cases, 119/135/78 checks, 0 failures |
| Gate at `-O0` | identical numbers, 0 failures; distinct binaries (32 KB vs 275 KB for S1) prove the flag took effect |
| Mutations (12 per slice, 6+6 halves, `-O0`) | 36/36 caught; every mutant passed `goose check` first |
| Export-name collisions in `goose/` (excluding `_archive`) | 0 hits for all 28 exported names |
| Prefix availability | `ss_`, `nd_` free; `rt_` occupied by `goose/cpu/goose-meta-backend.goose` → `rts_` used |
| Overlap with accepted neighbours | none: `sdp_` (EXP619) owns per-step policy, `cfg_validate` (EXP620) owns flag admission; boundaries are stated in the module headers |
| New env/CLI flags | none — the slices are pure functions, nothing to enable or disable (rule 10 is satisfied vacuously) |
| Repository files changed | none |

## 5. Assumptions

1. `session.cpp`, `runtime.cpp`, `ngram_draft.cpp` and the two headers are at
   the line numbers cited; if the carrier moves them, re-derive the oracle
   quotes (the gate transcribes the behaviour, not the offsets).
2. `bytes_per_slot` is supplied by the carrier and equals the native regex sum:
   expert-tensor bytes divided by `n_expert`, per tensor, summed. A wrong value
   here changes slot counts on the probe path but cannot make the port disagree
   with the oracle, because both take the same input.
3. `accelerator_free_mib()` returns MiB as `uint64_t`; the port compares it as
   `int64_t`. The only divergence would be a device reporting more than
   8 EiB free, which no `int` budget can express anyway.
4. `SpecConfig::source` is a closed three-value enum; any other code is treated
   as "a source is on" exactly like `enabled()` does, and `nd_source_name`
   reports "off" for it exactly like the native conditional.
5. `cparams.n_rs_seq` default is whatever llama.cpp sets; the port takes it as
   an argument instead of assuming 0.
6. Teardown ranks encode `~Impl`'s explicit sequence only. Implicit
   `unique_ptr` members (e.g. `workload`) have no stage id and are rejected by
   `ss_teardown_check`.

## 6. Conflicts and risks

1. **`rt_` prefix collision** — resolved by `rts_`, but the ledger should record
   why, or the next runtime slice will pick `rt_` again.
2. **`rts_config` is not a drop-in mapping** (section 3.3). A reviewer who sees
   "config mapping ported" may delete the native struct copy; that is a real
   regression path and the reason the constraint is stated twice.
3. **Three `goose_init` symbols** when the modules are linked together — needs
   the `objcopy --redefine-sym` step before the first `nm` clean build.
4. **The `bytes_per_slot` hoist touches `hot_slots_from_budget`'s signature**,
   which is also the function EXP621's stream-policy slice does *not* cover; no
   other accepted module calls it, so the refactor is local to `session.cpp`.
5. **Mutation B4 (S1) had to be replaced**: `s > 0` → `s >= 0` is semantically
   equivalent because `s == 0` is unreachable when `pb > 0`. A mutation table
   built without checking reachability would have counted a miss as coverage.
6. No performance claim and no speed regression risk: the added exports sit on
   the open path (once per session) and the per-call cost is a few integer
   operations against work measured in gigabytes of I/O. The gate does not
   measure this and does not pretend to.

## 7. Rollback

Everything is additive: three new Goose sources, three fixtures, two scripts,
three logs. Nothing in `runtime/` or `goose/` was touched, so rollback is
"delete the experiment directory and, if the ledger was updated, revert the
P9-05 entry to `TODO` with `slices_done` back to 0". After integration, rollback
means dropping the three `*-gen.c` from CMake and restoring the native
expressions — all of which are quoted verbatim in section 3.

## 8. Artifacts

Directory `/tmp/opencode/sub-636` (outside the repository):

```
source/ss-session-lifecycle.goose   ae013aef00c8d37e742d369373c70b1e924b46057c8837c620ebedf94f3e537c
source/rts-runtime-facade.goose     f69307f4579e80d19572bdda8304b38243a8b9a172b5da231ca3957ae1c3caf7
source/nd-draft-source.goose        15ef8fef31fd23421b7b33058c83a26460460941648482584367c5af45502d79
tests/fixture_ss.cpp                520c07f8275951a17a79f7c272021c9af860d6f952e86fb6f9a3daee1e43977c
tests/fixture_rts.cpp               df9ea70db867bf69359a5ea94bf02e4c161701324b2c0471d141b0a4503e03bd
tests/fixture_nd.cpp                32dee1aa4f240df84e53cb5ba4dfccdf61132e6e8f99ccf209878027ef694e7d
tests/build.sh                      3660832a3574036046aa443932be77c57f3a27e43e3d62162b28f4e5c8d2325f
tests/mutate.py                     4e3bd46c1afa4659cf5ed7a4e1ac362c313c52132429392efbbedea5dc7f85e1
results/gate-O1.log                 1312362374471ca9f7e935e0327834de74b385562e73896b0a4cd274fa3e2694
results/gate-O0.log                 0344d821bbacbb8573aa9f8e33af150bd42423ace8b1584163938b52b5409f93
results/mutations.log               9465034da3f2865feec435fe04cba2baa7219b122c7efeb20210f6a4bb227ba1
build/gateO1/ss.c                   d8eb9b12f5a5035a6184013e0b4fe6f9213dcb1c03d874ddcc9067e33b3935ee
build/gateO1/ss.h                   af12cfb794fb1159646c2c1703128b09de94151d693c78d0ef7527643d1cd4b4
build/gateO1/rts.c                  cd3acbe8dbf4ac359da11db326e3fc669fa6d4166a35d47128d22011515472f3
build/gateO1/rts.h                  bc762a84baeb4540d0a5e2ceaf3c862aab55296d950309f275366d1f098de909
build/gateO1/nd.c                   8e1325885dc30178d3cba10b4bf24eebb988a3424340a806b59a41c48c6519d9
build/gateO1/nd.h                   c0b0fad93aa9aadfc0145b07d6c609d689f75f250249741bf36b536a84d31029
```

Generated C/H are listed for reference only; they are reproducible from the
Goose sources with the pinned compiler and are not part of the deliverable.

## 9. Summary line for the ledger

`P9-05, 3 slices (ss_ session lifecycle, rts_ runtime facade, nd_ draft source),
64 cases / 332 checks / 0 failures at -O1 and -O0, 36/36 mutations, production
unchanged, no duplicates with EXP572/602/619/620.`
