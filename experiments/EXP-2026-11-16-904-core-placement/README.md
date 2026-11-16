# Moving the run off the background-loaded cores made it worse, not better

Tested the one remaining hypothesis for the 27-31 vs 42 gap: the fast run's saved argv carries `--cpu-mask 0f0f`
(cores 0-3, 8-11), while the host's heavy background processes (python3.14 at 218% CPU, igorek-factory at 45%) run
with affinity 0-31 and can occupy those same cores.

    same profile, processes confined to cores 16-31:  decode 22.21   prefill 1418.09   hash c2ff42cc1219
    same profile, default placement:                  decode 27.53-31.30   prefill 1561-1788

So confining the runtime to cores the background load does not favour did not help - it made the result worse by
roughly 20-30%. The contention hypothesis is not supported by this measurement.

What that leaves: cores 16-31 are evidently worse for this workload than 0-11 (CCD/NUMA placement, or the cache
state of the cores the model was warmed on), and the gap to 42 is not explained by which cores are busy with other
processes.

Not claimed: that the 42 point is unreachable, or that core placement is the cause. One measurement, one alternative
placement, and it points the other way. What it does establish is that the "move it to free cores" fix does not work,
so it should not be tried again as a remedy.

Recorded because a negative result that is not written down gets tried again.

Co-Authored-By: CommandCodeBot <noreply@commandcode.press>
