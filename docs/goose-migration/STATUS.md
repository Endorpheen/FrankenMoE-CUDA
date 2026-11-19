# FrankenMoE migration status

Updated: 2026-11-19 (25). This file is the current state; update it in the same change
as every accepted slice.

## Ledger

`docs/goose-migration/tasks.json`: **DONE 123**, **PARTIAL 19**, **TODO 10** of 152 functional requirements.
Do not read the DONE count as project percentage; it counts requirements, not code or time.

Generated from the records themselves; each row is that record's own remainder field.

### PARTIAL

| requirement | what is open |
|---|---|
| P6-10 | grammar reject port: unit gate green (tests/grammar/reject-alternates 7 cases, first-set 6 cases) and the defect fixed in Goose, but the real-vocabula... |
| P6-11 | MTP/draft-model routes for synth and rejection acceptance (needs a draft GGUF matching the target vocabulary); eleven decisions not yet wired to the h... |
| P7-06 | delete the replaced C++ tokenizer body - blocked by rule 15 until Igor accepts the served encoder |
| P7-07 | delete the replaced C++ detokenizer body - blocked by rule 15 until Igor accepts the served forms |
| P7-12 | 1) delete the replaced native GGUF/reader/tokenizer bodies - blocked by rule 15 pending acceptance (the wiring itself is done and measured: FG_GGUF_VA... |
| P8-06 | switch the reload sets and the connect/source choice, then exercise the lifecycle switches on a running model (load, stop, unload, remove) which needs... |
| P8-09 | wire web_serve and the API-key middleware into server-http.cpp |
| P8-12 | remove the remaining native server bodies after each switch is accepted: the Origin three-compare arm and the find_tool linear scan are the last two t... |
| P9-03 | replace the 25 call sites in the expert cache, dense placement and row streaming with the Goose decisions (commit-before-touch, budget return, caller-... |
| P9-04 | rewrite on_eval to the rh_topk_plan/rh_ask_plan contract and replace the overlap/row-source call sites |
| P9-05 | extract expert_bytes_per_slot(meta), keep rts_config as a drift pin, and wire rts_n_batch/rts_ubatch plus the MTP head admission order |
| P9-07 | wire the CSV, decode-trace and route-trace sinks and DeterministicWorkload to the Goose rows (integration step) |
| P9-08 | wire the closure decisions into session.cpp and cli/main.cpp |
| P10-06 | build-time CUDA architecture and flag decisions: CMake picks CMAKE_CUDA_ARCHITECTURES itself (vendor/flint/ggml/src/ggml-cuda/CMakeLists.txt), and a c... |
| P10-07 | write the adapter from the parallel arrays into the common_json pimpl, shadow-compare it against nlohmann at common/json.cpp:205 using the text the du... |
| P10-10 | Apple/Windows branches are transcription only on this host |
| P11-01 | standalone runtime has no CUDA, tokenizer engine or HTTP (the working Apex path is the INTEGRATED stage-run, not the standalone - rule 15 forbids repo... |
| P11-04 | D1: F2 gguf_write_to_buf, F3 gguf_get_meta_size and F4 gguf_get_meta_data (all native in vendor/flint/ggml/src/gguf.cpp, reached only by llama-quant.c... |
| C1-02 | T_NEXTN_HC_HEAD ids exist only in the integration tree, not the pinned upstream |

### TODO

| requirement | what is open |
|---|---|
| P11-07 | blocked on the 19 open APEX_COMMON tasks listed by tools/apex-accounting.py: P6-10, P7-06, P7-07, P7-12, P8-06, P8-09, P8-12, P9-03, P9-04, P9-05, P9-... |
| C1-03 | Qwen quantized precision, cold-bank and expert hook differences |
| C1-04 | shared loader, tokenizer and memory interfaces needed by Qwen |
| C1-05 | complete Qwen inference qualification in the common artifact |
| C1-06 | Apex protection while Qwen support is extended |
| C2-04 | remaining architecture extensions (Qwen builders, precision, loader and qualification) without regressing the common runtime or Apex |
| F1-01 | All workstreams implemented; no owned handwritten C/C++/CUDA execution hidden as bridge/vendor dependency; historical controls excluded from candidate... |
| F1-02 | All required functional tasks have evidence; real Apex/Qwen and representative other family fixtures; one common artifact and reproducible rollback. |
| F1-03 | Every remaining dependency justified with source/symbol/version/license; replaceable owned parser,kernel,scheduler or server is not called unavoidable... |
| F1-04 | Every functional task DONE; zero untracked residual native paths; source build reproducible and working runtime unchanged. Then comprehensive audits/o... |
