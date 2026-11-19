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

## Bounds guard: the difference between the gated entry and the module entry

The same grammar and candidates that pass in the experiment gate (`grammar composed:
cases=4`) abort inside the module entry. The gates differ in exactly one respect: the
experiment passes `cand_cp` with length `ncand * maxcp`, the module call passed a
slice of length 4 for three candidates with `maxcp = 4` - the module indexes
`cand_cp[i * maxcp + curs[i]]`, so the slice must be `ncand * maxcp` long. That is the
first thing to correct in the wrapper, and it is a wrapper defect, not a port defect.

Recorded rather than patched blindly: the next iteration fixes the length, re-runs the
same repro, and only then moves to the hook.

## Module entry works

With `cand_cp` passed at length `ncand * maxcp` the call returns the expected flags:

```
rejected=2 flags: 0 1 1 (expect 0 1 1)
```

for the grammar `"a" "b" "c"` with candidates `a`, `b`, `z`. That is the grammar
decision running inside the sampler module through a generated wrapper, with the
entry located by its full type list.

Confirmed cause of the earlier abort: the wrapper was handed a codepoint slice of
length 4 instead of `ncand * maxcp`; the module indexes `cand_cp[i*maxcp + cursor]`.
No port defect was involved.

Next: the hook in `llama_grammar_apply_impl`, flattening the immutable rule table
once per grammar object and the stacks per call.

## Grammar decision live in the server

```
[ON ] 'yes'   counters: FG171_GRAMMAR goose calls=1
[OFF] 'yes'   counters: none
```

Server, Apex model, `--grammar-file` with `root ::= "yes" | "no"`, temperature 0:
the grammar-constrained output is identical with the Goose decision enabled and
disabled, and the counter proves the call happens.

Two wiring defects were needed to get here and both are recorded: the shim wrapper
had to join the existing shim file (a second translation unit would duplicate every
symbol from `sampler_chain.c`), and the chain build regenerates that file, so the
wrapper is now emitted by `build-sampler-chain.sh` itself through
`tools/gen-grammar-shim.py`. Before that the symbol was a weak *undefined* reference
and the hook silently fell back to the native path - visible only because the counter
did not fire.

## Defect found: stack pointers converted against the wrong base

The live attempt aborts with `goose runtime error: array index out of bounds` at
`cand=1 stacks=1`, and the earlier long-generation run diverged (2 tokens against 64)
for the same root cause.

The hook converts each stack element pointer to a flat index as
`e - grammar.rules[0].data()`. That is only valid for elements of rule 0: every rule is
its own `std::vector`, so a pointer into rule 1 or later yields a meaningless offset,
which the automaton then uses to index `etype`. The flat table uses global indices
from `roff`, so the conversion must use a pointer-to-index map built during flattening.

Consequences: the previous claim of "grammar live, output identical" holds only for the
one-token case that happened to stay inside rule 0 (the `yes | no` grammar); the
multi-token case was never actually correct. That claim is corrected here.

Fix for the next iteration: store `std::unordered_map<const llama_grammar_element *, int64_t>`
in `fg_flat_rules` while flattening, and use it in the hook for both the seed stacks
and any future accept path.

## Pointer map fixed: multi-token grammar generation identical

```
[ON ] n=64  ' and the moon using the words tide and reflection is correct grammar usage ...'
     grammar: FG171_GRAMMAR goose calls=1
[OFF] n=64  ' and the moon using the words tide and reflection is correct grammar usage ...'
     grammar: (none)
```

Sixty-four grammar-constrained tokens, identical text, and the decision runs in
Goose. This replaces the earlier one-token claim, which had been made against a
grammar that stayed inside rule 0.

The fix: `fg_flat_rules` now carries a pointer-to-index map built during flattening,
and the hook refuses (returning to the native path) if any stack element is not in the
map rather than computing an offset from `rules[0]`. Refusing loudly on an unmapped
pointer is what turned a silent corruption into a visible fallback.

## Grammar hook cost: nearly 3x on decode, not acceptable

Paired runs, 512 tokens, same grammar and request:

```
on  16.49   off 51.59
on  16.46   off 52.08
```

