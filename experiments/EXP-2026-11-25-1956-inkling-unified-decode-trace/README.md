# EXP-2026-11-25-1956: Attribute decode gaps in the combined Inkling runtime

**Verdict: diagnostic trace.** Prioritize the repeated activation
round trips between the attention and MoE graphs.

Profile the exact EXP-1953 combined executable on 8192/256 at context
36864, Q4_0 K/V, 6720 MiB experts and a 736 MiB activation arena on
RTX 4070. A launch wrapper recorded an invalid PID on the first attempt;
no request ran. On the next attempt prefill failed under Nsight at
ubatch 4096 with a device allocation/output-transfer error. The final
capture used ubatch 2048, with the same fixture and other settings.
This capture is for attribution. Use EXP-1955 for unprofiled throughput.

The trace recorded 245 padded logits readbacks of 804096 bytes each.
The following categories partition the final 128 intervals between
the ends of those transfers. The modeled window is 6887.83 ms.

| Timeline class | ms per decode step |
| --- | ---: |
| CUDA kernel only | 26.314 |
| H2D only | 6.194 |
| D2D only | 0.395 |
| D2H only | 0.182 |
| Overlapping activity classes | 0.281 |
| No GPU activity recorded | 20.445 |
| Total | 53.811 |

Of the 20.445 ms without GPU activity, 5.477 ms occurred inside
copy APIs, 2.319 ms inside launch APIs, 0.729 ms inside explicit
synchronization, 0.050 ms inside other CUDA APIs and 11.869 ms
outside recorded CUDA APIs. Those categories describe the timeline;
they do not imply that any entire category is removable.

## Repeated boundaries

| Neighboring operations | Gaps per step | Idle ms per step |
| --- | ---: | ---: |
| 9216-byte activation D2H → 9216-byte H2D | 96 | 4.709 |
| Padded logits D2H → first activation H2D | 1 | 2.305 |
| IQ4_XS expert kernel → expert H2D | 21.1 | 2.184 |
| Vector kernel → 9216-byte activation H2D | 360 | 1.921 |
| 32-byte routing ID D2H → small H2D | 47.95 | 1.360 |
| Expert H2D → expert H2D | 25.23 | 1.084 |

The activation transitions are two per logical visit. In
`inkling-pinned-activation-readback/inkling-backend.c`, the single-token
`layer()` takes the attention output back to the host for
`i2_postconv_token()`, residual/history bookkeeping and `mix()`,
then uploads MoE input. The MoE output returns to the host before
the next attention visit. Preserving decode graph objects did not
remove these activation round trips. A minimal candidate should
keep one bridge on device by moving its post-convolution and
depth-mix operations together with their state. Validate the
history order, loop resets, state fingerprints and output IDs
before comparing the full 8192/256 response. The measured
4.709 ms is an upper bound for these particular idle intervals,
before the cost of any device implementation.

The 2.305 ms request-internal boundary after logits includes host
logit processing, sampling, diagnostic hashing and next-token
preparation. Isolate those costs before changing the sampler.
The 32-byte routing copies are another independent dependency.
Their copies themselves sum to 0.042 ms per step, while gaps
immediately following those copies total 1.371 ms. Capture more
overlap only where dependencies permit it.

Expert transfers use 2820096- and 1492992-byte planes, around
24.19 each per step, with 5.941 ms summed service. This service
can overlap and is already included in the timeline above.
The leading summed kernels are Q8 projections (7.167 ms),
Flash (5.348 ms), IQ4_XS expert (5.029 ms), vectors (4.986 ms) and
IQ4_NL expert (2.210 ms). A single bridge is insufficient by itself
to close the unprofiled 9.25 ms parity gap in EXP-1955.

## Checks and reproduction

All 256 output IDs, response text and 8703 state fingerprints
match EXP-1955. The profiled request reported 266.85 prefill and
18.42 decode tok/s. Peak board VRAM was 11542 MiB and peak
process RSS 14247936 KiB. Process swap and system swap-out were
zero. No Apex or shared runtime code changed.

Use the same binary and request as EXP-1955, the exact argv and env
in `results.json`, and Nsight Systems 2024.5.1. Launch under
`nsys profile --delay=24 --duration=65 --kill=none --trace=cuda
--sample=none --cpuctxsw=none --stats=false`. After health is ready,
submit one 8192/256 request. Terminate only the test server after
receiving the response and export the completed report with
`nsys export --type=sqlite --output inkling.sqlite inkling.nsys-rep`.

The raw report is under `build/inkling-apex-gate/decode-nsys-1956/ub2048`.
Use EXP-1933's `analyze-decode.py` for the timeline partition and
`python3 analyze-gaps.py /path/to/inkling.sqlite` for the neighbor
analysis. The receipt and both summaries are committed here.
