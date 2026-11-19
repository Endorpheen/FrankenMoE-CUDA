# FrankenMoE migration status

Updated: 2026-11-19 (25). This file is the current state; update it in the same change
as every accepted slice.

## Ledger

`docs/goose-migration/tasks.json`: **DONE 125**, **PARTIAL 18**, **TODO 9** of 152 functional requirements.

### PARTIAL

| requirement | what is open |
|---|---|
| P6-10 | grammar reject port: unit gate green (tests/grammar/reject-alternates 7 cases, first-set 6 cases) and the defect fixed in Goose, but the rea... |
| P6-11 | MTP/draft-model routes for synth and rejection acceptance (needs a draft GGUF matching the target vocabulary); eleven decisions not yet wire... |
| P7-06 | delete the replaced C++ tokenizer body - blocked by rule 15 until Igor accepts the served encoder |
| P7-07 | delete the replaced C++ detokenizer body - blocked by rule 15 until Igor accepts the served forms |
| P7-12 | 1) delete the replaced native GGUF/reader/tokenizer bodies - blocked by rule 15 pending acceptance (the wiring itself is done and measured: ... |
| P8-06 | switch the reload sets and the connect/source choice, then exercise the lifecycle switches on a running model (load, stop, unload, remove) w... |
| P8-09 | web_serve: static assets, MIME and ETag are still served by cpp-httplib set_mount_point - replacing that is the real work, and it needs a se... |
| P8-12 | remove the remaining native server bodies after each switch is accepted: the Origin three-compare arm and the find_tool linear scan are the ... |
| P9-03 | replace the 25 call sites in the expert cache, dense placement and row streaming with the Goose decisions (commit-before-touch, budget retur... |
| P9-04 | rewrite on_eval to the rh_topk_plan/rh_ask_plan contract and replace the overlap/row-source call sites |
| P9-05 | extract expert_bytes_per_slot(meta), keep rts_config as a drift pin, and wire rts_n_batch/rts_ubatch plus the MTP head admission order |
| P9-07 | wire the CSV, decode-trace and route-trace sinks and DeterministicWorkload to the Goose rows (integration step) |
| P9-08 | wire the closure decisions into session.cpp and cli/main.cpp |
| P10-06 | build-time CUDA architecture and flag decisions: CMake picks CMAKE_CUDA_ARCHITECTURES itself (vendor/flint/ggml/src/ggml-cuda/CMakeLists.txt... |
| P10-07 | rebuild libllama-common so the adapter is actually linked (vendor/flint/common/json-adapter.cpp exists and common_json::parse calls it under... |
| P11-01 | standalone runtime has no CUDA, tokenizer engine or HTTP (the working Apex path is the INTEGRATED stage-run, not the standalone - rule 15 fo... |
| P11-04 | D1: F2 gguf_write_to_buf, F3 gguf_get_meta_size and F4 gguf_get_meta_data (all native in vendor/flint/ggml/src/gguf.cpp, reached only by lla... |
| C1-02 | T_NEXTN_HC_HEAD ids exist only in the integration tree, not the pinned upstream |

### TODO

| requirement | what is open |
|---|---|
| P11-07 | blocked on the 19 open APEX_COMMON tasks listed by tools/apex-accounting.py: P6-10, P7-06, P7-07, P7-12, P8-06, P8-09, P8-12, P9-03, P9-04, ... |
| C1-03 | Qwen quantized precision, cold-bank and expert hook differences |
| C1-04 | shared loader, tokenizer and memory interfaces needed by Qwen |
| C1-05 | complete Qwen inference qualification in the common artifact |
| C1-06 | Apex protection while Qwen support is extended |
| C2-04 | remaining architecture extensions (Qwen builders, precision, loader and qualification) without regressing the common runtime or Apex |
| F1-01 | All workstreams implemented; no owned handwritten C/C++/CUDA execution hidden as bridge/vendor dependency; historical controls excluded from... |
| F1-02 | All required functional tasks have evidence; real Apex/Qwen and representative other family fixtures; one common artifact and reproducible r... |
| F1-04 | Every functional task DONE; zero untracked residual native paths; source build reproducible and working runtime unchanged. Then comprehensiv... |

## Measured state (2026-11-19, evening)

| measurement | value | tool |
|---|---|---|
| unit size | 156 modules, 1256 distinct exported doors | unit-carry, production-doors |
| shipped release carries | 198 doors, 6085 symbols, sha 0ffd98c2 | compare-artefacts |
| path A carries | 200 doors, 6090 symbols | compare-artefacts |
| path B carries | 11 doors, 6525 symbols, sha cbe39146 | compare-artefacts |
| release compiler | **only GCC 15.2.0**; both scripts pin gcc-13 | readelf -p .comment |
| suite | 10 run, 0 failures, 6 precondition skips | tests/run.sh fast |

The release reproduces from neither path: 192 doors of path B's set are absent from it and 5 are new.