Decode falls from about 51.8 to 16.5 tok/s (-68 %). Prefill numbers in the same runs
(4.1 against 17.0) are dominated by the tiny prompt's fixed cost and are not a
prefill measurement.

Causes, in the order worth fixing:

1. each token allocates roughly a dozen vectors (candidates, stacks, work pools) and
   rebuilds them from scratch;
2. the hook calls the entry **once per stack**, and each call walks all 100k
   candidates, so the cost multiplies by the stack count;
3. inside Goose every candidate does a bounded slice read with the runtime's bounds
   check, which is far more expensive than the native pointer walk.

Until this is fixed the grammar stage is live but must not be enabled by default: it
is correct, and it is also three times slower. The next step is buffer reuse plus one
call carrying all stacks, then re-measure.

## Buffer reuse did not move the cost

```
on  16.79   off 53.13
on  16.31
```

Reusing thread-local buffers changed nothing measurable (16.5 before, 16.3-16.8
after). So the allocation was not the problem; the cost is the per-candidate work
inside Goose: the hook hands the grammar roughly a hundred thousand candidates per
token, and every codepoint comparison inside the generated C goes through a
bounds-checked slice read, against the native pointer walk that does one comparison.

Two directions, both measurable:

1. find whether the generated code can drop bounds checks for this module (a build
   option), which would attack the reported 3x directly;
2. reduce the candidate set handed to the grammar by pre-filtering on the first
   codepoint in the host, which is cheap there and does not change the decision.

The stage stays opt-in until one of them brings it near the native cost.

## The slowdown cannot be the hook's inner work

The counter from the slow run says:

```
FG171_GRAMMAR goose calls=1   cand=1 stacks=1
FG171_GRAMMAR goose calls=100 cand=1 stacks=2
FG171_GRAMMAR goose calls=200 cand=1 stacks=2
```

One candidate and roughly one call per token. A single call over one candidate cannot
cost the 44 ms per token that the 3x slowdown implies, so **both** of my hypotheses
(per-token allocation, then a hundred thousand candidates through bounds checks) are
wrong, and the second one is refuted by this measurement.

What is left to check, in order:

1. the early `return;` path: if a stack element is not in the pointer map, the hook
   returns **without** falling back to the native rejection, so the grammar is not
   applied at all for that step - a correctness hole, and a different sampling path
   that could also explain a timing change;
2. whether the slowdown reproduces with the hook forced to the native path
   (`GGML_GOOSE_GRAMMAR=0` already does that, and it is fast), which isolates the
   hook from everything else.

Recorded before any further optimization: optimizing an inner loop that handles one
candidate would have been wasted work.

## Fallback fixed; the candidate set size is the open question

The early `return` is replaced by a labelled fallback: an unmapped stack element now
runs the native rejection instead of leaving the logits unmasked.

The measurement that follows is still unexplained and is recorded as such:

```
n=128 decode=5.92
FG171_GRAMMAR goose calls=1   cand=1 stacks=1
FG171_GRAMMAR goose calls=100 cand=1 stacks=2
```

`cand=1` means the grammar is handed a single candidate. With one candidate it cannot
reject anything, so the grammar decision has no effect on the output, and the earlier
"identical text" evidence for the grammar stage proves only that both paths produced
the same text, not that the constraint was applied. That claim is weakened here
explicitly.

Why the candidate set is 1 in this chain, and why decode fell to 5.9 tok/s, are two
questions for the next iteration; neither is answered by the hook's own work, which
the counter bounds at one call per token over one candidate.

## The chain applies grammar to a one-element candidate set

Instrumented sizes at the call:

```
FG171_GRAMMAR_SIZES cur_p=1 candidates=1 stacks=1
```

So in this runtime the grammar sampler receives a single candidate. With one candidate
there is nothing to reject, which means **the native grammar cannot constrain either**:
whatever text came out conformed because the model produced it, not because it was
constrained.

Consequences, stated plainly:

- the live grammar A/B measured earlier compares two unconstrained paths; the grammar
  stage's behaviour is therefore unproven, not accepted;
- the slowdown measured on the same configuration is also not attributable to the
  grammar decision, and the same run shows 1.99 tok/s with 32 tokens, which needs its
  own explanation;
- validating the stage requires a chain where grammar runs before the candidate set is
  truncated (upstream it is applied before top_k). That is a configuration change of
  the sampler order, not a change to the port.

