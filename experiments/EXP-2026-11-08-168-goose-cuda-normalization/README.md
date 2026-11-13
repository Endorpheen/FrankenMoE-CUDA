# EXP168: complete active CUDA normalization in Goose

Status: PASS scoped functional/GPU/rollback and one real Apex gate. Full Apex migration and stable fresh-C++ performance parity remain incomplete. Parent checkpoint before GLM integration: `d3c028f23acc4a7afc1006303a2fd5310db88f56`;unchanged previous runtime EXP167 `6dd3aae1fc5de7057eddda904f36584f5f15be3f`.

All eight norm/group/RMS/RMS-mul/RMS-scale/RMS-mul-add/backward/L2 host/device paths now execute self-hosted Goose algorithms. C/CUDA effects project typed POD fields,load/store/shuffle/synchronize,invoke ISA rsqrt/FMA and launch kernels. No original numerical callback on the default path. Native prefixed functions remain hidden as explicit rollback/oracles;complete native deletion is the separate P3-20 contract. Existing EXP155..167 objects are retained. Original CUDA relink SHA is identical before replacing only norm.cu.o. Compiler producer is pinned in producer.json;no compiler/production edits.

Exactly two authorized z-ai/glm-5.3-flash workers contributed host corrections and the GPU fixture. Both contributions are retained. Codex repaired response formatting,compile/metadata wiring,reference fusion rounding and test stream ordering;see correctness-findings.json and worker-review.json. The original fixture could race default-stream memset/H2D initialization against nonblocking compute;it now orders fixture setup explicitly. No runtime synchronization was added to hide it. One targeted memcheck and one racecheck were justified by reproduced sequence-dependent correctness failures;they are not a whole-runtime memory/security proof. The fixes were followed by normal unsanitized passing tests.

## Evidence

99GPUcases,95native-exact comparisons,12independent strided reference comparisons,2explicit NaN-policy cases,232515values. Default host/device mask0xff,scalar+vector mask3,fusion mask7. Native rollback87cases;legacy RMS rollback93cases;24preflight failures before stream/kernel effects;15additional malformed inputs reject and one native valid control completes. Corrected-stride cases remain tested against independent references;old-native rollback excludes those intentionally unsupported by native semantics. All bounded-scope evidence is in local-results.json;no broad correctness/performance/safety claim beyond it.

One unchanged fixed Apex32K/128 request:all128tokens,text and complete saved probabilities exact,127graph reuses. Actual new host/device mask0x18 (RMS-mul and RMS-scale),vector mask2,fusion mask5,cuda_status0;other modes are proven locally,not claimed exercised by this Apex profile. All3superseded numerical preloads removed only in this candidate. All instrumented host-owned heap counts0 at exit.

| Same workload | Prefill tok/s | Decode tok/s | Peak RSS MiB | Peak VRAM MiB |
|---|---:|---:|---:|---:|
| Archived C++ audit control | 2918.900 | 29.589 | 21309.152 | 3348 |
| EXP167 | 2635.581 | 26.567 | 21328.164 | 3246 |
| EXP168 | 2648.029 | 27.539 | 21328.535 | 3246 |

EXP168 vs EXP167:+0.472% prefill,+3.658% decode. Process swap0. One preliminary response with two external GLM CLI workers and uncontrolled external host load;archived C++ argv matches except executable,but this is not a fresh paired speed acceptance or stable acceleration claim. No model reruns,performance tuning,microbenchmarks or Qwen.

## Reproduce and rollback

From this experiment directory:`python3 build.py`,then `python3 check.py`. Only after all correctness checks pass,one authorized fixed-profile run with `python3 run_apex.py`;the runner refuses to overwrite an existing receipt. Generated host C compiles separately with explicit FFI prototypes;device extraction performs only selection/qualification/device decoration and raw storage mapping,with no arithmetic rewrites. `build.py --host-only` verifies immutable device source/object hashes before reuse. Raw model requests/responses and worker logs remain local,not newly published.

Use `GGML_GOOSE_CUDA_NORM=0` for original normalization;legacy `GGML_GOOSE_RMS=0` disables only modes2..5. The complete immutable EXP167 runtime remains available under `/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp167/run`;use its unchanged profile/preloads for full-runtime rollback. No production symlink/launcher changes. The initial source-only checkpoint-checks.json is historical evidence for d3c028f;current stage evidence is separate.

P3-01 is functionally DONE using joint EXP166/168 evidence. Other CUDA operations,ISA helpers,CPU math,provider/model/KV/loader/server and actual native-source deletion remain. No delivery cycle/stage/full-Apex completion is implied. Two GLM workers continue isolated row-reduction/argmax drafts;no scale above2until review throughput is demonstrated.
