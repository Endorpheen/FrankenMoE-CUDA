# P5 entry: what the KV subsystem looks like from here

P5-01/P5-02 are the next large unported block on the Apex path. This records what is known before
any code is written, because the block is 3.5k lines and starting it blindly would be the same
mistake as porting a seam nothing reaches.

## Size, measured

    src/llama-kv-cache.cpp   2965 lines
    src/llama-kv-cells.h      565 lines
                             3530 total

That is larger than several of the CUDA modules combined, and it is not a kernel — it is state
management with several interacting sequences.

## Is it on the path? Yes, and measurably

From a live 32K/128 Apex run:

    RAM recurrent prefix saved: tokens = 28672, size = 62.8 MiB
    RAM recurrent prefix saved: tokens = 32764, size = 62.8 MiB

Two checkpoints created in one request, with `--ctx-checkpoints 32` and `--cache-ram 0` in the
fixed profile. The KV cache is `q4_0` on both K and V, and the dequantization of that cache is
already Goose inside the attention kernel (EXP173). So the *data* path is partially ours already;
what is not ours is the cell allocation, sequence ownership and checkpoint bookkeeping around it.

## What that means for ordering

The CUDA kernels are the hot loop and they are done or blocked. The KV subsystem is per-request
bookkeeping: it runs hundreds of times per pass, not millions, so the same porting effort buys
much less here. That does not make it unimportant — P5-10 asks for closure on real models — but it
changes which of the two is worth a session.

## Concrete first step when this is picked up

1. Take the cell operations that a single-sequence run actually executes, by instrumenting
   `llama_kv_cache` the same way the CUDA modules were instrumented — a counter per entry point,
   one real run, then port only what fires.
2. Do NOT start from the file. The file has multi-sequence, defrag and serialization paths that
   this profile never enters; porting them first is exactly the "code nothing reaches" outcome the
   census rule exists to prevent.
3. Gate against the existing differential harness, which is already built and green, rather than
   inventing a new one.

Nothing in P5 was changed in this pass; this is the entry plan and the measurement that justifies
its order.
