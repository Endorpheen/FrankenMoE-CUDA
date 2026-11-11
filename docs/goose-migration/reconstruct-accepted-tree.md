# Reconstructing the tree the accepted runtime was built from

Why this matters: the accepted DSO (`libllama-exp244`) carries the Goose modules, but the source tree
it was built from is not on disk any more. `work/llama.cpp-integration` is a *different* line: it has
no `llama_batch_ext`, and a stage built from it cannot even start against the accepted
`libllama-common` (EXP357 diagnosis). Every live call site we add must go into the tree the accepted
runtime uses, or it cannot be promoted.

## What was checked (read-only)

- 47 `experiments/*/integration` directories, 51 source files.
- The copies are **cumulative**: `EXP-2026-11-10-243-.../integration/llama-batch.cpp` holds 34 `FG`
  references, `242` holds 18, `241` 11, `240` 7 — each experiment's copy contains the previous ones.
- The latest copy compiles against the flint tree with the flint flags: `llama-batch.cpp` from EXP243
  produces a 133 664-byte object, no errors.
- File sizes line up: flint `llama-kv-cache.cpp` is 2 965 lines, the latest integration copy 3 255 —
  the difference is the inserted call sites.

## Plan

1. **Staging tree**: copy `llama.cpp-flint-cuda/src` (read-only source) to
   `runtime/reconstructed/src`. Production stays untouched.
2. **Overlay**: for every file name that has integration copies, take the copy from the **latest**
   experiment that has one, and place it in the staging tree. Files without a copy stay flint.
3. **Build**: compile the staging tree with the flint flags and link a DSO against the accepted ggml
   libraries, exactly as the EXP358 stage did.
4. **Verify identity**: compare with `libllama-exp244` on two axes — the exported symbol set, and the
   witness lines visible in a run. If the reconstructed DSO shows the same witnesses as the accepted
   one, the tree is the right one.
5. **Add call sites** to the reconstructed tree only, and promote from there.

## Risks

- Two lines are mixed in the experiments (some copies come from `work/`, some from flint). The overlay
  must reject a copy whose base does not match the flint file — a cheap check is that the copy's line
  count is at least the flint file's and that flint-only markers (`llama_batch_ext`) are present where
  flint has them.
- A file with no integration copy but with a Goose call site from a *later* experiment would be
  silently lost. Step 4's witness comparison is the guard against that.

## Progress (this pass)

- Staging tree created: `runtime/reconstructed/src`, a copy of the flint `src` (236 files).
- **Base confirmed.** The flint `llama-kv-cache.cpp` object defines 296 symbols; the EXP233 copy defines
  315 and is missing only inlining artefacts (`*.part.0`, `std::` helpers). So the integrations were
  made from this line, and `work/llama.cpp-integration` remains the wrong line.
- Selection automated: `tools/select_reconstruction_copies.py` compiles each candidate with the flint
  flags and keeps the newest copy whose defined symbols contain the flint object's. Picks so far:
  batch 243, context 244, graph 200, hparams 200, sampler 212, vocab 220, unicode 218.
- Still to resolve: `llama-kv-cache.cpp` and `llama-model-loader.cpp` did not make it through the
  automated pass; the kv file was verified by hand instead.

## Status

Staging and selection done; overlay and the identity build are next. This is the prerequisite for
promoting any of the live routes of EXP359–EXP370 out of a side stage.
