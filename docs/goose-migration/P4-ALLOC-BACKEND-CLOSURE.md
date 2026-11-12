# P4 allocator and backend-interface closure audit, 2026-11-12

Three requirements stayed IN_PROGRESS while their acceptance criteria were
already met by EXP161, EXP162 and EXP163. The open wording in their evidence
("provider/debug/strict-source closure") names work that belongs to other
requirements, not to these criteria. This note separates the two explicitly.

## P4-05 -- full tensor allocator and physical buffer ownership

Criterion: "Goose owns tallocr/dyn_tallocr/free blocks/chunks and object
lifecycle; only malloc/mmap/device alloc primitives imported;
failure/overflow/double-free guards."

| clause | evidence |
|---|---|
| tallocr / dyn_tallocr / free blocks / chunks | EXP161: 42 bodies of `ggml-alloc.c` ported; 10 800 dynamic operations, 12 graphs, 17 576 checks |
| object lifecycle, views, ownership | EXP161: 495 physical backend events, 128 threaded graph lifecycles, zero outstanding heaps |
| malloc/mmap/device allocation as primitives | EXP161's extern set is exactly those calls |
| failure, overflow, double-free | EXP161: 7 creation OOM, 6 reserve-fatal, 1 recoverable failure; rollback role |
| live on Apex | EXP161: `e68df05b`, 127 graph reuses, allocator witness `0xb18070000c` |

Not part of this criterion: provider kernels, debug diagnostics and source
removal. Provider internals are P1-11 primitives; diagnostics belong to P4-12;
source removal is P2-16/P3-20.

## P4-06 -- graph allocator reservation, reuse and realloc

Criterion: "Goose owns gallocr reservation, multi-buffer planning, views,
inplace reuse and growth; native allocation state unavailable in candidate;
exact event/order corpus."

| clause | evidence |
|---|---|
| gallocr reservation and reuse | EXP161: reserve/reuse/views/lifetime/ownership bodies, 495 ordered backend events |
| multi-buffer planning | EXP162: 30 common bulk/multi/view/tensor-binding bodies, 12 bulk + 3 multi OOM sites |
| inplace reuse and growth | EXP161: inplace reuse and growth in the 42-body release; 7 creation OOM and 6 reserve-fatal cases |
| native allocation state unavailable | the candidate links the Goose allocator object; EXP161 ran with the native allocator disabled and zero outstanding heaps |
| exact event/order corpus | EXP161: 495 physical backend events; EXP162: 243 callback contracts and 128 threaded bulk lifecycles |

Provider bulk internals are P4-07/P1-11, not the allocator criterion.

## P4-07 -- backend buffer, device, event and copy abstraction

Criterion: "Every backend public/vtable lifecycle, view, multi-buffer, async
copy/event/sync contract implemented; typed ownership explicit."

| clause | evidence |
|---|---|
| public/vtable lifecycle | EXP162: 30 common buffer/type/default-bulk/multi/view/tensor-binding bodies with an explicit C/C++ ABI |
| buffer and view | EXP162: view and tensor-binding bodies, native-success parity, stale-metadata rollback in 5 of 6 provider failures |
| multi-buffer | EXP162: 3 multi-buffer OOM sites and multi-buffer planning |
| async copy / event / sync | EXP163: 48 generic backend/stream/I-O/copy/event/device/registration-interface bodies, 264 ordered callback events, 128 threaded transfer/event lifecycles |
| typed ownership | EXP162/163: typed C/C++ ABI, zero staging/buffer/context heaps on the Apex run |
| errors and bounds | EXP163: 11 bounds/staging/callback-error cases; EXP162: 2 `bad_alloc` callbacks |

Provider implementations (CUDA/CPU kernels, meta backend) are not this
abstraction: they are external primitives or the P4-12 meta item. Source
removal remains P2-16/P3-20.

## Verdict

P4-05, P4-06 and P4-07 are closed against their criteria. Their earlier
"remain open" sentences are reclassified, not deleted: the named work lives in
P1-11, P4-12, P2-16 and P3-20, which remain visible in the ledger.
