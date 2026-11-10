# EXP234: MMQ Q8_1 kernel-level A/B, six alternations — the route costs ~10%

Kernel-level, not Apex: `runtime/exp180/build/timing` launches the real entry point 2000 times on a fixed
Apex-like shape and reports ns per launch. This harness spawns its own process and reads
`GGML_GOOSE_CUDA_MMQ_Q8_1` directly, so unlike the Apex runner it is **not** affected by the env stripping
documented in `measurement-env-stripped.md`. The DSO under test is `runtime/exp225/run`, whose Goose arm is
the u64 source with the `ne2` fix from EXP222.

## Six alternations, native first then goose, repeated three times

| native | goose |
|---:|---:|
| 1838.6 | 2121.7 |
| 1959.8 | 2215.4 |
| 1900.5 | 2018.7 |
| 1994.2 | 2019.8 |
| 1889.8 | 2168.3 |
| 1913.2 | 2130.3 |

    native  mean 1916.0  median 1906.8  spread 8.5%
    goose   mean 2112.4  median 2126.0  spread 9.7%
    delta   +10.25% mean, +11.49% median

## What this does and does not settle

**Settles:** the Goose MMQ Q8_1 route is slower than native by roughly 10%, measured six times with the
same harness and with the arms interleaved through both slots. Every goose reading exceeds every native
reading except one overlap region (1959.8 native vs 1994.2 native), so the direction is not in doubt even
though the per-run spreads overlap.

**Does not settle:** the cause. EXP222 showed the instruction weight was a dead `ne2` branch (400 → 164
instructions, native 157) and EXP223 showed that cutting further bought no time; those two results are why
"The emission is 2.6× too big" is no longer on the table. `ncu` still refuses counters on this machine —
retested this session with `--replay-mode application` and the same `ERR_NVGPUCTRPERM`, and
`/sys/module/nvidia/parameters/restrict_profiling` does not exist here, so there is no unprivileged path.

## Consequence for the plan

MMQ Q8_1 stays native, and this is now a measurement rather than an inference from Apex numbers that
turned out to compare one configuration against itself. The remaining MMQ question — the `mul_mat_q`
family at 40% of kernel time — is a separate, larger port that has not been started.

The honest chain for anyone picking this up:

1. the feeder costs ~10% at kernel level (this file);
2. every 23% instruction reduction after the `ne2` fix bought nothing (EXP223);
3. so the next thing to try is not smaller code but a different shape — fewer per-element branches, or
   moving the checks out of the element loop;
4. and it cannot be validated with counters on this machine, only with this harness.

## Recording

Six numbers per arm, the medians, and the spreads are all above, so anyone can redo the statistics. I am
recording `+10.25% mean / +11.49% median` rather than a single figure because the two differ by a point and
neither is privileged.
