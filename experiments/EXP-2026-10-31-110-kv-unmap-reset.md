# EXP-2026-10-31-110: return physical KV pages after explicit reset

Status: NOT_RUN in this session.

EXP101 keeps the physical high-water after a large request: the 261632-token run
mapped the full 1440 MiB and there is no unmap API in the candidate, so a later
32K request would retain the high-water. The proposed mechanism (unmap unused
VMM ranges on explicit erase/clear, keep the virtual addresses, remap+zero on
reuse, synchronize GPU work, invalidate dependent checkpoints) was not
implemented or measured. Open direction.
