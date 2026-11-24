# EXP-2026-11-24-1847 — One-vector RMSNorm kernel: built and verified, A/B blocked by image drift

Status: **KERNEL VERIFIED, A/B BLOCKED.** Baseline `37ac0f1d` unchanged.

## Why this kernel

The acceptance trace ranks the kernels that actually run (0 launches for
`goose_gdn_mma` and `goose_q4_attention_merge`, so those were dropped as targets):

| kernel | launches | ms |
| --- | ---: | ---: |
| goose_flash_causal_mma | 41 | 479.3 |
| goose_mmq_q4_dense_m128n256_pipeline | 724 | 321.2 |
| goose_gdn_mma_v32 | 124 | 317.1 |
| goose_mmq_q4_routed_m128 | 329 | 303.1 |
| goose_vector_flat4 | 1 235 | 235.4 |
| **goose_rms_reference** | **784** | **222.3** |

`goose_rms_reference` carries a 64-byte local frame with 4 `ld.local` and 20
`st.local` in PTX and 38-40 registers, that is plenty of register headroom — the case
where the registry's method applies.

## The cause, found in the source

`goose/cuda/goose-cuda-normalization/device.goose`:

```goose
var xs: F4[4] = default < F4[4] > ();
while k < vpt { ... xs[k] = load4(x, i); ... k += 1; }
...
k = 0;
while k < vpt { ... let v = xs[k]; ... }
```

`vpt` is a runtime value, so `xs[k]` is a dynamic index and the array goes to local
memory. But the launch site computes `vectors = vec ? (width <= 4096 ? 1 : 4) : 0`, and
the acceptance width is 4 096, so **`vpt` is 1 in every one of those 784 launches**: the
four-element array is only ever used at index zero.

## The change

A one-vector entry that keeps the loaded `F4` in a scalar: the same load, the same
block reduction, the same fma order, no staging array.

- `goose/cuda/goose-cuda-normalization/device.goose`: `fn rms_row_one(...)`;
- `goose/cuda/standalone-rms-reference.goose`: `export fn fg_reference_rms_one(...)`;
- `integration/goose-standalone/cuda-rms-reference.cl`: `__kernel goose_rms_reference_one`;
- `tools/build-goose-cuda-kernels.sh`: extract the new function into the fragment;
- `cuda-driver.c`: load `goose_rms_reference_one` and use it when `vectors == 1`.

## Verification of the kernel (done)

Built image (`tools/build-goose-cuda-kernels.sh`, exit 0). PTX of the new entry
contains no `.local` declaration and no local access, and ptxas 12.6 for `sm_89`
reports:

| kernel | stack frame | registers | PTX ld.local | PTX st.local |
| --- | ---: | ---: | ---: | ---: |
| `goose_rms_reference` (baseline entry) | 64 B | 40 | 4 | 20 |
| **`goose_rms_reference_one` (new)** | **0 B** | **36** | **0** | **0** |

That is the registry's frame-removal result reproduced on this toolchain, with fewer
registers rather than more, so no occupancy was traded away.

## The blocker

The rebuilt image **cannot be swapped in for the accepted one**:

```
entries old 30, new 29
added:   goose_rms_reference_one
removed: goose_rms_decode_2048, goose_moe_add_shared_residual
changed: 28 of 30 entries (all except the two that are new/added)
```

The accepted baseline's CUDA image was therefore produced from a different state of the
tree than the current sources, and replacing it would change 28 kernels at once, which
makes any measured difference unattributable — and would remove two kernels the runtime
asks for.

A hybrid was tried: splice only the new entry into the baseline image's PTX. The entry
is self-contained (zero `call.uni`), so this is the clean experiment. `ptxas` accepts
the spliced file (`rc=0`) but silently drops the appended entry — the compiled cubin has
no `goose_rms_reference_one` — so the hybrid does not load either.

## What is needed to finish

One of:

1. a reproducible image of the **accepted** baseline (the sources and flags that
   produced `37ac0f1d`'s image), so the single added kernel can be A/B'd on top of it;
2. or acceptance of a full image rebuild, with the 28-kernel delta measured as its own
   step first (rebuild with no source change, pair it against the baseline, and only
   then add the kernel).

Until one of those is decided, the kernel stays verified but unmeasured at runtime, and
no gain is claimed.

## Artefacts

- `/tmp/opencode/img-one2/` — the built fragments, PTX, and `goose-cuda-image.h`.
- `/tmp/opencode/mmq2.cubin`, `mmq2.sass` — cubin and disassembly used for the table.
- `runs/base32768-20261124-082000` — the trace that ranked the kernels.
