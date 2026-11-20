# Production C++ audit and 152 requirement dispositions

Date: 2026-11-20. Scope includes required upstream, vendor, kernels, bridges, wrappers, callbacks, fallbacks and infrastructure across previously supported capabilities.

## Observed artifact and limits

Path B checkpoint franken-pathb-safe-20261120.7dnWLq, libllama SHA-256 3ab50925bd04686dac0381f38fe88cd155a5eff013ed33ef5d4ddc6dc35e2d15. Executable mappings come from results/apex-runs/pathb-safe512-b2-20261120-043140/maps.txt. The active run-jsondefault control is a separate artifact whose full producer is undocumented. The historical native reference passed all nine manifest checks.

The checkpoint core link includes **201 C++ translation-unit objects** and **12 C++ M1 objects**, from the staged source and object manifest and the build-from-tree link inputs. These are compiled objects, not observed paths.

| Mapped ELF | Defined dynamic C++ text symbols | SHA-256 |
|---|---:|---|
| libggml-base.so.0 | 23 | 7e5397942b9f0f0f644484512e69c78b7dabbd9bbfccc0bf853ad163aa8f2bf5 |
| libggml-cpu.so.0 | 208 | 8dff918c63ebd67e6dd1559f122a45777995a18ddf6c2336900a46bf03d93448 |
| libggml-cuda.so.0 | 825 | 0e7bd3120deba538b9dbbfd8eef216367ac5040313ec3575e68ea0bdaa51d210 |
| libggml.so.0 | 8 | 1e3ee56fc72c957b54b950352401014aea36723fe11f9a48203bb5fc83271f60 |
| libllama-common.so.0 | 3807 | 7418168d4002f351eec396220fbaa3e55522edb5fae6338b2bbef28927eca7d3 |
| libllama-server-impl.so | 2715 | 72da0b699fde24dd519663afe5a2d3506caeb06ae57073449460fb14c875ada9 |
| libllama.so.0 | 4503 | 3ab50925bd04686dac0381f38fe88cd155a5eff013ed33ef5d4ddc6dc35e2d15 |
| libmoe_mmid_tiled.so | 0 | 1821f513266ade87f316063555e80750a515662743d80e4b2b696452a0c4726f |
| libmtmd.so.0 | 690 | 7248a0d1507e7bdee5939fbda300f156f6894146864bf742d9dc75b2cf1affad |
| llama-server | 6 | 2c0b6da3c10a7f4d436ddc497885894541d6c21642b95d1e936f45a0f065e8b9 |

Counts include only defined dynamic Itanium C++ text symbols. This detects residual code but cannot count unique source functions or rule out hidden symbols, C-linked C++ bodies, CUDA device kernels, plugin routes or indirect calls. All such paths and non-Apex architectures require further attribution.

## Mapped C++ runtime dependencies

The [ELF dependency capture](PRODUCTION-ELF-DEPENDENCIES-20261120.json) reads DT_NEEDED from executable mappings in the control and the lazy-KV diagnostic. It resolves each declared SONAME to a file mapped in the corresponding process. The control mapped 27 ELF files and the diagnostic mapped 28, including its instrumentation.

Nine components in both snapshots directly require libstdc++: the server executable, server implementation, llama-common, MTMD, libllama, GGML registry, GGML base, GGML CPU and GGML CUDA. Required linkage is already disqualifying under the production rule. This snapshot does not enumerate unsampled plugin routes or all executed functions.

## Matching support build inputs

Byte comparison matched only libllama-common.so.0 to the recorded frankenmoe-cand-jsondefault/support build. Its Ninja link input and static archives map the object inputs below to compile_commands.json. An archive member is a link input and can be dropped by archive selection. These are build candidates for source attribution, not a count of executed functions.

Object inputs with resolved C++ or CUDA translation units: **59**. Unresolved object locators: **0**.

