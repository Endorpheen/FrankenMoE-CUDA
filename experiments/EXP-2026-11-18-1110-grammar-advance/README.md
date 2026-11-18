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
