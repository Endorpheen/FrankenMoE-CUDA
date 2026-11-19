# EXP-2026-11-19-1391: the ported encoder agrees with the native one on the shipped call path

## The result

With `FG_TK_BPE_DIAG`, every `llama_tokenize` call compares the port's ids with the native ones:

```
case 0  "Hello world"                  native 2   port 2   equal
case 1  "Привет, мир"                  native 5   port 5   equal
case 2  "def f(x): return x**2"        native 8   port 8   equal
case 3  "<|im_start|>system"           native 2   port 2   equal
FG_TK_BPE_COMPARE calls=1 disagreements=0
FG_TK_BPE_DIFF          (none)
```

Four of four, including the case that failed in EXP-2026-11-19-1390.

## What it took, in order

1. **the special scan moved into Goose**: `bt_special_hits` walks the text once with the order `bt_special_order`
   produces and returns (start, end, id) triples. The host splices `bt_bpe` over the gaps - the same rule the standalone
   CLI had, now shared instead of duplicated;
2. **the attribute bits had to be derived first**: `bt_build` fills the slot tables and leaves `attr` zeroed, so the scan
   found no specials at all. The token types come from the reader (`tokenizer.ggml.token_type`, a plain int32 array) and
   `bt_attrs` turns them into the bits, so the rule stays in Goose;
3. the table itself builds in seconds through `gr_arr_str_offsets`, and the GGUF check no longer takes an hour.

## What this establishes, and what it does not

Establishes: on the shipped `llama_tokenize` path, for these four inputs, the port and the native tokenizer produce the
same ids - and the comparison is live, so any prompt the server tokenises is checked too while the flag is set.

Does not establish: a full-corpus comparison, the `--no-bos` path inside the library, or the guard that keeps the port
away from pre-types it does not implement (`bt_bpe` is hardcoded to the qwen35 segmenter; EXP-2026-11-19-1378). Those are
the remaining three items before serving can be discussed.

## Shipped path

Unchanged: everything is behind `FG_TK_BPE_DIAG`. With the flag unset the acceptance run is 62.52 / 63.13 / 62.96,
prefill 2689 / 2945 / 2948, hash `c2ff42cc1219`.

## Next

1. run the comparison through a server with chat templates, where specials and BOS/EOS boundary handling are exercised by
   the template rather than by a hand-written string;
2. add the pre-type guard at the call site so the port is consulted only for the vocabulary family it implements;
3. then, and only then, serving with the acceptance triple.
