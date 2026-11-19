# EXP-2026-11-19-1381: speculative decoding on Apex, measured - and what is out of profile

## In profile, and therefore comparable

`tools/apex-test.sh` with `APEX_SPEC_TYPE` (default `none`, so no earlier result changes):

| configuration | decode (3 requests) | prefill | hash |
|---|---|---|---|
| none (record) | 62.73 median of nine, EXP1373 | 2951 | `c2ff42cc1219` |
| `ngram-simple` | 56.55 (56.93, 56.55, 52.99) | 2429-2913 | `0818aff9faef` |

N-gram drafting is **about 10 % slower** inside the accepted profile. The request is a 32K-token prompt with almost no
repetition, which is the case n-gram drafting cannot exploit: it pays for proposals and accepts few. The hash differs,
which is expected - speculative decoding changes how many tokens are sampled per step - but it means every acceptance
run under speculation needs its own correctness reference rather than the old hash.

## Out of profile: explorations, not improvements

These were run before rule 35 was written and are recorded as explorations. **None of them is a claim against the
record.**

| configuration | decode | note |
|---|---|---|
| short prompt, ctx 4096, b 2048 | 77.97 | a twenty-token prompt: far less KV to attend over, not a faster runtime |
| short prompt, ctx 262144 | 74.36 | same |
| short prompt, ctx 4096, b 4096 | 75.13 | same |
| 32K request, ctx 40960 | 61.81 / 60.54 / 34.97 | within noise of the record; the 34.97 run also carried a slow-prefill outlier |

The 77.97 figure was briefly read as a 24 % configuration gain. It is not: it was measured over a sequence two orders
of magnitude shorter, which is the comparison the project's rules forbid and the exact mistake rule 35 now encodes.

## MTP: it does not fit the accepted profile

`--spec-type draft-mtp` on Apex, at the accepted profile:

```
failed to initialize the context: failed to allocate compute pp buffers
failed to create MTP context
```

The NextN head needs a second context with its own compute buffers, and at context 262144 with a 4992 MiB expert cache
there is no room on a 12 GiB card. At a 40960 context the server starts but returns an empty reply during generation.

Under rule 35 this is **technical debt of MTP**, not a reason to shrink the context: the head's own footprint is what
has to come down - its pp buffer, its batch, or a shared KV - before an in-profile measurement of MTP exists.

## Next, in order

1. measure the MTP head's memory need at the accepted profile, so the debt has a number;
2. try the cheapest reductions inside MTP only (its micro-batch, its context share);
3. if none fits, record it and continue the migration - the record stands either way.
