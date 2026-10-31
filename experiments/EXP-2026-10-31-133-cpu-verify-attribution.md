# EXP-2026-10-31-133: CPU MoE verify attribution

Status: DIAGNOSTIC_COMPLETE, no baseline promotion.
Isolated EXP107 CPU library with node/barrier elapsed counters and existing
route-count occupancy; no arithmetic, transfer, scheduling or placement change.
Four manual requests (ordinary/MTP1,32K128/code256), all resource gates0 and
exact IDs/top5/probabilities versus corresponding uninstrumented profiles.

Ordinary single-token CPU nodes accumulate6.758s Q4_K +1.958s Q6_K across the
process including initialization/warmup/two requests. MTP two-token nodes
accumulate6.849s +1.855s. These are node intervals, not isolated dot timings.

Only46566 pairs across397488 two-token routed rows:23.43% of rows have a partner;
at most11.72% fewer weight processing instances through ideal two-row fusion.
Existing16-output-row tiles already reuse weight cache lines; DRAM savings
cannot be equated with unpack reuse. No achieved speedup from this diagnostic.

EXP107 restored with all nine mapped hashes matching. Next: exact shape/weight
microprobes and an optimized C/C++ control for each Goose algorithm candidate.
[Evidence and limits](../results/exp133-cpu-verify-profile/README.md).
