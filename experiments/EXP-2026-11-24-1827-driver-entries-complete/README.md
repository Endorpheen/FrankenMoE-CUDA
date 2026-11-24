# EXP-2026-11-24-1827 — Every driver entry measured: the remaining host time is inside libcuda, outside every entry point

Status: **MEASUREMENT, attribution complete as far as instrumentation reaches.**
Baseline `37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## The accounting, one 32768-token request

| segment | measured |
| --- | ---: |
| request wall | 12.3-12.4 s |
| user processor time | 13.38-13.50 s |
| system time | 0.23-0.28 s |
| context switches | 0 |
| CUDA API total (whole request, all entries) | ~5.5 s |
| — synchronising plan copies (`cuMemcpyHtoD_v2`, 1860 calls) | 4.90 s |
| — asynchronous expert copies (`cuMemcpyHtoDAsync_v2`) | **0.271 s** (155 667 calls, 1.7 us each) |
| — launches, events, synchronises, allocs, all others | ~0.3 s |
| mmap registration | 9.05 s, **start-up only** (EXP-1826) |

The async copy entry was the last driver door without a timer; it is now measured
(`GOOSE_UP_TRACE=1`, wrapper installed at the `EXPERT_LOAD` site) and it is
negligible: 155 667 calls, 95.7 GB, 0.271 s of host time.

## What that leaves

Request user CPU is 13.4 s while every driver entry point together accounts for
~5.5 s and the engine's own symbols account for 6.7 % of the samples (EXP-1818).
The instruction-pointer sampler still puts 88.3 % of the request's samples inside
`libcuda`, so roughly 6-7 s of user-space processor time runs *in libcuda but not
inside a timed entry point*. Nothing I can hook explains it, and the system time
is 0.25 s, so it is not syscall overhead either.

Two attempts to influence it failed outright:

- registering the expert mmap as writable (to avoid a possible driver bounce) is
  **refused** by the driver (`mmap expert registration failed (1)`) and the path
  degrades to 1996 tok/s prefill and 53.8 tok/s decode, so the read-only flag is
  mandatory and the bounce hypothesis cannot be tested that way;
- the five transport variants on the plan copies (EXP-1810, 1811, 1813, 1820,
  1825) moved nothing.

## Verdict on the 3500 target

The missing 1.13 s is not in any segment that instrumentation can name. The two
segments of that size or larger are (a) the 4.9 s of synchronising plan copies,
attacked five ways with no gain, and (b) the 6-7 s of driver-internal user time
with no hook, no syscall trail and no lever. The measured reserve is therefore
below the requirement on this configuration, and the target is unreachable
without changing what the box has to do — a larger expert cache on a larger card,
or a model whose expert set fits.

## Artefacts

`runs/upt2-070355` (async copy timer, 155 667 calls), `runs/ust-065953` (user, sys
and context-switch split), `runs/reg-065754` (registration is start-up only),
`runs/ro-070147` (writable registration refused), `runs/drvtime-235827` (sync plan
copies at 4.9 s).
