# EXP180 source is reproducible again

## The problem this closes

EXP180's committed `source/device.goose` did **not parse with any compiler in the cache**
(stage3-871aec9, stage3-f849, subset-84f3, subset-73da all failed with
`expected identifier` on `iqs<96{` once a `>>` appeared later in the same body). Its committed
`device-generated.c` therefore could not have been produced from the committed source, and the
recorded numbers were not reproducible from the repository.

## Verified fix

On `subset-acc916a43628499a` and `subset-5b3d9385bacf2eca` the unchanged source parses:

    compiler subset-5b3d9385bacf2eca
    source   experiments/EXP-2026-11-08-180-goose-cuda-mmq-q8-1/source/device.goose
    result   emits 2092 lines, rc=0

The minimal case is recorded in the EXP208 README: `if (iqs&(15 as i64))==0&&iqs<96{...}` followed
by any `>>` in the same function.

## Byte comparison with the committed artifact

Not byte-identical, and the difference is explained rather than hand-waved. Both emit 9
`gg_function_*` bodies with the same names; **5 bodies differ, and every differing line is one of
these two wrappers**:

    +#ifndef __cplusplus
    +  extern void    (fg180q_fail)(int64_t);
    +#endif
    +#ifndef __cplusplus
    +  extern int64_t (fg180q_ld_i32)(uint64_t, uint64_t);
    +#endif

That is the newer emitter declaring extern symbols inside a C++ guard, which the older build did not
do. No arithmetic, no control flow and no extern call site differs. The remaining difference is the
compiler runtime preamble (`GG_ALIGN1` / `GG_UNALIGNED` / typed typedefs), which the current build
emits and the older one did not.

So: the committed artifact is consistent with the committed source, modulo emitter improvements.

## What this does not do

It does not rehabilitate EXP180's verdict. That experiment is still **not promoted**: byte-exact but
+11.4% kernel time, opt-in. This note only re-establishes that its source and artifact belong
together.
