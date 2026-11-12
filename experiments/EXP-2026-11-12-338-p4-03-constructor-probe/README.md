# EXP338: constructor/view differential probe -- one live divergence found

A probe exercising the public surface identically on both lines: type sizes and block sizes, 1-D to 4-D constructors, view_1d/2d/3d,
permute, transpose, reshape_2d/4d, row_size and element_size, printing every `ne`, `nb`, `nbytes`, contiguity, view flag and op.

## Result

The four constructors and the type/row/size queries match flint exactly. Then the probe aborts on our line:

    ggml_view_3d(ctx, q4_K_tensor[256,8,3], 128, 4, 2, nb[1], nb[2], 8)

flint accepts it and reports `ne=[128,4,2,1] nb=[144,144,1152,2304] nbytes=1656 contig=0`. Ours dies with

    Goose tensor constructor validation failed: code=15

inside `fg156_gg_function_17` called from `ggml_view_3d`. So the Goose constructor validation rejects a case the native code accepts --
a real divergence in the surface P4-03 asks about, not a documentation gap.

Before the abort, every constructor line matched; the missing lines are exactly the views/permute/transpose/reshape and the two size
queries that come after it, so nothing beyond that point is evidence either way yet.

## Next step

Read the check that raises code 15 in the EXP156 unit and compare it against native `ggml_view_3d`'s preconditions for a quantized
source with these strides. The probe is kept as the reproducer and must run to completion, on both lines, before anything else in this
experiment is claimed.
