# Measurement status EXP175: resolved

The speed discrepancy recorded here earlier is now measured, not assumed, and it
is resolved. Two facts decide it: the same unchanged EXP174 binary recovered its
baseline once the terminal stopped contending for CPU, and EXP175 is marginally
faster than EXP174 under identical clean conditions.

## Clean-condition comparison

Both arms run the original EXP174 runner outside ghostty, one fixed-profile
request each, 32768 prompt tokens and 128 predictions.

| Build | Prefill tok/s | Decode tok/s | CUDA DSO sha256 | Cores used | CPU MHz | Threads |
|---|---:|---:|---|---:|---|---:|
| EXP174 (previous checkpoint) | 2610.76 | 27.46 | `0b187bc1cca2…` | 8.02 | 3712-4591 | 54 |
| EXP175 (CONCAT in Goose) | 2625.73 | 27.58 | `55e1beae086f…` | 8.05 | 4421-4730 | 54 |
| Delta, new versus old | **+0.573%** | **+0.437%** | | | | |

Against the historical 2663.00 / 27.86 baseline both arms sit inside the 2%
tolerance: EXP174 at -1.96% / -1.43%, EXP175 at -1.40% / -1.02%. Tokens, text
and all returned top-5 probabilities are bit-exact in every arm; response content
sha256 is `e68df05b`.

The EXP174 arm above loaded the same `libggml-cuda.so` hash that was recorded at
the original 2663/27.86 measurement (`0b187bc1cca2…`), so its recovery is
attributable to the environment, not to a rebuild.

CPU telemetry taken during the decode window shows 8.02 and 8.05 cores in use,
which matches `-t 8`, with per-core frequencies 3712-4591 and 4421-4730 MHz. The
expert threads were therefore not short of cores in either arm.

## Cause established by measurement

The earlier arms ran under a terminal that rendered through 32 llvmpipe threads,
consuming 1947% CPU on average and 2414% at peak, roughly 20-24 of the 32 cores.
With `--cpu-moe` the expert arithmetic runs on CPU, so that contention directly
competed with the expert threads. Removing it returned the unchanged EXP174
binary from 18.70 to 27.46 tok/s decode with no code change.

Arms measured under that contention, retained as contaminated observations and
not used as performance evidence: interleaved A/B 2189.82/18.70,
2231.25/19.39, 2251.26/17.10, 2222.58/18.86; same-DSO pair 2150.66/18.31 native
and 2210.01/19.03 Goose.

Two harness faults of my own are recorded and were fixed: the earlier sampler
called `nvidia-smi` four times per second instead of once per five seconds, and
it did not capture per-thread affinity, core frequency or per-thread CPU time
during decode.

## Owner corrections that shaped this

1. Mask `0f0f` spans eight distinct physical cores here, not four.
2. The Apex process itself had zero swap; system-wide swap occupancy did not
   prove the cause.
3. `powersave` with amd-pstate-epp, `balance_performance` and boost enabled is not
   proof of a forced low clock. Measured frequencies were in fact 3.7-4.7 GHz.
4. GPU idle clocks say nothing about inference speed.
5. An independent re-run at load 1.7-2.7 still reported 25.10 rather than 27.86,
   so a slow machine alone was not the whole story; here it took removing the
   competing renderer to recover the baseline.

## Evidence captured per run

`run_apex_recheck.py` is the original EXP174 runner with only the output
directories parameterised by `FG_STAGE`/`FG_SUFFIX`, so the one-`nvidia-smi`-per-
five-seconds cadence is preserved. It writes, per run: `argv-actual.json`,
`env-actual.json`, `loaded-library-hashes.json` (resolved paths, not symlinks),
`thread-affinity.json` (every thread's `Cpus_allowed_list`), `cpu-topology.json`,
`cpu-telemetry.json` (per-second cores used, frequencies, load average) and
`memory.json`. KEEP the measurement host free of CPU-rendering terminals for any
future Apex number.
