# Optimisation journal (small Apex path)

## Stage 1: control fixed

`config.json` records: git `799a77e`, compiler v61 `b87af17a`, artifacts and their SHA, build and run commands, the
model, the request, and the correctness expectation (`e68df05b`, equal probabilities, restore ok).

## Stage 2, finding 1: the regression is in libllama, and it is large

All measurements on the identical argv (`runtime/exp244/results-apex-or244-or-goose/argv.json`, 32K prompt / 128
decode), same machine state, browser load present and noted:

| libraries | prefill tok/s | decode tok/s | VRAM (own) MiB | content equal |
|---|---|---|---|---|
| current staging (libllama-exp371 + goose cpu) | 1588.7 | 21.93 | 6408 | yes |
| current staging + historical cpu (`*.inherited`, 63ad4984) | 1588.1 | 22.83 | 6408 | yes |
| **libllama-exp244 (2bc628a1) + historical cpu** | **2567.7** | **25.47** | **4590** | yes |
| libllama-exp244 + current cpu | 2570.0 | 25.35 | 4590 | yes |

So the CPU library contributes nothing to the loss, and libllama alone accounts for **+62% prefill, +16% decode and
-1818 MiB VRAM**. Both libllama variants are Goose builds (214 and 307 `fg` symbols), so this is not "native vs Goose" -
it is the newer libllama line being slower on the same profile.

Disabling suspected module groups inside exp371 did not recover anything (all within noise):
`GRAPH_INPUT_PLAN` 1586.1, `HYBRID_DELEGATION` 1547.6, and the batch/detok/sampler group 1598.5.

Also measured against the accepted EXP244 record (2500.1 / 24.51): the exp244 library still reproduces its own class
of numbers, so 2500-2570 is a reproducible baseline on this machine, not a historical artefact.

Next: find which code difference between the two libllama lines costs 62% of prefill, without reverting ported work to
C++.
