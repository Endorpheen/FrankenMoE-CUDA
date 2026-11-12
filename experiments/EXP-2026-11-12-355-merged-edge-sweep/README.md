# EXP355: edge-case sweep finds a pre-existing crash in the candidate

Six request shapes sent in sequence to native and to the merged candidate: empty prompt, `n_predict 0`, a stop string, 64 tokens, `n_probs`, and
`cache_prompt`. The candidate died on the second case and every later case then reported connection refused.

## The sequence that crashes

    request 1: {"prompt":"",    "n_predict":4, "temperature":0}   -> fine on both
    request 2: {"prompt":"Hi",  "n_predict":0, "temperature":0}   -> native answers, candidate aborts

Each request alone passes on the candidate. It is the pair that fails.

## It is not the merge

Three controls:

- The **EXP340 stage**, which predates every port from this merge, aborts on the same pair.
- The candidate with **all five server gates and both llama-side gates set to 0** -- checkpoints, slot select, batch compat, stop strings, queue,
  batch reject, mmap -- still aborts. The ports are not involved.
- Native, built from the current flint tree, answers both requests.

The abort stack is `server_context_impl::pre_decode()`, lambda #6, calling `ggml_abort` in `libggml-base`. So the trigger sits in server code
that predates the ports, and the assertion comes from the older base library.

## What that means

The candidate's base library and server implementation are older than the current flint tree and differ from it in behaviour on this input, not
just in features. That is a genuine defect in the deployed candidate, found by accident while sweeping edge cases, and it is invisible in the
Apex profile because that profile never sends `n_predict 0`.

It also matters for the plan: the fidelity target is "native semantics", and the native build is the current tree. A candidate whose base predicate
disagrees with it on a legal request is not equivalent no matter how many bridges are merged.
