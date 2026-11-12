# Merge map: which patched copies are cumulative and which are forks

Needed before any merged build. Each experiment kept a full copy of the file it edited, so the copies form lineages. Counting the `fgNNN*`
bridge symbols in each copy shows which lineages are cumulative (each copy contains all earlier ones) and which are forks made from a stale
base.

Symbols are shown per copy; `X` means present.

## `server-context.cpp` -- four forks, no cumulative copy

    union: fg268p_ fg272s_ fg274b_ fg276t_
    269-goose-checkpoint-policy-live  X...
    273-goose-slot-selection-live     .X..
    275-goose-batch-compat-live       ..X.
    277-goose-stop-strings-live       ...X

Each copy carries exactly one bridge. There is no file that contains checkpoints, slot selection, batch compatibility and stop strings
together, so these four ports **have never coexisted in one source file**. Merging them is three manual integrations, not a file choice.

## `llama-batch.cpp` -- one cumulative chain, three forks, plus a separate file that does merge the chain

    union: fg237s_ fg238q_ fg239b_ fg240q_ fg241u_ fg242o_ fg243c_ fg348s_ fg351s_ fg357s_
    237  X.........
    238  XX........
    239  XXX.......
    240  XXXX......
    241  XXXXX.....
    242  XXXXXX....
    243  XXXXXXX...
    354  ........X.
    355  .......X..
    357  .........X

The 237-243 line is properly cumulative. The three `-live` experiments afterwards each went back to a base and added one bridge, so 354,
355 and 357 are siblings, not descendants. Meanwhile `EXP-2026-11-12-291-goose-batch-reject/llama-batch-patched.cpp` carries all seven of
237-243 plus `fg291b_`, `fg383s_` and `fg398s_` -- the only copy that merges the chain with later work, under a different filename.

## `llama-kv-cache.cpp` -- a nine-step cumulative chain plus one fork

    union: fg199k_ fg204x_ fg205g_ fg206d_ fg227n_ fg228e_ fg229i_ fg230s_ fg233c_ fg345w_
    199  X.........
    204  .X........
    205  .XX.......
    206  .XXX......
    207  .XXX......
    209  XXXX......
    227  XXXXX.....
    228  XXXXXX....
    229  XXXXXXX...
    230  XXXXXXXX..
    233  XXXXXXXXX.
    347  .........X

`EXP-233` is the cumulative tip of the nine-step chain; `EXP-347` adds the write bridge on its own, from a base that predates it. A merge
needs EXP-233 plus the 347 edit, not either file alone.

## The rest, briefly

`llama-memory-recurrent.cpp` (three copies, cumulative through `fg232r_`), `llama-moe-cache.cpp` (EXP-265 carries `fg262a_` and `fg263c_` but
has lost `fg150_` from the older registry copy), `llama-model-loader.cpp` (EXP-286 supersedes EXP-201 and adds `fg389s_`, `fg403s_`,
`fg404s_`), `llama-vocab.cpp` (EXP-220 supersedes 202 and adds `fg213p_`), and single-copy files: `llama-context.cpp` (`fg244s_`),
`llama-graph.cpp`/`llama-hparams.cpp` (`fg200g_`), `llama-sampler.cpp` (`fg212s_`), `server-queue.cpp` (`fg270q_`).

## What this means

The merge is not a relink and not a "pick the newest file" exercise. Three files need hand integration of forks (`server-context.cpp` with
four, `llama-batch.cpp` with three siblings, `llama-kv-cache.cpp` with one), one file needs its bridge restored (`llama-moe-cache.cpp` lost
`fg150_`), and the rest can be taken from their cumulative tips.

That is the real size of the step between "the patch exists and its own gate passes" and "the candidate contains it", and it explains cleanly
why the deployed stage stops around EXP244: after that point the copies stopped being cumulative and each experiment became its own branch.

## Merge progress that already exists

`runtime/reconstructed/build.py` is the assembly mechanism: it compiles the patched files in `runtime/reconstructed/src`, discovers which
`fgNNN*` modules the objects reference, builds each module from its experiment directory and links
`runtime/reconstructed/build/libllama-exp371.so.0.7.0`.

That library, built today at 06:17, contains `fg237s`, `fg238q`, `fg239b`, `fg240q`, `fg241u`, `fg242o`, `fg243c`, `fg244s`, `fg383s`,
`fg398s`, `fg200g`, `fg212s`, `fg213p` and `fg202t` -- that is, the batch chain, the context reorder, the batch-container plan, the graph,
sampler and vocab work, all merged into one DSO. Measured absent from it: `fg291b` (batch reject) and `fg286m` (lazy/mmap).

So the merge is further along than the stage suggests: **`libllama` is merged through about EXP398**, while `libggml-base`, `libggml-cpu` and
`libllama-common` are still the older libraries, and the server-side ports (`fg268p`, `fg270q`, `fg272s`, `fg274b`, `fg276t`) exist only as
patched copies.

## What a merged batch file needs, measured

Compiling `EXP-291/llama-batch-patched.cpp` with the reconstructed build flags succeeds (143,776-byte object, no warnings) and references
ten modules: `fg237s fg238q fg239b fg240q fg241u fg242o fg243c fg291b fg383s fg398s`.

Module sources were located for eight of them (`sp-`, `sq-`, `cl-`, `ss4-`, `uc-`, `oi-`, `cp-generated.c` and `batch_reject-functions.c`).
`fg383s` and `fg398s` have no source file in the experiment tree, only prebuilt objects `b383.o` and `b398.o` in
`runtime/reconstructed/build` -- so those two modules are currently available as binaries only, which is a provenance gap to close before the
next full rebuild.

## The shortest correct path from here

1. Add `fg291b` and `fg286m` to `runtime/reconstructed/src` and rebuild, linking `b383.o`/`b398.o` as build.py already does.
2. Rebuild `libggml-base`/`libggml-cpu` from the staged patch set (EXP248-265) and relink a candidate.
3. Hand-integrate the four `server-context.cpp` forks and the `server-queue.cpp` patch.
4. Only then re-run the Apex profile, with Igor's agreement, to see whether `e68df05b` still holds.
