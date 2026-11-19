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
