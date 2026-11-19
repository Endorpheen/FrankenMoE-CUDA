# EXP-2026-11-19-1389: the table build is blocked by the per-element accessor, not by the plan

## The bug fixed on the way

The table was being built **before** `gr_parse` had filled the port's state, so `gr_find_kv` returned -1 and
`fg_bpe_init` was never called - silently, because the miss was behind an `if`. Moved after the parse:

```
FG_TK_BPE_ENTRY build=1 port=1 n_tok=248320 n_mrg=247587
```

Both doors resolve and the counts are right. That part is done and inert behind `FG_TK_BPE_DIAG`.

## What blocks it now, with a number

With the loop instrumented to print every 20 000 elements, **no progress line appeared in 180 seconds**. So one call costs
more than 9 ms, and the full table would take about an hour - which is why an earlier attempt "hung" instead of failing.

That is the accessor, not the plan: `fg_port_array_str` is called once per element and apparently walks the array to reach
element *i*, making 248 320 reads quadratic. The same shape would apply to the merges.

## The fix this defines

One call that fills the whole array in Goose:

```
bt_tok_offsets(gguf, key_index, off_out, len_out) -> n
```

walking the array once and writing every offset and length, then the host loop disappears. That is porting work in the
supplied direction - more of the reader in Goose, less host code - rather than a workaround, and it also removes half a
million C++ calls from a load path.

## What was not done

No bulk accessor was added in this iteration, and no claim is made that the encoder is closer to serving than the
measurements show. The table remains inert.

## State

| piece | state |
|---|---|
| init order | **fixed** - the table now builds when the flag is set |
| per-element accessor | **measured too slow**: >9 ms per call |
| bulk accessor in Goose | not written - the next work item |
| shadow comparison at `tokenize` | not written |
| shipped path | untouched |
