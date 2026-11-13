# EXP466: P6-02 fix — host-weight op offload is disabled when no layer is offloaded

## Cause

`ggml_backend_sched_backend_id_from_cur` moves an op whose weight sits in a host
buffer to the first higher-priority backend that claims `offload_op`. Under
`-ngl 0` that backend is CUDA0, so one elementwise op per layer (`GGML_OP_MUL`
over `norm-*` and the SSM norm) became a CUDA island with split-input copies into
CUDA buffers. The copy of a CPU activation into a CUDA buffer is what raised
`CUDA error: invalid argument` on the 371-token prompt.

Proof: an `LD_PRELOAD` wrapper around `ggml_backend_sched_new` that forces
`op_offload=false` served the same prompt with `HTTP=200 EXIT=0 CUDA=0 ABORT=0`
(`tests/noff.c`).

## Fix

`runtime/reconstructed/src/llama-context.cpp`, `llama_context::sched_new`:

    const bool op_offload = cparams.op_offload && model.n_gpu_layers() > 0;

With at least one offloaded layer the accepted profile keeps the previous
behaviour bit for bit; with a fully CPU placement there is nothing to offload an
op to, so the scheduler keeps those ops on the CPU.

## Gates

| gate | result |
|---|---|
| long prompt (1 800 chars), `-ngl 0 -c 2048 -np 1`, new libllama | `READY=1 HTTP=200 EXIT=0 CUDA=0 ABORT=0`, content returned |
| Apex 32K/128, new libllama | content sha `9d90a010…` — **identical to baseline** |
| Apex 32K/512, new libllama | content sha `c2ff42cc…` — **identical to baseline** |

## Performance, measured under load

The host ran a parallel compiler build during these measurements (`loadavg`
5.3–7.7 on 32 threads). The numbers below are therefore **measured-under-load**
and are not evidence of a regression or of parity:

| run | prefill tok/s | decode tok/s | loadavg at request | content sha |
|---|---:|---:|---|---|
| baseline 128 (recorded) | 2259.28 | 39.24 | not recorded | `9d90a010…` |
| new libllama 128 | 2164.65 | 37.14 | 6.73 | `9d90a010…` |
| new libllama 512 | 2184.80 | 38.40 | 5.35 | `c2ff42cc…` |

A clean performance verdict needs the same profile on an idle host; the functional
fix and the identical outputs do not depend on that.

## Status

P6-02 stays IN_PROGRESS until the clean performance check; the abort itself is
fixed and the accepted profile is byte-identical in its outputs.
