# EXP-2026-11-19-1375: the two encoder defects, fixed, and the streams now identical

## What the previous comparison found

EXP-2026-11-19-1374 measured 114 native ids against 118 from the port, first divergence at position 68. Bisecting by
prompt localized it to one line in ten:

```
<|im_start|>system
native [248045, 8678]        goose [27, 91, 316, 4747, 91, 29, 8678]
```

## Defect 1: no special-token pass

The native tokenizer matches special tokens **before** BPE when specials are parsed; the port went straight to `bt_bpe`
and split the marker into seven ordinary pieces. `bt_special_order` already existed and was unused - the encoder's other
half had been written and never called, which is the same shape as the sampler doors that were exported and unwired.

The CLI now scans the text with the order (longest text first), emits the matched id, and sends the segments between
matches to `bt_bpe` exactly as before.

## Defect 2: the CLI dropped a byte

With specials fixed the streams matched for 113 positions and differed by one at the end: the CLI removed a trailing
newline from stdin. A tokenizer that silently discards input bytes cannot be compared with `llama_tokenize`, and the
first comparison was misled by exactly this.

Both fixed, on the same input bytes:

```
encoder: identical, 114 ids
```

## What this is and is not

This is the **standalone** tokenizer: `frankenmoe-tokenize`, a program, not the shipped library. The runtime's encode
decision is still native, which is what P7-06's plan requires until the comparison holds - it does now, on this prompt
set, and that is the precondition for step 2 of the plan.

The comparison covers ten prompts as one text: ASCII prose, Python, Cyrillic, Japanese, a long rare word, doubled
spaces, a special marker, punctuation, numbers, and a trailing newline. It does not cover every special token in the
vocabulary, nor the `--no-bos` path, and the card says so rather than implying a full sweep.

## Reproduction

```
bash experiments/EXP-2026-11-19-1375-encoder-specials-fixed/verify.sh
```

One model load in the harness plus one in the CLI, about a minute each; prints `identical` and exits 0.
