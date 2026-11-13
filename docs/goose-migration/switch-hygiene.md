# Module switch audit — defaults are correct, one name is misleading

Rule: an accepted improvement is enabled by default, so every switch exists to **disable** it. A
switch that defaults to off would contradict the rule; a switch whose name collides with a rejected
module's switch is a hygiene defect even when the behaviour is right.

## All 18 switches default to enabled

`getenv(...)` followed by `return !(f && strcmp(f, "0") == 0)` in every bridge: unset or any value
other than `0` means the Goose route is live. Verified by reading each bridge rather than by pattern
match, after a first scripted pass mis-parsed the multi-line ones and flagged 11 false positives.

## The defect: `fg209i` reuses `GGML_GOOSE_KV_SEQPOS`

    EXP207 (rejected)  fg207p_enabled  getenv("GGML_GOOSE_KV_SEQPOS")
    EXP209 (live)      fg209i_enabled  getenv("GGML_GOOSE_KV_SEQPOS")

EXP207 was **rejected** at −59% decode and is not in the chain, so there is no live conflict: setting
the variable to `0` today disables only EXP209. Checked: `fg209i_` symbols are referenced by
`runtime/exp210/build/llama-kv-cache.o` and `FG209I_KV_SEQPOS` appears in the active run; `fg207p` is
referenced by nothing in the chain.

So this is **not** a behaviour bug. It is a trap for the next person: if EXP207 were ever revived, one
environment variable would silently disable two modules, and the only symptom would be a witness
line disappearing — exactly the failure class EXP219/220 cost two stages to find.

## Recommendation, not applied

Rename EXP209's variable to `GGML_GOOSE_KV_SEQPOS_INCR`. Not done here because it changes a public
switch surface and any external script or runbook that sets `GGML_GOOSE_KV_SEQPOS` would keep working
either way — but the rename is the kind of change Igor or Codex should make deliberately during
install, not something to slip into a checkpoint.

## What else was checked

- No bridge reads a variable that another *live* module also reads.
- `EXP190` has no `_enabled` function at all, consistent with it being outside the migration.
- Every switch name matches its module prefix except the `fg209i` case above.

Reproduce: `grep -rhoE 'GGML_GOOSE_[A-Z0-9_]+' experiments/*/source/bridge.c | sort -u`