| Object input | Recorded translation unit |
|---|---|
| common/CMakeFiles/llama-common-base.dir/build-info.cpp.o | /home/end0/AI/LlamaServer/frankenmoe-cand-jsondefault/support/common/build-info.cpp |
| common/CMakeFiles/llama-common.dir/arg.cpp.o | common/arg.cpp |
| common/CMakeFiles/llama-common.dir/chat-auto-parser-generator.cpp.o | common/chat-auto-parser-generator.cpp |
| common/CMakeFiles/llama-common.dir/chat-auto-parser-helpers.cpp.o | common/chat-auto-parser-helpers.cpp |
| common/CMakeFiles/llama-common.dir/chat-diff-analyzer.cpp.o | common/chat-diff-analyzer.cpp |
| common/CMakeFiles/llama-common.dir/chat-peg-parser.cpp.o | common/chat-peg-parser.cpp |
| common/CMakeFiles/llama-common.dir/chat.cpp.o | common/chat.cpp |
| common/CMakeFiles/llama-common.dir/common.cpp.o | common/common.cpp |
| common/CMakeFiles/llama-common.dir/console.cpp.o | common/console.cpp |
| common/CMakeFiles/llama-common.dir/debug.cpp.o | common/debug.cpp |
| common/CMakeFiles/llama-common.dir/download.cpp.o | common/download.cpp |
| common/CMakeFiles/llama-common.dir/fit.cpp.o | common/fit.cpp |
| common/CMakeFiles/llama-common.dir/hf-cache.cpp.o | common/hf-cache.cpp |
| common/CMakeFiles/llama-common.dir/imatrix-loader.cpp.o | common/imatrix-loader.cpp |
| common/CMakeFiles/llama-common.dir/jinja/caps.cpp.o | common/jinja/caps.cpp |
| common/CMakeFiles/llama-common.dir/jinja/lexer.cpp.o | common/jinja/lexer.cpp |
| common/CMakeFiles/llama-common.dir/jinja/parser.cpp.o | common/jinja/parser.cpp |
| common/CMakeFiles/llama-common.dir/jinja/runtime.cpp.o | common/jinja/runtime.cpp |
| common/CMakeFiles/llama-common.dir/jinja/string.cpp.o | common/jinja/string.cpp |
| common/CMakeFiles/llama-common.dir/jinja/value.cpp.o | common/jinja/value.cpp |
| common/CMakeFiles/llama-common.dir/json-adapter.cpp.o | common/json-adapter.cpp |
| common/CMakeFiles/llama-common.dir/json-schema-to-grammar.cpp.o | common/json-schema-to-grammar.cpp |
| common/CMakeFiles/llama-common.dir/json-schema.cpp.o | common/json-schema.cpp |
| common/CMakeFiles/llama-common.dir/json.cpp.o | common/json.cpp |
| common/CMakeFiles/llama-common.dir/llguidance.cpp.o | common/llguidance.cpp |
| common/CMakeFiles/llama-common.dir/log.cpp.o | common/log.cpp |
| common/CMakeFiles/llama-common.dir/ngram-cache.cpp.o | common/ngram-cache.cpp |
| common/CMakeFiles/llama-common.dir/ngram-map.cpp.o | common/ngram-map.cpp |
| common/CMakeFiles/llama-common.dir/ngram-mod.cpp.o | common/ngram-mod.cpp |
| common/CMakeFiles/llama-common.dir/parsers/cohere2moe.cpp.o | common/parsers/cohere2moe.cpp |
| common/CMakeFiles/llama-common.dir/parsers/deepseek.cpp.o | common/parsers/deepseek.cpp |
| common/CMakeFiles/llama-common.dir/parsers/functionary-v3-2.cpp.o | common/parsers/functionary-v3-2.cpp |
| common/CMakeFiles/llama-common.dir/parsers/gemma4.cpp.o | common/parsers/gemma4.cpp |
| common/CMakeFiles/llama-common.dir/parsers/gigachat-v3.cpp.o | common/parsers/gigachat-v3.cpp |
| common/CMakeFiles/llama-common.dir/parsers/gpt-oss.cpp.o | common/parsers/gpt-oss.cpp |
| common/CMakeFiles/llama-common.dir/parsers/harmony-xml.cpp.o | common/parsers/harmony-xml.cpp |
| common/CMakeFiles/llama-common.dir/parsers/k2-horizon.cpp.o | common/parsers/k2-horizon.cpp |
| common/CMakeFiles/llama-common.dir/parsers/kimi-k2.cpp.o | common/parsers/kimi-k2.cpp |
| common/CMakeFiles/llama-common.dir/parsers/kimi-k3.cpp.o | common/parsers/kimi-k3.cpp |
| common/CMakeFiles/llama-common.dir/parsers/lfm2.cpp.o | common/parsers/lfm2.cpp |
| common/CMakeFiles/llama-common.dir/parsers/ling3.cpp.o | common/parsers/ling3.cpp |
| common/CMakeFiles/llama-common.dir/parsers/llm-jp-harmony.cpp.o | common/parsers/llm-jp-harmony.cpp |
| common/CMakeFiles/llama-common.dir/parsers/minicpm5.cpp.o | common/parsers/minicpm5.cpp |
| common/CMakeFiles/llama-common.dir/parsers/minimax-m3.cpp.o | common/parsers/minimax-m3.cpp |
| common/CMakeFiles/llama-common.dir/parsers/ministral3.cpp.o | common/parsers/ministral3.cpp |
| common/CMakeFiles/llama-common.dir/parsers/muse-glimmer.cpp.o | common/parsers/muse-glimmer.cpp |
| common/CMakeFiles/llama-common.dir/parsers/muse-spark.cpp.o | common/parsers/muse-spark.cpp |
| common/CMakeFiles/llama-common.dir/parsers/parsers.cpp.o | common/parsers/parsers.cpp |
| common/CMakeFiles/llama-common.dir/parsers/qwen3-coder.cpp.o | common/parsers/qwen3-coder.cpp |
| common/CMakeFiles/llama-common.dir/parsers/translate-gemma.cpp.o | common/parsers/translate-gemma.cpp |
| common/CMakeFiles/llama-common.dir/peg-parser.cpp.o | common/peg-parser.cpp |
| common/CMakeFiles/llama-common.dir/preset.cpp.o | common/preset.cpp |
| common/CMakeFiles/llama-common.dir/reasoning-budget.cpp.o | common/reasoning-budget.cpp |
| common/CMakeFiles/llama-common.dir/sampling.cpp.o | common/sampling.cpp |
| common/CMakeFiles/llama-common.dir/speculative.cpp.o | common/speculative.cpp |
| common/CMakeFiles/llama-common.dir/subproc.cpp.o | common/subproc.cpp |
| common/CMakeFiles/llama-common.dir/trie.cpp.o | common/trie.cpp |
| common/CMakeFiles/llama-common.dir/unicode.cpp.o | common/unicode.cpp |
| vendor/cpp-httplib/CMakeFiles/cpp-httplib.dir/httplib.cpp.o | vendor/cpp-httplib/httplib.cpp |

