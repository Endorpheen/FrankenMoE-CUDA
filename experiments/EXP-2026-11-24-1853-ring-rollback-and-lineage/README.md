# EXP-2026-11-24-1853 — Ring variant rolled back; the rebuilt lineage is self-consistent and ready for kernel work

Status: **ROLLBACK + PRECONDITION ESTABLISHED.** Accepted baseline `37ac0f1d` untouched.

## The N-plane ring

Implemented as directed in a variant built only from the preserved archive
(`/tmp/opencode/apex-win/lib.a`) and the preserved image header:

- `prefill_planes` field, read from `GOOSE_CUDA_PREFILL_PLANES` (default 2, clamped 1-4);
- `next_slab = 1 - slab` replaced by `(slab + 1) % planes`;
- the start-up line reports the real depth.

The variant is broken: with `GOOSE_CUDA_PREFILL_PLANES=3` it prints "1 buffers", the
first completion returns HTTP 500, and the fill counters (`72 508` over the partial run)
do not correspond to a working pipeline. Cause: the depth is selected inside
`expert_prefill_layout()` after `prefill_double` is consulted, and the two-plane
assumption is also baked into `slab_bytes`/`prefill_workspace` arithmetic elsewhere, so
a partially converted ring loses the second plane instead of gaining a third.

**Rolled back.** Nothing from this variant is used; the accepted binary and its
configuration are unchanged.

## What this establishes for the next step

Rule 40 forbids substituting the accepted components on a rebuild, but it does not
forbid kernel work: a kernel change needs its own lineage, and that lineage is usable
exactly when it is self-consistent. Measured here (rebuilt image, accepted driver
sources otherwise unchanged, `runs/lineage-100116`):

| request | prefill tok/s | decode tok/s | ids | text |
| --- | ---: | ---: | --- | --- |
| 1 (cold) | 2024.1 | 67.803 | `d05522291b49` | `6cfb451273c4` |
| 2 | 2181.7 | 71.081 | `d05522291b49` | `6cfb451273c4` |
| 3 | 2174.5 | 71.465 | `d05522291b49` | `6cfb451273c4` |

Three consecutive requests give identical token ids and identical text, so the rebuilt
lineage is deterministic and can serve as the **baseline of its own lineage** for kernel
experiments. Its absolute numbers differ from the accepted `37ac0f1d` (3098-3111
prefill, text `f8664331bd30`) because its kernels are not the accepted ones; that
difference is a property of the lineage and is recorded, not conflated.

The comparison rule inside the kernel lineage is therefore: same image, same archive,
same flags, only the kernel under study changed; gate on identical ids and text
*within the lineage* plus prefill, decode, VRAM, VmHWM and swap.

## Next kernel experiment

From the registry, with measured sizes on this hardware:

- `goose_flash_causal_mma` is the largest single kernel (479 ms of 3.7 s) and carries a
  264-byte local frame with 158 `LDL` and 172 `STL`, at 255 registers;
- the registry's EXP-1609 result for the same entry is that inlining the body *and* the
  wrapper reduces the causal local stack from 64 to 56 bytes, at the cost of bloating
  the Stream-K entry from 168 to 4032 bytes — and Stream-K is disabled by default
  (`GOOSE_FLASH_STREAM_K=0`), so that cost lands on an unused path.

The experiment: apply that inlining to the `flash-mma` fragment, rebuild the image,
verify the causal entry's stack and local-access counts, then A/B the causal path within
the lineage against `d05522291b49` / `6cfb451273c4` with prefill, decode, VRAM, VmHWM
and swap, accepting only a gain outside the lineage's noise band.

## Artefacts

- `runs/ring3-100017` — the broken N-plane ring arm (500, "1 buffers").
- `runs/lineage-100116` — the three self-consistency requests of the rebuilt lineage.
- `/tmp/opencode/var-ring3` — the ring variant build (`9c828543…`), not used.
- `/tmp/opencode/arm-img-new/frankenmoe-infer` — the rebuilt-lineage binary used above.