The diagnostic print is kept behind `GGML_GOOSE_GRAMMAR_DIAG`.

## Conclusion: in this runtime the grammar is not a candidate filter

Searched the chain assembly in `common/sampling.cpp`: the chain is built only from
`params.samplers`, and although `grmr` is created (line 270/274), */

`grep grmr` finds it in the struct, in the free path and in the copy path - but **not**
as `llama_sampler_chain_add(chain, grmr)`. The server sources here contain no grammar
sampler wiring either.

That explains the measurement: the only place the grammar sampler runs in this build is
after selection, where `cur_p->size` is 1. Grammar therefore acts as a validator, never
as a filter, so:

- no candidate filtering exists to port into or out of;
- the Goose decision cannot be exercised on this path without first inserting the
  grammar sampler into the chain (a chain-level change in `common/sampling.cpp`, not a
  port change);
- the earlier live A/B, the 64-token "identical" run and the slowdown all measured
  something other than the grammar decision.

This closes the question that was left open. What remains for P6-10's grammar item is a
deliberate decision: insert the grammar sampler at the upstream position (before top_k)
and then re-run the comparison; that is new wiring, and it changes behaviour for every
grammar user, so it is not done here silently.

## Flag-gated insertion did not take effect

The insertion was added behind `GGML_GRAMMAR_IN_CHAIN=1`, but the run shows its marker
zero times and `cur_p=1` is unchanged:

```
FG171_GRAMMAR_CHAIN count: 0
FG171_GRAMMAR_SIZES cur_p=1 candidates=1 stacks=1
```

So either the function containing that assembly is not the one the server uses, or
`grmr` is null at that point in the server's path. The flag is inert, which is the safe
outcome, but it also means the placement question is still open: the next step is to
locate where the server actually assembles its sampler chain (the server sources here
contain no grammar wiring, so it may be assembled in a path we have not read).

Recorded as a factual result: nothing changed, and no claim of filtering is made.

## Correction: the grammar is a validator first and a filter on resampling

The previous conclusion ("never a filter") was too strong. `common_sampler_sample`
contains two grammar paths:

```
if (grammar_first && grammar_should_apply) llama_sampler_apply(grmr, &cur_p);   // full set
llama_sampler_apply(chain, &cur_p); id = ...
if (grammar_first) return id;
// validate the single sampled token
llama_sampler_apply(grmr, &single_token_data_array);
if (valid) return id;
// otherwise resample: reload logits, apply grammar to the full candidate set, rerun chain
```

`grammar_first` defaults to false and the server does not pass it, so the normal path
is: validate the sampled token, and apply the grammar to the full candidate set only
when that token is invalid. That is exactly why the instrumented run showed
`cur_p=1 candidates=1` - it was the validation call, which is the common case.

Both paths go through **our** `llama_grammar_apply_impl` hook, so the hook is exercised
on every token; what is untested is the resampling path with a full candidate set, which
needs a grammar and a model that actually produce invalid tokens often enough.

The earlier "not a filter" statement is corrected here; what stands is that the
single-candidate path cannot reject anything, so validation only observes whether the
chosen token is allowed.

## Both grammar paths exercised, output identical

With a dictatorial grammar (`root ::= "Z"`) and temperature 1.5 the instrumented run
shows both call shapes:

```
2  cur_p=1       candidates=1        (validation of the sampled token)
2  cur_p=248320  candidates=248071   (resampling with the full candidate set)
```

So the grammar **is** applied to the full candidate set on the resampling path, and the
Goose hook handles that call too. The output is the single character the grammar allows:

```
ON  'Z'   (grammar calls recorded)
OFF 'Z'   (no grammar calls)
```

The earlier diagnostic printed only every 200th call, which is why the full-set path
looked absent; that was a measurement defect, not a runtime fact, and it is the second
time a sampling rate in a diagnostic misled the conclusion.

What is still not measured: the cost of the full-set path, which is paid only when a
sampled token is invalid.

## Where the grammar cost comes from, and the measurement that would settle it

The two paths explain the earlier 3x: the permissive grammar `[a-z ]+` rejects any
uppercase or punctuation token, so the sampled token is frequently invalid and every
such token pays a full-set pass over ~248k candidates. The 16.5 tok/s figure therefore
describes the resampling path, not the validation path.

