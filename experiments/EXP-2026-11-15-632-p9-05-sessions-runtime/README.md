# EXP-2026-11-15-636 — P9-05: standalone sessions, runtime and n-gram draft

Three independent Goose slices for the parts of `runtime/core/src/engine` that
P9-06 (EXP602, session loop/budget/interrupt), EXP572 (CLI line protocol),
EXP620 (config admission) and EXP619 (spec decode decisions) left native.

- **Base commit:** `e920c612` (task), sources read at `6aec5a2f`.
  `git diff e920c612 6aec5a2f` over `session.cpp`, `runtime.cpp`,
  `ngram_draft.cpp`, `bmoe/session.h`, `bmoe/config.h` is empty, so every line
  reference below holds for both.
- **Production code changed:** none. No model and no GPU was started.
- **Working tree:** `/tmp/opencode/sub-636` (outside the repository).

## Hypothesis

What remains of the standalone session, its runtime facade and the draft source
is pure decision logic: integer arithmetic over GGUF shape and free VRAM, a
field mapping, an admission table and a call order. Each can be replaced by a
Goose export without the graph, the KV, the tokenizer or a file, and each can be
gated against a transcription of the native lines with no model loaded.

The hypothesis is only accepted if the port reproduces the native decisions on
every case in the tables, at `-O1` **and** `-O0`, and if every one of 12
mutations per slice is caught.

## Slices

| Slice | Module | Native source | Exports |
|---|---|---|---|
| S1 `ss_` | `source/ss-session-lifecycle.goose` | `session.cpp:49-81` (hot store sizing), `:669-700` (VRAM preflight), `:856-902` (post-context probe), `:415-431` (`~Impl` order) | 11 |
| S2 `rts_` | `source/rts-runtime-facade.goose` | `runtime.cpp:9-26,32-68`, `session.cpp:817` | 6 |
| S3 `nd_` | `source/nd-draft-source.goose` | `session.cpp:728,765-770,802,843,909-931,978-986,1169-1172` | 11 |

Prefix check against the canon (exports only, `goose/` without `_archive`):

- `ss_` — free. (`ss_fail`, `ss_put` exist, but they are non-exported helpers
  inside `goose/jinja/autoparser.goose` and `goose/jinja/parsers.goose`, so they
  never reach the link namespace.)
- `rt_` — **taken** by `goose/cpu/goose-meta-backend.goose`
  (`rt_n_threads_ok`, `rt_io_threads_ok`). The runtime slice therefore uses
  `rts_`.
- `nd_` — free.

## What each slice owns, and what stays native

S1 (`ss_`): the arithmetic that sizes the VRAM hot store and the two re-checks
around it, plus the one explicit teardown order. Native: GGUF parsing, the
`std::regex` over tensor names (a caller supplies `bytes_per_slot`),
`accelerator_free_mib`, `llama_backend_init/free`, every allocation.

S2 (`rts_`): the integer/boolean projection of `session_config_from`, the
derived `n_batch = n_ctx`, the effective `n_ubatch`, the order of run()'s five
calls and its two early returns, the four `GenerateRequest` fields run() sets,
and the `RunResult` invariant (a cancelled run is still `ok`). Native:
`model_path`, `SamplingConfig`, `MoeStreamConfig`, `SpecConfig` — copied whole
by the carrier and never touched here.

S3 (`nd_`): what the session builds from an already validated `SpecConfig` —
`load_mtp`, the MTP head admission, the streamed layer bound, `n_rs_seq`, the
draft context's `n_ubatch`, the verify batch width, and the four `RunInfo`
report fields. Native: the draft itself, the accept loop and every per-step
decision (`sdp_`, EXP619), and flag admission (`cfg_validate`, EXP620).

The three boundaries are stated in the module headers so a later reader does not
port EXP619's or EXP620's work a second time.

## Oracle

Per the accepted convention (EXP602, EXP573) the oracle is a transcription of
the native lines into the fixture, one function per decision, each quoting the
lines it reproduces. In addition each fixture carries a **pinned** table whose
values were computed by hand and are stated as literals, so a transcription
mistake cannot hide behind a matching port:

- S1: `17 cases`, `119 checks`. Case table over the hot-store arithmetic
  (budget off, negative, no metadata, no experts, no tensors, exact sentinel
  boundary, one above, expert-count clamp, top-k 2 vs 8 vs 12, 4 MiB slots,
  floor division, huge budget), the preflight clamp (layers 0, budget 0, free
  4096, 4097, fitting, clamped, below reserve), the probe (margin with and
  without MTP, at/below/above margin), all three probe actions (keep, resize,
  disable) with both outputs, and the teardown ranks, the pairwise `before`
  relation, the native order and three ways to break it.
- S2: `11 cases`, `135 checks`. Five configurations compared field by field
  (defaults, streamed launcher, pinned hot slots, `n_ubatch > n_ctx`, `n_ctx 1`),
  the short-slice refusal, five plan shapes (validate fails, open fails, success,
  capped at 2, capped at 1), the request fields per configuration, and the
  result invariant.
- S3: `36 cases`, `78 checks`. Source identity (0/1/2 and an out-of-range code),
  a short name buffer, six admission cases, five streamed spans, five `n_rs_seq`
  cases including a non-zero default that must survive, seven draft-context
  widths, six verify widths, the report fields for all three sources, and the
  pinned values.

