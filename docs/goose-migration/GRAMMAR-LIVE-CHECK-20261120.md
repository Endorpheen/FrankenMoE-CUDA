# Grammar on a live server: works, and my first test of it was wrong

Four runs on 2026-11-20 against the new baseline `builds/flint-acc/run-candidate`, fixed Apex profile,
port 9101, one at a time under the lock.

| run | request | result |
|---|---|---|
| 1 | `grammar: root ::= object ...` with `ignore_eos: true` | **HTTP 500**, "Unexpected empty grammar stack after accepting piece: $ (3)" |
| 2 | the same grammar against the **old** baseline | **the same 500** - so not caused by the new build |
| 3 | `ignore_eos: false`, grammar `root ::= "yes" \| "no"` | **exit 0**, answer `no` |
| 4 | run 3 with `GGML_GOOSE_GRAMMAR=1` | **exit 0**, answer `no` |

## The failure was mine

`ignore_eos: true` makes generation continue after the grammar is satisfied; the grammar stack empties and
the next token throws. That is correct behaviour for an impossible request, and it is the fourth time today
that a harness of mine produced a finding that dissolved on inspection. Runs 3 and 4 show the grammar path
answering correctly: the output is one of the two permitted literals.

## What this establishes, and the one thing it does not

Established: grammar-constrained generation **works on the live runtime**, on both the old and the new
baseline, and it still works with the Goose grammar switch on, producing a valid constrained answer.

**Not established:** that the Goose path answered in run 4. `GGML_GOOSE_GRAMMAR=1` enables it by code
(`vendor/flint/src/llama-grammar.cpp:1394`), but the run produced **no witness line**, and "the switch was
on" is not "the port answered". So the ledger does not claim it. What would settle it: a marker in the
reject door that prints on first use, in the same way the sampler doors print `FG171_TOP_P`.

## Where that leaves P6-10

Grammar is **default-off** on the Goose side (`GGML_GOOSE_GRAMMAR` must be set) and unwitnessed when it is
on, so P6-10 stays PARTIAL. What changed today is that the live path is now known to work at all - which,
given how much of this migration had been "verified" by symbol tables alone, was not previously known.

## The witness, obtained with the switch that was already there

The document above said the missing piece was a marker showing that the Goose path answered. The marker
already existed: `GGML_GOOSE_GRAMMAR_DIAG` prints `FG171_GRAMMAR_SIZES` and `FG171_PREFILTER`, and both
lines sit inside the hook **after** `fg_grammar_enabled()` - so they print only when the Goose grammar path
is the one deciding (`vendor/flint/src/llama-grammar.cpp:1502,1536`).

One run with `GGML_GOOSE_GRAMMAR=1 GGML_GOOSE_GRAMMAR_DIAG=1`, the same `root ::= "yes" | "no"` request:

```
6 FG171_GRAMMAR_SIZES
6 FG171_PREFILTER
exit 0, answer "no"
```

So the Goose grammar path **executed six times during a live constrained request and produced a valid
constrained answer**. That is the third of the three states this project distinguishes: linked, executed,
correct - and here the first two are proven and the third is proven for this request.

## What is still open in P6-10

- the switch is **off by default**, so a user of the shipped binary gets the native path unless they set
  it - the requirement is not closed by a working port, only by the port being the one that answers;
- deleting the native reject bodies needs the acceptance run at the full input length and the four axes,
  which is rule 15 and rule 20 territory, not this check.
