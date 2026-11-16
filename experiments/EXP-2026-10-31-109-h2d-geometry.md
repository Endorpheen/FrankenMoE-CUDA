# EXP-2026-10-31-109: real prefill H2D geometry and bounded-gap coalescing

Status: NOT_RUN in this session. The previous EXP102 rejection is NOT valid as
stated and is retracted here.

With batch-level id aggregation, 32 unique selected experts make one skipped
expert cost 1/32 = 3.125% of the copied bytes, below a 5% cap; the old 8-per-
token argument only bounds a per-token run. A counter diagnostic in
llama_context::sched_copy_experts (ids size, unique experts, original runs, gap
sizes, extra bytes, submission time) and a bounded-gap implementation were not
executed. No performance claim and no rejection.
