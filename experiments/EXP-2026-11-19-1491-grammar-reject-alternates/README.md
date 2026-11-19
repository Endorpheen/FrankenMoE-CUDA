# The grammar reject port dropped the continuation of a rule reference

Base commit: `d5b4a17d`. Slice under test: `goose/sampling/sampler-chain-full.goose`
(`source/runtime.goose` is the pinned copy).

## Oracle

`tests/fixture.cpp` compiles `vendor/flint/src/llama-grammar.cpp` into the fixture, so
`llama_grammar_reject_candidates` is called directly and is the reference. The port is reached
through the header the Goose compiler emits for the pinned slice. Only the reject decision is
compared, candidate by candidate.

The oracle file mentions logging, abort and three vocabulary accessors; they are stubbed so the
fixture links without the shipped library. Linking it would put a second copy of the non-static
grammar symbols into the process and the fixture could silently call that copy.

## The defect

`llama_grammar_advance_stack` (llama-grammar.cpp:902-928), at a rule reference, pushes
`pos + 1` - the continuation of the reference in its own sequence - and starts **every**
alternate of the referenced rule; `llama_grammar_reject_candidates` (:949-966) then intersects
the rejects of the resulting stacks.

The port replaced the reference with the first alternate and nothing else:

```
scratch[depth - 1] = roff[rid];
```

The continuation was gone. With `space ::= " "?` that means the descent enters `" "`, fails on
`"` and rejects, while the native automaton also offers the empty alternate and accepts. This is
the shape measured in EXP-1471: a two-codepoint piece, `pnr=0`, at a class reached through a
rule reference. The two earlier attempts fixed the class branch; the defect was one branch later.

## Change

- the reference is replaced by the continuation with the single alternate above it, which is
  `advance_stack` for a one-alternate rule;
- a rule with several alternates needs one candidate set per alternate and a cursor snapshot per
  row, which this call did not carry, so it returns `-2` and the native matcher decides;
- `g_reject_multi` propagates a negative result instead of ignoring it;
- the door counts `-2` on its own (`FG_GRAMMAR_DOOR ... multi=N`), so the cost of those declines
  is visible instead of folded into the general decline counter.

## The multi-alternate descent

Declining the alternation was correct but useless on a JSON grammar, where `space ::= " "?` is
reached on most calls. The rows are now really evaluated, which is the native recursion written
out: each alternate gets the continuation below it, the candidate cursors are restored from a
snapshot taken before the first row, and a candidate is rejected only when **every** row rejects
it. Three candidate-sized regions and one stack region are used per level, taken from a pool the
caller passes; past the pool the call declines with `-2` and the native matcher decides.

The pool is grown, never refilled: every region is written before it is read, so zeroing megabytes
of scratch on each token would be pure cost. Four levels is the depth a real grammar reaches
inside one piece. At the widest call seen so far (248k candidates) the pool is 23.8 MiB of
thread-local scratch, ~0.12% of the process RSS at the accepted Apex profile.

## Two further defects the gate found while it was being written

- the intersection was first accumulated by seeding each row with the current rejects, so a
  rejecting row kept a candidate rejected even when a later row accepted it - `cand=0` of the
  EXP-1471 shape failed. The pre-expansion rejects are now a separate mask and the accumulator
  starts at "rejected", which is the native intersection.
- `LLAMA_GRETYPE_ALT` ends a sequence exactly as `LLAMA_GRETYPE_END` does
  (`llama_grammar_is_end_of_sequence`), and seven places in the port tested only for `END`. A
  descent that landed on `ALT` fell into the "unsupported" branch and then rejected every
  candidate with codepoints left - `'{' ' ' '"'` failed against the oracle.

## Gate

`bash tests/build.sh` (gate at `-O1`, generated `gen.o` cached by source SHA + optimisation +
compiler):

```
goose/sampling/sampler-chain-full.goose: typechecked ok
gate: cases=7 fails=0
gate exit=0
```

Seven cases against the compiled-in oracle:

| case | shape | expected |
|------|-------|----------|
| `single-alternate` | `'{' REF(space) '"'`, `space ::= " "` | agrees, 5 pieces |
| `multi-alternate-empty` | the EXP-1471 shape, `space ::= " " \| <empty>` | agrees, 5 pieces |
| `two-alternates` | `space ::= "a" \| "b"` | agrees, 4 pieces |
| `nested-reference` | `REF(REF(" " \| <empty>))` | agrees, 4 pieces |
| `char-not` | `[^a]` | agrees, 4 pieces |
| `partial-lead` | incomplete UTF-8 lead byte next to `'a'` | agrees, 2 pieces |
| `deep-nesting` | five nested alternations, deeper than the pool | declines `-2` |

## Not measured here

The disagreement count and the decline rate on a real vocabulary need the sampler-chain library
rebuild and one constrained request through the Apex lock. That is a model run and needs Igor's
permission.

## What is still open

`deep-nesting` reaches the decline path deliberately; whether the `steps < 4096` budget or the
level bound produced that `-2` is not distinguished by the fixture, and the card says so rather
than guessing. Nothing else in the reject port is known to disagree with the oracle on these
shapes.
