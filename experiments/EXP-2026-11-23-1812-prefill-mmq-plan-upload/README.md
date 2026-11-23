# EXP-2026-11-23-1812 — The remaining prefill cost is the per-projection MMQ plan upload

Status: **INVESTIGATION.** No candidate. Baseline `37ac0f1d` (prefill 3122.4,
decode 78.225) unchanged.

## Who issues the copies

Return-address histogram inside `decode_upload` (`GOOSE_UP_TRACE=1`, one
4096-token request, binary built `-no-pie` so the addresses resolve):

```
Goose upload caller 0x534e1e: 612 calls 1366.19 MB   -> launch_mmq      (cuda-driver.c:3271)
Goose upload caller 0x52d809: 124 calls   69.33 MB   -> upload_packet   (cuda-driver.c:2045)
Goose upload caller 0x5318dd: 350 calls    4.00 MB   -> fg_cuda_load_dense (cuda-driver.c:2351)
```

The dominant class is `launch_mmq`: 306 uploads per request of about 2.23 MB,
one per projection, each synchronous on the legacy stream. That is the 1.28 s of
host time measured in EXP-1811 inside a 1.56 s prompt at 4096 tokens.

## Why the transport fixes could not help

Every one of those uploads feeds the kernel launched immediately after it on the
same stream, so the host blocks until the GPU has drained what came before. Two
transport-level attempts were measured and rejected (EXP-1810: -2.95 %;
EXP-1811: +6.2 % at 4096 tokens, with the ring verified engaged). Moving the
bytes asynchronously only moves the wait and adds a pinned memcpy, because the
copy and the consumer are 50 us apart in the same dependency chain.

## The structural lever, and where it has to live

The overlap the measurements ask for is "pack the plan of projection k+1 while
projection k runs". That cannot be done in `cuda-driver.c`: the driver only sees
the plan after the host has built it, and the build happens in the Goose module,
in the projection loop of `goose/cpu/goose-cpu-backend-api/matrix.goose` (the
call site at line 332 that feeds `fg_cuda_expert_batch`, and the MMQ packing
ahead of `launch_mmq`).

Concretely: double-buffer the plan (two device buffers, two pinned staging
slots), give Goose a "begin pack for slot s" / "submit slot s" pair, and let the
packing of the next projection run while the current kernel executes. Bytes and
kernel arguments are unchanged, so the result stays bit identical.

Expected size of the prize: removing the host block from the prefill loop takes
`prompt_ms` from 10557 to about 9300 at 32768 tokens — 1.28 s of 10.5 s — which
is **about 3540 tok/s**, that is the 3500 target.

Cost and risk: it is a change in a Goose module, so it needs the Goose compiler
to rebuild `main.o` (the composed `main.c` is 46 MB), a heavy build by the
repository's own rules, and the module has to be validated with
`goose check` before that.

## Artefacts

`runs/caller-210538` (histogram), `runs/sites-210254` (per-site driver time),
`runs/rip3-210012` (mapping attribution: 70 % libcuda).

## Correction after reading the composed unit

The uploader is `launch_mmq` in `cuda-driver.c`, that is the driver itself, not a
Goose module: the composed `release/linux/main.goose` does not contain
`goose-cpu-backend-api/matrix.goose` at all (`grep -c` returns 0 for every symbol
of that module), so the per-projection plan is packed and uploaded inside the
driver. The pipeline therefore *is* implementable in driver code, and that makes
the two rejected attempts more interesting, not less.

## Blocker, stated for the record

Two driver-level attempts to overlap the plan copy with the preceding kernel were
measured and lost, both with the fast path confirmed engaged:

| attempt | mechanism | result |
| --- | --- | --- |
| EXP-1810 | pinned ring for `upload_packet` up to 8 MiB | -2.95 % prefill |
| EXP-1811 | pinned ring for `decode_upload` above 4096 bytes, four slots, event fence | +6.2 % at 4096 tokens |

A two-slot pipeline that defers the copy to the *next* call is the next shape to
try, but the reason the simpler ring loses is not isolated: the copy is already
ordered behind the previous kernel on the legacy stream, so the host should have
been free to pack the next plan while the GPU worked, and it was not. Until that
is explained, further variants are trial and error.

Per the repository rule for a repeated blocker, this needs either the senior
helper's diagnosis of why an asynchronous plan copy on the legacy stream reads
slower than the synchronous one, or a decision to move the packing itself off the
critical path (precompute the plan for projection k+1 during kernel k).
