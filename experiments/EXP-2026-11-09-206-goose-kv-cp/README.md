# EXP206: KV cell state gather in Goose (P5-01, the "copy" clause)

`llama_kv_cells::cp()` (llama-kv-cells.h:128-180) is the save half of the rollback pair: before an
ubatch is emplaced, `llama-kv-cache.cpp:805` gathers the current cell state into a recovery stack.
EXP204 ported the restore half.

## Result — live

    FG206D_KV_CP cells=32907
    FG205G_KV_FIND_SLOTS calls=146 tested=32907
    2558.25 / 26.52, tokens and all 128 probabilities equal, text sha256 e68df05b

Gate: 9,181 checks / 0 failures over 400 randomized gathers plus an isolated trap for a non-zero
source shift (native asserts zero, here code 601).

## A real defect in EXP204/EXP205 that this stage exposed

**`llama_pos` is `int32_t`** (include/llama.h:68). The accessors added in EXP204 did
`reinterpret_cast<int64_t *>(pos.data())`, which reads two adjacent cells as one 64-bit value — so
every position the Goose module saw was a fusion of two cells.

It was not caught earlier because:
- the EXP204 and EXP205 gates model their own int64 arena, so they validated the algorithm, not the
  marshalling;
- the first EXP206 attempt failed loudly instead: **HTTP 500, "Context size has been exceeded"**,
  and `FG205G tested=0`, because the garbage positions made the walk give up immediately.

The fix is explicit conversion (`goose_pos_to` / `goose_pos_put` staging through int64 vectors),
not a pointer cast. This is recorded because both earlier stages were reported as live and the
marshalling underneath them was wrong.

## Second mistake, caught before running

The first rewrite of the gather staged the **whole cache** (`pos.size()`, 262144 entries) per
ubatch — the same O(cache) error that took EXP204 to 8.34 tok/s. Replaced with a compact m-entry
gather driven by `sinfo.idxs[s]`. The live witness shows O(m): 32907 cells gathered for 32907
restored, not 262144 per call.

## Wiring

- `source/cp2.goose` — the gather and the shift invariant.
- `integration/llama-kv-cache.cpp` — compact gather in the recovery path, restored from EXP205's
  copy so this stage composes 204+205+206.
- `integration/llama-kv-cells.h` — explicit int32↔int64 staging accessors.

One ODR trap worth recording: the EXP205 shim object must be recompiled against the **same** header
copy as `llama-kv-cache.cpp`. The first attempt linked a shim built against the older header (no
extra member) and the resulting layout mismatch produced the HTTP 500 as well.

## Not modelled

The native `ext` record copy: `has_cell_ext()` is false for this profile (n_pos_per_embd==1,
ple_n_heads==0), so ext is never read. It becomes a real gap for M-RoPE or PLE models.

## Switch

`GGML_GOOSE_KV_CP=0` restores the native `cells.cp(...)`.
