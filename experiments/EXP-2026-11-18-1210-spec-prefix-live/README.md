# EXP-2026-11-18-1210: the prefix cross-check, its first firing and the defect it exposed

## Which path is live depends on temperature

`slot.use_spec_rejection()` is `temp > 0`. So:

- `temp > 0` -> `common_sampler_sample_and_accept_n_rejection`, which is what the earlier attempt
  patched the wrong function for;
- `temp = 0` -> `common_sampler_sample_and_accept_n`, where the accepted prefix is a token-agreement
  prefix, and where the hook now sits.

With `temp = 0` and `--spec-type ngram-simple` the counter finally fired:

```
FG_SP_PREFIX goose calls=1 disagreements=1
FG_SP_PREFIX_DIFF goose=-1 native=7 draft=20
```

## The disagreement was mine, and it is the interesting part

`goose=-1` is the door refusing, not disagreeing: I asked it to compare `k = draft.size()` tokens
while the target array only holds the tokens actually sampled (seven, plus the one that broke the
loop). `sp_prefix` refuses when `target.len < k`, correctly. Bounding `k` by both lengths is the fix.

## Where this leaves the decision

After the fix the counter did not print on the next run, so the path was not exercised again in that
attempt, and the live agreement of the prefix decision is **still unmeasured**. What is measured is the
gated synthetic comparison (7 cases) and the fact that the hook is reachable and fires when the greedy
route runs.

Recording it this way matters: "the counter fired once and said -1" could be read as a defect in the
port, and it was a defect in my harness. The distinction is only visible because the door's refusal
value (-1) is distinct from a disagreement (0 or 1).

## Second attempt: the counter did not fire at all

After bounding `k` and with the print widened to the first five calls, a repetitive prompt (so the
ngram draft certainly produces tokens) at `temp = 0` produced **no counter line**. The same binary had
printed `calls=1` on the earlier run, so the hook is reachable and something in the route differs
between runs: the server has several accept branches (`synth_probs`, replay, rejection, plain) and the
plain one is taken conditionally.

## Conclusion: this is a control-flow question, not a print question

Three iterations have now gone into placing instrumentation on speculative paths, and each attempt
found a different branch. The honest state:

- the door is exported, resolvable and called at least once (proved);
- the ported decision is gated synthetically (7 cases) and proven equal to the native loop's stopping
  point on those;
- **live agreement is unmeasured**, and finding the right branch needs a deliberate read of the
  speculative control flow rather than more prints.

That read is the next step, and it is worth doing once properly: `slot.use_spec_rejection`, the
`synth_probs` branch and the replay path together decide which accept function runs, and the
`FG_SP_PREFIX` counter is a cheap way to confirm the answer afterwards.

## The control flow, read properly

The server picks the accept routine in this order:

```
if (!synth_probs.empty())            -> server_sample_and_accept_synth(...)
else if (replay && use_rejection())   -> server_accept_replay(...)
else if (use_rejection())            -> common_sampler_sample_and_accept_n_rejection(...)
else                                 -> common_sampler_sample_and_accept_n(...)
```

and `use_spec_rejection()` is `temp > 0`.

With `--spec-type ngram-simple` the run reports `"speculative.types":"none,ngram-simple"` and decodes
at about 80 tok/s, so speculation is working - but the counter stays silent. The ngram drafters fill
`synth_probs`, so the **synth** branch is taken and neither accept routine runs. The one firing seen
earlier came from a run in which the synth branch did not engage.

## Why the prefix decision cannot be verified live here

`common_sampler_sample_and_accept_n` is the **draft-model** route (`--spec-type draft-simple` /
`draft-mtp`), and that needs a draft GGUF whose vocabulary matches the target. The models on this
machine are Apex-35B and Edge0-120B, and the only MTP assets present are Edge0's `dense.bin` and a
`draft_vocab.bin`, not a draft model for Apex.

So: the ported prefix decision is gated synthetically (7 cases against the native loop) and its live
route is **not reachable in this environment**. That is an environment limitation to record, not a
defect, and it is the honest reason the live number does not exist.

The synth-probability acceptance, which IS the live route for ngram speculation, is a different
decision and has not been ported.
