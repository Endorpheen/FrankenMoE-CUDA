# EXP-2026-11-25-2059 — Long control for Bonsai FP16 MMQ partials

**Experimental, not promoted.** Reuse the exact EXP-2057 broad cached
MMQ prototype and artifact hashes. It keeps FP16 operands but rounds K32
partials to FP16 before adding them to the persistent FP32 accumulator.
This follow-up measures full workload speed and one independent factual
extraction diagnostic. It reuses the known CodeTrace result and replay.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair with the same executable and CUBIN,
the EXP-1977 raw request, context 36,864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. PrismML is the separate saved control.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, FP32 accumulation | 737.64 | 32.55 | 60.145 | 8932 | 7457476 |
| B, K32 FP16 partials | 815.16 | 32.71 | 55.842 | 8932 | 7457524 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Prefill improves 10.51% in this pair, from 44.423 to 40.198 s.
The complete request saves 4.303 s. All 512 IDs and response text
match across A, B and PrismML. Both FrankenMoE processes have
zero swap after the request.

## Targeted quality

The saved EXP-2057 CodeTrace diagnostic scores 18/63 in A and 13/63
in B, with a matching prefix of 59 IDs. At the first difference,
same-history replay gives control ID 15 and candidate ID 22.
The separate incident-extraction task is fully correct in both
modes, with all 47 output IDs matching. All nine sampled full-vocabulary
rows on the same history agree on top-1 and top-10, with maximum
absolute logit difference 0.4468.

This mixed evidence retains the candidate as experimental. A passing
long token sequence and one correct extraction task are insufficient
to promote changed arithmetic. Keep EXP-2049 as the accepted route.
Its prefill parity gap is still 13.37 s. Do not combine this numerical
gain with projected savings from other experiments. Prioritize routes
that preserve arithmetic for the working baseline. No other model
path changes.

## Reproduce

Build the EXP-2057 artifacts and use its HTTP profile. Set
`BONSAI_MMQ_HALF_ACC=0` for A and `BONSAI_MMQ_HALF_ACC=1` for B.
Send the exact EXP-1977 32K/512 request in each process. Run
EXP-2057's `source/quality.py` with `incident-extraction` as its
only case to reproduce the additional diagnostic.

[Full-request evidence](data/results.json), [fact extraction](data/incident.json),
and [saved CodeTrace](../EXP-2026-11-25-2057-bonsai-mmq-fp16-accumulator/data/quality.json).
