# EXP339: graph node/shape trace, instrument proven live first

P6-03 asks for node, shape and config traces. This is the node/shape slice at the API level, built before touching a real request.

## Instrument liveness proven before any result

Three things were established about the dump itself before comparing anything, because this session has already produced three broken
instruments:

1. `ggml_graph_dump_dot`'s third argument is a **path**, not a buffer. Passing a buffer made it `fopen` arbitrary bytes and abort on
   `GGML_ASSERT(fp)`.
2. The process aborts after the dump (identically on both lines), and **stdout was block-buffered**, so every node line was lost -- the
   comparison would have seen two lines instead of seven and looked "equal" for the wrong reason. The harness now sets `_IONBF`.
3. Liveness is shown by `n_nodes=4` and `dot_len=1362` actually appearing, not assumed.

## Result

Seven lines, identical between the lines:

    n_nodes=4
    node 0 op=2  ne=[32,16,1,1] nb=[4,128] type=0 name=add
    node 1 op=32 ne=[32,16,1,1] nb=[4,128] type=0 name=sc
    node 2 op=29 ne=[16,16,1,1] nb=[4,64]  type=0 name=mm
    node 3 op=2  ne=[32,16,1,1] nb=[4,128] type=0 name=out
    dot_len=1362 dot_head=digraph G {

The DOT files differ only in the hex node addresses, which are pointers and are expected to differ between processes.

So the op sequence, the op codes, the shapes and the strides of a shared-helper graph match. What this does **not** cover is the graph a
real request builds: that needs an instrumented build, and the two pitfalls above are exactly why it must be proven live first.
