# EXP-2026-11-19-1390: the encoder's table builds in seconds, and its first call hangs

## What moved

`gr_arr_str_table` already existed in Goose and does in one pass what the per-element accessor did quadratically. With
it, the table build went from "would take an hour" to seconds:

```
FG_TK_BPE_ENTRY build=1 port=1 n_tok=248320 n_mrg=247587
FG_TK_BPE_LOAD  ok n_tok=248320 n_mrg=247587 tcap=524288 mcap=524288
```

Both arrays are laid out as NUL-terminated text in one blob, which is exactly the slice `bt_bpe` indexes, and
`bt_build` fills the slot tables over it.

## The linkage defect, and the gate that named it

The call was declared inside the function body as a plain `extern` - C++ linkage - while the loader defines it
`extern "C"`. The library linked anyway, because a shared object may carry an unresolved symbol, and the failure would
have appeared at the first call. The gate caught it by name before any run:

```
check-unresolved-symbols: FAIL, 1 symbol(s) no linked library provides:
  _Z13fg_bpe_encodePKclPll
```

Moving the declaration to file scope with `extern "C"` fixed it; the gate is green again.

## What is broken now

With the comparison wired into `llama_tokenize` behind `FG_TK_BPE_DIAG`, the **first** tokenisation on the shipped
path does not return: a vocabulary-only harness with four short strings times out at 150 seconds, after printing the
table lines and the GGUF checks and nothing else.

So the port is accepted (the gate is green, and `bt_bpe` answered 227 ids correctly from the standalone CLI), but the
call from the library with these tables does not terminate. The difference between the working CLI and this call is in
the **table construction**, and the two are worth comparing directly: the CLI fills the offsets from the file's own
blob, while this path fills them from a copied, NUL-terminated blob and passes that copy as `gguf`.

The most likely candidate is that the slot tables reference offsets in the copied blob while some part of the encoder
still expects the file's blob - a mismatch that would not fail loudly but could make a merge lookup never resolve.

## Shipped path

Untouched: the comparison runs only when the flag is set, and with it unset an acceptance run gives

```
decode 62.52 / 63.13 / 62.96   prefill 2689.4 / 2945.4 / 2948.0   hash c2ff42cc1219
```

which is the baseline within its spread.

## Where the hang actually is, narrowed twice

The copy-blob hypothesis was tested first and refuted: the tables now come from a new door,
`gr_arr_str_offsets`, which writes offsets and lengths **into the file's own window** in one pass - no copy - and the
first call still does not return.

Then the call was bracketed with prints before and after it, and **neither printed**:

```
FG_TK_BPE_ENTRY build=1 port=1 n_tok=248320 n_mrg=247587
FG_TK_BPE_LOAD  ok n_tok=248320 n_mrg=247587
FG_GGUF_VALUES  integer=26/26
FG_GGUF_STRINGS keys=13/13
          <- the process ends here, 150 s
```

So `fg_bpe_encode` is never entered. The hang is **inside `fg_gguf_check`'s later sections**, after the table build and
after the values and strings checks. Without the flag the same harness tokenises all four cases in a second, so the
native path is fine and nothing in the shipped runtime is involved.

That leaves one suspicion with a clear test: the check's array section compares every element of every string array, and
with the vocabulary that is 248 320 elements through the per-element door - the same quadratic walk that took an hour
before, now hit by the check itself because the flag enables it. The next step is to run the check with the table build
disabled and see whether it completes: if it does, the table build leaves the port's state in a form that makes the
accessor slower, or the check's own array comparison is the cost and it has simply never been run with a 248k-element
array before.

## Next

1. build the same tables the way the CLI does - offsets into the file's own window - and see whether the call returns;
2. if it does, the copy is the problem and the fix is to point the port at `fg_port.data` with the offsets from
   `gr_arr_str_table`'s relation to it, or to pass the copy consistently everywhere;
3. only then the shadow comparison produces numbers, and only after that is serving discussed.
