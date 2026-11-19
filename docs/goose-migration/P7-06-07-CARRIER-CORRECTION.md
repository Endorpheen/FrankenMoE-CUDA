# Correction: the tokenizer port is wired, and my grep said otherwise

Written and withdrawn the same hour, 2026-11-20. The first version of this note claimed that the compiled
carrier `runtime/reconstructed/src/llama-vocab.cpp` references **zero** doors and that the shipped tokenizer
is therefore native. That was wrong, and the way it was wrong is the fifth instance of the same mistake
today.

## What I actually searched

`grep -c '\bfg_[a-z0-9_]+\b|bt_[a-z0-9_]+'` on the staged copy returned 0. I concluded "no door calls" and
wrote a page around it.

The staged copy reaches the doors through wrappers named **`llama_goose_*`** - `llama_goose_arr_data`,
`llama_goose_kv_type`, `llama_goose_ready` and 41 more. My pattern did not include them, so a tree full of
wiring read as empty.

## What the tree says

`runtime/reconstructed/check_carriers.py`, which is the authority for this question:

```
REFERENCE DRIFT 4 carrier(s) differ in which doors they call:
  llama-vocab.cpp: compiled copy references 16, the other 26; missing e.g.
  ['fg_all', 'fg_bad_token', 'fg_bpe_diag', 'fg_bpe_encode', 'fg_bpe_serve']
PASS the compiled copy of every carrier calls the same doors as the other copy
```

So the compiled copy calls **16** doors; the vendor copy calls 26. The checker classifies that difference as
expected for this file - the two copies reach the same decisions through different mechanisms - and it is
fatal only when the compiled copy references **none**, which is exactly the shape my wrong claim asserted.

## What P7-06 and P7-07 actually need

Not a carrier decision. The doors are called. What remains is what the records already said before this
note existed: the **acceptance run on the real vocabulary** for the shipped artefact, and then the removal
of the replaced native bodies under rule 15. The `remaining` fields have been restored to that.

## The mistake worth keeping

Five times today a search produced a finding that dissolved on a second look: the sampler, the JSON doors,
the library provenance, the extra objects, and now this. Every one had the same shape - a pattern that was
too narrow for the thing being looked for, reported as an absence. The durable rule is in `AGENTS.md` rule
36: a claim that something has no call site comes from a tool that searches the whole tree, not from a
remembered grep.
