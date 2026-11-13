# EXP491: lazy triggers in PEG build_grammar (P7-09 twentieth slice)

`build_grammar(lazy=true)` emits only rules reachable from trigger rules and
builds the root from the sorted trigger names. This slice ports that branch on
top of EXP490.

## What was ported

`source/peggram4.goose`:

| decision | rule |
|---|---|
| reachability | non-lazy: from the root; lazy: the union of trigger rules and their reachable rules (a fresh visit set per trigger) |
| emission | every reachable rule, as before |
| root (non-lazy) | the root parser's text |
| root (lazy) | trigger names sorted byte-wise and joined with ` \| ` |
| no triggers | empty root text and no rules |

## Gate

The full rule map is compared with native `build_grammar(gb, true/false)` on the
EXP490 corpus (31 cases) plus two triggers, a trigger chain, no triggers and a
non-lazy regression: **35 cases, 35 values, 0 failures**; ASan+UBSan clean.

Mutations 5/5: unsorted triggers, comma join, skipped trigger reachability,
lazy ignored, non-lazy root text in lazy mode. Marking the trigger index
explicitly is redundant because visiting the rule parser marks it; that mutation
is not kept.

## Scope

P7-09 stays PARTIAL: the schema parsers and the JSON side remain.
