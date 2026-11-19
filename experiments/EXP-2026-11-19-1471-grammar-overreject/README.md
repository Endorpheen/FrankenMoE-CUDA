# The grammar port rejects tokens the native automaton accepts

The shipped path, one constrained request, temperature zero:

```
FG171_GRAMMAR_DIFF cand=4753 id=4754 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_DIFF cand=0    id=763  goose_reject=1 native_reject=0 cp_len=2 pnr=0
```

Both disagreements are the same shape: the port **rejects** a candidate the native accepts, on a candidate two codepoints wide
with `pnr=0`. The first call of the run (one candidate, the opening brace) agrees; the divergence starts on the second call,
which sees the whole vocabulary.

That is why the constrained answer changes: a token the native automaton allows is removed from the candidate set, so greedy
decoding picks a different - still legal - continuation. Measured on the same build:

```
off: {"city": "Tokyo", "country": "Japan"}        2 of 2 runs
on:  { "city" : "Dhokar" , "country" : "Nepal" }  2 of 2 runs
```

## How to reproduce

```
bash experiments/EXP-2026-11-19-1471-grammar-overreject/repro.sh
```

It takes the Apex lock, starts the server with `GGML_GOOSE_GRAMMAR=1 GGML_GOOSE_GRAMMAR_VERIFY=1`, sends one grammar-constrained
request and prints the shadow comparison. The verify mode applies the **native** decision, so it cannot corrupt the answer - it
only counts.

## Where to look

`vendor/flint/src/llama-grammar.cpp`, the block that builds `pval`/`pnr` from each candidate's codepoints before
`fg_grammar_reject_call`. A two-codepoint candidate with `pnr=0` is the smallest failing input, and `id=4754` is a concrete id
to test against `llama_grammar_reject_candidates` directly.

## What this does not say

It does not say the port is generally wrong: on 248 071 candidates of the same call it agreed on all but one. It says the
rejection rule differs for a narrow shape, and that shape is enough to change an answer.

## The state the wrong rejection was made in

With the state dump the three failing calls are no longer opaque:

```
FG171_GRAMMAR_DIFF  cand=4753 id=4754 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_STATE cps=123,34 stacks=1 pval=34 pnr=0          <- "{" then '"'
FG171_GRAMMAR_STACK st=0 depth=1 head= r0+0(type=3)             <- a character class at rule 0, offset 0

FG171_GRAMMAR_DIFF  cand=0 id=763 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_STATE cps=34,58 stacks=1 pval=58 pnr=0            <- '"' then ':'
FG171_GRAMMAR_STACK st=0 depth=1 head= r0+7(type=3)

FG171_GRAMMAR_DIFF  cand=0 id=328 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_STATE cps=32,34 stacks=2 pval=34 pnr=0            <- ' ' then '"'
FG171_GRAMMAR_STACK st=0 depth=2 head= r0+11(type=2) r5+0(type=3)  <- a rule reference above a class
```

Every failing candidate is a **two-codepoint piece** decided at a character class, and the second codepoint is one the grammar
allows only after finishing or skipping the current rule (`space ::= " "?` sits between `{` and `"city"`).

That is the shape of the defect: when the codepoint does not match the class at the top, the port rejects, while the native
automaton can finish the current rule - or skip an optional one - and retry the same codepoint in the parent. The third case
shows the stack a rule reference above a class, which is exactly the state where that retry is required.

The next change is therefore in the class branch of `goose/sampling/sampler-chain-full.goose`: on a failed match, advance past
the end of the current rule and retry the same codepoint at the parent before rejecting. The shadow mode is the gate for it -
the disagreement count on this grammar must go to zero, and the pair run must produce the same text with the door on and off.

## The first hypothesis was tested and did not hold

The class-final hypothesis was implemented twice and measured, not argued about. A decline was added for "the class is the
last element of its rule, so it can be skipped", first after the rejection loop and then - after that first version was seen
to be too late - at the branch entry before any candidate was touched. Rebuilt and re-measured:

```
FG171_GRAMMAR_DIFF cand=4753 id=4754 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_STATE cps=123,34 stacks=1 pval=34 pnr=0
FG171_GRAMMAR_STACK st=0 depth=1 head= r0+0(type=3)
```

The disagreement is unchanged, so `etype[match_end(pos)]` is not 0 at the state that rejects: the class at `r0+0` is followed
by a rule reference, and the rejection happens one level deeper, after the reference is expanded. The decline was reverted -
it did not fix the defect and would have cost speed on every rule-final class.

What is established and reusable:

- the failing input is a two-codepoint piece at a class reached through a rule reference;
- the port rejects where the native accepts, so the direction of the error is known;
- the shadow mode plus the state dump is the gate a fix must move: disagreements must reach zero on this grammar and the pair
  run must give the same text.

What is not established: which exact transition the port loses. The next measurement is one step deeper - whether the class
inside `space` is reached with the candidate's cursor already advanced past the second codepoint, or whether the rejection comes
from the code point being matched against the parent class instead of the expanded one.

Stopping here rather than continuing to guess is the project's blocker rule: two meaningful attempts, both measured, both
negative.

## The defect was in the rule-reference branch, not in the class branch (found by reading the oracle)

`llama_grammar_advance_stack` (vendor/flint/src/llama-grammar.cpp:902-928) does three things at a
`RULE_REF` and the port did one of them:

- it pushes `pos + 1`, the continuation of the reference inside its own sequence;
- it iterates **every** alternate of the referenced rule (`LLAMA_GRETYPE_ALT` scan at :917-926);
- it recurses, so the resulting stacks carry no unresolved reference, and
  `llama_grammar_reject_candidates` (:949-966) then **intersects** their reject sets.

The port's branch did this instead:

```
if t == 2 {
    let rid: i64 = eval[pos];
    if roff[rid] >= 0 && etype[roff[rid]] != 0 { scratch[depth - 1] = roff[rid]; }
    else { depth = depth - 1; }
}
```

It overwrote the reference with the **first** alternate and dropped the continuation. `space ::= " "?`
compiles to a rule whose alternates are `" "` and the empty sequence, so the descending stack entered
`" "`, failed on `"`, and rejected - while the native automaton also offered the empty alternate and
accepted. That is exactly the two-codepoint, `pnr=0`, class-reached-through-a-reference shape of the
three recorded disagreements, and it explains why both earlier fixes in the class branch changed
nothing: the class was right, the branch taken after it was wrong.

This also explains the stack dumps: `head= r0+11(type=2) r5+0(type=3)` is a reference the **descent
itself** pushed after consuming a class, above the class it came from - not a native input stack.

## The change

`goose/sampling/sampler-chain-full.goose`, `g_reject`:

- the reference is now replaced by the continuation (when `pos + 1` is not `END`) with the single
  alternate above it, which is `advance_stack` for a one-alternate rule;
- a rule with more than one alternate needs one candidate set per alternate and a cursor snapshot per
  row, which this call does not carry, so it returns `-2` and the native matcher decides;
- `g_reject_multi` propagates a negative result instead of ignoring it;
- the door counts `-2` separately (`FG_GRAMMAR_DOOR ... multi=N`) so the cost of those declines is
  visible rather than folded into the general decline counter.

Gates already run: `goose check` on the module (ok, only pre-existing redundant-cast warnings) and a
`-fsyntax-only` compile of `vendor/flint/src/llama-grammar.cpp` with its recorded flags (exit 0).

What is not yet measured: the disagreement count and the decline rate on the real grammar. That needs
the sampler-chain library rebuild and one constrained request through the Apex lock, which is a model
run and therefore needs Igor's permission.
