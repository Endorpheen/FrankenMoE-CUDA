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

Fixed by using parser-produced rules and by advancing the seed stack on the native
side as the real grammar does:

```
grammar composed: cases=4 fails=0
```

Grammars: `"a" "b" "c"`, `[a-c]`, `root ::= r1 r2` with `r1 ::= "a" | "b"`, and a
parenthesised alternation. Both sides now agree on every candidate.

The lesson worth keeping: a hand-built rule table is not a grammar. The native
oracle asserts if the stack was not advanced, and a table that looks plausible can
still be laid out differently from what the parser produces.

## accept_chr, first slice

```
accept: cases=4 fails=0
```

Four grammars (`"a" "b"`, `[a-c] "d"`, `[^a-c] "d"`, plus a non-matching character)
compared by the number of resulting stacks **and** the type/value of each stack
top, which is pointer independent.

Scope: this slice shifts a stack past a matched class; it does not yet expand rule
references afterwards (the native calls `advance_stack` at that point). Rule
references and token rules at the top are therefore out of scope here and are the
next step.

## accept with reference expansion: blocked by the generated parameter order

Extending `accept` to expand rule references after the shift needs `advance` to
write into the output pool at an offset. The attempt compiles as a module but the
generated signature reorders the parameters:

```
gg_function_5(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4,
              int64_t p5, gg_slice p6, gg_slice p7, gg_slice p8, gg_slice p9,
              int64_t p10, int64_t p11, int64_t p12, int64_t base)
```

which does not follow the declaration order I assumed, so the wrapper call did not
type-check. This is the same class of problem the sampler solved with a single
stable entry; guessing the order is what fails. The working copy was reverted to
the committed state, where `accept` is the count-and-top gated slice, and the
expansion stays open until the call is built from the generated prototype instead
of from an assumption.

## Wrapper generation: tool written, selection still manual

`gen_shim.py` takes the generated C plus a description of each entry's variables and
emits a wrapper whose argument order follows the prototype it finds, instead of an
assumed order. That addresses the failure of the previous iteration in principle.

What is still missing is reliable *identification* of which prototype belongs to
which entry: the generated names are `gg_function_N`, several entries coexist in one
translation unit, and matching by parameter shape becomes ambiguous as soon as two
entries have similar shapes. Guessing the index is exactly what produced the earlier
mismatch, so this is not resolved by the tool alone.

Next: select the prototype by a signature that cannot collide (a unique trailing
marker whose type the generator can recognise), then the wrapper and the index both
come from the same parsed source.

## Tree consistency restored

The previous commit had left `accept.goose` calling `advance` with an extra argument
that the committed `advance.goose` did not have, because a directory-wide `git add`
picked up a half-finished edit. Both files are back to the versions that were gated
(simple `accept` without reference expansion, `advance` without the base offset), and
both the accept unit and the composed grammar unit compile again.

Rule for the future: stage experiment files by name, and compile every module in the
directory after a revert - a stale translation unit hid this for one iteration.

## Dispatch entry: inlined away with a single constant call site

The single-stable-entry approach (six slices + five scalars, a shape no other
function here has) compiles, but the entry does not appear in the generated C:

```
gg_function_3: (2 slices, 2 ints)          match_char
gg_function_5: (4 slices, 2 ints, ...)     accept
```

`g_disp` has one call site with a constant `op`, so the compiler inlines it and
drops the function - the same class of behaviour as the unreachable-entry problem
recorded as TD-S5, from the other direction. The fix for the next attempt is to
call the dispatch with at least two different operations (or to keep the call
non-constant), so the function is emitted and its shape can be located.

Not claimed: nothing new was gated in this iteration.

## Entry identification solved by exact shape with a uniqueness assertion

`gen_shim.prototype_by_shape` matches a prototype only when its **full parameter
type list** equals the expected shape, and it fails if the match is not unique:

```
shape: gg_slice gg_slice gg_slice gg_slice int64_t int64_t gg_slice gg_slice int64_t int64_t
entry: gg_function_5
call : gg_function_5(s0, s1, s2, s3, nstacks, chr, s4, s5, maxs, maxd, 0)
```

The accept gate then runs through that generated wrapper and still reports
`cases=4 fails=0`, so the order really does come from the prototype rather than
from an assumption. The dispatch idea was dropped: with constant call sites the
compiler inlines it, and chasing that is not worth more iterations.

Two lessons this closes: a stale generated C hides real mismatches (check its
timestamp), and shape matching is safe only with an exact type list plus a
uniqueness check.

## accept with reference expansion: still not emitted

With the expansion in place (accept calls `advance` with an output offset) the module
compiles, but the accept entry is not emitted in the generated C: the only prototype
present is `gg_function_5` with nine slices and four ints, which is `advance`.
Adding a second call site with a different character did not change that, so the
compiler keeps inlining this function into `main` in every pattern tried.

