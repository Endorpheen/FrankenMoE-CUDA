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
