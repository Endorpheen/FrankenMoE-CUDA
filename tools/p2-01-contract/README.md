# P2-01 contract probe

One C file, built against two library sets, raw bytes compared. Purpose: prove per-operation whether the ported Goose
arithmetic matches native, and keep proving it after any change.

## Run it

    ./build-and-compare.sh            # goose = staging set, native = flint build
    ./build-and-compare.sh <goose_dir> <native_dir>

Exit code 0 means the divergence set is exactly the recorded one. Exit code 2 means a new divergence appeared or a known one
stopped reproducing - both are signals worth looking at, not noise.

## Two things that make this work, both learned the hard way

1. `-Wl,--allow-shlib-undefined` is required at link time. The libraries resolve each other's symbols at run time through
   their own search path; the static linker does not follow it and fails on `fg160_*` even though those calls work fine in
   the real process (`FG160_SHARED` prints three times in the accepted run log). This is a linker-visibility issue, not a
   defect in the build.
2. The loaders ask for SONAME `.so.0` while the artefacts are versioned `.so.0.27.0`, so the script stages symlinks. The
   goose arm additionally preloads the first `libggml-base-*` variant that defines `fg160_fusion_enabled`, because the
   server gets that definition through its own dynamic loading.

## Why the default goose arm is staging, not the checkpoint

The four-file checkpoint (`runtime/checkpoints/CONTROL-OPT-20261112/artifact/`) is enough to **run** the accepted server,
but not to **link** this probe: the `fg160_*` definitions live in the `libggml-base-*` variants that only exist in the full
staging set (98 libraries, 1.6 GiB). Both facts are recorded in the control journal.

## Current result (2026-11-12)

26 of 37 cases bit-identical; the recorded divergences are exactly:

    silu_t1, silu_t8, silu_1000, clamp_t1, clamp_t8, scale_t1, scale_t8,
    norm_finite, softmax_finite, mul_mat_32x8x6, rope_16x2x4

Causes and the planned fixes are in `RESULT.md`; the classification feeding the plan is in
`docs/goose-migration/P2-01-INVENTORY.md`.
