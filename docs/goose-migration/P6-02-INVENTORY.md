# P6-02 inventory: model tensor ownership, backend attachment, metadata queries, unload cleanup

Criterion: "All model tensor ownership, backend attachment, metadata queries and unload/error cleanup implemented with ABI
parity."

State today: `llama-model.cpp` has **no Goose hooks at all** -- the file's only differences from flint are the expert-pin
include block and the pin owner/mutex members (96 added lines, 0 removed). So this surface is entirely native, and "ABI
parity" is currently satisfied by identity rather than by a port. That is a valid state for the parts we decide not to
move, but it is not the same as the item being done.

## Size of the surface

| area | evidence in the file |
|---|---|
| backend attachment | 94 references to `buft` / `buffer_type` |
| tensor ownership | 39 references to `ggml_backend_buffer`, with 5 allocation sites (`ggml_backend_alloc_ctx_tensors_from_buft`, its `_size` variant, and the per-split loops) |
| metadata queries | 5 entry points: `meta_val_str`, `meta_count`, `meta_key_str`, `meta_key_by_index`, `meta_val_str_by_index`, plus `model_desc` and `model_ftype` |
| unload / error cleanup | `~llama_model`, `llama_model_free`, the `expert_pin` owner released before buffers and mmaps, and the RAII `ggml_backend_buffer_ptr` sites |

## Slices, in the order they should be taken

1. **Unload and error cleanup.** Smallest and most bug-prone: destructor ordering, the expert-pin release before buffers
   and mmaps, and the partial-failure paths that must not leak a half-built buffer. Testable today with a failing load and
   with the unload path the accepted run already exercises.
2. **Tensor ownership and backend attachment.** The real work: which buffer type a tensor gets, how the split across
   devices is decided, and who owns the resulting buffers. The loader side already has Goosed decisions
   (EXP403 alignment, EXP404 weight placement, live as `FG404S_WEIGHT_PLACEMENT`), so this slice extends an existing
   decision surface rather than inventing one.
3. **Metadata queries.** Pure and ABI-sensitive: `snprintf` return semantics (the would-be length can exceed `buf_size`),
   `-1` on a miss with `buf[0] = '\0'`, and index order taken from an `unordered_map`. Porting the string handling would buy
   almost nothing and risks changing the iteration order, so this is the slice most likely to be closed as a stated
   native boundary -- the module may own bounds/found decisions, not the bytes.

## What "done" will look like

Each slice needs: a module (or a recorded native boundary with a reason), a gate that fails under a mutation, and either a
live witness or an explicit statement of dormancy. The item closes when all four areas are accounted for in that form, not
when the file happens to match flint.
