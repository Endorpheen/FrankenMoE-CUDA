# EXP329: make_qp_quants in Goose, with an honest account of what the gate does and does not cover

`make_qp_quants` is the helper that picks the Q4_K block scale and block min. It is static in `libggml-base`, so no external
oracle exists; the acceptance for it will be the end-to-end Q4_K encoder gate against the exported `quantize_row_q4_K_ref`.

## What is ported

The max scan, the zero short-circuit (`max < 1e-15`), the initial `nmax/max` scale and rounded levels, the mean-squared-error
selection over `is = -4..4` (skipping 0), the weighted `sumlx`/`suml2` pass and the final `sumlx/suml2`, with `MAX`/`MIN` as the
native macros and `nearest_int` as the reference's 12582912 trick (round half to even), not `roundf`.

## Gate

    EXP329_QP checks=12400 failures=0

400 random cases across n = 8/16/32/64 and nmax = 15/63, with zeroed cases, outliers, skewed weights and squared inputs mixed in.
The oracle is a **verbatim copy of the checked-in source** -- a transcription check, not an artifact check -- which is why the
end-to-end gate remains the real acceptance.

## What the gate does NOT cover, stated rather than implied

Two mutations were attempted and both were *unobservable* on this corpus, which is a finding about the gate rather than evidence
of correctness:

- changing the search step from `0.1f*is` to `0.2f*is` changed no result;
- perturbing the refinement acceptance test by `+1.0f` changed no result.

Extending the corpus (outliers, 25x weights on the outlier, alternating 0.01/3.0 weights, squared inputs) did not make either
mutation observable. The likely reason is that the initial levels from `nmax/max` are already a local optimum for this weighted
least-squares objective, so the five-iteration refinement block never flips a level on these inputs. That block is therefore
**not exercised** by this gate, and it is the part most likely to harbour a transcription error.

The end-to-end Q4_K encoder gate is the only way to settle it, because it compares against the exported reference on real data
rather than against a copy of the source on synthetic data.
