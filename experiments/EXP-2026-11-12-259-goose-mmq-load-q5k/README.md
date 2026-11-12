# EXP259: Q5_K MMA tile loader in Goose -- ported, gated, dormant on this profile

Third quantised loader of the MMQ family, after Q4_K (EXP255) and Q6_K (EXP257/258). Two of the three native loops
are on the MMA path; both are ported here.

## Native code (MMA branch)

`block_q5_K` = `{ ggml_half2 dm; uint8_t scales[12]; uint8_t qh[32]; uint8_t qs[128]; }` = 176 bytes:
`dm@0`, `scales@4`, `qh@16`, `qs@48`. `QI5_K = 32`, `QR5_K = 2` for `QK_K = 256`.

    // first loop
    ky  = QR5_K*txi
    ql  = get_int_b4(bxi->qs, txi)                       // qs + 4*txi
    qh  = get_int_b4(bxi->qh, txi % (QI5_K/4))
    qh0 = ((qh >> (2*(txi/(QI5_K/4)) + 0)) << 4) & 0x10101010
    qh1 = ((qh >> (2*(txi/(QI5_K/4)) + 1)) << 4) & 0x10101010
    kq0 = ky - ky % (QI5_K/2) + txi % (QI5_K/4)
    kq1 = kq0 + QI5_K/4
    x_qs[i*sram_stride + kq0] = ql0 | qh0 ; x_qs[... + kq1] = ql1 | qh1

    // second loop
    ksc  = threadIdx.x % 2
    sc32 = unpack_scales_q45_K(scales, ksc + 0) ; m32 = unpack_scales_q45_K(scales, ksc + 2)
    dm   = bxi->dm * make_half2(1.0f, -1.0f)
    for l in 0..3: x_dm[i*sram_stride + 4*ksc + l] = dm * make_half2(sc8[l], m8[l])

## Modules

`qs5.goose` carries the staging arithmetic; `dm5.goose` carries the ksc selection, the byte extraction and the row
addressing. The scale unpack and the half2 multiply stay primitives -- data and FP16 arithmetic, the same boundary
EXP255 used for Q4_K. Fix QI5_K/QR5_K arrive as parameters rather than being restated in the module.

Entry wrappers (`fg259q_run`, `fg259m_run`) are generated mechanically from the emitted source, so the caller does not
depend on emission order or on the trailing `base` argument.

## Gate

    FG259_AB mismatches=0 cuda=0 shmem=36864

Eight blocks, 32 threads, sentinel-filled `qs` (4096 ints) and `dm` (512 ints) staging compared against a verbatim
native arm. Zero differences.

## Integration and the result that matters

Integrated into the working flint copy exactly like EXP255/258; accepted profile:

    first_sha = e68df05b..., probs_equal, restore_ok, slot_file_size = 387989920

Then the liveness tests, which produced the finding:

| mutation | completion |
|---|---|
| `qs` high-bit mask 0x10101010 -> 0x10101011 | `e68df05b` (unchanged) |
| block size 176 -> 175 in both entries | `e68df05b` (unchanged) |

Two conclusions, recorded separately:

1. **The first mutation was unobservable on its own.** `qh0/qh1` are `(... << 4) & mask`, so bits 0-3 of every byte
   are already zero; changing mask bit 0 cannot alter a value. That mutation would have proven nothing even on a live
   path. A mutation must be checked for observability before it is used as evidence.
2. **The loader is not called on this profile.** A wrong block size changes every source address for both entries; if
   either entry ran, the completion could not stay at `e68df05b`. It does. This matches the model inventory:
   Apex-35B-V1.0-Q4_K_M carries Q4_K and Q6_K tensors only (`RUN_AUTHORIZATION.md`), so `Q5_K` never reaches
   `ggml_cuda_mmq_load_tiles_q5_K`.

So this item is **ported and gated, dormant by profile** -- the same category as the splitter branches in EXP354/355
and the penalty sampler in EXP380. It is coverage for other models and architectures, not an Apex route, and it is
not counted as live. The accepted profile is unaffected either way: the integration compiles the Goose body into an
instantiation no request reaches.

## Next

Q2_K and Q3_K loaders in the same header. Their type inventory question must be answered first the same way: only
port what can either be gated against native or witnessed on a profile that uses the type, and say which of the two a
module earned.
