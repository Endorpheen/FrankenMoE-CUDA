# EXP356: the n_predict 0 crash lives in the rebuilt libllama

The crash from EXP355 needed attribution. Four configurations were run with the same two-request sequence (empty prompt with `n_predict 4`, then
`n_predict 0`), changing one library at a time.

| server impl | libllama | result |
|---|---|---|
| merged (our ports) | merged exp371 | **abort** |
| restored native `aa360322` | merged exp371 | **abort** |
| restored native `aa360322` | rebuilt exp340 (stage) | **abort** |
| restored native `aa360322` | flint's own `62e4daad` | **both requests answered** |
| flint's own (native binary) | flint's own | both answered |

So the crash depends on `libllama` and on nothing else. Every configuration using a **rebuilt** library aborts -- the EXP340 stage, the merged
build, and the mixed stage-plus-merged pairing -- while the configuration using flint's own library works, regardless of which server
implementation sits on top.

## What that rules out

- Not the server ports: the restored native server implementation aborts just the same.
- Not the base library: the base is unchanged across all four rows.
- Not the merged bridge set: the crash predates it, in the EXP340 rebuild.

## What that implicates

`runtime/reconstructed/src` plus the reconstruction's patches produce a `libllama` whose behaviour differs from flint's on a legal request. The
abort happens in `pre_decode`, when llama reports `pos_min == -1` with `n_past > 0`, so the difference is in the batch or memory state that
llama tracks between the two requests, not in arithmetic.

The accepted runtime uses the same rebuilt library, so the accepted baseline carries this defect too. It stayed invisible because the accepted
profile never sends `n_predict 0`.

## Next step

Bisect `runtime/reconstructed/src` against flint's sources file by file, starting with the memory and batch files, since the divergence is in
`pos_min` after a zero-length generation.

## Narrowed to one file

Two further swaps, each rebuilding libllama with a single file taken from flint:

| file replaced with flint's version | result |
|---|---|
| `llama-context.cpp` | **still aborts** |
| `llama-kv-cache.cpp` | **both requests answered, no abort** |

So the divergence is in our `llama-kv-cache.cpp`. It is not a Goose decision: with **all fourteen** KV gates set to 0
(`GGML_GOOSE_KV_CELLS`, `_CELL_EXT`, `_FIND_SLOT`, `_INPUT_IDXS`, `_K_SHIFT`, `_LAYOUT_SPAN`, `_META_PLAN`, `_N_KV`, `_READ_META`, `_READ_SINFO`,
`_SCAN`, `_STATE_CLEAR`, `_STATE_WRITE`, `_WHOLE_CLEAR`) the crash still happens, so disabling every port leaves a behavioural difference behind.

That points at version skew rather than at a port: our copy of the file descends from an earlier state of the upstream source, and the fix for the
`pos_min == -1` path landed upstream afterwards. Supporting evidence is thin but consistent -- the non-bridge differences are mostly continuation
lines of multi-line `extern` declarations, so the file is not a rewrite, it is an older revision with bridges added. Settling it properly needs
the upstream revision our copy was taken from, which is what the next step should find.

## Attempted fix: the arena reads had no gate

`llama-kv-cells.h` answers `seq_pos_min` and `seq_pos_max` from an incrementally maintained arena whenever the arena exists:

    if (int64_t * gb = goose_bounds_ptr()) {
        return (llama_pos) gb[seq_id];
    }

There is no gate on that read, so the "gate off means native behaviour" contract was broken by construction: an experiment that switched the
incremental bookkeeping off still consumed its (possibly stale) values.

The four arena reads are now behind `fg209i_enabled()`. That is a correctness fix regardless of the crash, but it does **not** resolve it: with the
gate off, and again with it on, the two-request sequence still aborts.

So the arena is not the whole story, and the remaining difference inside `llama-kv-cache.cpp` is still to be found. What is already established
stands: replacing that one file with flint's version removes the crash, and disabling all fourteen KV gates does not.