That support build does not match the other mapped components. The separate Make producer is compared below.

The [per-object inventory](PRODUCTION-CPP-OBJECTS-20261120.tsv) records the 213 verified linked core and M1 C++ objects and their SHA-256 values. They define 8,394 text symbols at object level, including local and weak symbols and possible duplicate template instantiations. This count is not a unique function count. A search of local build bin directories found no other byte-matching support producer. The model SHA-256 separately matched the reference manifest.

## Byte-matched Make producer for server, CPU, registry and MTMD

The five outputs in this table match builds/flint-acc/bin byte for byte. The Make link commands and static archive inputs resolve through that build’s compile_commands.json. Archive members may be omitted at final link. Unresolved entries are reported separately.

| Loaded output | Linked object inputs | Attributed C++/CUDA inputs | Unresolved object inputs |
|---|---:|---:|---:|
| llama-server | 19 | 14 | 0 |
| libllama-server-impl.so | 22 | 17 | 0 |
| libggml-cpu.so.0 | 17 | 14 | 0 |
| libggml.so.0 | 2 | 2 | 0 |
| libmtmd.so.0 | 58 | 55 | 0 |

See the [per-component translation-unit inventory](PRODUCTION-CPP-SUPPORT-OBJECTS-20261120.tsv). The exact producers of GGML base, CUDA and the tiled MoE library still need attribution. The mapped CUDA file byte-matches the local EXP1463 checkpoint. The tiled MoE file byte-matches the EXP1476-loadable copy, whose README records the release artifact as its source. These copies establish artifact lineage; neither supplies the original compile and link inputs.

## Translation-unit names retained in mapped ELF files

The [ELF file-symbol inventory](PRODUCTION-ELF-FILE-SYMBOLS-20261120.tsv) preserves STT_FILE records from the exact checkpoint libraries. NVCC generated host labels provide a CUDA source basename hint. Matching basenames are candidate source locations, not evidence of the exact source revision or compiler command. Startup objects and generated labels stay visible.

| Loaded component | Distinct FILE names | Labels with tracked source candidates |
|---|---:|---:|
| libggml-base.so.0 | 11 | 10 |
| libggml-cuda.so.0 | 146 | 145 |
| libmoe_mmid_tiled.so | 2 | 1 |

GGML base retains executable implementation file labels including ggml.cpp, ggml-backend.cpp, ggml-backend-meta.cpp, ggml-threading.cpp, ggml-opt.cpp and gguf.cpp. The CUDA library embeds compiler-generated host translation-unit names. The tiled MoE library retains wrapper_mmid.c. The exact producers and compiler inputs remain open.

The exact mapped CUDA library carries **145 sm_89 cubins and no PTX**, according to cuobjdump --list-elf and --list-ptx. The cubin listing SHA-256 is b8e7b243483ce98d75699222738c2c0fa30801cb9b686d3ee821b6f1116af0fd. The ELF .comment identifies the host GCC as 13.4.0. This bounds the device images observed in this artifact; other SMs remain compatibility work and the full original compile command is not established.

