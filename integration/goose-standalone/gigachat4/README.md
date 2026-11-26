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

## Resident recurrent state

The CUDA backend retains convolution history and the recurrent state of all
24 GDN layers in 50 MiB. Disable this path with
`GOOSE_GIGACHAT4_GDN_STATE=0`. The graph marks in-place recurrent updates for
the existing Goose CUDA executor.

The fixed 29/8 comparison used the same placement and prefill planner above.
Runs followed resident, control, control, resident order without profiling.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 3.794 | 1.470 | 8261 |
| Control 2 | 3.798 | 1.485 | 8261 |
| Resident 1 | 3.832 | 1.645 | 8311 |
| Resident 2 | 3.847 | 1.645 | 8311 |

All four runs matched the eight reference IDs. Process swap and system
swap-out delta were zero. A separate diagnostic run recorded 24 resident
GDN operations per graph. Median decode increased by 11.3 percent.

## YaRN reference parity

GigaChat4 stores the YaRN logarithmic multiplier in conversion units.
The loader normalizes it by 0.1 before deriving the RoPE attention factor,
matching the external llama.cpp reference at 0541e50e2fd463b376a3332fd891243aa5ce9c6a.

The fixed 29/8 request SHA-256 is
`c3b98b3778170ecac6bf43632539968392dc3db635c299956ccfb87dc0aec3f4`.
Before the fix, Goose and the reference matched for two output tokens.
After the fix, they matched for all eight: 1292, 7154, 4008, 88, 57981,
6958, 7780, 391. This gate covers one fixed request.

## CUDA MLA key projection

The loader expands the nine Q5_0 key-projection tensors to F32 using the
existing Goose decoder and registers each head plane in the CUDA weight
table. This moves the supported matrix operation off the CPU. The path is
enabled with at least 6 GiB free VRAM and can be disabled with
`GOOSE_GIGACHAT4_MLA_F32=0`. Expanded weights occupy 72 MiB each in RAM and
VRAM, without changes to the GGUF file.

The fixed 29/8 comparison used the same executable and ran control,
candidate, candidate, control. It used the prior accepted GDN residency
and an enabled prefill planner, dense placement, and expert cache.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 3.747 | 1.686 | 8303 |
| Control 2 | 3.681 | 1.660 | 8312 |
| CUDA MLA 1 | 5.976 | 2.709 | 8379 |
| CUDA MLA 2 | 5.969 | 2.749 | 8386 |

All four runs matched the eight reference tokens from the YaRN gate. Process
swap and system swap-out delta were zero in all four runs. Median peak RSS
increased by 80 MiB. Median prefill increased by 60.8 percent and median
decode increased by 63.1 percent.

## CUDA F16 MLA attention

A separate Goose kernel computes F16 attention for 576-wide keys and
512-wide values. Its dispatcher preserves the Q4 path and selects the F16
kernel for the MLA layout. Disable the F16 path with
`GOOSE_GIGACHAT4_MLA_ATTENTION=0`.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 5.875 | 2.619 | 8379 |
| Control 2 | 6.054 | 2.733 | 8377 |
| CUDA F16 1 | 8.840 | 4.057 | 8379 |
| CUDA F16 2 | 8.908 | 4.037 | 8379 |

All eight output IDs matched the external reference in each fixed 29/8
run. The two medians increased by 48.8 percent for prefill and 51.2 percent
for decode. Median peak RSS increased by 58 MiB. Process swap was zero
in every run. The first pair recorded 73 and 91 pages of system swap-out,
and the reverse pair recorded none.

A diagnostic run exercised eight CUDA attention nodes per model step.
The existing Q4 CUDA test matched byte for byte with one and 29 queries
and 257 keys. The existing Q4 PTX instructions also matched after
label normalization in both generated images.

## Skip redundant short-context DSA ranking

When the attention cache has at most 2048 cells, DSA ranking would select
every cell. The decoder uses the original additive mask and still writes the
index keys. Larger graph shapes continue to rank and select 2048 positions.

The fixed 128/8 request SHA-256 is
`ec82485b4a39190f1b9c195c589bf62565cb52a7a6ad624748a264cb3eecffad`.
The configuration and memory placement were held fixed, with two runs per
variant and identical generated IDs matching the external reference.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 5.939 | 2.891 | 8408 |
| Control 2 | 5.914 | 2.848 | 8397 |
| Skip ranking 1 | 16.594 | 4.761 | 8405 |
| Skip ranking 2 | 16.622 | 4.634 | 8397 |

The median speed ratios were 2.80 for prefill and 1.64 for decode. Peak
RSS was approximately 18.58 GiB in all four runs. Process swap was zero.
System swap-out rose by 81 pages in the first candidate run and was
unchanged in the other three.

A graph check at 2048 and 2049 live rows found eight index-cache writes
in both cases and zero versus eight ranking nodes, respectively. It
validated the graph transition without executing a long request.

## Prompt MMQ and planned decode

GigaChat4 enables the existing CUDA MMQ path and execution planning by
default. Set `GOOSE_MMQ=0` to disable MMQ or
`GOOSE_GIGACHAT4_PLANNED=prefill` to plan only the prompt. Any explicit
environment value takes precedence over the model's defaults.

The fixed 128/8 request was run in control, candidate, candidate, control
order in one build, without profiling. The control used the prefill
planner and disabled MMQ. The candidate kept the planner for both phases
and enabled MMQ. Dense and expert placement were identical.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 16.503 | 4.744 | 8407 |
| Control 2 | 15.986 | 4.587 | 8409 |
| MMQ and planned decode 1 | 19.670 | 8.047 | 8409 |
| MMQ and planned decode 2 | 20.256 | 8.304 | 8406 |

All four runs matched the eight reference tokens. Median prefill increased
by 22.9 percent and median decode by 75.2 percent. Process swap was zero.
System swap-out was zero in the first pair and rose by 34,798 and 37,528
pages in the second candidate and control runs, respectively.
