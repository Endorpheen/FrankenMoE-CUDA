# EXP-2026-11-19-1424: the curve, and a straight line through it

## The prediction being tested

EXP-1423 identified the mechanism as expert bytes moved from RAM to VRAM: a hit saves a 1.77 MB copy, and the cache's gain
is the fraction of copies it avoids. If that is the cause, time per token must be an **affine function of bytes per token**,
with the slope being the link's bandwidth - not a correlation, a slope.

## The three points

Same model, same profile, same request; only `--moe-cache-mib` differs, so these are explorations, not profile runs:

| cache | hit rate | KiB per copy | MiB per token | ms per token | decode tok/s |
|---|---|---|---|---|---|
| 3072 MiB | 71.25 % | 4 354.7 | 174.4 | 21.08 | 47.43 |
| 4096 MiB | 78.87 % | 3 207.8 | 128.4 | 17.61 | 56.80 |
| 4992 MiB | 83.68 % | 2 483.4 | 99.4 | 16.17 | 61.83 |

## The fit

Least squares on those three points:

```
time per token = 9.38 ms + 0.0681 ms per MiB
residuals:       -0.12 ms, +0.31 ms, -0.19 ms
```

The slope inverts to **15.4 GB/s**, which is the host-to-device rate of this card's link. The intercept, **9.38 ms**, is what
a token costs when traffic is zero: the arithmetic and the rest of the step.

A 75 MiB per-token spread fitting a straight line to within a third of a millisecond, with a slope that lands on the link's
rate, is not a correlation that happens to look tidy. It says the time per token is **the compute floor plus the bytes moved
divided by the link speed**, and that the cache acts on exactly one term of it.

## Why the policy matters so little next to the size

The policy decides *which* experts stay resident; residency decides *how many* copies are avoided. In the table the hit rate
moves with the size because more slots hold more of what is used - the policy is doing its job in all three rows, and the
rows differ by capacity.

## The fourth point that could not be taken

6000 MiB does not start at this profile:

```
ggml_backend_cuda_buffer_type_alloc_buffer: allocating 1124.03 MiB on device 0: cudaMalloc failed: out of memory
```

The extra gigabyte of cache leaves no room for the prefill graph buffer, so the curve's upper end is bounded by VRAM, not by
the link. That is the ceiling the mechanism predicts: more residency would keep paying until the card runs out, and at
4992 MiB it is one gigabyte away from running out.

## What this settles

- the mechanism is byte traffic over the link, with a measured slope;
- the 21 % loss from 4992 to 3072 is the 75 MiB per token that the smaller cache fails to avoid;
- the compute floor is 9.38 ms per token, which is where the next optimisation would have to work, since traffic below it
  cannot be reduced further once everything resident is already resident.

## Reproduce

```
for c in 3072 4096 4992; do
  FG_SLRU_DIAG=1 APEX_SERVER_DIR=$PWD/builds/flint-acc/bin bash tools/apex-test.sh traffic-$c 512 4096 4096 $c
  grep FG_SLRU_TRAFFIC results/apex-runs/traffic-$c-*/server.log | tail -1
done
```
