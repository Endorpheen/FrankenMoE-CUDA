# EXP-2026-11-24-1911: Retain Inkling prefill activations in a larger device arena

An isolated Inkling profile compares the existing 64 MiB and 384 MiB
activation arenas at the same ubatch. Both arms disable parallel CPU
preparation. The executable and every other setting are identical.
The measured input has 512 prompt tokens and 16 output tokens,
and the context is 1024. Batch and ubatch are 512. The target
is Inkling-2-Mini AD-IQ4_XS, Q4_0 K/V and RTX 4070. Both arms keep
the exact ordered sampler scan enabled.

| Two paired means | 64 MiB | 384 MiB |
| --- | ---: | ---: |
| Cold prefill, tok/s | 73.98 | 110.34 |
| Warm prefill, tok/s | 82.41 | 130.24 |
| Cold decode, tok/s | 10.90 | 10.82 |
| Warm decode, tok/s | 10.85 | 10.95 |

Prefill improved in both orders. All four launches matched their 1086
fingerprints, generated IDs and text. Expert traffic and fills were
identical. Reported board peak was 9562–9564 MiB in A and 9858 MiB in B.
Process RSS did not increase, and all swap counters were zero. Decode
was flat within the paired variation. No global arena default changed.

The exploratory CUDA trace at ubatch 512 with the original arena and
parallel CPU preparation exposed about 4.53 GB of D2H payload across
240 transfers of 18 MiB in the warm prefill window. The traces motivated
the capacity test. They do not prove the exact number of copies eliminated
by the sequential candidate.

An exploratory parallel-preparation variant reached around 170 warm
prefill tok/s, but independent repeats disagreed in their numerical
fingerprints. Keep that configuration out of acceptance. The measured
512-token result does not establish 500 prefill or 25 decode tok/s.
The default runtime and Apex binaries remain unchanged. A combined
runtime regression gate is still required.

## Reproduce the isolated profile

Use the executable from the paired Inkling sampler experiment, identified
by the SHA-256 in `inputs.sha256`. Use the first 512 tokens of the frozen
32K benchmark input, with the sampler parameters of the short regression
request and a 16-token generation budget. The exact normalized request
checksum is in `inputs.sha256`.

Run `--ctx-size 1024 --batch-size 512 --ubatch-size 512 --cache-type-k q4_0
--cache-type-v q4_0`. Enable the same resident-plan, shared-attention,
resident-router, vector-constant and decode-expert-overlap settings as
the sampler candidate. Set `I2_PARALLEL_PREP=0` in both arms, with
`GOOSE_CUDA_ACTIVATION_MIB=64` for A and 384 for B. Keep the
weights in the OS page cache for both arms. Compare one cold expert-cache
request and one repeated request after each separate server start.
