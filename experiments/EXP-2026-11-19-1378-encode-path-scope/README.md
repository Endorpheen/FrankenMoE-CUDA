# EXP-2026-11-19-1378: the ported encoder implements one pre-tokenizer, and that sets the shape of the serving

## The finding

`bt_bpe` does not take a pre-type. It calls

```goose
let n_word: i64 = pt_qwen35_body(text, cpts, seg, enc, info);
```

one hardcoded segmenter. The native tokenizer has eighteen `LLAMA_VOCAB_PRE_TYPE_*` cases, each with its own regular
expression, and the segmentation decides every token boundary before any merge is looked up.

`tokenizer.ggml.pre = qwen35` in Apex maps to `LLAMA_VOCAB_PRE_TYPE_FLINT` (`llama-vocab.cpp:2670`), which is the case
the port implements - which is why the standalone comparison came out identical on 227 ids.

## What that means for serving, and what it does not

Serving `llama_vocab::tokenize` from the port is valid **only** for the pre-type the port implements. Doing it
unconditionally would give FrankenMoE a runtime that tokenises one model family correctly and every other family
silently wrong - the single-architecture shortcut rule 17 forbids, and the same mistake as an Apex-only graph path.

So the serving shape is:

```
if (pre_type is the one the port implements)  -> the port decides
otherwise                                     -> the native path, unchanged
```

That is a guard, not an opt-in: for the supported pre-type the port decides by default, and the native path remains for
the pre-types that were never ported - which is exactly the state of every other multi-architecture decision in this
migration.

## The next measurement this implies

The comparison has to be repeated **at the guard boundary**: the port must refuse, or not be consulted, for a
vocabulary of another pre-type, so that "identical on Apex" cannot be mistaken for "identical everywhere". There is no
second model on this machine, so the check is a fixture vocabulary with a different `tokenizer.ggml.pre` - which the
existing `make_vocab.py` fixture generator can produce.

## What this changes in the plan

Step 2 keeps its order - slots, shadow comparison, then serving - and gains one line: the call site must carry the
pre-type test, and the fixture check proves the test exists rather than assuming it.
