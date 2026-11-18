# EXP-2026-11-18-1110: grammar automaton on an index model

Goal: port `llama_grammar_advance_stack` to Goose over flat rule arrays, replacing
the native pointer stacks with global element indexes.

## Oracle

`tests/gate.cpp` compiles `llama-grammar.cpp` into the fixture, so the static native
automaton is called directly rather than re-implemented. The same rule table is
built in the native representation and as flat arrays:

```
rules[0] = RULE_REF 1, END      global 0..1
rules[1] = CHAR 'a', END        global 2..3
rules[2] = CHAR 'x', END        global 4..5
```

## Result after the worklist split

```
native:     n=1 d=1 t=3 v=97
goose :     n=1 d=1 t=3 v=97

native alt: n=2 d=1 t=3 v=98  d=1 t=3 v=97
goose  alt: n=2 d=1 t=3 v=98  d=1 t=3 v=97
```

Both the plain rule reference and the alternation match, including the order in
which the alternates are produced. `advance_stack` is ported on the index model.

## Earlier state (kept for the record)

```
native: n=1 d=1 t=3 v=97          (one final stack, terminal 'a')
goose : n=2 d=1 t=2 v=1  d=1 t=3 v=97
```

The Goose version finds the right terminal stack but also emits the intermediate
stack that still holds the `RULE_REF` bookkeeping entry. The native keeps
in-progress stacks in its worklist and only appends a stack to the result when its
top is terminal or the stack is empty.

Fix: separate the worklist pool from the result, and append to the result only in
those two cases. That needs one more slice (the work pool) in the entry signature.

Element type values used: END=0, ALT=1, RULE_REF=2, CHAR=3, CHAR_NOT=4, CHAR_ANY=7
(checked in `llama-grammar.h`, not assumed).

## Character classes match the native matcher

```
match_char: cases=12 fails=0
```

Cases: exact character, inclusive range `[a-z]`, negated range `[^a-z]`, `CHAR_ANY`,
alternation inside a class `[ab]` and its negation. Encoding used between Goose and
the caller: matched -> `next_pos + 1`, unmatched -> `-(next_pos + 1)`, so one integer
carries both the verdict and where the class ends.

A detail that cost one iteration: the parser stores the second character of a class
in the `CHAR_ALT` element's own value (`{CHAR,'a'}, {CHAR_ALT,'b'}`), not in a
following `CHAR` element. My gate data had it in the wrong place, which looked like
a matcher defect until the native structure was checked.

## reject_candidates: structure read, port not started

The function is recursive and per-codepoint, not a flat loop:

```
reject_candidates(rules, stacks, candidates)
    for each stack: rejects = reject_for_stack(stack, rejects_so_far)
reject_for_stack(stack, candidates):
    empty stack        -> reject any candidate that still has codepoints or a partial sequence
    top is TOKEN*      -> check the token id, or the end-of-token partial case
    otherwise          -> for each candidate, match the first codepoint:
                          match      -> descend with codepoints+1
                          no match   -> reject
                          exhausted  -> reject iff a partial UTF-8 sequence cannot satisfy
                                        the class (match_partial_char)
    then advance the stack past the matched class and recurse into it
```

So the port needs three pieces beyond what is already done (`advance_stack`,
`match_char`): `match_partial_char`, `match_token`, and the recursive descent with
a worklist of candidates (each carrying index, codepoint cursor, partial-sequence
state and token id).

Not started, and not claimed: this slice is the next step, gated against
`llama_grammar_reject_candidates` compiled into the fixture exactly as the previous
two slices were.

## match_partial_char ported

```
match_partial: cases=148 fails=0
```

Exact characters, ranges, negated ranges, `CHAR_ANY` and class alternation, each
across `n_remain` 0..3 and a sweep of partial values, against the native function
compiled into the fixture.

The remaining piece for `reject_candidates` is `match_token` (a token id inside the
rule, small) and then the recursive descent itself, which now has both of its
matching primitives ported and gated.

## reject_candidates: first gated version

```
reject: cases=2 fails=0
```

Cases: rule `"abc"` with candidates `a, b, z`, and rule `[a-c]` with `a, d, Z`,
against the native `llama_grammar_reject_candidates`. The descent is iterative
because the stack transition after a class is candidate independent; a matched
class clears the candidate's codepoint, which is what the native cursor does.

Now with multi-codepoint candidates (`'abc'` accepted, `'ax'` rejected for the rule
`"abc"`, `'cc'` rejected for `[a-c]`). Two defects of mine were caught by the gate
and fixed: the cursor was not advanced, and a candidate with codepoints left when
the stack ran out was accepted instead of rejected.

Scope of this version, stated so it is not mistaken for the whole function:

- one stack (the native combines several by intersecting their rejects);
- partial UTF-8 is wired: a candidate whose codepoints are exhausted but whose byte
  sequence is incomplete is rejected exactly when the class cannot complete it
  (third gated case, native and Goose agree);
- no `TOKEN`/`TOKEN_NOT` rules;
- the rule-reference case expands only the first alternate.

The first full run of this version caught a real defect of mine: without clearing
the codepoint the loop retested the same character at the next class and rejected
`"abc"` for candidate `a`. Fixed and re-gated.

## Multiple stacks: rejects intersect

```
native: 0 0 1        (candidates 'a', 'b', 'z' against stacks for "a" | "b")
goose : 0 0 1
```

`g_reject_multi` evaluates each stack with a fresh cursor set and keeps a candidate
only when at least one stack accepts it, which is the native intersection of
per-stack reject sets. One gate defect of mine: the second stack row was written at
the wrong stride, which showed up as a wrong verdict for 'b' until the row offset
matched `maxd`.

Remaining scope: token rules (`TOKEN`/`TOKEN_NOT`), and alternates inside a rule
are still expanded one level only.

## Token rules

```
reject: cases=4 fails=0
```

The fourth case is a `<5>` token rule with candidates id 5 (accepted) and id 6
(rejected), both carrying a codepoint. The defect it caught: after a token rule the
candidate's remaining codepoints were treated as leftovers and rejected the valid
candidate; a token rule consumes the whole token, so the cursor is moved to the end.

Remaining scope for `reject`: alternates inside a rule expand only one level, and
`TOKEN_NOT` is implemented but not yet exercised by a case.

## Composed entry: compiles, gate blocked by the fixture, not by the port

`g_grammar_reject` chains `g_advance` (expand rule references, alternates included)
into `g_reject_multi`, which is the shape the sampler filter needs. It compiles
together with all the previously gated pieces.

Its gate is not passing yet, and the reason is my fixture rather than the port: with
the hand-built table `rule0 = ref1, ref2, END` the **native** oracle aborts on
`GGML_ASSERT(is_positive_char || pos->type == LLAMA_GRETYPE_CHAR_NOT)` inside
`llama_grammar_match_char`, i.e. the native advance left a `RULE_REF` on a stack top
for this layout. A rule table built by hand has to match what the parser produces;
until the fixture uses parser output (or a layout the native accepts), this gate
proves nothing and is recorded as blocked.

No claim of equality is made for the composed entry.
