# EXP-2026-11-15-737 — speculative decoding: MTP unavailable, ngram slower, draft-simple parity

Base: `11e55cee`. The model carries `nextn_predict_layers = 1`, i.e. an MTP head, and the reference
profile sets `--spec-type none` - so speculative decoding has never been exercised in this project.
Four variants tried at ub 2048 / cache 5376.

## 1. Results

| `--spec-type` | prefill | decode | VRAM | content sha | verdict |
|---|---:|---:|---:|---|---|
| `none` (baseline) | 2019.3 | 42.35 | 7972 | `8c50dd2d` | - |
| `draft-mtp` | - | - | - | - | **fails to load** |
| `ngram-simple` | 2020.9 | 39.35 | 7992 | - | **-7.1%** |
| `draft-simple` | 2020 | **42.51** | 7972 | `8c50dd2d` | parity |

`draft-simple` measured twice, six runs total: median 42.51, range 42.23-42.72, against the baseline's
42.35 and range 41.97-42.56. **The ranges overlap; this is parity, not a gain.**

## 2. MTP fails, and the reason matters

```
E common_speculative_init_result: failed to create MTP context
E srv    load_model: failed to create MTP context
```

The MTP head exists in the weights (41 blocks of decoder plus one nextn block, which is exactly why
`n_layer = block_count - nextn` in EXP-699), but the runtime cannot create a context for it. It needs
its own draft parameters which the reference profile does not carry. That is a configuration gap in
this build, not a property of the model - and it is the one speculative path that would plausibly
beat parity, because draft-mtp uses the model's own head rather than heuristics.

**Not pursued further in this card**: it would need a separate context with its own KV settings, and
its own correctness gate. Recorded as an open item rather than attempted at the end of a run of
experiments.

## 3. ngram-simple is simply worse

39.35 against 42.35, -7.1%, with slightly more VRAM (7992). With a single 32K prompt and no repeated
n-grams in the output, ngram matching has nothing to exploit and pays the bookkeeping. No trade here,
just a loss.

## 4. draft-simple is safe but pointless

It produces a **byte-identical answer** (sha `8c50dd2d` in both arms, 2241 chars) at parity speed. So
it is not a risk - but it is also not a gain, and enabling it would add a code path with no measured
benefit. Not adopted.

That the answer is identical is worth noting on its own: speculative decoding is designed to be
output-preserving, and this confirms the implementation in this build is.

## 5. Balance and decision

| axis | baseline | draft-simple | change |
|---|---:|---:|---:|
| prefill | 2019.3 | 2020 | +0.0% |
| decode | 42.35 | 42.51 | +0.4% (inside spread) |
| VRAM | 7972 | 7972 | 0% |
| content | `8c50dd2d` | `8c50dd2d` | identical |

+0.4% inside an overlapping range is not evidence. **No configuration change.**

## 6. Open item

`draft-mtp` is the only speculative mode that could plausibly be worth more than parity, and it is
blocked on a missing MTP context configuration rather than on anything fundamental. Closing it needs:
the draft context parameters this build expects, a correctness gate on the output hash, and a
measurement at the three lengths.

## 7. Cost

Four configurations, nine successful runs, one failed start.
