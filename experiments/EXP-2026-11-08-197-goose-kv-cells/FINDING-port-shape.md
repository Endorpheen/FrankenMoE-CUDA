# Finding: the KV subsystem is not the same kind of port as the CUDA kernels

Recorded before writing any KV code, because it changes what "port it" can mean here.

## What the measurement showed

EXP197's probe counted what executes in one 32K/128 pass:

    prepare=138 apply_ubatch=276 init_update=138
    get_size=617 get_n_kv=150 cpy_k=170 cpy_v=170 build_k_idxs=17
    init_batch=0 state_write=0 state_read=0 state_clear=0

So the target set is small. But reading the code behind those names shows the shape is different
from every module ported so far.

## The difference

The CUDA modules operate on flat device buffers. Their state is an address plus strides, and the
body is arithmetic over it — which is why a Goose body can replace them one for one.

`llama_kv_cells` keeps its state in C++ containers:

    std::vector<llama_pos>          pos       (519)
    std::vector<llama_kv_cell_ext>  ext       (502)
    std::vector<llama_pos>          shift     (519)
    std::vector<seq_set_t>          seq       (522)   where seq_set_t = std::bitset<LLAMA_MAX_SEQ>

13 `std::` uses in the header and 64 in the implementation. The methods that fire —
`is_empty`, `seq_count`, `seq_get`, `pos_get`, `seq_pos_min` — are accessors over those
containers: a bitset test, a map begin/rbegin, a vector index. `apply_ubatch` mutates them.

There is no arithmetic to move. Porting these bodies to Goose would mean either reimplementing
`std::vector`/`std::bitset`/`std::map` semantics in Goose — a prelude far larger than the code
being replaced — or porting only the index arithmetic around container calls, which would put a
Goose call around `vector[i]` and move nothing.

## What this means for the plan

The census rule "port by fact" was written for kernels, where the fact is an executing body. Here
the executing bodies are container manipulation, so the same rule argues **against** porting them
one for one: the result would be the "code nothing reaches" outcome in a different form — Goose
calls that wrap a `std::bitset::test`.

The honest options, in order of what they would actually buy:

1. **Port the pure index arithmetic** that exists around the containers — `apply_ubatch`'s
   `s*sinfo.size() + ii`, the `head = idxs.back() + 1` update, the purge bounds — leaving the
   container operations native. Small, real, and it is arithmetic.
2. **Port `cpy_k`/`cpy_v`**, which are tensor-shaped and closer to the CUDA modules, and where the
   dequantization side is already Goose.
3. **Leave the rest native** and record why, rather than manufacture a port whose Goose body is a
   trampoline.

## Not done in this pass

No KV code was changed. This file records the shape finding and the ordering it implies, so the
next attempt does not start by rewriting `llama-kv-cells.h` in Goose and discover this halfway.
