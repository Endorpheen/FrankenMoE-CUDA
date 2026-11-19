# A loadable copy of the EXP1476 checkpoint

`builds/checkpoint-20261119-EXP1476` is 23 files and does not load: `libggml-cpu.so*` requires
`libmoe_mmid_tiled.so`, which the directory does not carry (see
`docs/goose-migration/BASELINE-DEPENDS-ON-UNTRACKED-ARTEFACT.md`).

This directory is that checkpoint **plus the one missing library**, copied out of the published release set
so no file comes from an experiment build directory. Nothing in the original checkpoint was modified or
deleted - this is a separate, derived set, which is why it does not violate rule 22.

## Verified

- `tests/regression/set-self-contained`: **ok**, every non-system dependency is present;
- `LD_LIBRARY_PATH=$PWD ./llama-server --version`: **exit 0**, `version 0.7.0-dev (build 2609, commit
  b4a43343)`, `built with GNU 15.2.0`, Goose witness lines on start;
- `libllama.so.0` here is byte-identical to the checkpoint's (`0ffd98c2`), so this is the accepted artefact,
  not a rebuild.

## What it is not

It is not a new accepted baseline. It is the accepted baseline made runnable, so that whoever measures next
is measuring the accepted artefact rather than discovering halfway through that it cannot start.