## Staged model factory coverage

The [factory matrix](PRODUCTION-MODEL-FACTORIES-20261120.tsv) comes from the exact staged llama-arch.cpp and llama-model.cpp under the audited checkpoint. It contains 164 registry names, with 161 names matched to a C++ factory return. Entries without a constructor match are LLM_ARCH_GPTJ, LLM_ARCH_MAXWELL, LLM_ARCH_UNKNOWN. This is static registry and factory evidence. It neither certifies the models nor establishes whether an unresolved entry is supported by the active release. It gives the concrete compatibility surface to reconcile before removing the C++ factory and graph builders.

The benchmark journal records nine successful Maxwell-V1-Flash responses. The audited Path B factory has no LLM_ARCH_MAXWELL case, even though its registry names maxwell_moe and the staged model source is present. The factory defaults to an unsupported-architecture exception. The tracked vendor factory contains a Maxwell case. This is a concrete compatibility gap to close and validate against the prior Maxwell control before any universal promotion. GPTJ appears in runtime switches outside the model factory and needs a separate support-history check. UNKNOWN is the sentinel.

## Requirement-by-requirement disposition

Source locators resolve against tracked repository and normalized vendor, reconstructed and upstream paths. A compiled core example matches a staged source with a linked C++ object. Source leads include C and headers. A missing match is unresolved and requires direct attribution. Historical evidence remains in the ledger.

