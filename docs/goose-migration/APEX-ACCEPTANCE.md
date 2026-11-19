# Where the Apex acceptance stands, generated from the registry

```
APEX_COMMON tasks: 133
  DONE    114
  PARTIAL 18
  TODO    1

Open tasks and their own remainder:
- P6-10 [PARTIAL] Port sampling,grammar and constrained generation
    grammar default-on blocked twice over: TD-S10 cost (12x) and a reproducible difference in the accepted set on the shipped path (EXP-1465-grammar); deletion of replaced native sampler bodies pending acceptance (integratio...
- P7-06 [PARTIAL] Port vocabulary loader and Apex tokenizer through a general tokenizer interface
    delete the replaced C++ tokenizer body - blocked by rule 15 until Igor accepts the served encoder
- P7-07 [PARTIAL] Port detokenization,token metadata and incremental text decoding
    delete the replaced C++ detokenizer body - blocked by rule 15 until Igor accepts the served forms
- P7-12 [PARTIAL] Close GGUF,loader,tokenizer and common native dependencies
    1) delete the replaced native GGUF/reader/tokenizer bodies - blocked by rule 15 pending acceptance (the wiring itself is done and measured: FG_GGUF_VALUES 34/34, STRINGS 13/13, ARRAYS 744231/744231, TINFO 753/753 on ever...
- P8-06 [PARTIAL] Port model management,connectors and hot reload
    switch the reload sets and the connect/source choice, then exercise the lifecycle switches on a running model (load, stop, unload, remove) which needs a model actually started through the router
- P8-09 [PARTIAL] Port web/UI serving and application integration
    wire web_serve and the API-key middleware into server-http.cpp; live server run needs Igor's approval
- P8-12 [PARTIAL] Remove native server implementation from candidate executable
    remove the remaining native server bodies after each switch is accepted: the Origin three-compare arm and the find_tool linear scan are the last two that still have one; relink the shipped application binary and remove t...
- P9-03 [PARTIAL] Port dense weights and expert streaming/LRU sources
    replace the 25 call sites in the expert cache, dense placement and row streaming with the Goose decisions (commit-before-touch, budget return, caller-owned quiesce_spec)
- P9-04 [PARTIAL] Port row streaming,router hooks and I/O-compute overlap
    rewrite on_eval to the rh_topk_plan/rh_ask_plan contract and replace the overlap/row-source call sites
- P9-05 [PARTIAL] Port standalone sessions,runtime and ngram draft
    extract expert_bytes_per_slot(meta), keep rts_config as a drift pin, and wire rts_n_batch/rts_ubatch plus the MTP head admission order
- P9-07 [PARTIAL] Port metrics/traces and deterministic workload infrastructure
    wire the CSV, decode-trace and route-trace sinks and DeterministicWorkload to the Goose rows (integration step)
- P9-08 [PARTIAL] Close standalone engine native implementation
    wire the closure decisions into session.cpp and cli/main.cpp
- P10-02 [PARTIAL] Port maintained command-line tools and utilities
    attach the three tool decisions to hosts that do not exist in this tree (checked 2026-11-19): vendor/flint/tools/CMakeLists.txt builds only mtmd, ui and server, there is no bench or tokenize source, and the fit-line prin...
- P10-06 [PARTIAL] Integrate deterministic Goose build,generated C/CUDA and packaging
    build-time CUDA architecture and flag decisions: CMake picks CMAKE_CUDA_ARCHITECTURES itself (vendor/flint/ggml/src/ggml-cuda/CMakeLists.txt), and a configure-time host cannot call the compiled door without a generated h...
- P10-07 [PARTIAL] Replace or justify vendored executable dependencies
    write the adapter from the parallel arrays into the common_json pimpl, shadow-compare it against nlohmann at common/json.cpp:205 using the text the dumper now produces, then serve it
- P10-09 [PARTIAL] Port test runners,fixtures and diagnostics as needed for native-free builds
    attach bench-diagnostics.goose once its hosts exist: scripts/bench-analyze.py and scripts/trace_io.py are absent from the tree (checked 2026-11-19), so there is nothing to wire them into
- P11-01 [PARTIAL] Assemble one fully migrated Apex-capable common runtime
    standalone runtime has no CUDA, tokenizer engine or HTTP (the working Apex path is the INTEGRATED stage-run, not the standalone - rule 15 forbids reporting one as the other); full 41-layer stack execution (dispatch verif...
- P11-04 [PARTIAL] Remove every replaced owned C/C++/CUDA implementation
    D1 remainder: F2-F5 (GGUF writer entries and the split plan), F8 (determine_vocab_type) and F10 (validate); D2-D4: the partial units, the quant row bodies last, each with the symbol audit; run the Apex A/B once approved
- P11-07 [TODO] Verify full Apex functional completion and remaining compatibility count
    blocked on the 19 open APEX_COMMON tasks listed by tools/apex-accounting.py: P6-10, P7-06, P7-07, P7-12, P8-06, P8-09, P8-12, P9-03, P9-04, P9-05, P9-07, P9-08, P10-02, P10-06, P10-07, P10-09, P10-10, P11-01, P11-04

P11-07 needs both conditions:
  all APEX_COMMON DONE: no - 19 open
  zero native executable logic on Apex paths: depends on the removals listed under P11-04 and the PARTIAL items above
```
