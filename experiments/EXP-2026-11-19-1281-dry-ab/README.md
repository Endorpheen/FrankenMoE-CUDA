# EXP-2026-11-19-1281: a real DRY defect, found by driving both paths through the public API

```
dry ab: 200 cases, every logit bit-identical (6721 lines)
apex: sha c2ff42cc1219, decode 45.33-46.23
```

## The method

The accepted DRY evidence compared the port with a C++ **restatement** of the native formula. This card drives
the shipped library through the public sampler API instead: one `llama_sampler_init_dry_testing` object, tokens
fed with `llama_sampler_accept`, candidates through `llama_sampler_apply`, twice per case - once with the Goose
door on (`GGML_GOOSE_SAMPLER_DRY=1`) and once off, which runs the native formula. The switch is a process-wide
`static const`, so the harness writes both runs to files and compares them line by line.

## The defect

The first run differed in 48 of 6721 logits, and the minimal case was three tokens long:

```
context [7,7,7,7,10], breaker {10}, allowed_length 2
  door on : tok 7 = -0.800000
  door off: tok 7 =  0.000000
```

`llama_sampler_dry_apply` builds `last[i] = last_tokens.rat(i)` - **newest first**. The ported
`sm_dry_rep_limit`, `sm_dry_repeat_counts` and `sm_apply_dry` read `last[n - 1 - x]`, i.e. they assume the array is
**oldest first**. With a single-token breaker at the newest position the native finds `rep_limit = 0` and returns
without penalising; the port walked off the other end of the array, found no breaker there, and applied a penalty
the native would never apply.

That is the shape of the failure: silent, plausible, and in a path enabled by default. Contexts that end with a
breaker - a newline, a space - are the common case, not the corner.

## Why the existing gate could not see it

The DRY step-1 harness in EXP-1188 defines its own `rat(k) = last[NN-1-k]` and computes the expected value with
it. That mirrors the array **in the harness**, matching the mirrored port, so both sides agreed 200/200. The
harness adapted its input to the implementation instead of modelling the caller:

```
auto rat = [&](int k){ return last[k]; };   // what the real caller means
```

With that one line changed, the harness still passes against the corrected port, and it now models
`llama_sampler_dry_apply` rather than the port's assumption.

## The fix

Five index expressions, all in the same direction: `last[n - 1 - x]` becomes `last[x]` in the limit scan and the
count scan, and step 3 keeps the native expression `last[n_last - 2 - i]` because that expression was already
correct for a newest-first array. My first attempt changed step 3 as well and dropped the mismatches only from 48
to 46 - which is what showed the step-3 index had never been part of the error.

## After the fix

- the A/B is bit-identical on all 6721 logits;
- the step-1 harness passes with the caller's convention;
- the full sweep is **12 passed, 0 failed**;
- the acceptance hash is unchanged (`c2ff42cc1219`), because the fixed profile sets no DRY multiplier, so the
  sampler never reaches this code - the defect was real but dormant in the acceptance configuration.

`FG171_DRY` does not appear in the acceptance log at all, which is the same statement from the other side: the
path under test here is not on the acceptance route, so its correctness had never been exercised by an official
run.
