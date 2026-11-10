# Both GPU profilers are unavailable on this machine (2026-11-10)

The stale census (2026-11-09) and the MMQ slowdown both need a measurement of the *running* kernel.
Neither profiler can produce one here. Recorded with the evidence, because "the census is stale" and
"the profiler is blocked" are very different states and only one of them is work someone can resume.

## ncu — permission denied

    ==ERROR== ERR_NVGPUCTRPERM - The user does not have permission to access
              NVIDIA GPU Performance Counters on the target device 0.

Unblocking requires root or `NVreg_RestrictProfilingToAdminUsers=0` on the nvidia module. That is a
system change; the task rules forbid touching system settings. Nothing was changed.

## nsys — the server aborts under tracing

    APEX_DIAGNOSTIC_STARTED pid=3533020
    APEX_PHASE READY
    APEX_WARMUP_FAILED Remote end closed connection without response
    APEX_PROCESS_FINISHED -6

Exit −6 is SIGABRT. The backtrace ends in `ggml_abort` from `libggml-base` during
`llama_context::decode`, inside `server_context_impl::decode`. **No assertion message reached the
log**, so the abort is silent — ggml_abort fired without text.

Important distinction: this is **not** evidence of a defect in the port. The same binary on the same
profile outside the profiler is exact:

    exp220 normal run: 2567.91 / 26.47, tokens True, probs True, sha e68df05b

So the abort is specific to running under tracing. The most likely interaction is Nsight's CUDA
injection against this project's own allocator/pool path (VMM pool, pinned pool, host registration),
which is custom code that replaces the driver's default behaviour. That is a hypothesis, not a
finding — I did not isolate it, and after `ncu` already failed I stopped rather than open a second
investigation.

## Consequence

- The census cannot be re-taken here. The 2026-11-09 census remains the authority and remains stale.
- The cause of MMQ's 13.5% stays unestablished.
- What *is* available and was used instead: EXP180's kernel harness, which times the real entry point
  2000 times per arm and reproduced +13.5%/+14.2% with under 1% spread (EXP221 README). That gives a
  reliable number without counters; it does not explain it.

## If someone has the permissions

    export LD_LIBRARY_PATH=<repo>/runtime/exp180/run:$LD_LIBRARY_PATH
    ncu --metrics gpu__time_duration.sum,\
      sm__warps_active.avg.pct_of_peak_sustained_active,\
      dram__throughput.avg.pct_of_peak_sustained_elapsed,\
      smsp__inst_executed.sum,\
      smsp__average_warps_issue_stalled_long_scoreboard_per_issue_active.ratio \
      -c 4 --launch-skip 100 <repo>/runtime/exp180/build/timing

    GGML_GOOSE_CUDA_MMQ_Q8_1=1 <command>      # goose arm
    GGML_GOOSE_CUDA_MMQ_Q8_1=0 <command>      # native arm

For a census of the whole run, `nsys` needs the abort above resolved first — do not assume it will
just work.
