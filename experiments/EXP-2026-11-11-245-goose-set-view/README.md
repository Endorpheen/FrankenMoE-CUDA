# EXP245: SET destination view in Goose — gated candidate, deliberately NOT integrated

The view computation from `ggml_cuda_op_set` (`set.cu:25-42`), the last of the P3-03 items that had no Goose
implementation.

## What it preserves

    dst_view.data  = (char *) dst->data + offset;        // offset is in BYTES
    dst_view.ne[]  = src1->ne[];                          // shape from src1
    dst_view.nb[0] = ggml_element_size(dst);              // destination element size
    dst_view.nb[1..3] = nb1..nb3 from op_params;          // strides from op_params

SET writes `src1` into a **window** of `dst`: `offset` bytes in, shaped by `src1` and strided by `op_params`.
Two things are easy to get wrong and neither throws:

- `nb[0]` comes from the **destination** element size, not from `src1`'s packed stride. For a same-type
  operation these agree, which is exactly why the difference survives casual testing;
- the shape comes from `src1` while the strides come from `op_params`. Taking the shape from `dst` copies the
  whole tensor instead of the sub-range, and the row count decides how much is overwritten.

## Gate

    220044 checks, 0 failures

4 named cases plus 20000 random with `elem_size`, extents and `offset` varied independently. Named cases pin
`offset == 0` (must not shift) and `nb[0] != nb1` (the element-size case).

| mutation | failures |
|---|---:|
| `nb[0]` taken from `nb1` instead of the destination element size | 19717 |

## Why it is not integrated

`SET` is **not executed by the fixed 32K/128 Apex profile** — this is recorded in the plan (P3-03) and in the
census. Integrating it would put an unexercised code path into the shipped DSO, where a zero witness cannot
distinguish "correct and unused" from "broken and unused". The shipped DSO carries eight verified routes and
adding a ninth that the profile never reaches buys nothing and costs a full rebuild plus a risk of the exact
failure mode that EXP240 documents: a linked route that silently never fires.

Same reasoning as leaving `SET`/`PAD`/`ROLL` at low priority in the plan. The implementation and its gate exist
so that a profile which *does* use SET can adopt it immediately:

- module `source/sv.goose`, bridge `source/bridge.c`, prefix `fg245v_`;
- replace the four assignments that build `dst_view`, keeping `ggml_cuda_cpy` calls native;
- gate before linking, and confirm a non-zero witness before claiming the port.
