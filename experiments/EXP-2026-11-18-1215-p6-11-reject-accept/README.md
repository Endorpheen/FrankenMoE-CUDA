# EXP-2026-11-18-1215: rejection acceptance, the decision the server actually runs

After the build-source mistake was found, the live accept branch is the **rejection** routine. Its
decision, ported:

```
q_x <= 0                                  -> reject   (the native code guards the division)
p_x >= q_x                                -> accept
u < p_x / q_x                             -> accept
otherwise                                 -> reject
```

```
reject accept: cases=5006 fails=0
```

5000 random cases plus six boundaries, each chosen because a plausible rewrite differs there:

| case | why it is in the test |
|---|---|
| q zero never accepts | writing `u < p/q` without the guard divides by zero; writing `p >= q` first would accept |
| p above q accepts | the shortcut branch |
| p equals q accepts | `>=`, not `>` |
| u below ratio accepts | the ratio branch |
| u above ratio rejects | the other side |
| u equals ratio rejects | strict `<`, the only place `<=` would differ |

## Wired, and the counter still does not fire

The host hook is in `common_sampler_sample_and_accept_n_rejection`, the symbol resolves (the format
string is in `libllama-common.so`), and the merged server calls that function once per verify step -
but no counter line appeared.

The likely reason, from the dispatch conditions: the rejection branch requires
`use_spec_rejection() && !slot.spec_draft_q.empty()`. The branch diagnostic reported `reject=1`, which
is only `temp > 0`; if `spec_draft_q` is empty for ngram drafters, the plain `_n` overload runs instead,
and the hook is in neither.

That is the next thing to check - one line in the branch diagnostic printing `spec_draft_q.size()` -
and it is a different question from the decision itself, which is now gated.
