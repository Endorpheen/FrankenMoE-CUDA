# P11-04 D4: quantisation types - the number in the ledger is not a verified current fact

Recorded 2026-11-19 on `main`, text level. No build was run for this note, and that is the point
of it.

## What the gate measures

`experiments/EXP-2026-11-19-1468-quant-rows-gate/verify.sh` compiles `quant-rows.goose` and
`quant-rows2.goose` standalone, links them with `gate.c` and `host.c` against `libggml-base`, and
prints one line:

```
quant rows: types=<N> port1=<a> port2=<b> neither=<c> row_size_failures=<d>
```

Two different facts hide in it, and the gate says so itself: a wrong row size fails the gate, while
a type **no port answers** is a coverage gap. Deleting a native body that is the only
implementation of a type is what risk R-1 forbids (rule 15, rule 28).

## Two recorded numbers, and they disagree

| source | line |
|---|---|
| `experiments/EXP-2026-11-19-1470-quant-type-reach/README.md` | `types=35 port1=9 port2=16 neither=10 row_size_failures=0` |
| `experiments/EXP-2026-11-19-1468-quant-rows-gate/out/verify.log` (last run in this tree) | `types=35 port1=7 port2=16 ... 12 type(s) are answered by neither` |

The difference is real and it matters: the D4 item says "ten quantisation types", which comes from
the EXP-1470 card, while the recorded gate log in this tree counts **twelve**. `port1` moved from 9
to 7, so one of the two runs predates the other by a content change, not by noise.

## Consequence

The count in the removal plan is not established by these records. Before any native quant body is
deleted, the gate has to be re-run on the current tree and its own line becomes the number:

```
bash experiments/EXP-2026-11-19-1468-quant-rows-gate/verify.sh
```

That compiles two Goose modules and two C files and links against the shipped ggml - about a
minute. It was not run for this note because the session must not be held by a long command
(rule 39); the command is the hand-over.

## What is not in question

The row sizes the ports do answer are right: `row_size_failures=0` in both records, and the gate
fails on a wrong size. So the open item is coverage, not correctness.

## Until it is re-run

D4 keeps "the remaining quantisation types must be closed before the only implementations are
deleted" as open work, and the ledger states both numbers with their sources instead of picking
one. Picking the smaller number would make the removal plan look closer to done than the evidence
supports.
