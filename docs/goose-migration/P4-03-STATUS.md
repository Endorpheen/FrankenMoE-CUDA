# P4-03 status: verified surface, two open gaps, both behind the base-library blocker

## Verified

| check | result |
|---|---|
| public symbol parity | `libggml-base` ggml\_\* 550 flint vs 551 ours (one additive), `libllama` 255 vs 255, `libggml-cpu` 6 vs 6 |
| constructors and views | 15 probe lines identical: 1D-4D constructors, view_1d/2d/3d, permute, transpose, reshape_2d/4d, row_size, element_size |
| operations and graphs | 14 probe lines identical: names, cont, dup, set_zero, scale, add, mul_mat, view_src/view_offs identity, graph build, graph_n_nodes, graph_dump_dot, used_mem |
| error decisions | six of seven failing calls abort on both lines, for the same reason |

## Open gaps

1. **Extra block-alignment check.** `constructors.goose` requires `ne0 % blck == 0`; native does not. A `view_3d` with `ne0 = 128`
   over a Q4_K tensor aborts here and succeeds in flint. Root-caused, patch written (`fix-n0-block-alignment.patch`), not applied.
2. **Error text never matches.** The Goose unit emits `Goose tensor constructor validation failed:code=N`; native emits
   `GGML_ASSERT(<expression>) failed` with file and line. Same decisions, different strings, which matters to anything that parses the
   abort output.

## Why neither gap is fixed in this build

Both live in the constructors unit that is linked into `libggml-base`. That library's deployed bytes cannot be reproduced from source
(P6-02), and relinking it would replace the artifact the Apex baseline (`e68df05b`) is measured against. Fixing either gap therefore
requires resolving P6-02 first -- or accepting a new baseline, which is an owner decision.

This is the same trap recorded in `BLOCKER-20261112-sched-cuda-split.md`, and it is the reason P4-03 is parked rather than patched.

## What would close P4-03

Apply the prepared patch, make the validation report the native assert text (expression plus location), relink `libggml-base`, then
re-run all three probes plus the Apex profile to confirm `e68df05b` is unchanged. The first two steps are source work that can be done
now; only the relink waits.