Native pays that pass too, but with a pointer walk; the Goose implementation walks the
same candidates with bounds-checked slice reads, which is what the ratio measures.

The decisive measurement, for the next iteration, is one paired run with a grammar that
rarely resamples (`root ::= [a-z ]*`, where empty is allowed and EOG stays legal) against
the same grammar with the hook off:

- if the two are close, the cost lives entirely in resampling and the stage can be
  enabled by default for permissive grammars;
- if they are still far apart, the per-candidate cost itself needs work.

No further optimization is attempted here, because the two possibilities require
different fixes and the measurement is cheap.

## The cost is resampling caused by our own rejections

Measured with the permissive grammar `[a-z ]*`:

```
on  16.39   off 55.14      (512 tokens)
FG171_GRAMMAR goose calls: about 500 for 512 tokens
```

One call per token, one candidate per call - and still three times slower. A call over a
single candidate cannot cost tens of milliseconds, so the time must be spent **outside**
the hook: `common_sampler_sample` resamples when the validation rejects, and a resample
reloads the logits and runs the whole chain again. The hook is very likely rejecting
tokens the native validator accepts.

First suspect: `fg_flatten` caches the flat rule table keyed by `&grammar.rules`. That
address can be reused between requests or after a grammar is rebuilt, in which case a
stale table is used and the decisions are wrong - rejecting valid tokens and triggering
resampling on every step.

Fix to try first: key the cache by content (rule count and the first and last element) or
rebuild it on every call, since the tables are tiny compared with the candidate pass.
Then re-measure: a correct validator should return to about 55 tok/s with a permissive
grammar.

Recorded with the evidence rather than patched blindly: the symptom is speed, the cause
is a wrong decision.

## Cache removed; the slowdown does not change

Rebuilding the flat table on every call removed a real staleness risk, but the timing is
identical:

```
on 16.86   off 54.83      (512 tokens, permissive grammar)
```

So the pointer-keyed cache was not the cause either. The state of the question is now:
one call per token, one candidate per call, and a 3x difference that cannot be explained
by the call's own work. What is needed is a direct start/stop timing pair inside the
hook, which I started and then reverted rather than leaving a half-instrumented
translation unit in the tree.

Hypotheses left, none tested:

- the resampling path is more frequent than the counter suggests (the counter prints
  only at fixed intervals, and this has already misled once);
- `fg_flatten` per call interacts with `grammar.rules` being rebuilt between steps;
- the difference is not in this hook at all, and `GGML_GOOSE_GRAMMAR` also gates
  something else on this path.

The next iteration should add a proper `start = now; ...; acc += now - start` pair, which
is a few lines and settles it in one run.

## Measured: about 84 ms per call, because the full-set path is the common one

Start/stop timing inside the hook:

```
FG171_GRAMMAR_TIME calls=128 avg_ms=159.061
FG171_GRAMMAR_TIME calls=256 avg_ms=83.769   total_ms=21444.8
decode=10.18 n=256
```

So the time *is* inside the hook, and it averages tens of milliseconds per call. With one
call per token, that means the full-candidate path (248k candidates) is being taken on
nearly every token: the permissive grammar still rejects uppercase, digits and
punctuation, so the sampled token is usually invalid and a resample follows.

84 ms over 248k candidates is about 340 ns per candidate, against a native pointer walk
that does one comparison. That is the whole 3x, and it is now measured rather than
assumed.

Consequences for the port:

- the decision is correct on both paths and the output matches native;
- the per-candidate cost is the remaining problem, and it lives in the generated C
  (bounds-checked slice reads, no vectorisation), not in the algorithm;
- optimising it means either a cheaper generated access pattern or reducing the
  candidate set before the grammar, both of which are separate changes.

The stage stays opt-in until that cost is addressed.

## Host-side optimisations exhausted; the cost is inside the generated loop

Measured again after removing the fixed codepoint stride and the blanket zeroing:

```
on 10.14   off 51.33      (256 tokens, before: 10.18)
```

No change. Together with the earlier attempts this rules out, one by one:

