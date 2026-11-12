# EXP340: request-level graph trace -- instrument live, Apex unchanged

P6-03's node/shape trace needs the graph of a real request, not of a toy graph. Nothing built-in provides it (GGML_SCHED_DEBUG is
compiled out), so the trace is an env-gated hook in our `llama_context::graph_compute`:

    FG_TRACE_GRAPH=N   dump the first N graphs: node count, then op/ne/nb/type/name per node

The hook prints and returns -- no arithmetic, no control-flow change -- and is off unless the variable is set.

## Liveness first

A short request with `FG_TRACE_GRAPH=2` produces exactly two dumps:

    FG_TRACE_GRAPH dump=1 n_nodes=3819 batched=1
    FG_TRACE_GRAPH dump=2 n_nodes=3819 batched=1

and the first nodes are recognisable rather than noise:

    G 0 op=40 (GET_ROWS)  ne=2048,2  model.input_embed
    G 1 op=40 (GET_ROWS)  ne=2048,2  node_1
    G 2 op=1  (DUP)       ne=2048,2  node_2
    G 3 op=42 (VIEW)      ne=2048,2
    G 4 op=25 (RMS_NORM)  ne=2048,2  norm-0

So the instrument is live and the numbers are real for the accepted configuration at `-c 2048`.

## Apex unchanged after relinking

`libllama` was rebuilt with the hook and relinked into a stage: the accepted profile still returns `e68df05b` with 128 tokens. The
trace is therefore additive in the build and inert by default.

## The differential half, and why it is not done here

A true node-by-node differential needs the same trace on the flint line. That would mean modifying the reference tree, which this
repository's rules forbid: the flint tree is the baseline the whole migration is measured against, and a patched reference is no longer
a reference. The API-level trace in EXP339 (identical op sequence and shapes on both lines) remains the differential evidence; this
experiment supplies the request-level structure and proves the instrument works for when the owner decides how to obtain a reference
trace -- for example by allowing a throwaway copy of the flint tree with nothing but this hook added.
