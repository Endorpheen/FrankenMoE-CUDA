# EXP244: swap-minimising output reorder in Goose — integrated, gated, dormant

`llama_context`'s output reorder (`llama-context.cpp:2187-2210`), the selection sort that restores the user's
batch order and records the transpositions to replay on the logits rows.

## What it preserves

    for i in 0..n_outputs-1: j_min = argmin(out_ids[i..n-1])
                             if j_min != i: swap; output_swaps.push({i, j_min})
    fill(output_ids, -1); for i: output_ids[out_ids[i]] = i

Three reasons this is not "just a sort":

- **It is this sort.** Any correct sort yields the same final `out_ids`, but a different swap list — and the
  list is replayed verbatim on the logits rows. A stable or insertion sort would order the outputs correctly
  and swap the wrong rows.
- `j_min` starts at `i`, so an already-minimal position records **no** swap. Recording a no-op still works and
  doubles the replay.
- The inverse map is indexed by the **original batch index**, not by position.

Strictly-less comparison keeps the first minimum, which matters when ids repeat.

## Gate

    325375 checks, 0 failures

7 named cases (empty, single, already sorted, one swap, reversed, duplicates, all-equal) plus 20000 random
with duplicates on purpose.

| mutation | failures |
|---|---:|
| `<` → `<=` (takes the last minimum) | 46055 |
| `j_min != i` skip removed (records no-op swaps) | 40386 |
| inverse map built as `inv[t] = idx` instead of `inv[idx] = t` | 74217 |

## A fixture precondition I got wrong

The first gate run aborted with validation code 2501: the generator produced ids outside `inv_len`. Native
indexes `output_ids[out_ids[i]]` **without** a bound test, so such an id would write past the array; the module
traps instead of corrupting memory. The generator was changed to respect the same precondition the native code
silently assumes, rather than removing the guard.

## Dormant, with the reason from the code

    FG244S_OUTPUT_REORDER calls=0
    FG240Q/241U/242O      calls=146/146/292
    prefill 2500.05  decode 24.509
    tokens True  probabilities True  sha e68df05b

The block runs only when `!sorted_output && n_outputs > 1`. With `--spec-type none`, every ubatch in this
profile carries a **single** output row — prefill reports the last token, decode reports one token per step —
so the condition is false and the loop is never entered. Not a guess: the witness is 0 and the guard is an
explicit `n_outputs > 1` in the source.

Unlike EXP243, this route is genuinely on the live path and integrated into the shipped DSO — it is simply not
reached by this profile. A different invocation (multiple outputs per ubatch, or a recurrent model) would
reach it.

## Integration note

`llama-context.cpp` compiles to its own object (`src/CMakeFiles/llama.dir/llama-context.cpp.o`), not into a
unity TU, so the patched object substitutes directly in the link. I first linked it into the stage directory
and then deleted that directory while preparing the run — the DSO had to be relinked into `runtime/exp244/dso`
before anything could run. Cheap to avoid: write link outputs outside the directory that is about to be
rebuilt.
