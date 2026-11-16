# EXP-2026-10-31-115: deferred mmap expert registration

Status: NOT_RUN in this session.

Historical measurement (EXP089 probe): registering the 19.79 GB host expert
union took 1.339 s. Deferring it to the first real GPU expert upload would move
that cost from startup to the first long-prompt request; it is not a speedup for
that request. The deferral mechanism (ownership, budget, rollback, per-mode
fallback) was not implemented. Startup/first-request/later-request timings were
not re-measured.
