# EXP467: baseline control runs for P4-03

Two runs of the canonical Apex configuration (stage-run binary, exp244 argv plus
`--moe-cache-mib 3584`, 32 768-token prompt) to check that the accepted baseline
still reproduces before any base rebuild is attempted.

## Results, measured under load

A parallel compiler build was running during both runs. Load average on 32 threads
was 7.66 at the 128-token request and 3.66 at the 512-token one. The numbers are
therefore **measured-under-load** and are not clean baseline figures.

| run | prefill tok/s | decode tok/s | content sha | loadavg |
|---|---:|---|---|---|
| 128 | 2161.14 | 36.33 | `9d90a010…` | 7.66 |
| 512 | 2140.41 | 39.06 | `c2ff42cc…` | 3.66 |

Both content hashes match the recorded baseline exactly, so the execution path is
the accepted one. The absolute decode rates sit below the idle record (39.24 and
43.04) for both runs, consistently with the load; a clean comparison is deferred
until the host is idle.

These runs also serve as the same-load control arm for EXP466: with the fixed
`libllama` the paired values were 37.14 and 38.40 tok/s, within 2 % of these.

## What this does not establish

The base library is still the preserved deployed artifact (`e213fa19…`); the
reproducible-rebuild half of P4-03 remains open. This experiment only fixes the
reference point any rebuilt base must match.
