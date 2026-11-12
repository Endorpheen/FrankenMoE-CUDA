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

## Root cause: an extra block-alignment check that native does not have

The validation code identifies it. In `constructors.goose`, inside `tensor()`:

    let n0=dim(dims,0); ensure(n0%bs==0, 15);  var bytes = typesize*ne0/blck

Native `ggml_new_tensor_impl` performs the same division but has **no such requirement** -- it accepts `ne0 = 128` for Q4_K and simply
divides. So the Goose unit adds a precondition that native does not enforce, and rejects a call the native code accepts.

Confirmed by A/B on the same call: with `ne0 = 256` (block-aligned) our line accepts and prints its counters; with `ne0 = 128` it aborts
with code 15. The block size for Q4_K is 256, which is exactly the boundary.

## Fix prepared, not applied

`fix-n0-block-alignment.patch` removes the check and keeps code 15 unused rather than renumbering. It is **not** applied because this
module is linked into `libggml-base`, and the deployed base cannot be reproduced from source (P6-02). Relinking it would replace the
artifact the Apex baseline is measured against -- the same trap recorded there -- so the patch waits for that blocker to clear.

That is a deliberate boundary, not an omission: the divergence is understood, the one-line fix exists, and the reason it is not in the
build is written down in the same place as the blocker.

## The rest of the surface matches, once the known defect is avoided

Two further probes close the gap between "one divergence found" and "the surface is understood":

`probe-aligned.c` -- the original probe with `ne0 = 256` instead of 128 for the Q4_K `view_3d`, so the extra alignment check is not
tripped. All fifteen lines match flint exactly: every constructor, view_1d/2d/3d, permute, transpose, reshape_2d/4d, row_size,
element_size.

`probe-ops.c` -- names and get_name, `cont`, `dup`, `set_zero`, `scale`, `add`, `mul_mat`, a 2-D view's `view_src` identity and
`view_offs`, graph construction with two expanded nodes, `graph_n_nodes`, `graph_dump_dot`, and `used_mem`. All fourteen lines match.

So on this surface the only divergence found is the extra block-alignment check, whose root cause and one-line fix are recorded above.
Everything else in the probe set is bit-identical between the lines.
