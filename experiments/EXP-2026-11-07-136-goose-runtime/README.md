# Self-hosted Goose runtime integration checkpoint

Experimental snapshot; no production runtime or compiler source was changed.
This is one independent runtime integration study, separate from the compiler roadmap.

## Implemented and exercised on Apex

- CUDA SiLU, scale+bias and norm square execute emitted Goose device arithmetic.
- A complete Q4_K/Q8_K dot product executes Goose unpacking, packed SIMD products,
  scale/min correction and FMA reduction. A 16-row tile is retained as a candidate.
- Goose chooses reusable allocator blocks and mutates the free list for insertion,
  removal and prefix/suffix/bridge coalescing.
- Goose updates live tensor child/view counters and chooses release requests.
- The full allocation-graph pass now traverses leaves and nodes, counts dependencies,
  chooses buffer IDs and orders allocation/release service calls in Goose.

Native C still owns tensor objects, hash lookup, lower-level allocation/in-place
reuse, graph-output protection, physical backend memory and CPU/CUDA execution.
This is not a complete Goose executor or a proof of superior memory safety.

## Correctness and rollback

The live Apex sequence was 512/32, 32768/128, then 512/32. Text, token IDs and
probabilities match the unchanged EXP107 reference exactly. The graph pass executed
16 times across 69,168 nodes and 13,216 leaves, with 188,480 allocation requests,
23,598 release requests and 264,304 checked metadata transitions. These are service
requests, not physical allocation counts. Original passes remain switchable.
The previous free-list state/digests are identical with the graph pass on and off.

Standalone graph fixtures compare 84,962 ordered service events and every final
state byte across 200 graphs plus 128 graphs in eight threads. Fixture allocator
services are mocks; real inference evidence is recorded separately.

An important rejected version inferred a local value from an extern-returned
reference and therefore changed a copy. A one-node fixture exposed two missing
releases. Explicit `NodeState&` bindings preserve the live reference. Both the
rejected source/emission and diagnostic evidence are retained. No compiler patch
was made. The generated type ID in the ABI adapter is pinned to this emission.

## Performance status

All measurements are preliminary under concurrent download/build/other activity.
Some runs show possible approximately 4% decode differences; none establishes a
causal Goose regression. They are retained and do not block larger isolated trials.
Production acceptance requires repeated controlled comparisons, not a single run.

Coalescing v1/v2 were more than twice as slow in the operation microbenchmark.
V3 uses bounded standard `memmove` only for overlapping array movement; coalescing
decisions, field changes and counts remain in Goose. V3 reached preliminary parity.
The larger graph fixture currently has non-LTO FFI overhead; do not confuse its
mock-service timing with full model throughput.

Current official llama.cpp b12179 was tested with identical arguments and inputs.
Its first token differs from both old native C++ and Goose, even on CPU. The exact
cause is unresolved; no accuracy or cross-version superiority claim is made.
See the per-stage reports for all measured values and limitations.

## Reproduce the standalone graph check

Run `python3 tools/check_graph.py` from this directory. It uses a temporary build
and the frozen emitted C, so it does not require model weights or alter the tree.
A C compiler and a C++ compiler with threads are required. `CC` and `CXX` can select
them. The test is independent of the actual GGML allocator; actual Apex evidence
must not be replaced by this fixture test.

Goose sources and frozen C emissions are provided. The producer hashes in
`provenance.json` identify completed self-hosted Stage3 builds, not the old C++
Goose compiler. For regeneration, supply that producer externally with
`stage3 -o generated/graph-plan.c source/graph-plan/kernel.goose` and rebuild the
ABI adapter after checking emitted types. Producer binaries are intentionally absent.

## Integration artifacts

`patches/` targets the exact source hashes in `provenance.json`; each allocator
patch is an alternative complete stage against the same base, not a patch series.
Only use them in an isolated compatible GGML copy. Link the corresponding C ABI
unit(s) from `source/` alongside the modified allocator and the matching baseline
objects. Existing integration command manifests and full raw runtime logs remain
in the local experiment; this checkpoint contains code and compact evidence.
CUDA sources use headers from the matching GGML tree; their optional trace paths
are local diagnostic plumbing. Default untraced math does not read trace files.

Rollback switches are `GGML_GOOSE_ALLOC=0`, `GGML_GOOSE_COALESCE=0`,
`GGML_GOOSE_LIFETIME=0`, `GGML_GOOSE_GRAPH_PLAN=0`, and `GGML_GOOSE_Q4_TILE=0`.
Q4 dot interposition supports `GGML_GOOSE_Q4=0`. CUDA leaves are reversible by
removing their experimental preload modules. Production promotion remains separate.

Qwen3.8-Flash-Next IQ2_XS and MTP are fully downloaded and hash-verified.
Native probing has begun; Goose coverage on Qwen, controlled performance series,
larger CPU/GPU scheduling work and the later independent memory-safety audit remain
open. No model files, runtime binaries, credentials or private instruction files
are included in this checkpoint.
