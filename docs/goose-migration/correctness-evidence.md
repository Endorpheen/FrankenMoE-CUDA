# Correctness evidence of the active runtime

What is actually compared on each fixed 32K/128 Apex run, and what the last one recorded.

## Compared, not asserted

The runner writes both the response and the reference into the run directory, so each claim can
be re-checked from the artefacts rather than trusted from a summary field:

| quantity | how it is compared | last value |
|---|---|---|
| token ids | full list against the saved oracle | equal, 128 tokens |
| output text | string equality, plus sha256 | equal, `e68df05b…` |
| completion probabilities | `response['completion_probabilities'] == reference[...]` | equal |
| first differing index | computed, reported | `None` |

The probability comparison is the strictest of the three and it is not a stub: every one of the
128 entries carries five `top_logprobs`, and the whole list is compared element-wise against the
saved wholly-C++ oracle. First token of the last run:

    token ',' logprob -0.00022611
    top5  ','  -0.00023   '.'  -8.816   ' check' -10.29719   '\n' -10.75411   '\n\n' -11.29759

So the port reproduces not just the chosen tokens but the model's distribution over the top five
at every position. That is a much stronger statement than matching 128 greedy outputs, and it is
what makes "the Goose modules compute the same thing" testable across re-builds.

## What it does not cover

- **The reference is a saved oracle, not a live pair.** Performance is the only quantity measured
  in two arms in one build; correctness is against the recorded oracle. A live C++/Goose
  correctness pair would need the native route for every module at once, which the switches
  allow, but the oracle comparison is what the project has used throughout.
- **`concurrent_load` is recorded as "other host activity uncontrolled".** Absolute timings are
  comparable only within a back-to-back pair, which is why every figure quoted in these documents
  comes from one.
- **128 tokens is the fixed profile.** Positions beyond 128 are not exercised.

## Reproducing

    cd experiments/EXP-2026-11-08-196-goose-cuda-pinned-pool
    FG_STAGE=exp196 FG174_MEASURE_MODE=goose FG_SUFFIX=-x python3 -u run_apex_pair.py

and then read `summary.json` in `runtime/exp196/results/apex-pinned-x-goose/` alongside
`response.json`, which contains the full probability list.
