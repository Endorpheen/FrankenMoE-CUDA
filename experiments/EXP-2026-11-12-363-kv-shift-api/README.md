# EXP363: the KV shift port executes and agrees with native

EXP362 could not reach the shift path through the server or the CLI. Driving it through the C API does reach it.

## Method

A small probe loads the tiny model on CPU with a 64-token context, decodes a prompt, then calls

    llama_memory_seq_add(llama_get_memory(ctx), 0, 0, -1, -4);

which moves every position back by four, and decodes one more token. That second decode builds a graph with a k-shift input, which is the only
condition under which `llama_kv_cache::set_input_k_shift` runs. The probe prints an FNV-1a hash over all 512 logits.

## Result

    native libllama : shift_logits_hash=c27499e4f131e6bd count=512
    our libllama    : shift_logits_hash=c27499e4f131e6bd count=512

Identical over the full vocabulary, not a sampled subset.

## The port really ran

A hash comparison alone would not distinguish "the port ran correctly" from "the port was skipped and the native loop ran". gdb settles it:

    Thread 1 "probe" hit Breakpoint 1, fg230s_fill_shift ()
    #1 llama_kv_cache::set_input_k_shift(ggml_tensor*) const
    #2 llm_graph_input_k_shift::set_input(llama_ubatch const*)

So the Goose function was called from the shift input path, and it returned a result (`>= 0` becomes the early return in the call site), after which
the logits matched native exactly.

## What this closes and what it does not

It closes the gap EXP362 left: the shift port is not dormant, it executes when a shift is actually applied, and its output agrees with native on a
64-token context with a four-position shift.

It does not cover shifting at Apex scale, repeated shifts, or shifting with several sequences -- the `n_stream <= 64` guard allows the latter but only
one sequence is exercised here.
