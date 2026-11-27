# EXP-2215 — Walk graph nodes in Goose batches

The CPU graph executor borrows the native node array once and reads up
to sixteen handles per bridge call. The stack buffer holds 128 bytes.
The bridge uses memcpy to preserve the pointer representation without
aliasing typed C pointer arrays. Goose owns traversal and index bounds.

Against EXP-2214 (`338d48da`), all 8192 positive and negative index checks
matched, as did a full walk, an empty graph and a partial final block.
Median in the six alternating CPU trials was approximately 1.39 ns/node
for the GGML walk and 0.50 ns/node for the batched Goose walk. This is an
isolated traversal microbenchmark.

GNU linker maps removed `ggml_graph_node` (123 bytes) and
`ggml_graph_n_nodes` (8 bytes). The universal Linux candidate retained
63581 bytes of GGML text versus 63712 before the change. The C input file
line count is unchanged. Full-model throughput and memory are deferred
to a later accumulated inference check.

Use KeelWing v0.3.0 preview and GCC 13.4.0. Supply the v0.15.0 Linux
build's `common` output directory for the reference.

```bash
GOOSE_BIN=/path/to/keelwing bash source/measure.sh /path/to/empty-output /path/to/baseline/common
/path/to/empty-output/bench
```
