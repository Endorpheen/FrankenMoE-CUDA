# EXP-2026-11-19-1361: the codepoint window widened, and what it bought

## The change

`FG_MAXCP` was 16 - a reading limit in the grammar hook, not a storage limit, since `code_points` is a vector. A piece
with more than 16 codepoints was judged by its first 16, so the port could accept a piece the native code rejects for
its tail. That was the recorded cause of the last 203 disagreements of 1.7 million candidates.

The window is now 64. The stride is `maxcp_used`, computed per call from the pieces actually present, so widening the
limit costs nothing on calls whose pieces are short - the comment at the allocation site already says why a fixed
stride was rejected.

## Measured on the real vocabulary

A constrained request (`response_format: json_object`) against Apex, hook counters on:

```
FG171_PREFILTER        40 calls
FG171_TRUNC seen=1     one piece in forty calls fills even a 64-codepoint window
answer                 {"a": 1, "b": 2}
```

At 16 the same run counted truncation repeatedly; at 64 it is a single piece. The request itself is constrained and
well-formed, so the grammar is doing its work on the real model.

## What the run also shows

`FG171_GRAMMAR_SIZES` printed nothing: that counter sits behind `fg_grammar_enabled()`, so the ported matcher is still
opt-in and the native decision ran. The prefilter hook is active either way, which is where the truncation is counted.

## Why the ported matcher stays opt-in

Its cost was measured at 11.7x the native path on a constrained request. That is a decision about the default rather
than a defect, and rule 10 says an accepted improvement is enabled by default - this one is not accepted on cost
grounds, and the card says so instead of leaving the flag's existence unexplained.

## What remains for grammar in P6-10

The truncation mechanism is now bounded by the vocabulary rather than by a constant: one piece still fills the window.
Raising the window further costs nothing until a piece appears that needs it, and the counter will say when one does.
