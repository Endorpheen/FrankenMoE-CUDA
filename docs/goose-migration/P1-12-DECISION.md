# P1-12 — CPU MoE closure decision note

Status: **owner decision required**. The orchestration is ported; the native
compute inside the CPU MoE dot products cannot move to Goose as it stands.

## What is ported

The CPU MoE orchestration around the kernels is Goose across accepted slices:

- expert routing bookkeeping and recipe matching (`ar_` in
  `goose/model/arch-registry.goose`);
- expert LRU, residency, eviction and the cache budget (`xl_` in
  `goose/model/expert-lru.goose`);
- dense placement, range complement and row tables (`dw_`);
- row-stream slab planning, generation guard and materialise bound (`sp_`);
- router hook rules, call-order plans, overlap waves and row-source selection
  (`rh_`, `ov_`, `rs_`);
- meta backend, registry identity and threading/config acceptance
  (`goose/cpu/goose-meta-backend.goose`);
- the optimizer decisions (`goose/cpu/goose-opt.goose`).

## What cannot move

The dot products themselves (`ggml_vec_dot_*` / the MoE mat-mul kernels) are
SIMD kernels: they rely on intrinsics, lane-level shuffles and architecture
dispatch. Goose has no SIMD intrinsic surface, so the kernel body stays native in
the same sense as libc, libm or a GPU driver: an external primitive the Goose
code calls.

## Options

A. **Reclassify the SIMD dot products as substrate.** Close P1-12 with the
   orchestration ported and the kernels recorded as an unavoidable external
   primitive (same category as libc/libm). Requires Igor's explicit approval,
   because it changes the requirement's criterion rather than meeting it.

B. **Keep the criterion and wait for a substrate change.** Leave P1-12 open until
   Goose gains a SIMD surface or a portable vector primitive, then port the
   kernels and close it as written.

C. **Partial closure.** Close only the orchestration part and keep the kernel
   part as a separate open item. This is honest but changes the requirement's
   accounting.

## Recommendation

Option B is the default: do not weaken a correctness/coverage criterion without
Igor's decision. Option A is defensible if the project treats SIMD kernels as
substrate everywhere else (they already are for GPU work); it needs one explicit
sentence from Igor.

No code change is proposed by this note.