Gates, both optimisations, same numbers:

```
-O1  FG636_SS  cases=17 checks=119 failures=0
     FG636_RTS cases=11 checks=135 failures=0
     FG636_ND  cases=36 checks=78  failures=0
-O0  FG636_SS  cases=17 checks=119 failures=0
     FG636_RTS cases=11 checks=135 failures=0
     FG636_ND  cases=36 checks=78  failures=0
```

Total: 3 slices, 64 cases, 332 checks, 0 failures on both `-O1` and `-O0`
(`results/gate-O1.log`, `results/gate-O0.log`). The two builds differ in binary
size, so the optimisations really differ; the gen.o cache is keyed by source
SHA + opt flag + compiler version, so `-O0` rebuilds from C.

## Mutations

12 per slice, six in each half, each mutant validated with `goose check`, built
at `-O0` into its own directory, and required to make that slice's gate fail:

| Slice | Half 1 | Half 2 | Total |
|---|---|---|---|
| `ss_` | 6/6 | 6/6 | 12/12 |
| `rts_` | 6/6 | 6/6 | 12/12 |
| `nd_` | 6/6 | 6/6 | 12/12 |

`mutations 36/36 caught` (`results/mutations.log`). One candidate mutation was
discarded before the run because it was semantically equivalent (`s > 0` ->
`s >= 0`, where 0 is unreachable); the replacement mutates the disable branch's
slot count instead. Mutations that only move a boundary which no case exercises
would be false confidence, so the table was chosen against the case tables.

## Recorded approximations and gaps

1. `bytes_per_slot` is an **input**. The native code derives it with a
   `std::regex` match over the GGUF offset table; that regex is not ported and
   belongs to a GGUF slice, not to the session lifecycle.
2. S1 models the teardown as a rank table rather than as C++ destructor
   semantics. It is the order `~Impl` states explicitly; the implicit
   `unique_ptr` members are not modelled.
3. S2 maps an integer/boolean projection. `model_path`, `sampling`, `moe` and
   `spec` are struct copies in the native mapping and are out of scope by
   construction; the projection is a contract pin, not a replacement for the
   struct copy (see REPORT.md).
4. S3 passes the context's default `n_rs_seq` in for the "speculation off" case,
   because the native code never writes that field when speculation is off.
   Inventing a zero would be a port artefact, not a native decision.
5. `nd_enabled` follows `SpecConfig::enabled()` (`source != none`) for any code
   other than 0, which is what the native predicate does. The enum has three
   values; the gate pins the predicate, not a range check that does not exist.
6. No performance claim: there is no model and no tok/s number, by design.

## Reproduce

```bash
tests/build.sh all                  # gate at -O1
OPT=-O0 OUT_DIR=build/o0 tests/build.sh all
tests/mutate.py --module ss  --half 1
tests/mutate.py --module rts --half 2
tests/mutate.py --module all --half all
```

`build.sh` derives the experiment root from its own location and takes
`GOOSE_BIN`, `CC`, `CXX`, `OPT`, `GOOSE_SRC`, `OUT_DIR` from the environment.
`GOOSE_BIN` defaults to the toolchain used for every accepted slice
(`goose-2026.3-string`), `gcc-13` / `g++-13` for the C and C++ sides.

## Artifacts

| Path | SHA-256 |
|---|---|
| `source/ss-session-lifecycle.goose` | `ae013aef00c8d37e742d369373c70b1e924b46057c8837c620ebedf94f3e537c` |
| `source/rts-runtime-facade.goose` | `f69307f4579e80d19572bdda8304b38243a8b9a172b5da231ca3957ae1c3caf7` |
| `source/nd-draft-source.goose` | `15ef8fef31fd23421b7b33058c83a26460460941648482584367c5af45502d79` |
| `tests/fixture_ss.cpp` | `520c07f8275951a17a79f7c272021c9af860d6f952e86fb6f9a3daee1e43977c` |
| `tests/fixture_rts.cpp` | `df9ea70db867bf69359a5ea94bf02e4c161701324b2c0471d141b0a4503e03bd` |
| `tests/fixture_nd.cpp` | `32dee1aa4f240df84e53cb5ba4dfccdf61132e6e8f99ccf209878027ef694e7d` |
| `tests/build.sh` | `3660832a3574036046aa443932be77c57f3a27e43e3d62162b28f4e5c8d2325f` |
| `tests/mutate.py` | `4e3bd46c1afa4659cf5ed7a4e1ac362c313c52132429392efbbedea5dc7f85e1` |
| `results/gate-O1.log` | `1312362374471ca9f7e935e0327834de74b385562e73896b0a4cd274fa3e2694` |
| `results/gate-O0.log` | `0344d821bbacbb8573aa9f8e33af150bd42423ace8b1584163938b52b5409f93` |
| `results/mutations.log` | `9465034da3f2865feec435fe04cba2baa7219b122c7efeb20210f6a4bb227ba1` |

Generated C, headers, objects and gate binaries are reproducible from the Goose
sources above and the recorded toolchain; they are not part of the deliverable
(and are not to be committed).

## Verdict

Accepted at the slice level: 3 slices, 0 gate failures at `-O1` and `-O0`,
36/36 mutations caught. See `REPORT.md` for the integration package.