| hypothesis | test | result |
|---|---|---|
| per-token allocation | thread-local buffers | no change |
| stale rule cache | rebuild per call | no change |
| cost outside the hook | start/stop inside | 84 ms per call, inside |
| host array writes | stride + no zeroing | no change |

What remains is the generated loop itself: about 340 ns per candidate, which is a
function call and several bounds-checked slice reads per codepoint comparison. That is a
code-generation question for the Goose compiler, not something to fix by rearranging the
host.

Decision for this stage: functionally complete, measured 3-5x slower on the full-set
path, kept opt-in, and the performance item recorded as a compiler-level task. The
migration continues with the acceptance measurement for the sampler stages, where the
cost is already known to be about 1.3 %.

## Next optimisation design: Goose computes the allowed set, host applies it

The measured cost is the per-candidate loop inside the generated code, and four host-side
rearrangements have already been ruled out. The remaining design that keeps the decision
in Goose and removes the loop is:

1. Goose, once per token, derives from the current stacks the set of **allowed first
   codepoints** (as ranges, bounded to a small fixed count) - this is the grammar
   decision, and it is tiny compared with the candidate array;
2. the host applies that set to the ~248k candidates with a cheap C++ loop, rejecting the
   obvious mismatches before any per-candidate call;
3. only the surviving candidates go through the full automaton.

This keeps every grammar decision in Goose while paying the per-candidate cost in code
the compiler can optimise. It is a new entry and a new host loop, and it needs its own
gate against the native rejection (same candidates rejected, no more and no fewer).

Not implemented here: the size of the change and the need for a dedicated gate make it a
separate slice, and starting it at the end of this iteration would leave a half-built
path in the tree.

## Allowed-first-set entry, gated against match_char

`g_first_set` derives, from the current stack tops, the inclusive ranges of codepoints
that the grammar accepts as the next character. It refuses (-1) when the decision cannot
be expressed as positive ranges: an empty stack (end of sequence), an inverse class, or a
token rule.

```
first_set: cases=1200 fails=0
```

Four class shapes (exact, range, character alternation, any) probed over codepoints
0..299 against `llama_grammar_match_char`, so the ranges agree with the matcher rather
than with my reading of the format.

This is the piece the host needs to filter the ~248k candidates cheaply before any
per-candidate call; hooking it into the host is the next step, and the full automaton
stays the authority for the survivors.

## The first-set entry is dropped as unreachable (third time this class appears)

The entry was added to the module and the generator extended, but the generated C contains
no prototype for it: the only six-slice function is `gg_function_36`, which carries two
floats and is `sm_apply_dry`, not the first-set entry.

Reason: the chain build root calls the entries it needs explicitly, and
`fg_grammar_first_set` is not among those calls, so the compiler removes it - the same
rule as TD-S5 and the dispatch case. The fix is one line in the build root; recording it
here because the shape-based generator then failed with `matched 0 prototypes`, which is
the correct refusal: it will not guess.

The generator's failure mode is what I want: a shape that matches nothing stops the build
instead of wiring an arbitrary function.

## Pre-filter wired; it computes ranges but sees single-candidate calls

The host pre-filter is in place and the diagnostic confirms it works:

```
FG171_PREFILTER ranges=2  cur_p=1
FG171_PREFILTER ranges=-1 cur_p=1
```

Two ranges were derived for `[a-z ]+`, so the Goose first-set entry is reachable and
correct on the live path, and the second sample shows the refusal path (`-1`) when the
decision is not expressible.

What it also shows is that the calls observed are single-candidate calls, where a
pre-filter has nothing to save. The 84 ms average over 256 calls therefore cannot come
from calls shaped like these, so the next measurement must log `cur_p` on **every** call
over a window, not on a sample, to find where the time is. That is a one-line change and
it is the only thing that will settle the question.

The pre-filter stays: it is inert when the entry refuses, and on full-set calls it removes
candidates before the per-candidate work.

## Call distribution over the first 40 calls

```
26  ranges=-1  cur_p=1
 9  ranges=-1  cur_p=248320
 3  ranges=2   cur_p=1
 2  ranges=2   cur_p=248320
```

Two facts, both actionable:

1. **About a quarter of the calls carry the full 248k candidate set** (11 of 40), which is
   where the time goes; the single-candidate calls are cheap.
