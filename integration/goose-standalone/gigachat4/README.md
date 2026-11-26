# GigaChat4 Goose runtime candidate

Build the isolated server with `bash integration/goose-standalone/gigachat4/build-server.sh`.
Set `GOOSE_BIN` to the installed Fried Goose compiler when needed. The model
remains on its external disk and the server maps it in place.

The candidate builds the decoder and MTP graphs in Goose. The standard
standalone Goose executor runs inference. The C adapter passes mapped tensor
descriptors and server buffers to Goose. Draft cache catch-up is diagnostic and
speculative verification is pending.

The short comparison used a fixed 29-token input, eight generated tokens,
context 36864, batch/ubatch 256/128 and decode/batch threads 8/16. Both
configurations used 1922 MiB of dense weights and 4093 MiB of expert cache.
The experimental prefill planner is selected with
`GOOSE_GIGACHAT4_PLANNED=prefill`. Dense and expert placement are enabled in
the measured configuration with `GOOSE_GIGACHAT4_DENSE=1` and
`GOOSE_GIGACHAT4_EXPERTS=1`.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 3.292 | 1.461 | 8256 |
| Control 2 | 3.339 | 1.471 | 8273 |
| Prefill planner 1 | 3.822 | 1.494 | 8277 |
| Prefill planner 2 | 3.814 | 1.493 | 8277 |

All four runs generated the same eight IDs: 1292, 7154, 15166, 347, 4620,
926, 20201, 7919. Process swap was zero and system swap-out was unchanged.
These are short-request diagnostics. The 32K/512 gate and model-specific
OpenCode tool round trips remain pending.
