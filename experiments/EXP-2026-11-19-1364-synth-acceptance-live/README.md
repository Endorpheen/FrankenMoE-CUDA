# EXP-2026-11-19-1364: the synthetic acceptance path on the real model - three attempts, not enabled

## The goal

`FG_SP_SYNTH` compares the port's acceptance decision against the native one per accepted position. Its record says
"synthetic and rejection acceptance gated synthetically only - their routes need a draft model". A draft model is not
needed for the **synthetic** route, only an initialized speculative context, so a live count looked reachable.

## What was tried, in order, with the exact result

| attempt | flags | result |
|---|---|---|
| 1 | `--spec-synth-rates` (6 values) | `load_model: synthetic acceptance requires an initialized speculative decoding context` |
| 2 | `--spec-synth-rates` (6) + `--spec-type ngram-simple` | `failed to initialize speculative decoding context: synthetic acceptance rates must contain 48 values, got 6` |
| 3 | 48 rates + `--spec-type ngram-simple`, repetitive prompt, two requests of 128 tokens each | server ran, both requests completed, **`FG_SP_SYNTH` never printed - zero calls** |

So the two load-time requirements are now known exactly (an initialized spec context, and one rate per draft
position - 48 for this build), and the run-time one is not met by either prompt.

## What the third attempt rules out

- the flags itself: the server loaded and answered, so the configuration is valid;
- a short run: 256 generated tokens over two requests is enough for drafting to happen if it was going to;
- prose alone: the second attempt already used a four-fold repeated sentence with an explicit "repeat it" instruction,
  which is the shape `ngram-simple` exists to exploit.

## Remaining hypotheses, none acted on

1. The synthetic branch sits behind a slot field (`slot.spec_draft`) that is only filled when the drafter proposes,
   and `--spec-type ngram-simple` proposed nothing on this vocabulary/text pair.
2. The synth rates are accepted at load and then stored per slot only for a **draft model** configuration, leaving
   `synth_probs` empty for an ngram-only context - which would make the message at load time misleading rather than a
   precondition.
3. The acceptance branch is reached only when `use_spec_rejection()` is true, and the synthetic route bypasses the
   check that sets it.

## Attempt 4 - and the precondition found

One specific suspicion was checkable rather than guessable: `run.sh` itself passed `--cache-ram 0`, and an ngram drafter
looks its history up in that cache. With the flag removed and the branch diagnostic on:

```
FG_ACCEPT_BRANCH synth=48 replay=0 reject=1 draft=12
FG_ACCEPT_Q draft_q=0
```

So the synthetic branch **is** reached, with 48 probabilities and a 12-token draft: `--cache-ram 0` was what prevented
drafting in attempts 1-3, and that is now recorded as a precondition rather than a hypothesis.

`FG_SP_SYNTH` still did not print, and the difference is no longer in the path: a sibling speculative door in the same
run printed (`FG_SP_NMAX goose calls=1 disagreements=0`), so door resolution works in this binary while this one's does
not. The next step is the resolve print inside `fg_sp_synth_resolve()`, and it is blocked for now: rebuilding
`llama-server` needs the UI asset target, whose download fails without network
(`ui-assets.cmake:137`, `dist.tar.gz.sha256`). The diagnostic edit was reverted rather than left in the tree
uncompiled.

## Why stop here

This is the third attempt and the repository's blocker rule applies: report the exact behaviour, the attempts, and the
hypotheses instead of trying a fourth guess. The measurement is not obtained and the record now says so with the
evidence, which is better than a number produced by a path nobody can explain.

## Files

`run.sh` reproduces attempt 3 exactly; attempts 1 and 2 differ only in the flags named in the table.
