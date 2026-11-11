# APEX.CPU_CONTROL: the exact remainder after the build was made reproducible

State: the Goose `libggml-cpu` rebuilds from source in this tree (326 symbols, identical set to the
inherited one), the candidate runs with it, and all four witnesses (FG143, FG147, FG149, FG150) are live.

## What is actually still native

The item names "expert lifecycle/ownership and the native-removal boundary". Checked where the code is:

| module | state |
|---|---|
| EXP143 control plan | **wired**: `fg143_control_plan` is called from the C path, 43 call sites in the backend |
| EXP145 backend interface | **wired**: `set_threadpool`, `set_abort_callback` and others route through `fg145_call` |
| EXP149 job pool (`fg149_pool_control`, `fg149_job_kickoff`, `fg149_action`, `fg149_system_alloc/free`) | **not wired**: `grep` finds no call to any `fg149_*` in `goose-live/source/cpu-backend`, yet the symbols are exported |
| EXP150 expert registry | live (11 report lines) through the interface module |

So the "expert lifecycle/ownership" half is exactly the job-pool module: it exists, it is in the library,
and nothing calls it. That is the same defect class as the unicode routes (EXP215–217) and the hybrid file:
implemented and exported, never routed.

## Next step, concrete

1. Find where the CPU MoE experts are split across threads in this tree — the profile runs `--cpu-moe`, so
   the split is the used path. Candidate sites: the graph-compute entry (`ggml_graph_compute_threads`),
   the pool's job kickoff, and the expert loop in the MoE op.
2. Route that decision through `fg149_job_kickoff` / `fg149_pool_control` with the existing switch, keeping
   the native path behind `=0`.
3. Rebuild EXP149 and EXP150 with `reproduce/build.py` (both now work end to end), replace the library,
   and require: the four witnesses plus `FG149` non-zero, completion `e68df05b`, exact restore, and one
   metrics checkpoint.

Until step 2 is done, the item stays PARTIAL. The native-removal boundary then means deleting the fallback
bodies once the switch has been exercised — not before.
