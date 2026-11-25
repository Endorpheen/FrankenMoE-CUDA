# EXP-2026-11-25-1942: Attribute Inkling late prefill time

This is an EXP-1940 diagnostic on RTX 4070. Context stays at
36864, batch and ubatch at 4096, expert limit at 6720 MiB and
activation arena at 736 MiB. The full frozen 32768-token
prompt is unchanged. The output budget is one token so the
capture ends after prefill and the vocabulary projection.
The generated ID and text prefix match the saved EXP-1940
response. The measured prefill was 217.53 tok/s under the
instrumentation. The unprofiled EXP-1940 result remains the
throughput baseline.

The first request used pretty-printed JSON and hit HTTP 413
before evaluation. Reissuing those same IDs in compact JSON
produced a 205575-byte request and completed.

## Host phases across all eight microbatches

Elapsed phase ranges include work submitted to CUDA and
waits for its results. They are disjoint within a visit.
The final-microbatch NVTX ranges cover all 48 visits.

| Host scope | Total, ms | Final microbatch, ms |
| --- | ---: | ---: |
| Attention graph | 71675.8 | 9730.5 |
| MoE graph | 53165.2 | 6664.9 |
| Input preparation | 7994.9 | 985.4 |
| Preparation between convolution and MoE | 4163.4 | 517.0 |
| Post-attention convolution | 2039.1 | 263.6 |
| Whole microbatch wall | 150637.2 | 19596.5 |

## CUDA timeline of the last 4096 inputs

Classify each interval using only CUDA activity recorded
inside the NVTX range. The categories below are disjoint.

| Activity | ms |
| --- | ---: |
| Kernels only | 10828.3 |
| No recorded GPU work | 4523.7 |
| H2D only | 2085.8 |
| D2H only | 1931.1 |
| Multiple activity classes | 226.8 |
| D2D only | 0.6 |

The main summed kernel families are dense Q8 projections
(4362.6 ms), parallel-relative prefill Flash (2660.8 ms),
and grouped IQ4 MoE matrices (3069.5 ms). They are already
included in the timeline. Within Attention, 1006.8 ms is
H2D-only and 964.2 ms D2H-only. MoE has 1078.9 ms H2D-only
and 966.9 ms D2H-only. There are 96 D2H copies of 37748736
bytes, matching two full 4096-by-2304 FP32 activation
readbacks per visit. The trace also reports 448 H2D copies
of the same row block size.

## Follow-up tests

1. Measure the full activation copies with pinned and
   pageable destinations at 36 MiB. Keep the original
   graph and preserve the activation values and completion
   dependency. Reuse a single bounded staging buffer if it
   improves device service and end-to-end host time. Removing
   the activation boundary requires migrating downstream
   depth mixing, with its ordered arithmetic preserved.
2. Target Q8 input reuse across several output rows per CTA.
   EXP-1938 ruled out a simple paired-loop change for the
   current wide batches. Device tests should keep the FP32
   FMA order and show a throughput improvement across the
   actual shapes before testing the full request.
3. Track Attention separately by position and kernel family.
   EXP-1941 removed repeated Q4 decode inside full Attention
   with little measured request gain. Recheck the new copy
   and compute timeline after broader dataflow changes.

This measurement does not infer that all 4523.7 ms of
unrecorded GPU time can be eliminated. It does not measure
decode or qualify an Apex performance gate. The frozen
Apex checksum manifest passed. System swap-out and process
swap were zero. The diagnostic peaks were 11765 MiB board
VRAM and 14807072 KiB RSS.

## Reproduction

Apply `source/instrument.py` to the local EXP-1937 host source
linked against the EXP-1940 archive using the diagnostic build
helper. Only this diagnostic links NVTX. Generate compact
32768/1 JSON from the frozen 32768/512 request by changing
`n_predict`. See `inputs.sha256` and `results.json` for exact
identities and arguments. Extract the Nsight report to SQLite
and run `analyze-prefill.py` on its directory.

The raw artifacts are under
`build/inkling-exp-baseline/device-attention/inkling-late-prefill-profile/profile/late-prefill-compact`.
No runtime or deployment settings were changed.