This is a compiler-behaviour question, not a grammar question, and the same class as
TD-S5 and the dispatch inlining: a function that the host needs must be emitted, and
the current tooling has no way to require it. Until that is solved, the expanded
`accept` cannot be gated and the simplified one (no reference expansion, 4/4) stays
the accepted version.

## accept: the workable split

`accept` itself is not emitted from the accept-only unit (inlined), while in the
composed unit the emitted entries are `g_grammar_reject`, `g_advance`, `g_reject`,
`g_reject_multi` and `g_match_char`. So the practical split, the same one used for
XTC, is:

- the **host** shifts the matched class off the stack top (a few lines: match is
  already decided, the top is popped and the position after the class is pushed);
- **Goose** expands the result with `g_advance`, which is emitted and gated (2/2),
  and which handles rule references and alternates.

This keeps every grammar decision in Goose and needs no new compiler behaviour.
`accept` as a single Goose function stays open; its logic is covered by
`match_char` + `advance`, both gated.

## Split accept gated: host shift + Goose advance, 4/4

```
split accept: cases=4 fails=0
```

Grammars: `"a" "b"` for a matching and a non-matching character, and
`root ::= r1 "c"` with `r1 ::= "a" | "b"` for both alternates. The host shifts the
matched class off the stack and Goose expands the result with `g_advance`; the
resulting stacks match the native `accept` flow exactly, including reference
expansion and alternation.

One process note that cost two wasted runs: the concatenated unit
(`grammar_all.goose`) is generated, and I compiled it without regenerating after
editing `advance.goose`, so the prototype I inspected was stale. The concatenation is
now rebuilt in the same command as the compile.

## Next step: wiring the decision core into the live grammar filter

The decision part is complete and gated. What `apply_impl` still does natively per
token is:

1. decode each candidate's piece into codepoints (host, needs the vocabulary);
2. ask which candidates are rejected (this is the Goose core);
3. set `-inf` on the rejected logits (host).

Only step 2 moves. The plan for the next iteration:

- append the gated functions (`match_char`, `match_partial_char`, `advance`,
  `reject`, `reject_multi`, `grammar_reject`) to the sampler module, since a library
  must be one translation unit;
- add one entry `fg_grammar_reject` with flat rule arrays, flat stacks and the
  candidate codepoints, returning the reject flags;
- flatten the rule table once per grammar object and cache it beside the grammar
  (not per token), because the table is immutable;
- hook it into `llama_grammar_apply_impl` where `llama_grammar_reject_candidates` is
  called today, with the same counter/reporting pattern used by the sampler stages;
- gate in the live path the same way: identical text with the hook on and off.

No part of this is claimed as done yet.

## Moved into the sampler module

All gated functions now live in `goose/sampling/sampler-chain-full.goose` together
with a single entry `fg_grammar_reject`, and the chain builds:

```
GG_SAMPLER entries: CHAIN DRY TYPICAL TOP_P XTC PENALTIES ADAPTIVE MIROSTAT
                    DIST DIST_WORDS DIST_PICK TEMP_EXT
```

The grammar entry is present in the generated C as the unique prototype with 19
slices and six integer parameters (`gg_function_53`); its sibling with five integers
is the internal `g_grammar_reject`. It is not yet in the build script's entry table
nor in the shim, because the table matches one regex per entry and this shape needs
the same shape-driven selection that `gen_shim.prototype_by_shape` already
implements. That is the next small step, then the hook.

Nothing is claimed as wired: the functions are present and the module builds.

## Grammar entry prototype confirmed (mapping for the shim)

The generated prototype for the entry is, in order:

```
p0..p4    slices   etype eval roff rlen seed
p5        int      seedd
p6..p10   slices   cand_cp cp_len pval pnr cand_id
p11       int      ncand
p12..p20  slices   wp wpd stacks stackd scratch tmp_rej tmp_cur rowbuf rej
p21..p24  ints     maxs maxd maxcp marker
```

So the compiler preserved the declaration order here, and a shim can be written
directly in that order. The attempt to add a shape-based selector to the build script
was reverted because the insertion did not take effect and a half-edited generator is
worse than none; the mapping above is what the next attempt needs.

## Grammar shim generated; first functional call trips a bounds guard

`tools/gen-grammar-shim.py` finds the entry by the complete ordered type list,
appends `FG_GRAMMAR_REJECT` to the ids header and writes
`sampler_chain_grammar.c` with the wrapper. It found `gg_function_53` and the wrapper
compiles.

The first call with the grammar `"a" "b" "c"` and candidates `a, b, z` aborts with the
Goose runtime message `array index out of bounds` instead of returning the flags. The
repro is `/tmp/gr_check.cpp` (rules at 0..3, one rule, three single-codepoint
candidates, pools 8x8). This is the next thing to localize: either a slice length the
entry does not receive (the entry passes `wp`/`stacks`/`scratch` sized `maxs*maxd` but
`seed` only `maxd`) or an index computed from `stackd` before it is filled.

Not claimed: the grammar hook is not wired and nothing passes a functional check yet.
