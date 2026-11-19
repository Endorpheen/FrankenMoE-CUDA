# EXP-2026-11-19-1249: the prompt-lookup drafter against the real native function

`bmoe_core` exports `bmoe::ngram_draft`, and the ported door `sdp_ngram_draft` exists in
`goose/misc/spec-decode-decisions.goose`. This is the first decision in the speculative family verified against
real native code rather than against my reading of it:

```
ngram draft differential: cases=3000 fails=0
```

3000 random cases: contexts of 4-43 tokens over a five-symbol alphabet (so repeats are frequent and matches
actually occur), `n_max` 1-8, `min_match` 1-3, `max_match` from min to min+3. The comparison is exact: the
returned length and every drafted token, against `std::vector<int32_t>` from the native side.

## Why this matters more than the earlier symmetric gates

The native function is compiled and called. Nothing in this path depends on my transcription being right, which
is the failure that produced a false "154 disagreements" and then a false "4 disagreements" in the same day.

It also needs no model and no GPU: `bmoe_core` builds with `llama.cpp present = OFF`, so this differential runs in
seconds and can be re-run after every change to the module.

## What it covers in the roadmap

- P9-05's draft lifecycle: the proposal side is now verified against the real engine;
- P6-11's draft decision: same function, same evidence;
- the acceptance and width decisions around it remain gated synthetically (sdp_accept, sdp_mtp_width) - their
  native counterparts are static in the engine and not exported, which is the same limitation recorded for
  `match_expert`.

## Reproduce

```
bash experiments/EXP-2026-11-19-1249-ngram-draft-diff/build.sh
```
