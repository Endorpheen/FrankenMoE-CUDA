# Long-term strategy additions (recorded 2026-11-09, not yet active)

Two standing instructions from Igor, recorded here so they survive session boundaries. Neither
changes the current priority: the Apex path stays the focus until it is complete and verified.

## 1. Compiler version tracking

The Goose `stage3` compiler this repo builds against is pinned in `runtime/compiler/current.json`
and must be checked for updates periodically, then adopted when a newer qualified build exists.

**Tooling now in place:** `scripts/check_compiler_updates.py`.

    python3 scripts/check_compiler_updates.py
    exit 0  pinned compiler is the newest found
    exit 1  a newer build exists; the qualification gate is required before use

Status at the time of writing: pinned `stage3-871aec9cc92d` (mtime 2026-11-08 19:42) is the newest
of the 18 builds on this machine; every other `stage3-*` predates it. Nothing to adopt.

**Adoption rule, unchanged from the existing qualification policy:** a newer binary is not usable
until it reproduces the four bootstrap C files byte-identically, matches its recorded hash, and
emits byte-identical C for the current FA module. Only then does `current.json` move and the older
build become the rollback. The check script deliberately does not adopt anything by itself.

`const`-related behaviour is relevant here: EXP199 found that `const` values in generated code live
in `gg_globals`, initialised inside `gg_program`, so a bridge that calls a generated function
directly reads zero. That is a codegen observation worth reporting upstream and worth re-testing
against any newer compiler.

## 2. Post-Apex staging of the remaining work

**Not applied yet.** To be analysed only after the Apex path is complete and verified. The intent:

- Stage A -- the complete current Linux path: every architecture and mode the runtime supports on
  Linux, with no platform-specific shortcuts.
- Stage B -- everything else: other platforms, other backends, optional capabilities.

Whether that split is technically sound has not been established. It is recorded as a question to
answer with evidence, not as a decision.

The analysis that would settle it has to check, at minimum:

1. **Shared code paths.** Does anything in stage B change the shape of the Linux path rather than
   sit beside it? Backend registration and dynamic loading are already shared (`P4-08`); a split
   that duplicates them would be worse than doing both at once.
2. **Architecture coverage inside stage A.** "All supported architectures" is itself a matrix
   (model architectures x CPU ISAs x CUDA compute capabilities). If a single universal Goose
   runtime is the goal, stage A is not independent of stage B for the architectures it does not
   yet cover -- the Qwen extension (stage 11 in the plan) is the concrete example: it is "another
   architecture", not "another platform".
3. **Completion criteria per stage.** Each needs a gate that can fail: for stage A, the full
   Linux matrix passing with native compute disabled where the plan requires it; for stage B, the
   per-platform contracts in `GOOSE_MIGRATION_PLAN.md` stages 9-13.
4. **Rollback.** Stage A must be installable while stage B is still native, which means the switch
   surface has to be per-component rather than per-stage. It already is for the CUDA and KV work,
   so this looks satisfiable -- but it has to be shown, not assumed.

If the evidence says the split is not sound, the real dependency gets written down instead: which
platform or backend work cannot be separated from the Linux path, and why.

Until then the plan's existing 13-stage campaign remains the schedule.
