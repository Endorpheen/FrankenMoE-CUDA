# FrankenMoE architecture: modular source, one optimised compute path

Recorded 2026-11-15. This is a direction, not a refactor instruction. The constraint is explicit
and belongs at the top: **do not start a large architectural refactor for this concept now.**
Finish the Apex standalone and the migration first. Apply these principles when they are actually
needed, not for symmetry.

## The principle

Modularity in the source, no modularity in the final binary.

Nine points, as given:

1. **One FrankenMoE core on Goose.** Memory management, scheduler, model loading, GPU expert
   cache, CPU/GPU backends and the common infrastructure are shared, not copied.
2. **Per-architecture units.** Apex, Qwen, LFM and the rest get their own modules for the
   computation that is genuinely theirs. Never a separate copy of the whole runtime per model.
3. **A shared optimisation library.** SIMD, fusion, quantised operations and caching are
   available to every architecture without duplication.
4. **Compile-time specialisation.** Fried Goose specialises the architecture unit for the model's
   real parameters - tensor sizes, expert counts, quantisation, compute routes - where it can.
5. **Modularity must not cost runtime.** The compiler inlines, does inter-module optimisation,
   specialises and removes dead code where that is safe and actually useful.
6. **Independent development.** A change to one architecture does not break the others, and a
   shared optimisation improves every compatible architecture.
7. **Performance by default.** Do not push hand-optimisation onto whoever writes the next
   architecture unit if the compiler can do it reliably.
8. **100% Goose as the end state.** No new permanent C/C++ layers. Keep the useful algorithms
   when porting them.
9. **Measurement is mandatory.** Every significant integration is checked for correctness,
   prefill, decode, VRAM and RAM, against the previous accepted Goose build.

## Why this is the right shape for this project

It is already what the tree does, in outline:

- `tools/build-main-runtime.sh` composes one common unit of 120 modules.
- `tools/build-arch-unit.sh <arch>` composes that common set plus one architecture's modules.
- `tools/compose-goose.py` resolves the cross-module collisions instead of letting them block
  composition - including the `base/model_graph.goose` alias, which turned out to be the same file
  as `goose-model-graph.goose` minus `main()`.

So the split is not aspirational. What was missing until this commit is that the **standalone**
was built from the common unit only, so it had no architecture at all.

## What changed now

`tools/build-standalone-runtime.sh` builds from `build/goose-main/main.o`, which
`tools/build-main-runtime.sh` produces. Adding the three Apex modules to that build set gives the
standalone the architecture it was missing:

| export | what it decides |
|---|---|
| `q35_plan`, `q35_kind`, `q35_shape` | the 41 layer kinds and every tensor shape |
| `qg_split` | the q/gate split of the fused projection |
| `gdn_l2norm`, `gdn_repeat_to_v`, `gdn_qkv_view`, `gdn_conv_*` | the recurrent layer's decisions |
| `state_slot` | one slot mechanism for both caches |

Measured: 1081 global symbols, 129 externs, 0 duplicates, and the binary carries 14 Apex exports
with both gates passing.

The build cost is 4.9 MiB of object for a 5.2 MiB binary - the unit is already a single
translation unit, so module boundaries cost nothing at run time. That is principle 5 holding in
practice, not by design document.

## The constraint that keeps this from becoming busywork

Per-architecture units are alternatives, not layers. Two arch builders in one unit collide -
`qwen35moe.goose` and `qwen3next.goose` both import `base/model_graph.goose`, whose `fail`/`ensure`
collide with the canonical ones. That is why `build-arch-unit.sh` takes exactly one architecture
and why the standalone currently carries Apex and only Apex.

So the rule for future architectures: one unit each, all sharing the common set, none sharing a
binary with another architecture unless the composition actually proves clean.

## What is not being done now

- No restructuring of the 120 common modules.
- No attempt to make one binary serve several architectures.
- No refactor for its own sake.

The current open work is unchanged: the standalone has Apex decisions but no CUDA, no tokenizer
engine, no HTTP and no full-stack execution; the working end-to-end Apex path is still the
integrated stage-run. That is what to finish next.
