# Architecture

## Data path

```text
split GGUF on NVMe
        |
        v
tensor index: shard + offset + length
        |
        v
bounded RAM expert cache ---------> CPU cold mul_mat_id
        |
        v
pinned host staging
        |
        v
bounded per-layer VRAM banks
        |
        v
CUDA hot mul_mat_id + lossless hot/cold merge
```

Dense weights use the normal `llama.cpp` loader and scheduler. Expert tensors remain CPU-addressable and are rebound to virtual RAM-cache banks. Qwen's large PLE tensor is detected from graph row-gather operations and served by a separate bounded row window.

## Component boundary

BigMoeOnEdge supplies split-GGUF indexing, positioned reads, `O_DIRECT` fallback, virtual-memory commit/evict primitives, the RAM LRU, router interception, row streaming, and SSD telemetry.

The `expert-tier` fork supplies routing heat statistics, fixed-shape per-layer GPU banks, hot/cold graph construction, slot maps, sentinel handling, migration limits, and the CUDA `mul_mat_id` path.

The integration layer:

1. forces expert source tensors to stay CPU-addressable;
2. sizes VRAM slots from actual GGUF expert tensor sizes;
3. asks the SSD/RAM tier to prepare a complete expert before promotion;
4. copies through pinned staging;
5. exposes cumulative H2D and VRAM metrics;
6. aborts on partial reads instead of computing with incomplete weights.

## Correctness on cache misses

The original top-k routing is unchanged in the default configuration. A cold miss commits an entry, reads every projection slice, and publishes readiness only after all reads succeed. A hot promotion occurs only after that readiness check. The old GPU mapping remains valid until the replacement copy completes.

RAM eviction excludes entries with an active read or lease. VRAM resynchronization occurs between decode steps, and sentinel slots are never evicted. Any SSD or row-stream error propagates to the graph abort callback.

The small-model gate compares resident and streamed greedy output byte for byte. Full-model resident comparison is impossible on the validated 64 GB/12 GB machine, so the large run is additionally checked through coherent output, bounded RSS, zero process swap, and non-zero SSD/H2D/VRAM telemetry.

## PLE row streaming

The 27.3 GiB `per_layer_token_embd` tensor is never materialized as a whole. The runtime reserves its virtual address span, reads only slabs containing requested rows, and evicts slabs under a configurable window. If a future graph shape attempts a full-table operation larger than the window, decode fails explicitly rather than violating the RAM budget.

## Current synchronization boundary

SSD reads can overlap CPU cold-expert computation through the expert-ready hook. The hot promotion path uses pinned memory, but `ggml_backend_tensor_set` synchronizes H2D before returning. True H2D/compute overlap therefore requires a new explicit stream/event interface and eviction leases spanning event completion.
