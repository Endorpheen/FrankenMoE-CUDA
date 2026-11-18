# P4-13 — unified allocation and scheduler interface (contract)

Decision: option B (`P4-13-DECISION.md`). This note fixes the contract that the
Goose adapter and its differential gate must implement. It proposes no code.

## 1. Ownership

- The **stream source** owns the block tables: the table, its slots and the
  resident/evicted state of every block live in the source, not in the
  allocator. `xl_touch` records a committed access; `xl_set_budget` returns the
  budget; the plan it returns is ordered.
- The **allocator** owns buffers only: it allocates, resets and frees backend
  buffers and places tensors inside them. It never decides which block is
  evicted.
- The **scheduler** owns the graph lifetime: reservation for a graph, reuse
  inside a graph, release at graph end. It does not own block tables, and it
  uses one block size on both lineages.

## 2. Lifetime table

| object | created by | lives until | freed by |
|---|---|---|---|
| block table | stream source constructor | source destruction | source destructor |
| block slot state | `xl_touch` / plan application | next plan application | source (reset) |
| buffer | `allocate_buffer` | `free_buffer` or buffer reset | allocator |
| tensor placement | `tensor_alloc` | buffer reset / free | allocator |
| graph reservation | graph reserve | graph release | scheduler |
| eviction plan | `xl_set_budget` / plan build | application by the caller | caller (borrowed) |

Borrowed versus owned: a plan is **borrowed** — the caller must apply it before
the next call that can produce a plan for the same source. Block slots are
**owned** by the source and are never handed out by pointer to the allocator.
Buffers and tensor placements are **owned** by the allocator.

## 3. Operations

- `allocate(buft, size)` → buffer handle; failure returns 0 and no state change.
- `free(handle)` → releases the buffer; a second free of the same handle is a
  contract violation and must be rejected in the gate, not silently ignored.
- `reset(handle)` → returns every placement to the free pool without releasing
  the buffer; placements become invalid afterwards.
- `evict(source, budget)` → returns an ordered plan of blocks; applying it moves
  those blocks to the evicted state and makes their slots reusable. The plan is
  the only eviction mechanism; there is no callback.
- `reuse(buffer, size)` → returns a placement that was released by `reset` or by
  a graph release, when one satisfies the size and usage; otherwise allocates.

## 4. Block sizes

One block size per source, shared with the scheduler: the size recorded for the
source is the only one the allocator may use when placing blocks. A request that
does not align to it is rounded up by the allocator, never split by the source.
Reuse is allowed only between placements of the same block size and usage.

## 5. Threading

- The allocator is safe under concurrent `allocate`/`free` from the graph
  threads; a buffer's placements are single-writer per graph.
- The stream source serialises plan production; a plan is consumed by one
  thread. Two concurrent plan requests for the same source are a contract
  violation, detected in the gate.
- No call into the allocator happens while a plan is being applied.

## 6. Gate (mandatory before any call site switch)

Differential, against the current implementation, on identical inputs:

1. ownership and lifetime: allocate/reset/free sequences, double free rejected;
2. reuse: same block size and usage reused, different size not reused;
3. eviction: plan is ordered, deterministic, and its application equals the
   current eviction result for every budget value in a swept range;
4. block size: one size on both paths, misaligned request rounded once;
5. concurrency: two graph threads allocating and releasing, and two plan
   requests serialised, under ASan/UBSan;
6. leak and use-after-free: reset then use is rejected, free then use is
   rejected.

Call sites are switched only after all six pass, and the replaced C++ body is
removed only after the switched paths reproduce the accepted Apex results.

## 7. Live inventory (2026-11-18)

Measured on the two shipped libraries, not assumed:

| lineage | library sha | Goose allocator symbols (`fg161*`) | public allocator API |
|---|---|---:|---|
| current baseline (accepted) | `23363cbd…` | **0** | `ggml_backend_alloc_ctx_tensors[_from_buft[_size]]`, `ggml_backend_buft_alloc_buffer[_n]`, `ggml_backend_buffer_free`, `ggml_backend_buffer_reset`, `ggml_backend_tensor_alloc` |
| stage-run lineage | stage `libggml-base.so.0` | 2 exported, module called internally | same API |

Consequence: on the current baseline the allocator is entirely native C++, and
the Goose allocator module exists only in the other lineage. P4-13 therefore
adds the adapter to the baseline; it does not merge two Goose interfaces. The
adapter surface is exactly the public API above, with the block tables owned by
the stream source as agreed in the contract.
