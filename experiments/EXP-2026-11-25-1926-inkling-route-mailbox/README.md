# EXP-2026-11-25-1926: Validate an Inkling expert route mailbox

**Verdict: INCONCLUSIVE for throughput.** Keep EXP-1925 as the
accepted checkpoint. The isolated mailbox prototype removes most
individual route readbacks on the fixture and preserves correctness.
The repeat decode gain is too small to promote the candidate.

## Implementation

Inkling sorts 128 experts with the generic Goose sort. The separate
CUDA entry calls that sort, keeps the normal eight IDs in device
memory, then publishes a copy and a sequence counter into mapped
pinned memory. The counter uses a system-scope release store and the
host polls it with acquire loads. The host checks the exact pending
host address, device address and 32-byte extent before reading from
the mailbox. A bounded wait falls back to the original device
download. The cache policy and ordering are unchanged. Eligibility
requires the Inkling specialized path and one 128-to-8 sort. Use
`I2_ROUTE_MAILBOX=0` for the original dispatch.

One correctness request at 2048/64 explicitly enabled
`I2_VERIFY_ROUTE_MAILBOX=1`. By the 3000th logged read, the host
had verified 2942 publications against the GPU output and exercised
11 forced fallbacks and 47 natural timeouts. The 64 output IDs
and text prefix match EXP-1925. A first performance pair accidentally
enabled diagnostics through a default-on option helper. Its timings
were discarded. The final binary makes verification opt-in.

## Clean timed A/B

RTX 4070, frozen EXP-1917 2048/256 fixture, context 4096, batch and
ubatch 2048, Q4_0 K/V and EXP-1923 resident 576 MiB arena. The
final binary ran A–B–B–A using a first and repeat request per launch,
with full prompt computation and matched state hashing. Timed
mailbox logs show zero forced fallbacks and zero direct validation
downloads.

| Paired mean | Control | Mailbox |
| --- | ---: | ---: |
| First prefill, tok/s | 202.56 | 202.08 |
| Repeat prefill, tok/s | 213.56 | 213.33 |
| First decode, tok/s | 14.063 | 14.094 |
| Repeat decode, tok/s | 14.042 | 14.072 |
| Repeat decode, ms | 18160.0 | 18120.6 |
| Repeat client request, s | 27.959 | 27.932 |

The candidate saved 33.5 and 45.3 ms of repeat decode, about
0.22% in the paired mean. First-request time moved in opposite
directions in the two pairs. At the 24000th logged read there
were 388 and 396 timeouts in the two candidate arms. The 98.4%
hit rate establishes that the mailbox worked. End-to-end timings
put a small bound on its standalone benefit here.

All 5118 fingerprints per launch, both sets of 256 IDs and text,
and expert fills and bytes match EXP-1925. Within each pair VRAM
peaks were equal, at 10246 and 10232 MiB. The difference across
arm maxima was 1040 KiB of RSS. Swap and swap-out were zero.
The frozen Apex checksum gate passed. Apex performance and the
Inkling 32K/512 candidate remain unmeasured in this experiment.

## Reproduction

Apply the driver patch to the isolated EXP-1925 driver, copy
the mailbox OpenCL file and use both build scripts. The image
wraps the frozen Goose-emitted routing code under
`build/exp1887-attention-q4-loads/device`. The kernel has 40
registers, 16896 bytes of shared memory and zero spills.
See `inputs.sha256` and `results.json` for exact binary and
run receipts. The code is retained as an experiment with the
working profile still on EXP-1925.
