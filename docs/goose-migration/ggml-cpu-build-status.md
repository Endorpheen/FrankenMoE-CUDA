# Building the Goose `libggml-cpu` in this tree: state after the first attempt

Igor corrected my earlier position: building our own ggml libraries is part of the integration, not a
language defect. So this was attempted, not deferred.

## What works

| step | result |
|---|---|
| configure an isolated build of the flint tree with `-DGGML_CUDA=OFF -DBUILD_SHARED_LIBS=ON -DGGML_NATIVE=ON` | **ok** |
| build the `ggml-cpu` target | **ok**, `bin/libggml-cpu.so` produced |
| reproduce the EXP149 pipeline (`reproduce/build.py --pthread`) up to the object stages | **ok**: `control`, `backend-goose` and `backend-cpp` all exit 0 |

## Where it stops

The pipeline's next step renames symbols with `objcopy` using a recipe file, and that file is gone:

    objcopy: cannot open symbol rename file .../variants/pthread/build/rename-0.txt

So the recipe's rename list was not preserved — the same family as the lost EXP205 bridge. Without it the
objects are built but not yet combined into the library under the expected symbol names.

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