| Requirement | Prior → now | Linked core C++ matches | Tracked native source leads | Disposition |
|---|---|---|---|---|
| P0-01 | DONE → DONE | 0 | 0 | Reference preservation verified. All nine manifest entries match. |
| P0-02 | DONE → PARTIAL | 0 | 415: runtime/checkpoints/exp186-cuda-matvec-v61-20261111/generated/exp186v61/cuda-runtime.cu, runtime/checkpoints/exp186-cuda-matvec-v61-20261111/generated/exp186v61/device-functions-generated.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P0-03 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P0-04 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P0-05 | DONE → PARTIAL | 0 | 17: tests/grammar/first-set/fixture.cpp, tests/grammar/reject-alternates/fixture.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P0-06 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P1-01 | DONE → PARTIAL | 0 | 8: vendor/flint/ggml/src/ggml-cpu/arch/arm/quants.c, vendor/flint/ggml/src/ggml-cpu/arch/loongarch/quants.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-02 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/arch/x86/quants.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-03 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-quants.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-04 | DONE → PARTIAL | 0 | 4: vendor/flint/ggml/src/ggml-cpu/tiled/tiled-kernel.cpp, vendor/flint/ggml/src/ggml-cpu/tiled/tiled-kernel.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-05 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ggml-cpu.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-06 | DONE → PARTIAL | 0 | 9: vendor/flint/ggml/src/ggml-cpu/arch/arm/quants.c, vendor/flint/ggml/src/ggml-cpu/arch/loongarch/quants.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-07 | DONE → PARTIAL | 0 | 6: vendor/flint/ggml/src/ggml-cpu/arch/arm/repack.cpp, vendor/flint/ggml/src/ggml-cpu/arch/riscv/repack.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-08 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ggml-cpu.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-09 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-cpu/ggml-cpu.c, vendor/flint/ggml/src/ggml-threading.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-10 | DONE → PARTIAL | 1: src/llama-moe-cache.cpp | 3: runtime/reconstructed/src/llama-moe-cache.cpp, vendor/flint/ggml/src/ggml-cpu/ggml-cpu.c | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P1-11 | DONE → PARTIAL | 0 | 40: vendor/flint/ggml/src/ggml-cpu/amx/amx.cpp, vendor/flint/ggml/src/ggml-cpu/amx/amx.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P1-12 | DONE → PARTIAL | 0 | 69: vendor/flint/ggml/src/ggml-cpu/amx/amx.cpp, vendor/flint/ggml/src/ggml-cpu/amx/amx.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-01 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-cpu/binary-ops.cpp, vendor/flint/ggml/src/ggml-cpu/ops.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-02 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/src/ggml-cpu/ops.cpp, vendor/flint/ggml/src/ggml-cpu/vec.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-03 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-cpu/simd-gemm.h, vendor/flint/ggml/src/ggml-cpu/simd-mappings.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-04 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/src/ggml-cpu/ggml-cpu.c, vendor/flint/ggml/src/ggml-cpu/llamafile/sgemm.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-05 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ops.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-06 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ops.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-07 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ops.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-08 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ops.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-09 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/src/ggml-cpu/common.h, vendor/flint/ggml/src/ggml-cpu/ops.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-10 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ops.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-13 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-cpu/quants.c, vendor/flint/ggml/src/ggml-cpu/traits.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-14 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-cpu/ggml-cpu.cpp, vendor/flint/ggml/src/ggml-cpu/hbm.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-15 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/ggml-cpu.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-16 | DONE → PARTIAL | 0 | 69: vendor/flint/ggml/src/ggml-cpu/amx/amx.cpp, vendor/flint/ggml/src/ggml-cpu/amx/amx.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P2-17 | DONE → PARTIAL | 167: src/models/afmoe.cpp, src/models/apertus.cpp | 168: vendor/flint/src/models/afmoe.cpp, vendor/flint/src/models/apertus.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P2-18 | DONE → PARTIAL | 0 | 69: vendor/flint/ggml/src/ggml-cpu/amx/amx.cpp, vendor/flint/ggml/src/ggml-cpu/amx/amx.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-01 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-cuda/norm.cu, vendor/flint/ggml/src/ggml-cuda/scale.cu | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-02 | DONE → PARTIAL | 0 | 4: vendor/flint/ggml/src/ggml-cuda/common.cuh, vendor/flint/ggml/src/ggml-cuda/convert.cu | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-03 | DONE → PARTIAL | 0 | 17: vendor/flint/ggml/src/ggml-cuda/concat.cu, vendor/flint/ggml/src/ggml-cuda/concat.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-04 | DONE → PARTIAL | 0 | 14: vendor/flint/ggml/src/ggml-cuda/acc.cu, vendor/flint/ggml/src/ggml-cuda/acc.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-05 | DONE → PARTIAL | 0 | 18: vendor/flint/ggml/src/ggml-cuda/argmax.cu, vendor/flint/ggml/src/ggml-cuda/argmax.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-06 | DONE → PARTIAL | 0 | 12: vendor/flint/ggml/src/ggml-cuda/arange.cu, vendor/flint/ggml/src/ggml-cuda/arange.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-07 | DONE → PARTIAL | 0 | 4: vendor/flint/ggml/src/ggml-cuda/dequantize.cuh, vendor/flint/ggml/src/ggml-cuda/quantize.cu | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-08 | DONE → PARTIAL | 0 | 123: vendor/flint/ggml/src/ggml-cuda/mmvq.cu, vendor/flint/ggml/src/ggml-cuda/mmvq.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-09 | DONE → PARTIAL | 0 | 134: vendor/flint/ggml/src/ggml-cuda/mmq-config-ampere.cuh, vendor/flint/ggml/src/ggml-cuda/mmq-config-blackwell.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-10 | DONE → PARTIAL | 0 | 6: vendor/flint/ggml/src/ggml-cuda/mmf.cu, vendor/flint/ggml/src/ggml-cuda/mmf.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-11 | DONE → PARTIAL | 0 | 6: vendor/flint/ggml/src/ggml-cuda/mmid.cu, vendor/flint/ggml/src/ggml-cuda/mmid.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-12 | DONE → PARTIAL | 0 | 127: vendor/flint/ggml/src/ggml-cuda/fattn-common.cuh, vendor/flint/ggml/src/ggml-cuda/fattn-mma-f16.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-13 | DONE → PARTIAL | 0 | 18: vendor/flint/ggml/src/ggml-cuda/dsv4-hc.cu, vendor/flint/ggml/src/ggml-cuda/dsv4-hc.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-14 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/src/ggml-cuda/fwht.cu, vendor/flint/ggml/src/ggml-cuda/fwht.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-16 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cuda/ggml-cuda.cu | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-17 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cuda/ggml-cuda.cu | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-18 | DONE → PARTIAL | 0 | 15: vendor/flint/ggml/src/ggml-cuda/allreduce.cu, vendor/flint/ggml/src/ggml-cuda/allreduce.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-19 | DONE → PARTIAL | 0 | 284: vendor/flint/ggml/src/ggml-cuda/acc.cu, vendor/flint/ggml/src/ggml-cuda/acc.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P3-20 | DONE → PARTIAL | 0 | 284: vendor/flint/ggml/src/ggml-cuda/acc.cu, vendor/flint/ggml/src/ggml-cuda/acc.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-01 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-alloc.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-02 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-alloc.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-03 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/include/ggml.h, vendor/flint/ggml/src/ggml.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-04 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/src/ggml-opt.cpp, vendor/flint/ggml/src/ggml.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-05 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-alloc.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-06 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-alloc.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-07 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/src/ggml-backend-impl.h, vendor/flint/ggml/src/ggml-backend.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-08 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-backend-dl.cpp, vendor/flint/ggml/src/ggml-backend-reg.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-09 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-backend.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-10 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-backend.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-11 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-backend.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-12 | DONE → PARTIAL | 0 | 3: vendor/flint/ggml/src/ggml-backend-meta.cpp, vendor/flint/ggml/src/ggml-opt.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-13 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/src/ggml-alloc.c, vendor/flint/ggml/src/ggml-backend.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-14 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-alloc.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P4-15 | DONE → PARTIAL | 1: src/llama-context.cpp | 3: runtime/reconstructed/src/llama-context.cpp, vendor/flint/ggml/src/ggml-backend.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P4-16 | DONE → PARTIAL | 0 | 50: vendor/flint/ggml/src/ggml-alloc.c, vendor/flint/ggml/src/ggml-backend-dl.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P5-01 | DONE → PARTIAL | 1: src/llama-kv-cache.cpp | 4: runtime/reconstructed/src/llama-kv-cache.cpp, runtime/reconstructed/src/llama-kv-cells.h | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-02 | DONE → PARTIAL | 1: src/llama-kv-cache.cpp | 2: runtime/reconstructed/src/llama-kv-cache.cpp, vendor/flint/src/llama-kv-cache.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-03 | DONE → PARTIAL | 5: src/llama-kv-cache-dsa-iswa.cpp, src/llama-kv-cache-dsa.cpp | 5: vendor/flint/src/llama-kv-cache-dsa-iswa.cpp, vendor/flint/src/llama-kv-cache-dsa.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-04 | DONE → PARTIAL | 1: src/llama-memory-recurrent.cpp | 2: runtime/reconstructed/src/llama-memory-recurrent.cpp, vendor/flint/src/llama-memory-recurrent.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-05 | DONE → PARTIAL | 4: src/llama-memory-hybrid-idx.cpp, src/llama-memory-hybrid-iswa.cpp | 5: runtime/reconstructed/src/llama-memory-hybrid.cpp, vendor/flint/src/llama-memory-hybrid-idx.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-06 | DONE → PARTIAL | 6: src/llama-kv-cache-dsa-iswa.cpp, src/llama-kv-cache-dsa.cpp | 8: runtime/reconstructed/src/llama-kv-cache.cpp, vendor/flint/ggml/src/ggml-cuda/ggml-cuda.cu | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-07 | DONE → PARTIAL | 6: src/llama-context.cpp, src/llama-memory-hybrid-idx.cpp | 10: runtime/reconstructed/src/llama-context.cpp, runtime/reconstructed/src/llama-memory-hybrid.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-08 | DONE → PARTIAL | 2: src/llama-context.cpp, src/llama-io.cpp | 3: runtime/reconstructed/src/llama-context.cpp, vendor/flint/src/llama-context.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-09 | DONE → PARTIAL | 7: src/llama-context.cpp, src/llama-kv-cache-dsa-iswa.cpp | 9: runtime/reconstructed/src/llama-context.cpp, runtime/reconstructed/src/llama-kv-cache.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P5-10 | DONE → PARTIAL | 11: src/llama-kv-cache-dsa-iswa.cpp, src/llama-kv-cache-dsa.cpp | 14: runtime/reconstructed/src/llama-kv-cache.cpp, runtime/reconstructed/src/llama-memory-hybrid.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-01 | DONE → PARTIAL | 3: src/llama-arch.cpp, src/llama-cparams.cpp | 4: runtime/reconstructed/src/llama-hparams.cpp, vendor/flint/src/llama-arch.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-02 | DONE → PARTIAL | 1: src/llama-model.cpp | 4: runtime/reconstructed/src/llama-model.cpp, runtime/reconstructed/src/llama-model.h | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-03 | DONE → PARTIAL | 1: src/llama-graph.cpp | 4: runtime/reconstructed/src/llama-graph.cpp, runtime/reconstructed/src/llama-graph.h | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-04 | DONE → PARTIAL | 3: src/llama-graph.cpp, src/models/qwen35.cpp | 4: runtime/reconstructed/src/llama-graph.cpp, vendor/flint/src/llama-graph.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-05 | DONE → PARTIAL | 167: src/models/afmoe.cpp, src/models/apertus.cpp | 167: vendor/flint/src/models/afmoe.cpp, vendor/flint/src/models/apertus.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-06 | DONE → PARTIAL | 167: src/models/afmoe.cpp, src/models/apertus.cpp | 167: vendor/flint/src/models/afmoe.cpp, vendor/flint/src/models/apertus.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-07 | DONE → PARTIAL | 2: src/llama-batch.cpp, src/llama-context.cpp | 4: runtime/reconstructed/src/llama-batch.cpp, runtime/reconstructed/src/llama-context.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-08 | DONE → PARTIAL | 2: src/llama-context.cpp, src/llama.cpp | 3: runtime/reconstructed/src/llama-context.cpp, vendor/flint/src/llama-context.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-09 | DONE → PARTIAL | 2: src/llama-adapter.cpp, src/llama-model-saver.cpp | 2: vendor/flint/src/llama-adapter.cpp, vendor/flint/src/llama-model-saver.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-10 | PARTIAL → PARTIAL | 2: src/llama-grammar.cpp, src/llama-sampler.cpp | 4: runtime/reconstructed/src/llama-sampler.cpp, vendor/flint/common/sampling.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-11 | PARTIAL → PARTIAL | 1: src/llama-context.cpp | 6: runtime/reconstructed/src/llama-context.cpp, vendor/flint/common/ngram-cache.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P6-12 | DONE → PARTIAL | 201: src/gguf-reader-owner.cpp, src/llama-adapter.cpp | 287: runtime/reconstructed/src/gates.c, runtime/reconstructed/src/llama-adapter.h | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P7-01 | DONE → PARTIAL | 0 | 2: vendor/flint/ggml/include/gguf.h, vendor/flint/ggml/src/gguf.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P7-02 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/gguf.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P7-03 | DONE → PARTIAL | 3: src/llama-io.cpp, src/llama-mmap.cpp | 4: runtime/reconstructed/src/llama-model-loader.cpp, vendor/flint/src/llama-io.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P7-04 | DONE → PARTIAL | 1: src/llama-quant.cpp | 2: vendor/flint/common/imatrix-loader.cpp, vendor/flint/src/llama-quant.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P7-05 | DONE → PARTIAL | 2: src/unicode-data.cpp, src/unicode.cpp | 4: runtime/reconstructed/src/unicode.cpp, vendor/flint/common/unicode.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P7-06 | PARTIAL → PARTIAL | 1: src/llama-vocab.cpp | 2: runtime/reconstructed/src/llama-vocab.cpp, vendor/flint/src/llama-vocab.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P7-07 | PARTIAL → PARTIAL | 1: src/llama-vocab.cpp | 2: runtime/reconstructed/src/llama-vocab.cpp, vendor/flint/src/llama-vocab.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P7-08 | DONE → PARTIAL | 1: src/llama-chat.cpp | 41: vendor/flint/common/chat-auto-parser-generator.cpp, vendor/flint/common/chat-auto-parser-helpers.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P7-09 | DONE → PARTIAL | 0 | 6: vendor/flint/common/json-adapter.cpp, vendor/flint/common/json-schema-to-grammar.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P7-10 | DONE → PARTIAL | 0 | 4: vendor/flint/common/arg.cpp, vendor/flint/common/common.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P7-11 | DONE → PARTIAL | 0 | 7: vendor/flint/common/console.cpp, vendor/flint/common/debug.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P7-12 | PARTIAL → PARTIAL | 1: src/llama-vocab.cpp | 100: runtime/reconstructed/src/llama-vocab.cpp, vendor/flint/common/arg.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P8-01 | DONE → PARTIAL | 0 | 3: vendor/flint/tools/server/main.cpp, vendor/flint/tools/server/server-common.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-02 | DONE → PARTIAL | 0 | 3: vendor/flint/common/http.h, vendor/flint/tools/server/server-cors-proxy.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-03 | DONE → PARTIAL | 0 | 3: vendor/flint/tools/server/server-context.cpp, vendor/flint/tools/server/server-queue.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-04 | DONE → PARTIAL | 0 | 3: vendor/flint/tools/server/server-chat.cpp, vendor/flint/tools/server/server-context.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-05 | DONE → PARTIAL | 0 | 2: vendor/flint/tools/server/server-context.cpp, vendor/flint/tools/server/server-schema.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-06 | PARTIAL → PARTIAL | 0 | 2: vendor/flint/tools/server/server-connect.cpp, vendor/flint/tools/server/server-models.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-07 | DONE → PARTIAL | 0 | 4: vendor/flint/tools/server/server-decision.cpp, vendor/flint/tools/server/server-mcp.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-08 | DONE → PARTIAL | 0 | 1: vendor/flint/tools/server/server-common.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-09 | PARTIAL → PARTIAL | 0 | 30: vendor/flint/tools/server/main.cpp, vendor/flint/tools/server/server-chat.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-10 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P8-11 | DONE → PARTIAL | 0 | 1: vendor/flint/tools/server/server-context.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P8-12 | PARTIAL → PARTIAL | 0 | 126: vendor/flint/common/arg.cpp, vendor/flint/common/arg.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-01 | DONE → PARTIAL | 0 | 2: runtime/core/src/config.cpp, runtime/core/src/moe/arch_registry.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-02 | DONE → PARTIAL | 0 | 5: runtime/core/src/io/file_reader.cpp, runtime/core/src/io/file_reader.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-03 | PARTIAL → PARTIAL | 0 | 2: runtime/core/src/moe/dense_weights.cpp, runtime/core/src/moe/expert_stream_source.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-04 | PARTIAL → PARTIAL | 0 | 3: runtime/core/src/moe/router_hook.cpp, runtime/core/src/moe/row_stream.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-05 | PARTIAL → PARTIAL | 0 | 3: runtime/core/src/engine/ngram_draft.cpp, runtime/core/src/engine/runtime.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-06 | DONE → PARTIAL | 0 | 3: runtime/cli/main.cpp, runtime/core/src/engine/chat_parse.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-07 | PARTIAL → PARTIAL | 0 | 4: runtime/core/src/metrics/csv_metrics_sink.cpp, runtime/core/src/metrics/decode_trace_sink.cpp | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P9-08 | PARTIAL → PARTIAL | 0 | 415: runtime/checkpoints/exp186-cuda-matvec-v61-20261111/generated/exp186v61/cuda-runtime.cu, runtime/checkpoints/exp186-cuda-matvec-v61-20261111/generated/exp186v61/device-functions-generated.cuh | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P10-01 | DONE → PARTIAL | 201: src/gguf-reader-owner.cpp, src/llama-adapter.cpp | 2596: experiments/EXP-2026-11-07-136-goose-runtime/source/allocator-coalesce/benchmark.c, experiments/EXP-2026-11-07-136-goose-runtime/source/allocator-coalesce/reference-release.c | Compiled C++ found. Prove reach, replacement and active-link removal. |
| P10-02 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P10-05 | DONE → PARTIAL | 0 | 1: vendor/flint/common/build-info.cpp.in | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P10-06 | PARTIAL → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P10-07 | DONE → PARTIAL | 0 | 843: vendor/flint/common/arg.cpp, vendor/flint/common/arg.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P10-08 | DONE → PARTIAL | 0 | 36: runtime/core/include/bmoe/config.h, runtime/core/include/bmoe/decode_trace.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P10-09 | DONE → PARTIAL | 0 | 28: benchmarks/exp021_fused_bench.c, benchmarks/exp021_iq2s_grid.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P10-10 | DONE → PARTIAL | 0 | 19: vendor/flint/ggml/src/ggml-cpu/arch/arm/cpu-feats.cpp, vendor/flint/ggml/src/ggml-cpu/arch/arm/quants.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P10-11 | DONE → PARTIAL | 0 | 843: vendor/flint/common/arg.cpp, vendor/flint/common/arg.h | Source leads found. Production dispatch, fallback and removal remain unverified. |
| P10-12 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-01 | PARTIAL → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-02 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-03 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-04 | PARTIAL → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-05 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-06 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-07 | TODO → TODO | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| P11-08 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| C1-01 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| C1-02 | PARTIAL → PARTIAL | 3: src/llama-arch.cpp, src/llama-graph.cpp | 4: runtime/reconstructed/src/llama-graph.cpp, vendor/flint/src/llama-arch.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| C1-03 | DONE → PARTIAL | 0 | 1: vendor/flint/ggml/src/ggml-cpu/arch/x86/quants.c | Source leads found. Production dispatch, fallback and removal remain unverified. |
| C1-04 | TODO → TODO | 13: src/llama-kv-cache-dsa-iswa.cpp, src/llama-kv-cache-dsa.cpp | 18: runtime/reconstructed/src/llama-kv-cache.cpp, runtime/reconstructed/src/llama-memory-hybrid.cpp | Compiled C++ found. Prove reach, replacement and active-link removal. |
| C1-05 | TODO → TODO | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| C1-06 | TODO → TODO | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| C2-04 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| F1-01 | TODO → TODO | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| F1-02 | TODO → TODO | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| F1-03 | DONE → PARTIAL | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |
| F1-04 | TODO → TODO | 0 | 0 | Direct trace or artifact evidence required. The locator does not prove absence. |

## Next subsystem

Finish integrated KV as one subsystem. [Status](STATUS.md) tracks evidence for each verified candidate. Qualify allocation, cell and sequence ownership, transaction rollback, graph inputs and state I/O. Validate multi-sequence, hybrid and speculative paths before production promotion. Each accepted block records the production switch, removed routes, safety checks, representative supported models, paired Apex measurements and C++ file, function and route deltas.
