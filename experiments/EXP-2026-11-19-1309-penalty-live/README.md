# EXP-2026-11-19-1309: the penalty door is reached on real tokens

The baseline profile leaves the repeat penalty at its default, so the penalties sampler is never added to the chain
and the door has no live counter in any accepted run. That is a gap in the evidence, not in the code, and this card
closes it with a deliberately different profile.

## Run

One lifecycle of the shipped binary, the fixed profile plus `--repeat-penalty 1.15 --repeat-last-n 64`, one 64-token
request. Functional check only: the profile differs from the baseline by design and nothing here is compared with it.

```
FG171_PENALTIES goose calls=1
FG171_TOP_P     goose calls=1
FG171_DIST      goose calls=1
```

The counters print on the first call and every hundredth after, so one line each means all three doors were reached
in the first decoding step, and the request returned 1881 characters without error.

## What it establishes

The penalty door is exercised on real tokens by the shipped server, not only by harness-built arrays. With the
candidate-array differential for the same door (EXP-2026-11-19-1284) the door is now verified in both worlds.

## The native A/B, run afterwards

Two lifecycles, identical in everything except the door's own switch, request 96 tokens with
`--repeat-penalty 1.15 --repeat-last-n 64`:

```
door ON    FG171_PENALTIES present;  output 449 chars
door OFF   FG171_PENALTIES absent;   output 449 chars
identical: True
```

So the penalised generation is the same either way, and the counter's presence/absence confirms which path ran. That
is the evidence the first version of this card asked for.

A note on why the first attempt was useless: it took the prompt from the baseline request, whose token-array form ends
after nine characters. Reachability was shown, equality was not, and a nine-character answer would have been a weak
claim dressed as a result. The request was changed to a text prompt long enough for the penalty to have history.

## What it does not establish

Nothing about performance: this run used a different profile on purpose.

## Housekeeping

The script starts the server in its own session and terminates the group on exit. Checked afterwards: nothing listens
on 9101, and the unrelated `igorek-llama-server` on 8022 was neither started nor touched.
