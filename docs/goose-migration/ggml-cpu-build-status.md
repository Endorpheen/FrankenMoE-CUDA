# Building the Goose `libggml-cpu` in this tree: state after the first attempt

Igor corrected my earlier position: building our own ggml libraries is part of the integration, not a
language defect. So this was attempted, not deferred.

## What works

| step | result |
|---|---|
| configure an isolated build of the flint tree with `-DGGML_CUDA=OFF -DBUILD_SHARED_LIBS=ON -DGGML_NATIVE=ON` | **ok** |
| build the `ggml-cpu` target | **ok**, `bin/libggml-cpu.so` produced |
| reproduce the EXP149 pipeline (`reproduce/build.py --pthread`) up to the object stages | **ok**: `control`, `backend-goose` and `backend-cpp` all exit 0 |

## The rename recipe was not lost

The `objcopy` failure was a path problem, not a missing artefact: `rename-0.txt` lives in
`franken-goose-live-20261107/variants/cpu-backend/build/`, while the pipeline looked inside the EXP149
directory. Copied into place, the whole chain runs.

## Reproduced end to end

| step | result |
|---|---|
| EXP149 `cpu-backend` and `pthread` variants | **link-cpu ok** |
| EXP150 (expert registry) on top | **link-llama ok**, `libggml-cpu.so.0.27.0`, 1 966 936 bytes |
| Goose symbol set against the inherited library | **326 / 326 — identical, no missing and no extra** |
| run with the rebuilt library in place | completion `e68df05b` twice, probabilities equal, restore exact |
| CPU_CONTROL witnesses | FG143 4, FG147 21, FG149 7, FG150 11 — all live |

The inherited library is kept as `libggml-cpu.so.0.27.0.inherited` next to the rebuilt one for rollback.

## What already exists

A Goose `libggml-cpu.so.0.27.0` built earlier is present in
`franken-goose-live-20261107/variants/cpu-backend/run/` (1 917 080 bytes, 256 Goose symbols), and the
library this candidate actually loads (from `stage-run`) carries **280** Goose symbols, including
FG143/FG147/FG149/FG150. So the CPU_CONTROL routes are already live from a Goose library; what is missing
is reproducible *rebuilding* of that library from source in this tree.

## Next step

Reconstruct the rename list from the two built libraries (compare exported names of `backend-goose` and
`backend-cpp` objects against the final library), rather than guessing. That is a mechanical diff, and it
restores a reproducible path without changing any semantics.
