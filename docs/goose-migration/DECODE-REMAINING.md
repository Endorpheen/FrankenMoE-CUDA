# The decode gap: what is closed, what is open

Base: `70f0bf04`. Written after the disassembly line of investigation stopped producing
results, so that the next attempt does not repeat it.

## Closed

| question | answer | evidence |
|---|---|---|
| Is the GPU expert cache active? | yes, 6778-7804 MiB resident | EXP-714, measured on our pid |
| Does more cache help? | yes to 4608 MiB, OOM at 5120 | EXP-715, four runs each |
| Is the CPU library the reference one? | yes, byte-identical `63ad4984` | hash comparison |
| Are the threads and CPU mask right? | yes, 8 threads, `0f0f`, 39 total | /proc and cmdline |
| Are the `GGML_GOOSE_*` values right? | yes, all 18 identical to `env-actual.json` | profile diff |
| Is it the base library? | fixed - was the wrong build, fusion now bound | EXP-713 |
| Can the reference `libllama` be swapped in? | no, ABI mismatch with this ggml | cudaMalloc 1124 MiB with 8.5 GiB free |
| Do the two `libllama` have different modules? | no, the same 240 names | symbol set diff |

## Open, and the honest reason each is open

**1. Why the same `libggml-cpu` reports 482400 `FG148_QUANT` calls in one process and zero in
another.** This is the central unanswered question.

What has been tried and did not resolve it:

- `nm -D` on every library: nothing references `fg148_row_q4_0` as an undefined symbol, in our
  tree **or** in the reference `libllama`. So the call is not a PLT import.
- `objdump` over the whole CPU library: `fg148_row_q4_0` has no `call` site, no address
  reference, no `.data.rel.ro` entry.
- String search: the name exists in `.rodata` twice, next to `ggml_vec_dot_q4_0_q8_0`, which
  looks like a kernel-selection table - but no instruction references either string address, so
  it is debug metadata, not a registry.
- The `*_unused_main` wrappers that call `gg_function_0`: 15 of them, none called by anything.

The conclusion that fits all of this: the binary is a build where the quantised route was
compiled in and **not wired**. The reference counters must therefore come from a *different
build of the same file name*, not from the same bytes - and every artefact that would prove it
is absent (`runtime/exp160/run/` is gone, the checkpoint copy is this same `63ad4984`).

**2. Where the missing 5-6 tok/s decode comes from.** Unexplained. Decode is 37.23 median at
4608 MiB; the reference is 41.01-41.94 at 3584 MiB.

## What I will not do

Keep guessing at the disassembly. Four hypotheses were tested and eliminated in this session;
each cost a build-free but time-consuming cycle, and none moved decode. The remaining work is a
**build**, not an inspection: generate the CPU library from the Goose sources in this repository
with the quantised, pool and expert call sites present.

That is the same conclusion `PERF-INVENTORY.md` reached from the other direction, and it is the
migration itself rather than a workaround for it.

## The build, concretely

`goose/cpu/goose-cpu-q4k-dot.goose`, `goose-cpu-q6k-dot.goose`, `goose-cpu-quant-batch.goose`,
`goose-cpu-pool`, `goose-moe-control` and `goose-quantized-moe` hold the Goose implementations.
The missing piece is the dispatcher that calls them - the file that in the reference build had
call sites and in this build has `*_unused_main` stubs.

An attempt to rebuild `exp160` failed on missing `runtime/exp159/build/*.o` objects, which are
needed by its link step but are not in the tree. That is a concrete, solvable blocker: the
objects are produced by the `exp159` build script, which is present.

## Current best configuration

4608 MiB cache, prefill 2280 median, decode 37.23 median, VRAM 7804 MiB, RAM 21.8 GiB, output
sha `9d90a010...` matching the reference. Recorded in the journal as the throughput configuration;
3584 MiB remains the economical one at 36.23 / 6784 MiB.
