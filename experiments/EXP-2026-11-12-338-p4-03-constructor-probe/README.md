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

## Error paths: the decisions match in six of seven cases, the text never does, and one decision diverges

`probe-errors.c` takes a case number and drives one failing call; each case is run once per line and the abort line is compared.

| case | flint | ours | decision |
|---|---|---|---|
| 1 invalid type | `GGML_ASSERT(type >= 0 && type < GGML_TYPE_COUNT)` at ggml.c:1785 | `validation failed:code=10` | same |
| 2 negative ne | `GGML_ASSERT(GGML_TENSOR_SIZE <= SIZE_MAX - obj_alloc_size)` at :1822 | `code=13` | same |
| 3 overflow | `GGML_ASSERT(INT64_MAX / ne[i] > current_nelements)` at :1798 | `code=14` | same |
| 4 view past end | `GGML_ASSERT(... data_size + view_offs <= ggml_nbytes(view_src))` at :1808 | `code=16` | same |
| 5 reshape mismatch | `GGML_ASSERT(ggml_nelements(a) == ne0*ne1)` at :3751 | `code=21` | same |
| 6 duplicate permute axis | `GGML_ASSERT(axis0 != axis1)` at :3911 | `code=31` | same |
| 7 non-block-aligned view_3d | **no abort** | `code=15` | **differs** |

Two findings, and the first is broader than the case that started this:

1. **Every constructor error message differs in text.** The Goose unit reports a numeric code from one helper; native reports
   `GGML_ASSERT(<expression>) failed` with the source location. The *decision* agrees in cases 1-6, but any consumer that greps the
   abort text -- a test suite, a wrapper, a log parser -- sees a different string. This is not ABI, but it is behaviour, and the
   migration rule says ports reproduce native semantics including ugly details. It is recorded as an open compatibility gap, not
   absorbed into "errors handled".
2. Case 7 is a genuine decision divergence, already root-caused to the extra `n0 % blck` check with a prepared patch.
