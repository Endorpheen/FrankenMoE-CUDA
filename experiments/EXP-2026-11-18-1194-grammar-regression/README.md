# EXP-2026-11-18-1194: grammar regression in the working build, isolated but not yet explained

## Reproduction

Same binary (`stage-run/llama-server`), same model, same grammar file, only the library differs.

```
LD_LIBRARY_PATH=builds/flint-acc/bin   -> 500 "Unexpected empty grammar stack after accepting piece: $ (3)"
LD_LIBRARY_PATH=<reference stage-run>  -> '{"name":"piano","n":4,"name":"piano","n":4}'
```

## What is already excluded

1. **Not the grammar file.** A simpler grammar that cannot match empty
   (`root ::= "{" pair ("," pair)* "}"` with fixed keys) fails identically, and the reference
   library accepts it.
2. **Not the Goose decision.** The failure is identical with `GGML_GOOSE_GRAMMAR=1` and with
   `GGML_GOOSE_GRAMMAR=0`, in the same message and at the same point, so the hook's answer is not
   involved. That also means the equivalence observed earlier holds on this path too - it fails
   the same way on both sides.
3. **Not the harness.** The request is one completion with a fixed seed; the error is the
   server's own exception, returned as HTTP 500.

## What is left

The working build `builds/flint-acc` differs from the reference in two ways that touch grammar:
my edits in `vendor/flint/src/llama-grammar.cpp` (the pre-filter block, the pointer-to-index map
for the stacks, the diagnostic print) and the Goose chain being linked into the same library.

The pre-filter and the diagnostic are both gated - the first by `fg_grammar_first_set_call` being
non-null and `fg_grammar_enabled()`, the second by an environment variable - so neither should run
with the hook off. "Should not" is exactly what has to be tested rather than asserted.

## Next step, written before it is done

Bisect by building the same tree with `llama-grammar.cpp` from the previous commit and with the
Goose chain removed from the CMake target, one variable at a time, and running this same request.
The failure is deterministic (identical message every run), so a single run per variant decides
it. Until then the grammar stage stays opt-in and this is a known regression in the working
build, not in the accepted baseline - the reference library still produces valid constrained
output.

## Found by bisecting, and the answer is worse than the symptom

Bisect, one variable at a time, same request, same binary:

```
ab15fbbd  llama-grammar.cpp   -> works
96605a58  (cache removal)     -> works
HEAD with GGML_GOOSE_GRAMMAR=0 -> works
HEAD with the hook enabled     -> fails, and also corrupts output
```

So the regression is **the grammar hook's decision itself**, not the linkage and not the
pre-filter alone. Two symptoms, both from the same cause:

```
grammar ON, pre-filter off: '{"name":"piano","n":4,"nam\xef\xbf\xbde":"pian","n":3}'
grammar ON, pre-filter on : '{"name":"piano","n":4}'      (then empty-stack exception on retry)
```

The first shows a **corrupted byte in the middle of a token piece that the grammar should have
rejected**, and the second shows the same over-acceptance collapsing a stack. Accepting a token
the automaton rejects is a correctness defect and is not something to trade for speed. The
earlier "identical output" result held only for permissive grammars, where almost everything is
accepted and a wrong acceptance is invisible.

## What was changed as a result

Both doors are now **opt-in**, and the default path is native:

- `GGML_GOOSE_GRAMMAR=1` enables the hook (was: enabled unless set to 0);
- `GGML_GOOSE_PREFILTER=1` additionally enables the first-set pre-filter.

Verified after the change, default environment, two consecutive requests:

```
'{"name":"piano","n":4,"name":"piano","n":4}'   valid ascii: True
'{"name":"piano","n":4,"name":"piano","n":4}'   valid ascii: True
```

The sampler stages are unaffected: they remain default-on and measured, because their equality
is established by 39 000+ differential cases rather than by a permissive workload.

## Next step for the grammar stage

The port must be made to produce, for every candidate, the same accept/reject the native stack
produces, and the way to prove it is a per-candidate differential against `llama_grammar_apply`
on real vocabularies - not an end-to-end text comparison, which cannot see a wrong acceptance in
a permissive grammar. Until that exists the stage is an opt-in experiment and P6-10's grammar item
is **not** complete.

## The cause, and the fix

The ported matcher works on **complete codepoints**. It never looks at the trailing partial of a
piece: `decode_utf8` returns a `llama_partial_utf8` for a piece that ends in the middle of a
UTF-8 sequence, and the native code passes that to `llama_grammar_match_partial_char`. The module
has no such path, so a candidate whose piece ends with a partial byte was accepted without its
trailing bytes being checked - which is exactly the corrupted byte seen above.

The hook now **declines the whole call** when any candidate carries a pending partial
(`n_remain != 0`), and the native path decides in that case. Declining is safe; guessing is not.
This is the same shape as the existing rule for stacks that are not in the pointer map.

```
grammar ON, three consecutive requests:
run 1: '{"name":"piano","n":4,"name":"piano","n":4}' | clean: True
run 2: '{"name":"piano","n":4,"name":"piano","n":4}' | clean: True
run 3: '{"name":"piano","n":4,"name":"piano","n":4}' | clean: True

ON : '{"name":"piano","n":4,"name":"piano","n":4}'
OFF: '{"name":"piano","n":4,"name":"piano","n":4}'
identical: True
```

## What this does and does not establish

Establishes: with a grammar that cannot match empty and a strict constraining grammar, the hook
now produces exactly the native output, three times in a row, instead of a corrupted byte or an
exception.

Does not establish: that it is equal for **every** candidate set. The partial path is now handled
by declining, not by matching, and the codepoint path still needs a per-candidate differential
against `llama_grammar_apply`. The stage therefore stays opt-in, and the item stays open.

## A per-candidate differential exists now, and it is safe by construction

`GGML_GOOSE_GRAMMAR_VERIFY=1` makes the hook compute **both** decisions on the same candidates:
the port's mask and `llama_grammar_reject_candidates` from the same file. The native rejects are
what get applied, so the mode cannot change behaviour even if the port is wrong - it only counts
disagreement. That removes the reason this comparison was avoided until now.

Measured, on real vocabularies with two different grammars:

```
FG171_GRAMMAR_VERIFY call=1  cand=1 cases=1  disagreements=0
FG171_GRAMMAR_VERIFY call=2  cand=1 cases=2  disagreements=0
...
FG171_GRAMMAR_VERIFY call=16 cand=1 cases=16 disagreements=0
```

`Zendaya` from the grammar `"Z" [a-z]{1,8}`, which forces a resample, and no disagreements.

## What this does not reach yet

Every call reports `cand=1`: in this tree the grammar sampler is created but never added to the
chain, so it acts as a validator on a single sampled token, and the full-set path is only taken on
the resample route - which in this configuration also arrives with one candidate already chosen.
Reaching 248k candidates requires a chain-order change, and the earlier instrumented run that saw
`cur_p=248320` was a different route.

So the honest state: the codepoint decision agrees with native on every candidate that has been
compared (16 and 3 in two runs), and the full-set route is **not** measured. The stage stays
opt-in, and the item stays open until the comparison covers both routes.
