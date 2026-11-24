# EXP-2026-11-24-1848 — The accepted image is not reproducible: a rebuild is 30 % slower and changes the text

Status: **BLOCKER FOUND, no gain claimed.** Baseline `37ac0f1d` unchanged.

## What was done

To attribute the one-vector RMSNorm kernel (EXP-1847) instead of mixing it with build
drift, the image was rebuilt twice from the same tree — once with the kernel's source
change reverted (`/tmp/opencode/img-drift`) and once with it (`/tmp/opencode/img-one2`)
— and each image was linked into the same runtime, then measured against the accepted
binary at the acceptance request (warm-up plus measured each arm,
`GOOSE_DECODE_SPLIT=4/COUNT=1`).

## The three arms

| arm | prefill tok/s | decode tok/s | text | tokens |
| --- | ---: | ---: | --- | ---: |
| accepted `37ac0f1d` | **3100.7** | **78.176** | `f8664331bd30` | 512 |
| rebuilt image, no source change | 2164.7 | 52.378 | `d45882943669` | 512 |
| rebuilt image + one-vector RMS | 2179.6 | 52.660 | `d45882943669` | 512 |

`runs/drift-091004` holds all three arms' requests, responses and server logs. VmHWM
and VRAM were not compared because the arms are not equivalent (see below).

## Why the rebuild is not equivalent

The rebuilt image is missing two kernels that the accepted image contains and that the
driver asks for by name:

```
removed: goose_rms_decode_2048, goose_moe_add_shared_residual
```

and the tree no longer contains their sources — `grep` over `goose/`, `integration/*.cl`
and the build script finds no definition, while `cuda-driver.c` still looks both up
(`cuda.function_get(..., "goose_moe_add_shared_residual")`,
`cuda.function_get(..., "goose_rms_decode_2048")`) and the routed-FFN path uses the
former as a fused tail when it exists.

With those two absent, the runtime takes its fallback paths, which is consistent with
the measured −30 % prefill, −33 % decode **and** with the changed text: the fused
residual and the specialised decode normalization round differently from the fallbacks.
The remaining 28 entries also differ, but they cannot be separated from this while the
two kernels are missing.

## Consequence

- The one-vector RMSNorm kernel cannot be attributed on a rebuilt base: the drift is 30
  times larger than any plausible gain from that kernel, and the base itself is
  numerically different.
- The larger finding is a **reproducibility defect of the accepted checkpoint**: the
  image inside `37ac0f1d` cannot be produced from the current tree, because two of its
  kernels have no source there. That is the situation rule 33 and rule 22 exist to
  prevent, and it blocks every kernel-level experiment on this base.

## Next step

Recover, in this order:

1. the sources of `goose_moe_add_shared_residual` and `goose_rms_decode_2048` (they are
   in the accepted image, so a preserved build tree or an earlier revision must have
   them), or the exact build inputs recorded for `37ac0f1d`;
2. rebuild with **no** source change and pair it against `37ac0f1d`; the arms must match
   3100.7 / 78.176 and `f8664331bd30` before any kernel work is measured;
3. only then add the one-vector RMSNorm kernel and A/B it, with the PTX/ptxas checks
   from EXP-1847 as its gate.

Until step 2 passes, no kernel change in this tree can be measured honestly.

## Artefacts

- `runs/drift-091004` — the three arms (requests, responses, logs).
- `/tmp/opencode/img-drift`, `/tmp/opencode/img-one2` — both rebuilt images and their
  build logs.
- `/tmp/opencode/arm-drift/frankenmoe-infer`, `/tmp/opencode/arm-one/frankenmoe-infer`
  — the two linked binaries (`23add847…` and `4172da92…`).
