# EXP-2026-11-23-1814 — Ubatch-level expert prefetch: measured as unable to close the gap

Status: **NOT IMPLEMENTED, and the measurement says it cannot help.** Baseline
`37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## The request

Prefetch the expert set of ubatch N+1 while ubatch N computes, with double
buffering and the same stream order. Two facts have to be checked before writing
that code: whether the routes of N+1 are knowable during N, and whether the copy
would move fewer bytes.

## Fact 1: the routes of ubatch N+1 do not exist during ubatch N

A token's routed experts are produced by that token's own router, which runs
after the attention of its layer. For ubatch N+1 the router work is part of
ubatch N+1. The driver sees routes only after the host has computed them, which
is the same barrier that makes every staging decision serial. There is no
lookahead to exploit, exactly as EXP-1802 measured on the decode side (0 of
96 162 copies predictable from the previous token).

## Fact 2: the same ubatch's set is already fully re-used, and the copy would be the same bytes

The accepted run reports its own prefill staging counters:

```
Goose CUDA prefill temporary: 960 projections, 59856 resident planes,
  55334240256 uploaded bytes, 627 prefetched, 0 discarded
Goose CUDA prefill compact: 13 full-buffer projections, 13 prefetch skips for capacity
```

960 projections is exactly `8 ubatches x 40 layers x 3 planes`. So:

| quantity | value |
| --- | ---: |
| uploaded per request | 55.3 GB |
| uploaded per ubatch | 6.92 GB |
| expert set of the model | 19.5 GB |
| per-ubatch share of the set | 35 % |
| share already resident between ubatches | 65 % |
| traffic if nothing were resident | 156 GB |

Two consequences. The replacement policy already keeps 65 % of a ubatch's expert
set across ubatches, so "prefetch the previous ubatch's set" would copy the same
6.92 GB — a timing change, not a volume change. And the DMA is already overlapped
where it can be: the prefill window is 6.43 s of MMQ span with 3.98 s of kernels
and 2.29 s of copies inside a 6.03 s busy union, so only about 0.4 s of the
window is idle at the transport level. The remaining ~2.4 s of the window is GPU
idle *inside* the compute chain, which is dependency, not transfer.

## What the four rejected attempts already showed

| attempt | axis | result |
| --- | --- | --- |
| EXP-1810 | pinned ring for `upload_packet` | -2.95 % |
| EXP-1811 | pinned ring for `decode_upload` | +6.2 % at 4096 tokens |
| EXP-1813 | plan on its own stream | -0.19 % |
| EXP-1813 | plan on its own stream with two device slots | -0.08 % |

Transport and scheduling of the host side are therefore closed as an axis, and
the ubatch prefetch belongs to that same axis: it changes when the bytes travel,
not how many.

## What is left, stated plainly

Only two things can move prefill by the ~8 % still missing:

1. **Fewer bytes per ubatch**, that is a better eviction choice at the same
   6.29 GB (35 % miss share today). The policy lives in the Goose SLRU
   (`ec_prepare`/`ec_prepare_compact`), so it needs the Goose compiler to rebuild
   `main.o` and a Belady bound computed from an access trace to know whether the
   headroom exists at all.
2. **More capacity**, which the profile fixes at 6290 MiB and the 12 GiB card
   cannot extend beyond 6515 MiB (and 6510 MiB measured slower).

Neither is a driver change, and both need the owner's decision: the first needs
a heavy Goose build, the second changes the accepted profile.
