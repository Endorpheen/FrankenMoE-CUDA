# EXP559: permutation rules and cycle-aware dump (P7-08 slice 6, part 5)

## Hypothesis

`common_chat_peg_builder::permute(rule_prefix, parsers)` builds a chain of named
rules so that every parser matches exactly once in any order. The expansion is
structural, and permutations share subsets, so the port also has to reproduce the
dump's `[cycle]` marker before the two dumps can be compared.

## What the native code does

- Empty input returns `eps()`. One parser, or more than
  `COMMON_CHAT_MAX_PERMUTE` (6), falls back to a plain sequence.
- Otherwise every subset of remaining parsers becomes a rule named
  `rule_prefix + "-" + mask`; a subset rule is a `Choice` of
  `parsers[i] + remaining_of(mask without i)`, and the root is the rule for the
  full mask.
- `remaining == 0` returns a **fresh** `eps()` each time, so those leaves are not
  shared.
- Rules and choice nodes are created bottom-up, and a subset that is reached
  twice reuses one node; `common_peg_arena::dump` keeps a visited set and prints
  `[cycle]` on the second visit. `Tag` re-dumps its child with a fresh set.

## Port

`peg_permute` walks masks from 1 to `(1 << n) - 1`, builds each choice with
`peg_seq2`/`peg_choice2`, names the rule with `peg_app_i64`, and caches rules by
mask; a single alternative is still wrapped in `Choice` because native
`operator|=` always produces a choice node. `peg_seq1` covers the one-parser
case. The dump gained two visited sets (`vis`, `vis2`) with the slot passed
through, printing `[cycle]` exactly like native and switching sets inside `Tag`.

## Gate

```
FG559_RUNTIME_PERMUTE cases=672 values=1152 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=672 values=1152 failures=0 skipped=0
```

Nine new grammars: empty, one, two, three, four, six (the largest combinatorial
case), seven (the fallback), permute followed by `end`, a shared subtree that must
print `[cycle]`, and permute followed by a content tag.

Twelve mutations: empty not epsilon, a duplicated single parser, every size
falling back, shared vs fresh epsilon for the zero subset, a missing rule prefix,
an off-by-one rule mask, a missing single-alternative choice, reversed
alternative order, the wrong root, a different cycle marker, the fallback
threshold moved to five, and a mask walk that skips a value.

## Findings

- Native builds a `Choice` even when a subset has a single alternative; the first
  port version returned the bare sequence and the dumps differed.
- Native creates a fresh `Epsilon` per zero-subset use. Reusing one node made the
  dump print `[cycle]` where native prints `Epsilon`.
- Sharing appears for the first time in this slice, so the dump needed the
  visited sets that earlier slices could ignore.

## Still open

- The standard JSON/XML tool builders and `permute`'s `trigger` flag.
- The AST mappers (base, gemma4, minimax) and partial streaming.
- The typed direction, autoparser/grammars, model parsers, `use_jinja=false`, and
  the chat API's missing time injection.
