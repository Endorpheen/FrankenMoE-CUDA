# EXP304: graph-shape probe found a live copy-path defect

The probe was meant to compare graph shapes between lines by running two prompt lengths (5 and 371 evaluated tokens) on
both. The native line returned both answers; our line returned the short one and then died on the long one, which is a much
more important result than the comparison.

## Result

| arm | short prompt (5 tokens) | long prompt (371 tokens) |
|---|---|---|
| native | `1da0f49942ac7858`, " over the lazy dog." | `c5ca940bbe49eaa5` |
| ours | `1da0f49942ac7858`, identical | **connection closed**, process aborted |

## Where it dies

    #5  ggml_abort
    #6  ggml_cuda_error                  libggml-cuda.so.0
    #7  ggml_backend_cuda_buffer_set_tensor
    #8  fg163_io                         libggml-base.so.0
    #9  fg163_gg_function_21
    #10 fg163_gg_function_1
    #11 ggml_backend_tensor_copy          libggml-base.so.0

with `CUDA error: invalid argument` raised from the CUDA backend at `shared-cuda.cu:114`.

Both lines run with `-ngl 0` and neither prints a CUDA device line, so nothing should be copied to a CUDA buffer at all.
The run log shows the recurrent prefix checkpoint being written before the failure:

    slot create_check: id 0 | task 0 | RAM recurrent prefix saved: tokens = 1, size = 62.813 MiB, retained = 1

## Why this matters for the ledger

P6-02 was closed minutes earlier on the short-prompt evidence, with attachment recorded as ported and live and ownership as
native-and-exercised. A second prompt shape contradicts the "exercised" half: a copy driven by fg163 reaches a CUDA buffer
and aborts. The item is reopened until this is explained and fixed; closing it again requires both shapes green.

The probe itself is otherwise sound and will be rerun once the copy path is fixed, since the graph-shape comparison it was
written for is still useful.

## Localization: the scheduler decides the copy

A breakpoint on `ggml_backend_tensor_copy` stops inside the scheduler, not the loader:

    #1 fg164_component
    #2 fg164_gg_function_84
    #3 fg164_gg_function_1
    #4 ggml_backend_sched_graph_compute_async
    #5 llama_context::graph_compute
    #6 llama_context::process_ubatch

So the copy is part of graph scheduling for the 371-token ubatch: the scheduler module (EXP164) decides a tensor needs to move,
and its destination is a CUDA buffer even though the run is `-ngl 0` and no CUDA device line is printed. The short prompt
never reaches this split, which is why the first request succeeds.

That reframes the defect: it is not weight placement (P6-02's attachment decision) and not the recurrent checkpoint, but the
scheduler's backend assignment for a split graph. The next step is to see what fg164 returns for that node -- a valid backend
index or something out of range -- rather than to guess from the tensor side.
