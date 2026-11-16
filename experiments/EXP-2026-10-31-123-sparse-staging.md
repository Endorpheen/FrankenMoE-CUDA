# EXP-2026-10-31-123: sparse physical backing for expert staging

Status: NOT_RUN.

EXP121 attributes ~1370 MiB of retained 32K memory to compute/mask/graph/staging
together, but does not isolate the staging destination capacity or its physical
pages. The required per-destination diagnostic (logical capacity, selected
expert union, padding, aliasing, lifetime) was not built, so no sparse-mapping
candidate was implemented. The EXP102 per-token gap error is not repeated: any
union must be over the whole batch.
