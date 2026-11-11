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

## Attempt and result

Overlay done (`runtime/reconstructed/overlay.py`), all 27 called modules assembled from
`experiments/*/source/*.c`, DSO linked at 5 403 256 bytes with **zero** undefined `fg` symbols. Then the
run failed:

    FG205G validation code 21

Code 21 in that module is `ensure(cont == 0 || cont == 1)`. Exact cause, checked by signature:

    EXP233 call site:  fg205g_find_slots(&cells, n_swa, swa_type, ...)
    EXP205 bridge:     fg205g_find_slots(const uint8_t * can, size_t size, int64_t head0, ...)

The call site passes a `llama_kv_cells *`, the only preserved bridge expects a `can` array and a
different argument list. `grep -rln fg205g_find_slots experiments/*/source/` returns exactly one file,
so the revised bridge that matched this call site was never preserved. The two artefacts are from
different revisions — the bridge and the call site were each updated in later experiments, and the
revision that went into the accepted DSO is not the one preserved here.
Supporting evidence: the accepted DSO defines 218 `fg` symbols, the reconstruction 198, and the accepted
one also exports `llama_model::pin_mmap_experts`, which comes from a patch not present in the flint tree.

## Conclusion

Exact reconstruction from the preserved artefacts is **not possible**: the call sites and the bridges
were versioned independently and only their *last* states survive. The staging tree still builds and
links, so it is usable as a *development* tree, but it is not the accepted runtime and cannot be used to
prove equivalence with it.

Two honest options for promotion, both needing the owner's decision:

1. Treat the reconstructed tree as a new line, accept it on its own runs, and keep the accepted DSO as
   the rollback — this changes the baseline and must be an explicit decision.
2. Keep promoting nothing until the accepted tree or its exact bridge revisions are recovered from a
   backup or an archived image.

## Working state reached

With the one lost bridge disabled in the staging copy only, the reconstructed DSO (5 402 688 bytes,
26 modules) runs the fixed profile **exactly**: completion `e68df05b`, save
387 989 920 bytes, truncated restore rejected. Witnesses alive: `FG227N`, `FG228E`, `FG229I`,
`FG232R`, `FG233C`, `FG237S`, `FG239B`, `FG240Q`, `FG241U`, `FG242O` — i.e. the accepted runtime's
routes, minus the one whose bridge is lost.

So the tree is not just buildable, it reproduces the accepted answer and almost all of its Goose
routes. What it cannot carry is `fg205g`, and it also lacks `llama_model::pin_mmap_experts` (the patch
that is absent from the flint tree).

## Trap worth remembering

The first run after rebuilding still failed with the same `FG205G` code 21 — because the run directory
held a **hard link** to the previous DSO. The path was right and the content was stale, exactly as in
EXP357. The run directory now uses symlinks, so a rebuild is always picked up.

## Promotion attempt on the reconstructed tree

The batch area turned out to be **already Goose-owned** in the accepted runtime: the staged
`llama-batch.cpp` (EXP243) calls `fg240q` (set building), `fg241u` (classification), `fg242o` (output
ids) and `fg243c` (positions). Our EXP349/350/352 modules duplicate those routes, so overlaying them
adds a second Goose implementation where one already runs — that overlay was dropped.

What is genuinely new is the KV pair (`fg345w` write selection, `fg367s` clear plan): neither appears
among the accepted runtime's witnesses.

With the KV pair wired into the reconstructed tree, the DSO (5 415 656 bytes) reproduces the accepted
answer exactly and shows the accepted witnesses (`FG227N`, `FG228E`, `FG233C`, `FG237S`, `FG241U`,
`FG242O`), but **neither `FG345W` nor `FG367S` fires** on save/restore. So on this tree the save does
not reach `llama_kv_cache::state_write` the way it does on the flint-side stage of EXP363. Next step is
to find which memory implementation the save goes through here — the accepted tree's `llama-context`
calls `memory->state_write`, and the memory object may be the hybrid one, whose `state_write` delegates.

