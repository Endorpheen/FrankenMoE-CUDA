# EXP-2026-11-18-974: the standalone artifact rebuilt, and a broken build found

Status: artifact rebuilt with the engine in it. The build was failing before this
commit, and the status report had been reading a stale binary as if it were current.

## Why this cycle was code, not accounting

Three cycles went on the report. Checking the artifact showed why that mattered:
`builds/goose-standalone/frankenmoe-v4` was from Nov 16, the engine landed Nov 18,
and the build that would have refreshed it **could not run at all**.

## What was actually broken

Running the standalone build produced:

    tokenizer_decisions.c:(.text+0x53a): undefined reference to `tok_bpe_add_allowed'
    tokenizer_decisions.c:(.text+0x594): undefined reference to `tok_bpe_merge_valid'
    tokenizer_decisions.c:(.text+0x670): undefined reference to `tok_bpe_ignore_merges'
    collect2: error: ld returned 1 exit status

The gate `tests/tokenizer_decisions.c` tests eight `tok_*` predicates. That
interface no longer exists anywhere in the repository:

    grep -rn '^export fn tok_' goose/        -> nothing
    goose/misc/goose-tokenizer.goose         -> 0 exports, 159 lines

The eight predicates were folded into the single `bt_bpe` entry point when the
engine landed (EXP949-956). So the gate was orphaned, the build failed on it, and
because the build failed the artifact stayed stale - which is why the status report
kept reading `tok_*` out of a Nov 16 binary and calling it a passing gate.

Two honest corrections follow:

- the build now **skips** that gate with a printed reason and points at the
  evidence that replaced it (the EXP920 differential, EXP955/956), instead of
  failing the whole build on an obsolete test;
- the report no longer prints "tokenizer decisions: no" when `tok_*` is absent but
  `bt_bpe` is present - that reads a removed interface as a missing capability. It
  says the predicates were folded into the engine.

## A bug of mine, found by running it

The first fixed build died silently after the graph gate. Cause: `grep -c` exits 1
when it counts zero, and `set -e` ended the script at the assignment. Fixed with
`|| true` and a default. Worth recording because the failure mode was silence, not
an error - the same shape as the stale report.

## Result

| check | before | after |
| --- | --- | --- |
| standalone build | **fails to link** | **exit 0** |
| `frankenmoe-v4` bytes | 5 210 120 (Nov 16) | **5 600 704** |
| `btl_load` / `bt_build` / `bt_bpe` / `bt_token_to_piece` | absent | **defined** |
| `tp_apply` / `tns_apply` / `dry_apply` | absent | **defined** |
| `rs_status_line` | absent | **defined** |
| tokenizer engine line | "stale artifact" | **"yes (4 exports)"** |
| decisions line | "no" | **"inside bt_bpe, folded into the engine"** |
| honest-reading paragraph | fixed text about a stale artifact | **follows the measurement** |

Dependencies of the new binary: `libstdc++`, `libm`, `libgomp`, `libgcc_s`, libc.
`libstdc++` is substrate - `libggml-base.a` contains six C++ objects and
`ggml_critical_section_start` references `std::__throw_system_error` - which the
build script already documents.

Substrate used: `/tmp/opencode/sub-668/flint-cpu/build-off/ggml/src`, because the
script's default `/tmp/opencode/sub-661` is absent. Static archives only, no CUDA.

## Not claimed

The engine is in the standalone binary; nothing in the standalone *calls* it yet.
`btl_load` needs a caller that parses a GGUF and runs the token tables, and the
standalone has no such entry point - that is a separate port, not a link.

Four modules in the unit export nothing (`goose-tokenizer`, `goose-model-graph`,
`goose-model-loader`, `cuda/dispatch`). They typecheck and contribute no symbols.
Recorded, not removed: deleting them is a decision for their owners under rule 15,
and three of the four are named in the ledger's remaining work.

No model was started; the build and the report are local.

## Reproduction

    FG_SUBSTRATE=/tmp/opencode/sub-668/flint-cpu/build-off/ggml/src \
        bash tools/build-standalone-runtime.sh
    nm builds/goose-standalone/frankenmoe-v4 | grep -cE ' T (btl_load|bt_bpe|tp_apply)'
    bash tools/migration-status.sh | grep -A2 'tokenizer'
