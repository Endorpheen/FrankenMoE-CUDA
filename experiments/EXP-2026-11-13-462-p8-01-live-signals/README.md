# EXP462: live server signal and lifecycle evidence (P8-01)

The policies behind this evidence are EXP445 (startup/config), EXP446
(pre-routing/readiness/auth), EXP448 (signals/shutdown) and EXP456 (`/props`).
This experiment records what the running Apex profile actually does.

## Setup

    binary   runtime/exp421/unified/llama-server
    model    Apex-35B-V1.0-Q4_K_M.gguf
    args     -c 262144 -ngl all --cpu-moe -fit off -fa on -ctk/-ctv q4_0
             -b/-ub 4096 -t 8 -tb 16 --cpu-mask 0f0f --cpu-mask-batch ffff
             --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap
             --moe-cache-mib 3584 --host 127.0.0.1 --port 18093

Scripts: `tests/run-signal.sh <tag> <signals>` and `tests/run-endpoints.sh`.
Each run starts only its own process and stops three ways, confirming no process
is left behind (`pgrep` count printed in the result).

## Results

| run | action | result file | outcome |
|---|---|---|---|
| single | `/health` 200, one SIGINT | `p801-single.result` | ready in 5 s, exit **0**, cleanup logged, 0 leftover |
| double | `/health` 200, two SIGINTs | `p801-double.result` | exit **1**, `second interrupt` message, 0 leftover |
| endpoints | observe loading state, then endpoints, SIGINT | `p801-endpoints.result` | **503 while loading**, `/health` 200, `/props` 19 keys, `/metrics` 501, exit 0 |

Both shutdown paths match the EXP448 policy: one interrupt takes the ordered
graceful path and returns 0; the second bypasses it and exits 1.

## Reproduce

    bash tests/run-signal.sh p801-single 1
    bash tests/run-signal.sh p801-double 2
    bash tests/run-endpoints.sh

The scripts need the baseline runtime and the Apex model; they bind port 18093
and take about two minutes each.