## KV promotion done

Cause of the zero calls was our own harness: the run script inherited from EXP369 set every
`GGML_GOOSE_*` switch to `0`. With the switches left alone, the reconstructed tree runs the fixed
profile with **both** new routes and every accepted route alive:

| group | witnesses |
|---|---|
| new (this line) | `FG345W_KV_STATE_WRITE`, `FG367S_KV_STATE_CLEAR` |
| accepted | `FG227N`, `FG228E`, `FG229I`, `FG232R`, `FG233C`, `FG237S`, `FG239B`, `FG240Q`, `FG241U`, `FG242O` |

and the answer is unchanged: completion `e68df05b`, save 387 989 920 bytes, truncated restore rejected.

This is the first candidate where the accepted runtime's Goose routes and ours execute together.

## Throughput is not equivalent

Same profile, one run each, no parallel work:

| metric | accepted EXP244 | reconstructed | delta |
|---|---:|---:|---:|
| prefill tok/s | 2500.05 | 1623.23 | **−35.1 %** |
| decode tok/s | 24.51 | 25.16 | +2.7 % (within noise) |

Correctness is exact, so the gap is not a defect in the ported code — it is missing **performance work
that lives in patches, not in the integration copies**: expert tier, pinned ring, mmap expert
registration and the rest of `patches/*.patch` are part of the accepted build's tree and none of them
were overlaid. `llama_model::pin_mmap_experts` being absent is the visible symptom of exactly that.

Consequence: this tree may be used to verify routes and correctness, but it must not be promoted as the
candidate until the patches are applied and the throughput is re-measured.

## Patch set needs a full tree

Applying `patches/*.patch` to the staged tree fails with "No file to patch": the patches touch
`ggml/src/...`, `ggml/src/ggml-cuda/...` and new headers next to `src/`, while the staging tree holds
only `src`. `expert-tier-integration.patch` reports six real conflicts as well, so even a full tree will
need that one rebased.

Next attempt should stage the whole flint checkout (`src`, `ggml`, `include`, `common`) and apply the
patch set in a fixed order, checking the build after each patch. Covered so far read-only: at least 12
patches report zero conflicts against the `src` tree alone, which is promising but not sufficient.

## Why throughput cannot be restored from these artefacts

Neither available line contains everything the accepted build needs:

| tree | `llama_batch_ext` | `llama-expert-tier.cpp` | `llama-mmap-expert-pin.h` |
|---|---|---|---|
| `llama.cpp-flint-cuda` | yes (36 references) | absent | absent |
| `work/llama.cpp-integration` | absent | present | absent |
| accepted build | yes | yes | yes |

The accepted runtime was built from a **hybrid** of the two, and that hybrid is not on disk. The
patches that carry the expert-tier and mmap-pin work also target files that exist in only one of the
lines, which is why they report "No file to patch" here.

So correctness can be reproduced (it is, exactly) but throughput cannot, and the gap must be expected
until the hybrid tree or its image is recovered.

## What is still native on this tree

Counted by Goose calls inside each function of the staged `llama-kv-cache.cpp`:

| function | Goose calls |
|---|---:|
| `find_slot` | 5 |
| `apply_ubatch` | 6 |
| `prepare` | 4 |
| `seq_rm` | 4 |
| `state_read_sinfo` | guards 3 (EXP373) |
| `state_write` / `state_clear` | added by us (EXP371/372) |

So the read orchestration is the one KV block the accepted runtime left native, and it is also the
block we have a gated plan for (EXP344). It is the natural next call site, and it is reachable with a
**successful** restore — the opposite of the truncated file used to witness the clear.

## Status

The reconstructed tree is a **correctness** base, not a performance equivalent. Two gaps documented:
`fg205g` (lost bridge, native walk used) and the patch set (throughput, needs a full tree and one
rebased patch). Promoting it is the owner's decision and needs the patches first.