2. **The pre-filter refuses in 35 of 40 calls** (`ranges=-1`). Refusal means the top of a
   stack is an empty stack, an inverse class or a token rule, so the cheap path is not
   being taken in exactly the case that needs it.

So the optimisation to pursue is not "make the first-set faster" but "make it apply to the
common class shapes". The likely cause of the frequent `-1` is that `g_first_set` iterates
`nstacks` and refuses if *any* stack is empty or inverse, while the correct pre-filter
only needs the ranges that the surviving stacks accept - a candidate is acceptable if it
is acceptable for at least one stack, which is a union, not a refusal.

That is the next change: build the union of accepted ranges across the stacks, and refuse
only when *no* stack yields ranges.

## The refusal never comes from the Goose side

I encoded the two Goose refusal reasons into distinct negative values (`-t-100`,
`-t-200`) to see which one fires. The measurement still reports exactly `-1`:

```
13 ranges=-1 cur_p=1
 4 ranges=-1 cur_p=248320
 3 ranges=2  cur_p=1
 2 ranges=2  cur_p=248320
```

Since neither encoded value appears, the entry is either not being called at all for those
cases or the host's own guard sets `-1` before the call: the host requires every stack
element to be found in the pointer map, and it checks `!grammar.stacks[0].empty()` before
calling. Both are host-side conditions, so the frequent refusal is a host-guard effect,
not a Goose decision, and the previous "empty stacks" hypothesis is also ruled out.

Next step: log which host guard rejects (`call==nullptr`, stack empty, too many stacks,
pointer not mapped) instead of guessing between four conditions. That is one print and it
ends the loop this question has been going round.

The union change (skipping empty stacks) stays: it is correct on its own and costs nothing.

## Pre-filter now active everywhere, and that is the end of this road

After removing the host guard on `stacks[0]` the pre-filter runs on every call:

```
31 ranges=2 cur_p=1
 9 ranges=2 cur_p=248320
decode=7.24
```

No speedup. The reason is simple and worth stating: for the permissive grammar `[a-z ]+`
most candidate tokens start with a lowercase letter or a space, so they pass the range
check and still go to the automaton. A first-codepoint filter only helps a grammar that
rejects most first characters; for the common "text-like" grammar it filters almost
nothing.

The measured conclusion after eight attempts is therefore:

- the grammar decision is correct and gated on both paths;
- its cost is the per-candidate automaton over ~248k candidates, ~340 ns each;
- the first-codepoint pre-filter is now always available but does not help permissive
  grammars, and it cannot help by construction;
- the remaining lever is the generated per-candidate loop itself, which is compiler work.

The stage stays opt-in. Further work on this belongs with the compiler, not with more host
rearrangements, and this card now records eight tests that closed the alternatives.

## Pre-filter is correctness-neutral

```
[ON ] ' and the moon using the words tide and reflection is correct grammar usage and w'
[OFF] ' and the moon using the words tide and reflection is correct grammar usage and w'
```

Same 32-token output with the pre-filter enabled and disabled, so the cheap rejection does
not change the decision. It is kept as an always-on part of the hook because it removes
candidates for grammars that reject most first characters and costs two ranges checks
otherwise.

Final state of the grammar stage: correct on both paths (validation and full-set
resampling), correctness-neutral pre-filter, and a per-candidate cost that needs compiler
work before the stage can default on.

## Decision needed on P6-10's grammar item

Everything else in P6-10 is accepted with numbers. Two things remain, and both need a
decision rather than more code:

1. **Grammar default-on.** It is correct and its cost is measured: native and Goose both
   pass over ~248k candidates per token, but the Goose loop costs ~340 ns per candidate
   against a pointer walk. Making it acceptable needs the generated per-candidate loop to
   get about 3x faster, which is compiler work, not a rearrangement in the host (eight
   alternatives have been closed on this card).
2. **Removal of the replaced native sampler bodies.** The hard rules allow this only after
   full acceptance, and full acceptance includes the grammar stage.

The options are therefore: (a) accept P6-10 with the grammar stage documented as an
opt-in performance item and keep the native grammar body; (b) treat the compiler-side
speed-up as a hard prerequisite and leave P6-10 partial until it lands. Both are recorded
here; the choice belongs to Igor, not to an autonomous commit.
