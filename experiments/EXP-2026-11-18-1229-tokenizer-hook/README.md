# EXP-2026-11-18-1229: the tokenizer door is wired, and Apex never reaches it

`fg_tk_norm` is exported from the chain build, composed into the accumulated unit (1190 symbols, green
typecheck) and called from `llama_vocab::normalize` through a struct-typed dlsym. The counter does not move,
and the reason is the same class of finding as the speculative routes:

```
grep -n "void normalize" vendor/flint/src/llama-vocab.cpp     -> one definition, at 1149,
                                                                 inside llm_tokenizer_ugm_session
grep -n "normalize\|preprocess" in the tokenize() bodies      -> WPM (833) and UGM (1031) only
```

The **BPE** tokenizer (line 642) splits with `unicode_regex_split` and never normalizes. Apex-35B is a BPE
vocabulary, so the decision this door carries is not on its path at all - my hook sits in the UGM session's
method, confirmed by the mangled symbol in the object (`llm_tokenizer_ugm_session...fg_tk_norm_resolve`).

## What that means

- the ported normalization is correct on its own gate (11 cases, including the escaped-space defect it
  caught), and it applies to **WPM and UGM** vocabularies;
- verifying it live needs a model of one of those families, and this machine has Apex-35B and Edge0-120B
  only, both BPE;
- the door stays exported and the hook stays in place: it is ABI-correct, inert for BPE, and becomes live
  the moment a WPM/UGM model is loaded.

## The pattern, named for the fourth time

Every time I place instrumentation I have found the code correct and the path wrong: the grammar
diagnostic on every 200th call, the pre-filter on a declined shape, the server hook in the vendor copy,
the prefix hook in the non-taken accept overload, and now the normalization in a session the model does not
use.

The cheap defence that works, and that I should apply first every time:

```
strings <built .so> | grep <my own counter name>     # is it compiled in?
<run>                                                # does the counter move?
+ read which branch the model's type actually takes  # before writing the hook
```
