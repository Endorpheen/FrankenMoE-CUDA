# EXP-2026-11-18-1210: the prefix cross-check is in place and does not fire yet

The cross-check compares the ported prefix decision with the loop's own stopping point, and the
native result is what returns either way:

```
FG_SP_PREFIX goose calls=N disagreements=M
```

## It does not fire, and why

A speculative generation with `--spec-type ngram-simple` produced 32 tokens and the counter stayed
silent. The server takes the **rejection** variant of the accept loop
(`common_sampler_sample_and_accept_n_rejection`), while the hook was placed in
`common_sampler_sample_and_accept_n`. The speculative path in this tree is rejection-based, so the
patched function is not called at all.

## The lesson, again and in a new place

The hook is correct and compiled; it is simply in a function the server does not use. That is the
third time in this migration that instrumentation was placed on a path that turned out not to be the
live one (the grammar diagnostic printing every 200th call, the pre-filter on a call shape that was
declined, and now this). The cheap defence is the one that worked twice: **check that the counter
moves** before drawing any conclusion from its silence.

## Next step

Patch `common_sampler_sample_and_accept_n_rejection` the same way - the accepted prefix there is
derived from the rejection decisions, so the comparison has to be expressed against its own count -
and confirm the counter moves on a speculative run before anything is claimed about the decision.
