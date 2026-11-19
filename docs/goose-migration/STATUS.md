# FrankenMoE migration status

Updated: 2026-11-19 (25). This file is the current state; update it in the same change
as every accepted slice.

## Ledger

`docs/goose-migration/tasks.json`: **DONE 127**, **PARTIAL 18**, **TODO 7** of 152.

Read slice counters as port slices: 17 records show N/N and stay PARTIAL.

### PARTIAL

| requirement | what is open |
|---|---|
| P6-10 | grammar reject port: unit gate green (tests/grammar/reject-alternates 7 cases, first-set 6 cases) and the defect fixed... |
| P6-11 | MTP/draft-model routes for synth and rejection acceptance (needs a draft GGUF matching the target vocabulary); eleven ... |
| P7-06 | acceptance run on the real vocabulary against the shipped artefact |
| P7-07 | acceptance run on the real vocabulary against the shipped artefact |
| P7-12 | 1) delete the replaced native GGUF/reader/tokenizer bodies - blocked by rule 15 pending acceptance (the wiring itself ... |
| P8-06 | switch the reload sets and the connect/source choice, then exercise the lifecycle switches on a running model (load, s... |
| P8-09 | web_serve: static assets, MIME and ETag are still served by cpp-httplib set_mount_point - replacing that is the real w... |
| P8-12 | remove the remaining native server bodies after each switch is accepted: the Origin three-compare arm and the find_too... |
| P9-03 | replace the 25 call sites in the expert cache, dense placement and row streaming with the Goose decisions (commit-befo... |
| P9-04 | rewrite on_eval to the rh_topk_plan/rh_ask_plan contract and replace the overlap/row-source call sites |
| P9-05 | extract expert_bytes_per_slot(meta), keep rts_config as a drift pin, and wire rts_n_batch/rts_ubatch plus the MTP head... |
| P9-07 | wire the CSV, decode-trace and route-trace sinks and DeterministicWorkload to the Goose rows (integration step) |
| P9-08 | wire the closure decisions into session.cpp and cli/main.cpp |
| P10-06 | build-time CUDA architecture and flag decisions: CMake picks CMAKE_CUDA_ARCHITECTURES itself (vendor/flint/ggml/src/gg... |
| P10-07 | one build carrying both halves: bridge with an init entry for goose/json/json-parser.goose plus the _GOOSE_PAIRS entry... |
| P11-01 | standalone runtime has no CUDA, tokenizer engine or HTTP (the working Apex path is the INTEGRATED stage-run, not the s... |
| P11-04 | D1: F2 gguf_write_to_buf, F3 gguf_get_meta_size and F4 gguf_get_meta_data (all native in vendor/flint/ggml/src/gguf.cp... |
| C1-02 | T_NEXTN_HC_HEAD ids exist only in the integration tree, not the pinned upstream |

### TODO

| requirement | what is open |
|---|---|
| P11-07 | blocked on the 19 open APEX_COMMON tasks listed by tools/apex-accounting.py: P6-10, P7-06, P7-07, P7-12, P8-06, P8-09,... |
| C1-04 | shared loader, tokenizer and memory interfaces needed by Qwen |
| C1-05 | complete Qwen inference qualification in the common artifact |
| C1-06 | Apex protection while Qwen support is extended |
| F1-01 | All workstreams implemented; no owned handwritten C/C++/CUDA execution hidden as bridge/vendor dependency; historical ... |
| F1-02 | All required functional tasks have evidence; real Apex/Qwen and representative other family fixtures; one common artif... |
| F1-04 | Every functional task DONE; zero untracked residual native paths; source build reproducible and working runtime unchan... |

## Measured state (2026-11-20 01:40)

| measurement | value |
|---|---|
| baseline artefact | `builds/flint-acc/run-syncfix` - 200 doors, 20/20 components |
| speed, paired | decode **61.53**, prefill **2915.3**, VRAM 9465 MiB, hash `c2ff42cc1219` |
| with/without adapter | 61.53 / 59.57 - the sampler-sync fix recovered 3-5% |
| JSON path | accepted: door + adapter + swap 7/7 in strict mode |
| suite | 10 run, 0 failures, 6 precondition skips |
| grammar | Goose path witnessed 6x on a live constrained request |
| OpenWing | `export fn` works; three blockers reproduced (float-literal adaptation, `from` as a name - fixed on our side, stdlib path) |
