# EXP-2026-11-23-1811 — Where the prefill host time really is, and two rejected transports

Status: **INVESTIGATION plus two REJECTED candidates.** The baseline stays
`37ac0f1d` (prefill 3122.4, decode 78.225, owner-accepted).

## The processor time is in the driver

Instruction-pointer sampler (`tools/sampler.c`, 1 ms of processor time) over one
32768-token request, resolved against `/proc/self/maps`:

| mapping | samples | share |
| --- | ---: | ---: |
| `libcuda.so.580.178.04` | 18 204 | **70.0 %** |
| `frankenmoe-infer` | 1 929 | 7.4 % |
| `libc.so.6` | 1 432 | 5.5 % |
| unresolved (anonymous) | 4 439 | 17.1 % |

So the Goose per-token loop, the module arithmetic (`gg_div`, `gg_mod`) and the
ownership check are not where prefill spends its processor time: 70 % is inside
the CUDA driver, called from a single server site.

## The single driver site is `decode_upload`

Wrapping the `upload` pointer with a return-address histogram
(`GOOSE_TIME_TRACE=1`, 4096-token prompt, two requests):

```
Goose driver time: upload_sync 1560 calls 2.56 s
Goose sync site 0x52f7f8: 1560 calls 1.51 GB 2.557 s (1.64 ms each)
addr2line -> decode_upload  (cuda-driver.c:781)
```

780 synchronous copies per 4096-token request, about 1 MB each, 1.28 s of host
time in a 1.56 s prompt. Everything else in the driver is rounding error: the
expert staging is pinned and asynchronous (`submit_expert_stage` 0.051 s for two
requests) and the SLRU plan costs 0.22 s.

## Both transport fixes were tried and lost

| candidate | change | result |
| --- | --- | --- |
| EXP-1810 | pinned ring for `upload_packet` up to 8 MiB | prefill **-2.95 %**, rejected |
| this card | pinned ring for `decode_upload` above 4096 bytes | 1658.7 ms against 1562.5 ms at 4096 tokens (**+6.2 %**), rejected |

The second one was verified engaged — the server prints
`Goose CUDA: job ring active (pinned 4 slots)` — so the loss is not a silent
fallback.

## Why the transport does not matter

The copies are not bandwidth- or submission-bound, they are **serialised by
dependency**: each 1 MB copy feeds the kernel that follows it on the same stream,
so the host blocks until the GPU has drained what came before, and queueing the
same bytes asynchronously only moves the wait later and adds a pinned memcpy.
The GPU is busy only about half of the prompt window (0.8 s of kernel and DMA
work inside a 1.56 s request at 4096 tokens), and that idle time is the
turnaround of this chain, not a transport cost.

The lever that remains is therefore not "make copies asynchronous" but "have the
next copy in flight while the current kernel runs", that is a graph-level
pipeline across ubatches or layers, or fewer copies by construction. Both are
structural changes and neither is attempted here.

## Artefacts

`runs/rip3-210012` (maps + samples), `runs/sites-210254` (driver time and the
single site), `runs/upring-210346` (rejected ring at 4096 tokens),
`runs/engage-210419` (engagement message).
