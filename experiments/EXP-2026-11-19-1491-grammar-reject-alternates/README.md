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

## Gate

`bash tests/build.sh` (gate at `-O1`, generated `gen.o` cached by source SHA + optimisation +
compiler):

```
goose/sampling/sampler-chain-full.goose: typechecked ok
gate: cases=2 fails=0
gate exit=0
```

- `single-alternate`: `'{' RULE_REF(space) '"'` with `space ::= " "`, five candidate pieces -
  the port now agrees with the oracle on every one. Before the change the port rejected
  `'{' '"'`, which the oracle accepts.
- `multi-alternate`: the EXP-1471 shape with `space ::= " " | <empty>` - the port declines
  (`-2`) and the native matcher decides. No wrong answer, but also no port decision yet.

## Not measured here

The disagreement count and the decline rate on a real vocabulary need the sampler-chain library
rebuild and one constrained request through the Apex lock. That is a model run and needs Igor's
permission.

## What is still open

The multi-alternate case must be decided by the port, not declined: with `space ::= " "?` in a
JSON grammar that state is reached on most calls, so declining it would leave the door deciding
almost nothing. The next step is the row-by-row descent with a per-row candidate set and cursor
snapshot, in Goose, with `-2` kept only for the case where those buffers run out.
