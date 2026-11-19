# EXP-2026-11-19-1397: the encoder swept over the whole vocabulary - 248 320 texts, zero disagreements

## The sweep

Each token's own text is put through `llama_tokenize`, once per token, with the ported encoder serving and the
comparison counting inside the library. One vocabulary load instead of a model load, so the whole thing costs minutes:

```
sweep: 248320 token texts tokenised of 248320
FG_TK_BPE_DIFF          (none)
FG_TK_BPE_SERVED        15520 lines printed (every 16th call)
```

## Two false disagreements first, and they were mine

The first run reported exactly two, both `native=0 goose=1`, on texts of 13 and 10 characters. They were **not** encoder
divergences: the comparison stripped a leading BOS and a trailing EOS **unconditionally**, while the native tokenizer
only adds those when `add_special` is true. A text that itself begins with the BOS token's own text therefore looked like
"native produced nothing".

The sweep also could not see it - the harness asked the native call for its count, and the count was right; the wrong
number was the one the comparison computed. Conditions on `add_special`:

```
sweep: 248320 token texts tokenised of 248320
FG_TK_BPE_DIFF          (none)      <- two before, zero now
```

This is the fourth instrument error of the same family in this report, and the same lesson: when two sides disagree,
check what each side was actually asked before believing either.

## What it establishes

Over the entire vocabulary - every token's own text, ASCII and multi-byte alike - the ported encoder and the native one
produce the same ids, and the port is serving every one of those calls. That is the "full corpus" item P7-06 lists,
restricted to the vocabulary's own texts rather than to arbitrary text.

## What it does not cover

Arbitrary text beyond the vocabulary's own pieces (the 73 template-driven calls of EXP-1393 and the 32 000-token text
prompt of EXP-1395 cover some of that), and the other pre-tokenizer families, which the guard keeps native.

## Shipped path

Acceptance after the fix: decode 62.95 / 63.07 / 63.41, prefill 2690.8 / 2945.7 / 2940.8, hash `c2ff42cc1219`.
