# EXP-2026-10-31-116: sparse physical backing for expert staging buffers

Status: NOT_RUN in this session.

Depends on the EXP109 geometry diagnostic (logical destination capacity,
selected ranges, physical allocation, aliasing/lifetime). No diagnostic or
implementation was produced. The mechanism is distinct from the rejected
EXP093 pool trimming and must account for MMQ padding, kernel reads past logical
bounds, allocator reuse, clear/memset and captured graphs.
