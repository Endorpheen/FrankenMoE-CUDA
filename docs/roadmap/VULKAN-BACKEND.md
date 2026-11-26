# Vulkan backend for FrankenMoE

**Status: experimental Vulkan multi-model distribution.** The Vulkan
runtime accepts the documented Apex, Inkling, Bonsai and Qwen2.5 GGUF
variants. Short generation passes on Linux RTX 4070 and for Windows
executables under Wine. Native AMD testing remains open. The existing
CUDA inference path remains separate.
[Vulkan multi-model alpha](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.14.1-vulkan-multimodel-alpha)

## Verified bring-up

The probe builds as Linux ELF and Windows PE x64. It dynamically loads
Vulkan and selects a Vulkan 1.3 compute device with timeline semaphores and
synchronization2. Automatic probe selection requires an AMD integrated or
discrete GPU. The explicit device-index path can select another supported GPU;
CPU devices also require `--allow-cpu`.

The self-test copies 65540 bytes from host-visible staging through a
device-local buffer and back. The test uses a compute queue, scoped copy
barriers, a timeline semaphore and explicit cache maintenance for
noncoherent memory. The build has no direct Vulkan or CUDA link dependency.

| Environment | Build | Device and transfer | Model inference |
| --- | --- | --- | --- |
| Linux x64, Mesa Lavapipe 26.0.8 | PASS | PASS | NOT TESTED |
| Linux x64, RTX 4070, NVIDIA 580.178.04 | PASS | PASS | NOT TESTED |
| Windows x64 probe, Wine with Linux Lavapipe | PASS | PASS | NOT TESTED |
| Native Linux AMD RADV and Windows AMD ICD | Build targets available | NOT TESTED | NOT TESTED |

The Linux fixture also checks the absent ICD, invalid device index, CLI
arguments and software-device exclusion from default selection.

```bash
bash tools/build-goose-vulkan.sh build/vulkan-bringup/linux linux
bash tools/build-goose-vulkan.sh build/vulkan-bringup/windows windows
bash integration/goose-standalone/tests/vulkan/device.sh \
    build/vulkan-bringup/linux/frankenmoe-vulkan-probe \
    /usr/share/vulkan/icd.d/lvp_icd.json
```

### Compute graph fixture

The host records all three dispatches before a single timeline submission.
The shaders use FP32, 32-bit storage words, and shared workgroup reduction,
so no optional narrow-storage or subgroup features are enabled. Descriptor
views validate bounds, alignment, usage and read/write overlap. The command
pool, descriptor pool and timeline are reused across two graph epochs.
Storage in this fixture is host-visible. A resident weight allocator and
device-local graph transfers still need to be integrated into inference.
[Khronos synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html)

The build embeds SPIR-V after validation for Vulkan 1.2. The tested tools
were glslang 16.2.0 and SPIRV-Tools 2026.1. Only the build uses them.
Test cases cover width 96 with 67 output rows and three input tokens,
and width 32 with one output row and one token. The fixture rejects a
short Q4 input view and an aliased RMSNorm write before recording.

| Environment | Compute graph | Maximum absolute error vs FP64 CPU |
| --- | --- | ---: |
| Linux x64, Mesa Lavapipe 26.0.8 | PASS | 3.11e-6 |
| Linux x64, RTX 4070, NVIDIA 580.178.04 | PASS | 1.35e-6 |
| Windows x64 probe, Wine with Linux Lavapipe | PASS | 3.11e-6 |
| Native Linux AMD RADV and Windows AMD ICD | NOT TESTED | — |

The attention fixture adds NeoX RoPE with theta 1,000,000, FP32
K/V writes at an explicit position and causal GQA with 14 query heads,
two K/V heads and head width 64. Against a two-pass FP64 CPU reference,
the maximum error is 2.22e-7 on Lavapipe and 2.01e-7 on RTX 4070.
The five-token prefill and 2+1+2 submissions match bitwise on each of
Linux Lavapipe, RTX 4070 Vulkan and Windows EXE under Wine with Lavapipe.
The fixture checks the K/V write boundary and rewinds to one visible
token while leaving old rows resident. It rejects invalid head geometry.
The fixture uses FP32 cache storage and one workgroup per query head.

The projection fixture chains a Q4_0 gather, a Q8_0 projection recorded
as two aligned weight stripes, a broadcast bias and an elementwise
residual. All results match independent CPU arithmetic exactly on the
three tested configurations above. It covers negative Q8 bytes, an
out-of-range token ID, a partial final stripe and an invalid output
offset. The stripe parameters allow the model host to keep each binding
and dispatch within device limits while writing one contiguous logits
array. [GGML packed block layouts](https://github.com/ggml-org/llama.cpp/blob/master/ggml/src/ggml-common.h)

```bash
# Point GLSLANG_VALIDATOR and SPIRV_VAL to the tools if they are outside PATH.
bash tools/build-goose-vulkan.sh build/vulkan-bringup/linux linux compute
bash tools/build-goose-vulkan.sh build/vulkan-bringup/windows windows compute
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json \
    build/vulkan-bringup/linux/frankenmoe-vulkan-compute-test 0 1
```

### Real Qwen2.5 GGUF fixture

The `qwen2` build target links the existing Goose GGUF parser and the
Qwen2 layer ordering in `integration/goose-standalone/vulkan/qwen2-step.goose`.
The C adapter verifies all 291 required tensor descriptors. It uploads
weights into a device-local arena in 4 MiB chunks and keeps the model
workspace and FP32 K/V device-local. The token input and final logits
are host-visible. The initial reference processes one token per submission
and exposes a raw token ID CLI. It splits the Q8_0 output into five
dispatches. Both binaries dynamically load Vulkan with no CUDA linkage.

The pinned GGUF SHA-256 verified locally. An independent llama.cpp CPU
reference used the same file, FP32 K/V, Flash Attention off, and greedy
decoding. Its server executable SHA-256 was
`200682b17f13b293c2bd6c8c9df0692d022891168c09cc87c48c489f585c174b`.
The full graph matched all four output IDs for the prompt
`The capital of France is` on native Linux RTX 4070, native Linux
Lavapipe, and the Windows x64 binary through Wine with Lavapipe.
NVIDIA and Lavapipe agreed on the top ten first-step logits, with a
maximum absolute difference of 2.53e-5 across the 151936 logits.
The short fixture does not establish model speed, general numerical
parity with llama.cpp or native AMD support.

```bash
# Point GOOSE_BIN, GLSLANG_VALIDATOR and SPIRV_VAL to the tools outside PATH.
bash tools/build-goose-vulkan.sh build/vulkan-bringup/linux linux qwen2
bash tools/build-goose-vulkan.sh build/vulkan-bringup/windows windows qwen2
bash integration/goose-standalone/tests/vulkan/qwen2-smoke.sh \
    build/vulkan-bringup/linux/frankenmoe-vulkan "$QWEN_GGUF" --device-index 0
```

### Text CLI and tokenizer

The Vulkan unit links the existing Goose BPE and detokenizer. The build
specializes the existing pre-tokenizer for Qwen2, which excludes combining
marks from letter runs. It leaves the CUDA units and source unchanged.
Token and merge tables, token attributes and end-of-generation IDs come
from the GGUF metadata. Text mode stops at an end-of-generation token
and writes UTF-8 to standard output. The raw-ID path preserves its
original output contract. Text mode runs greedy continuation without
applying a chat template or sampling configuration.

`--prompt-file` reads UTF-8 bytes directly on both platforms. This avoids
Windows command-line code page conversion. `--special` opts into parsing
literal control-token spellings. `--tokenize-only` opens no Vulkan device
and prints only token IDs.
[Qwen tokenizer configuration](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct/raw/main/tokenizer_config.json)

```bash
build/vulkan-bringup/linux/frankenmoe-vulkan "$QWEN_GGUF" --n-predict 4 \
    --prompt 'The capital of France is'
python3 integration/goose-standalone/tests/vulkan/tokenizer.py \
    build/vulkan-bringup/linux/frankenmoe-vulkan "$QWEN_GGUF"
```

The six stored fixtures match the llama.cpp CPU tokenizer, covering
ASCII, Cyrillic, CJK, emoji, combining marks, whitespace and split or
literal special tokens. Direct text continuation on RTX 4070 and Windows
x64 under Wine with Lavapipe matches the CPU continuation byte-for-byte
after adding the CLI's final newline.

### Local HTTP completions

The Vulkan unit reuses the standalone Goose HTTP framing and JSON parser.
The native adapter keeps one resident model and handles requests serially.
Each generation starts at position zero and writes its own visible K/V
prefix. The listener binds to `127.0.0.1` only. It supports
`GET /health`, `GET /v1/models`, `POST /tokenize`, `POST /completion`,
`POST /v1/completions` and `POST /v1/chat/completions`. Completion accepts a prompt string or input
IDs and an output-token limit. The current mode is greedy and returns
prompt/completion/total usage. Chat supports string messages with
system, user and assistant roles. The built-in Qwen2 ChatML text template
inserts control token IDs separately from message content. Both OpenAI-style
routes support SSE, optional terminal usage and `[DONE]`. No tool calls,
multimodal content or non-greedy sampler options are supported.

```bash
build/vulkan-bringup/linux/frankenmoe-vulkan "$QWEN_GGUF" --serve 29188 \
    --ctx-size 96 --max-requests 15
python3 integration/goose-standalone/tests/vulkan/http-smoke.py 29188
```

The fifteen-request fixture passed on Linux RTX 4070 and a Windows x64
build under Wine with Lavapipe, using explicit device selection. Text
completions match all four stored CPU reference IDs and usage. Chat using
the default system message matches the CPU reference answer `Paris`
and its 41 prompt tokens. SSE returns the same text and usage as non-streamed
generation. A subsequent completion matches after a client disconnects
mid-stream. The fixture checks malformed JSON, context limits and rejected
requests. The server exits after the optional request limit.
[Qwen ChatML template](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct/raw/main/tokenizer_config.json)

## Source audit and integration boundary

The shipping source audit uses `origin/main` at `48480ea22`. Paths marked
“main” below refer to that revision. The CUDA experiment line is
`codex/hybrid-exp1768-slru` at `1c562fc43`. The two trees have different
integration maturity. Recheck the exact branch tip and accepted binary
manifests before implementation.

| Area | Existing implementation and constraints | Vulkan reuse and boundary |
| --- | --- | --- |
| CLI and HTTP | Main: `integration/goose-standalone/cli/{infer-main,serve-main,tokenize-main}.c`, `goose/server/inference-http.goose`, `integration/goose-standalone/goose-host-llm.{c,h}`. The host maps weights, exposes `hose_tokenize`, `hose_step`, `hose_reset_state` and generation calls. Its current load/step path calls Qwen35-specific preparation and graph code. | Preserve endpoints, token and generation contracts, and the CUDA host. Add an isolated model host for the minimum Vulkan path that provides the same interface. Select the backend once before allocating model state. |
| Model loading and tokenization | Main: `goose/model/gguf-{reader,parser,metadata,tensors,offsets}.goose`, `bpe-{load,tokenize,detokenize}.goose`, `arch-registry.goose`, `qwen35-model-loader.goose`, `integration/goose-standalone/tensor-data.{c,h}`. Model identification and tensor validation precede backend weight placement. An architecture descriptor alone does not imply shipping support. | Share GGUF parsing, tensor layout and tokenizer contracts. Compile the required Goose source into the Vulkan unit without CUDA-linked objects. Validate all tensor types, size/offset arithmetic and tokenizer metadata before committing VRAM. |
| Graph | Main: `integration/goose-standalone/graph-execute.{c,h}` and `graph-bindings.c`, `goose/model/goose-model-graph.goose`, `goose/model/qwen35-step.goose`. The reusable plan assumes fixed topology and bindings and publishes selected outputs. CUDA launch packets and graph replay are backend-specific. [EXP-1953](../../experiments/EXP-2026-11-25-1953-apex-inkling-single-executable/README.md) is evidence for architecture selection in a common host, with custom node and expert-layout dependencies. | Reuse operation, shape, stride, alias and state semantics. Lower a validated subset into Vulkan resource bindings and dispatches. Keep tensor descriptors, command recording and opaque device handles separate. |
| CUDA and memory | Main: `integration/goose-standalone/cuda-driver.c` resolves Driver API functions for allocation, VMM, transfers, events and graphs. Sources include `cuda-device.cl`, `cuda-mmq.cl`, `cuda-mmvq.cl`, `cuda-dense-mma.cl`. `tools/build-goose-inference.sh` explicitly compiles the CUDA driver and the model host. | Preserve these objects, images and launch commands for the NVIDIA path. Vulkan owns its device, queues, buffers, staging, allocation budgets and retirement independently. Do not reuse CUDA device pointers as resource handles. |
| Attention and KV | Main: `goose/model/qwen35-{attention,kv,device-state}.goose` and `integration/goose-standalone/cuda-flash-{vec,mma}.cl`. State includes positions, typed rows and cache lifetimes. Inkling additionally has full/sliding attention and relative-position behavior. | Implement causal/GQA and an explicit cache contract first. Preserve model-specific scales, masks, cache types and quantization thresholds when adding each architecture. |
| Recurrent state and GDN | Main: `goose/model/qwen35-gdn.goose`, `gdn-ops.goose`, `integration/goose-standalone/cuda-gdn-mma.cl`. [EXP-1974](../../experiments/EXP-2026-11-25-1974-bonsai-recurrent/README.md) supplies Bonsai-specific evidence. | Use an ordered Vulkan recurrence with explicit state ownership. Keep Bonsai-specific semantics scoped to Bonsai. Check chunk partitions, cache restore and reset against per-layer state fixtures. |
| MoE and expert transport | Main: `goose/model/qwen35-trunk.goose`, `goose/cuda/goose-moe-cache-admission.goose`, `integration/goose-standalone/slru-runtime-shim.c` and accelerator code in `cuda-driver.c`. Inkling's valid missing expert plane and its grouped IQ4 formats require their own layout contract. | Reuse routing and cache-policy logic only where inputs and ordering agree. Start with resident experts. Add Vulkan-native staging, copy lifetimes and explicit expert placement before supporting oversubscribed profiles. |
| Sampling | Main: `goose/model/qwen35-generation.goose`, `goose/sampling/goose-sampler-chain-live.goose`, and `goose/sampling/sampler-chain-full.goose`. The host also handles EOG and detokenization. | Keep the CPU sampler as the first implementation. Read back only requested logits. Use identical sampler state, seed, stops and context semantics across backends. |
| Bonsai | [EXP-1976](../../experiments/EXP-2026-11-25-1976-bonsai-trunk/README.md) composes the Goose model with a standalone Driver API host. [EXP-2000](../../experiments/EXP-2026-11-25-2000-bonsai-mmq-k64/README.md) is the accepted short-profile K64 checkpoint. | Reuse verified graph semantics and PQ2 layout fixtures. Treat normal HTTP integration as a prerequisite for advertising native Bonsai support on Vulkan. |

### Proposed backend contract

Use a small C ABI, with Goose owning model orchestration. Define an opaque
device, buffer and plan. A tensor view has a buffer, byte offset, range, type,
dimensions and byte strides. Define open/probe, allocate/free, transfer,
record/submit, poll/wait, and error/teardown operations. Use a submission value
to track when input, output and temporary buffers can be reused. An executable
plan declares operator coverage, buffers, uploads, writes, readbacks and
state dependencies. Keys include shapes, strides, precisions, pipeline
variants and binding generation. Keep prompt slots and recurrent state
isolated by session. Use normal status returns for unsupported graphs, OOM
and device loss. No C++ libraries or external inference engine.

Initially place this ABI underneath a separate Vulkan host and build target.
A later backend-neutral adapter may dispatch at the existing host entry
points, without rewriting the CUDA graph executor or kernels. Keep existing
CUDA binaries buildable directly. A suggested future `--backend auto`
front door preserves CUDA on eligible NVIDIA devices and selects Vulkan on
eligible AMD devices. For mixed systems, prefer the existing eligible
CUDA path by default and expose explicit backend and stable device
identifiers. `--backend vulkan` must report clearly if the selected model
or device is unsupported. Select before loading weights, print the
chosen backend and device, and honor an explicit choice without silent
fallback. Do not restart on another backend after request state has mutated.
Software ICDs are useful for API CI and must be excluded from automatic
GPU selection. Test missing loader, missing ICD and mixed/duplicate devices.

Load `libvulkan.so.1` on Linux and `vulkan-1.dll` on Windows dynamically.
Keep Vulkan headers and GLSL/SPIR-V tools build-time only. Package validated
SPIR-V artifacts with a versioned manifest, or embed them at build time.
The AMD distribution must start with no CUDA driver, toolkit, image or
transitive CUDA library dependency. Keep platform-specific loader and file
mapping code at the ABI edge. [Khronos loader guide](https://docs.vulkan.org/guide/latest/loader.html).

## AMD and SPIR-V capability policy

- **Required first tier.** Target Vulkan 1.3 for the first implementation.
  Query and enable timeline semaphore and synchronization2 features before
  use. Probe compute support, workgroup/grid limits, shared memory, storage
  buffer range/alignment, push constants, descriptor limits, and timestamp
  support. First kernels operate with 32-bit SSBO words, FP32 arithmetic
  and workgroup shared-memory reductions. Compile GLSL offline using a
  pinned glslang version and validate with SPIRV-Tools for the exact target.
  Build separate modules for optional capabilities. [Khronos compute guide](https://docs.vulkan.org/guide/latest/compute_shaders.html).
- **Subgroups.** Query supported stages and operations. Implement shared
  memory fallback, then test subgroup shuffle, ballot and reductions with
  partial groups. Use required subgroup size only when supported for compute
  and within device limits. Never assume CUDA warp width or apply an
  unordered subgroup reduction where a fixed numerical order is required.
  Profile wave32 and wave64 where available, recording LDS, VGPR, occupancy,
  bandwidth and pipeline variant. [Khronos subgroup guide](https://docs.vulkan.org/guide/latest/subgroups.html), [AMD RDNA guide](https://gpuopen.com/learn/rdna-performance-guide/).
- **Quantization and matrices.** Start with exact bit unpack and FP32 scaling.
  Gate FP16 arithmetic and 8/16-bit storage independently. INT8 dot products
  require the feature and the appropriate reported signed/unsigned packed
  acceleration properties. KHR cooperative matrices require querying scope,
  tile dimensions, A/B/C/result types and feature enablement. Keep the
  scalar/tiled fallback for every shape/tail and unqualified driver. RDNA
  branding or ISA instructions alone do not establish Vulkan support.
  [KHR cooperative matrix](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_cooperative_matrix.html),
  [integer-dot properties](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceShaderIntegerDotProductProperties.html).
- **Precision.** Use matching reference operation order and contraction
  policy in reference kernels, with explicit SPIR-V no-contraction and
  supported rounding/denormal controls where needed. Device capability does
  not guarantee cross-vendor bit identity for transcendental operations.
  Compare logits and selected IDs on a shared history, then compare
  independent long generations and score difficult prompts against verified
  answers before enabling any arithmetic-changing INT8/FP16 path. Establish
  thresholds before a candidate test. Keep deterministic replay tests and
  identify the first divergent operation or state write. [Float controls](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceFloatControlsProperties.html).
- **Synchronization.** Start with one compute-capable queue. Use scoped
  buffer dependencies between transfer/compute, compute/compute, compute/host
  and compute/indirect reads. Use timeline values for completion and
  resource retirement. Add a transfer queue only after profiling shows
  useful overlap, with explicit queue-family ownership if required.
  Treat indirect dispatch as a way to consume GPU-written parameters for
  recorded commands, not as dynamic shader-side graph construction.
  [Khronos synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html).
- **Hardware matrix.** Validate Linux with AMD RADV and Windows with the
  AMD Vulkan ICD on actual AMD hardware. Record PCI device ID, GPU and
  memory, OS and driver revision, UUID, enabled features and pipeline cache
  UUID. Test a device without cooperative matrices and a device exposing
  each optimized feature advertised. Use Vulkan validation and
  synchronization validation in diagnostics, and measure release builds
  separately. [Mesa RADV](https://docs.mesa3d.org/drivers/radv.html).

## Implementation sequence

Every proposed path below is new unless identified as an existing integration
point. Keep all checkpoints open until their specified tests have run.

| Stage | Proposed files or integration point | Dependencies and risks | Readiness and tests |
| --- | --- | --- | --- |
| 0. Freeze references | `docs/roadmap/`; accepted CUDA manifests, EXP-1880 Apex, [EXP-1972](../../experiments/EXP-2026-11-25-1972-inkling-checkpoint-before-bonsai/README.md) Inkling, current Bonsai | Same workload, model, context, precision, cache and startup/warm regime. HEAD and released binaries can differ. | Record artifact hashes, compiler, argv/env, exact input IDs and expected output. Save 32K/512 CUDA metrics wherever a complete path exists, including peak VRAM/RAM and swap. |
| 1. Initialization | `integration/goose-standalone/vulkan/{loader,device}.c`, `tools/build-goose-vulkan.sh`, `integration/goose-standalone/tests/vulkan/device.c` | Vulkan headers, loader with real ICD, C ABI. Risks include function pointer calling convention, missing features and partial initialization cleanup. | Probe and print capability report on Linux and Windows. Test invalid device, absent loader or ICD, explicit selection, cleanup after staged failure, and operating AMD with no CUDA libraries. |
| 2. Memory | `vulkan/{memory,queue}.c`, `tests/vulkan/buffers.c` in the same standalone integration | Device-local suballocated arenas, host-visible mapped staging ring, bounded scratch, timeline lifetime tracking. Use reported per-heap budget if available. Handle 64-bit weights, range limits, alignment, non-coherent flush/invalidate at atom boundaries, UMA vs discrete and OOM rollback. | Transfer round-trip with byte patterns, non-aligned and large offsets, repeated ring wrap, alias and destruction tests, stress with several in-flight epochs, inject OOM and failed submissions. [Memory budget extension](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_memory_budget.html). |
| 3. Base kernels and plans | `goose/vulkan/dispatch.goose`, `vulkan/{graph,pipeline}.c`, `vulkan/shaders/{copy,norm,activation,rope,gemm}.comp` | Reuse Goose tensor semantics, SPIR-V manifest and descriptor ABI. Risks include strided views, padded rows, descriptor limits, differing FP contraction and incorrect barriers. | Kernel oracle tests for odd dimensions and strides, NaNs and precision, matrix tails and reused scratch. Run multi-op graphs with sync validation and verify one selected readback. |
| 4. Quantization | `vulkan/shaders/{get_rows_q4_0,matvec_q4_0,mmq_q4_0}.comp`; existing GGUF type/layout readers | Preserve exact Q4_0 nibble/scale semantics. Inventory actual mixed tensor types before admitting the GGUF. Handle F32/F16 auxiliary weights and tied output. | Compare unpacked blocks and matmul outputs to CPU fixtures, malformed blocks and boundary cases. Confirm registered unsupported types fail before allocation. |
| 5. Attention and state | `vulkan/shaders/{kv_write,attention_reference}.comp`, `goose/vulkan/kv.goose`; later `gdn_reference.comp` and `moe_route.comp` | First implement causal GQA and simple F16 KV storage via packed words if typed storage is unavailable. Bring in Q4_0 KV, GDN and MoE only for architectures that require them. | Check full prefill vs stepped prefill, decode with padding and long positions, cache reset/rewind, GQA mapping, chunk boundaries and two isolated slots. For later architectures verify recurrent snapshots, top-k ties and expert indices, and grouped output against a scalar reference. |
| 6. Full inference | `vulkan/model-host.c`, `goose/vulkan/qwen2-dense.goose`; use existing HTTP and CLI contracts with a separate linked model host | Pin the GGUF, tokenizer and chat template. Account for host logits/sampling and the non-CUDA release link. Only model-specific admitted operators are available initially. | Pass tokenize/detokenize, greedy IDs and logit tolerances, EOG/stop, chat and completion streaming, usage, cancellation, clear errors, and repeated long sessions. Establish prefill/decode, client TTFT, full wall time and memory baseline on both platforms. |
| 7. Optimize and expand | `vulkan/shaders/`, `goose/vulkan/`, experiment registry and `.github/workflows/ci.yml` | Profile fixed workload first. Check command recording, barriers, exposed transfers, LDS and register pressure, decode weight bandwidth, then subgroup and matrix variants. A distinct pipeline can change numerical behavior. | Freeze the exact baseline, compare A–B–B–A, check full-request and kernel times separately and reprofile the critical path. Re-run affected architecture gates, test AMD-only packaging and keep CUDA release/CI coverage. |

## Multi-model bring-up

The scope includes all CUDA release models as additional Vulkan targets.
The local GGUF inventory confirms distinct requirements.

| Model | Packed weight types in the local GGUF | Vulkan status |
| --- | --- | --- |
| Qwen2.5-0.5B Q4_0 | Q4_0, Q8_0 | Experimental CLI and HTTP release |
| Bonsai 2.5 27B | PQ2_0 trunk, Q4_0 KV | Experimental raw-ID and text CLI, serial HTTP completions, SSE and no-thinking ChatML. Short generation passes on Linux RTX 4070 and Windows EXE under Wine with RTX 4070. |
| Apex 35B | Q4_K/Q6_K weights, Q4_0 KV | Experimental complete 40-layer trunk, raw-ID and text CLI, serial HTTP. Short generation passes on Linux RTX 4070 and Windows EXE under Wine with RTX 4070. |
| Inkling-2-Mini | Q8_0, IQ4_NL, IQ4_XS, Q4_0 KV | Experimental complete 48-visit model path, raw-ID and text CLI, serial HTTP completions and text chat. Short generation passes on Linux RTX 4070 and Windows EXE under Wine with RTX 4070. |

### Bonsai FFN component

The Vulkan graph records forward signed FWHT, both PQ2_0 projections,
SwiGLU, the second FWHT and the down projection. The fixture extracts
the three complete layer-0 matrices and sign vectors from the local
Bonsai Q4_0/PQ2_0 MTP GGUF. The weights are uploaded to device-local
memory with the existing staging path. The input is deterministic
and synthetic. The GPU chain agrees with the independent CPU
calculation at relative L2 below 1.37e-7 on RTX 4070, Linux Lavapipe,
and Windows x64 under Wine with Lavapipe. The forward and grouped
transform match bitwise. The inverse recovers the input exactly as
floats, allowing positive and negative zero to compare equal.
The offset PQ2 gather matches unpacking of the selected real weight row.
The rebuilt Qwen2.5 Vulkan CLI matches the four saved CPU reference IDs
on RTX 4070.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/bonsai-first
mkdir -p "$out"
python3 "$tests/bonsai-fixture.py" "$BONSAI_GGUF" "$out/ffn-layer0.bin"
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 "$out/ffn-layer0.bin"
```

Fixture SHA-256 `09f64a1611b8775025d4335875976ff4f8d14ec83070787c74bb9bb0e4a0599a`.
This is a layer fixture, not a model throughput or generation result.
[PrismML GGUF artifacts](https://huggingface.co/prism-ml/Ternary-Bonsai-2.5-27B-GGUF)

### Bonsai recurrent component

The Vulkan graph records four-tap causal convolution with SiLU,
Q/K L2 normalization and a one-token GDN update. History is kept in
three in-place rows and the GDN state is kept in head/value/key order.
The shaders use FP32 and workgroup reductions without subgroup width
requirements. State and history writes participate in the same graph
dependencies as activations. Independent GDN rows own disjoint state
cells. The C caller rejects overlapping input and state views.

The deterministic synthetic fixture uses Bonsai's 48 value heads,
16 key heads and head width 128 for three successive inputs. Linux
Lavapipe, Linux RTX 4070, and Windows x64 through Wine with Lavapipe
all pass. The largest errors against FP64 CPU calculations were
5.21e-9 for state and 7.75e-10 for output. Convolution history appears
exactly in the expected order, and resetting both state buffers
reproduces the final state and output bytes on each device.

[Gated DeltaNet reference](https://arxiv.org/abs/2412.06464)

### Complete real Bonsai recurrent layer

The reusable Vulkan layer recorder now adds the F32 decay/beta
projections, gated RMS normalization, grouped transform, output
projection, both residuals and FFN. It records 20 dispatches per
token and keeps weights and temporary activations device-local.
The fixture initializes the convolution history and GDN state once,
then compares output, history and state after each of four steps.

The test uses the real layer-0 tensors and CPU-unpacked embeddings
of tokens 1511, 17, 248046 and 1511. Independent NumPy output is
checked with `atol=2e-5, rtol=5e-4`. Linux RTX 4070, Linux Lavapipe,
and Windows x64 under Wine with Lavapipe pass. Maximum absolute
errors are 5.73e-6 for layer output, 8.40e-5 for convolution history
and 2.82e-5 for GDN state. The history and state match the combined
absolute and relative acceptance gate.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/bonsai-full-layer
mkdir -p "$out"
OPENBLAS_NUM_THREADS=1 python3 "$tests/bonsai-layer-fixture.py" "$BONSAI_GGUF" "$out/layer0.bin"
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 \
    build/vulkan-bringup/bonsai-first/ffn-layer0.bin "$out/layer0.bin"
```

Fixture SHA-256 `8b6243f89bc325fc141b74d9a2ba06dcf93074d696e6f125de51b2271be4b34f`.
No throughput or peak model memory is inferred from the layer fixture.

### Complete real Bonsai attention layer

The Vulkan recorder prepares Q and the sigmoid output gate from one
interleaved projection. It uses 24 query heads and four KV heads,
each 256 wide, and rotates the first 64 coordinates with base 1e7.
Both K and V are Q4_0. The packer writes pairs of 18-byte blocks as
non-overlapping 32-bit words. Its read side uses the same nibble
ordering, FP16 scale and 6:1 grouped causal Attention. The layer
finishes with the same residual and FFN tail as the recurrent path.

The layer-3 fixture uses four successive real embeddings on Linux
Lavapipe, Linux RTX 4070 and Windows x64 under Wine with Lavapipe.
On the same FP32 K/V input bytes, Vulkan and the portable CPU packer
write identical Q4 rows. Earlier and unused rows also match byte for
byte. Independent NumPy and Vulkan projection arithmetic produces
three different cache bytes of 4608 checked on each configuration.
The maximum output error in that independent run is 5.86e-4. The
original layer gate can fail on the independent run. Replaying with
identical cache bytes retains the original `atol=2e-5, rtol=5e-4`
gate and passes on all three platforms, with maximum output error
4.01e-5. This isolates the observed discrepancy to data entering
Attention from the quantized cache. Whole-model numerical and
generation checks remain open.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/bonsai-attention-layer
mkdir -p "$out"
OPENBLAS_NUM_THREADS=1 python3 "$tests/bonsai-layer-fixture.py" "$BONSAI_GGUF" "$out/layer3.bin" --attention
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 \
    build/vulkan-bringup/bonsai-first/ffn-layer0.bin \
    build/vulkan-bringup/bonsai-full-layer/layer0.bin "$out/layer3.bin"
```

Fixture SHA-256 `454994ab8ec7f8f55e6f6fc9ecc74016fd39e735c51c8433145d633bc7a9e229`.
The rebuilt Qwen2.5 CLI still matches the four saved IDs on RTX 4070.

### Complete Bonsai raw-ID Vulkan inference

The Vulkan unit reuses the Goose Bonsai tensor binder and GGUF reader.
The loader maps all 64 trunk layers, keeps weights device-local and
holds recurrence state and Q4 KV in a device-local arena. One weight
allocation per layer keeps individual allocations bounded. Embedding
and output weights use aligned storage views, and logits are written
in stripes below the core workgroup and descriptor limits. The same
Goose unit orders the layer execution. The raw-ID CLI reads GGUF EOG
metadata and emits the terminating ID before stopping, as the CUDA
reference does. The MTP tensors are validated with the original layout
but are not uploaded or used.

The local GGUF SHA-256 is
`ededc9fc0a8a6a8dd30914d107b2d2ce7674d789f1d05a1a481f240609aa6c23`.
The pinned EXP-2105 CUDA HTTP reference, Q4_0 KV, context 32,
batch/ubatch 64, greedy, MTP off, yields the two frozen outputs under
`integration/goose-standalone/tests/vulkan/bonsai-model.json`.
On Linux RTX 4070, Vulkan matches both, stopping at `17, 248046` on
the first fixture and emitting `315, 9625, 374, 315` on the second.
Vulkan uses the shortest capacity for each CLI request, seven and
eight positions. Windows x64 under Wine with NVIDIA's Vulkan ICD
matches the EOG fixture on RTX 4070. The Vulkan Qwen2.5 control still
matches four out of four reference IDs on RTX 4070.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/bonsai-model-first
bash tools/build-goose-vulkan.sh "$out/linux" linux qwen2
bash tools/build-goose-vulkan.sh "$out/windows" windows qwen2
python3 "$tests/bonsai-model-smoke.py" "$out/linux/frankenmoe-vulkan" \
    "$BONSAI_GGUF" --device-index 0
```

Tested executable SHA-256 values:

- Linux `9cefe4f8d805e86860595c473009f56c16720de6784e3c043a21dabba720eec4`.
- Windows `a4f7b35b1778e2e92eeaf5168726f61d00e0d0a292ab99c853cdce56e181d668`.

### Bonsai text and HTTP inference

The Vulkan unit uses the GGUF's Qwen3.5 pretokenizer and shared Goose BPE.
Plain text, raw token IDs, streaming completions and text-only ChatML
with `enable_thinking: false` are available in the serial HTTP server.
Each request clears recurrent and KV state on the device. Generation
uses greedy selection. Tool calls, multimodal inputs, structured reasoning
and MTP remain outside this path.

On Linux RTX 4070, the nine-request smoke test passed. The two
tokenization cases match llama.cpp using vocabulary metadata from the
pinned Bonsai GGUF. The plain and chat generation cases match the visible
text and generated IDs from EXP-2105 CUDA with greedy selection, Q4_0 KV,
context 96, batch/ubatch 64 and MTP off. For plain completion the Vulkan
HTTP API omits the terminating EOG from returned IDs. EOG stop, SSE usage
and replay after intervening requests passed. The Windows x64 text CLI
under Wine with NVIDIA's Vulkan ICD emitted the same plain completion.
The Qwen2.5 tokenizer and HTTP suites passed on Linux.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/bonsai-http
bash tools/build-goose-vulkan.sh "$out/linux" linux qwen2
"$out/linux/frankenmoe-vulkan" "$BONSAI_GGUF" --bonsai --device-index 0 \
    --serve 29188 --ctx-size 96 --max-requests 9
# Run from a second shell.
python3 "$tests/bonsai-http-smoke.py" 29188
```

Tested executable SHA-256 values:

- Linux `659df2058149392fb0b325333901718144f2500968216f8a9ee3b80aadf0b5c7`.
- Windows `e559ac3e3756a0d9414d72f845c8eb52c51d9e13b415a781947a7bf5da213e9c`.

Long-sequence numerical checks, peak memory, throughput, native Windows
and AMD hardware qualification remain open.

### Apex and Inkling quantized weight operations

The Vulkan graph accepts Q4_K, Q6_K, IQ4_NL and IQ4_XS projection
stripes and Q4_K and Q8_0 embedding gathers. The shaders read packed
weights through 32-bit storage buffers and accumulate projections in
FP32. They do not require shader integer-dot-product or subgroups.

The component fixture uses seven real 512-element rows for each
format. It covers Apex token embeddings and the first expert down
projection, plus Inkling token embeddings and the first available
IQ4_NL down and IQ4_XS gate/up expert tensors. The CPU oracle is
llama.cpp `gguf-py` at `b8635075ffe27b135c49afb9a8b5c434bd42c502`
with FP64 dot-product accumulation. Linux Lavapipe, Linux RTX 4070 and
Windows x64 under Wine with Lavapipe each passed. The two gathers match
every output element exactly. The maximum absolute projection errors
for Q4_K, Q6_K, IQ4_NL, IQ4_XS and Q8_0 respectively are
1.49e-8, 2.98e-8, 1.19e-7, 1.19e-7 and 1.91e-6.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/moe-quant
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
PYTHONPATH="$LLAMA_CPP/gguf-py" OPENBLAS_NUM_THREADS=1 \
    python3 "$tests/moe-quant-fixture.py" "$APEX_GGUF" "$INKLING_GGUF" "$out/real-weights.bin"
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 --moe-quant "$out/real-weights.bin"
```

Fixture SHA-256 `b87e72da921c79ce799b2edaf9f9d7d5d3d0d73aa74e874db1b7a915606a1c03`.
These operations are ready for model-graph integration.

### Apex expert selection

The Vulkan graph can project the FP32 router and produce a compact
selection per token containing the expert IDs and their renormalized
softmax weights. Ties select lower IDs first. Non-finite logits mark
the entire route invalid with sentinel IDs and zero weights. The route
is suitable for reading the chosen experts on the host before staging
weights from an oversubscribed bank.

The first-layer fixture uses the real 256-by-2048 Apex router, two
real token embeddings, an equal-logits row and a non-finite row.
Independent FP64 CPU calculations provide the expectation. All
selected IDs matched and the maximum absolute weight error was
1.49e-8 on Linux RTX 4070, Linux Lavapipe and Windows x64 under Wine
with Lavapipe. The scale of 0.75 exercises the non-default weight
scaling path. This is a router component test.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/moe-router
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
PYTHONPATH="$LLAMA_CPP/gguf-py" OPENBLAS_NUM_THREADS=1 \
    python3 "$tests/moe-router-fixture.py" "$APEX_GGUF" "$out/router.bin"
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 --moe-quant \
    build/vulkan-bringup/moe-quant/real-weights.bin "$out/router.bin"
```

Fixture SHA-256 `5186da54c2968afd838722425b5cde8d08572ecf80b31fcc7081b79209622059`.

### Apex routed expert execution

The split expert path loads only the routed gate, up and down slices
from the GGUF. It bounds-checks the offsets and route IDs, uploads one
packed request to a reusable device buffer and executes the Q4_K gate
and up projections, SwiGLU, the Q6_K down projection and the ordered
weighted sum. The eight slots in the first-layer fixture hold
16,318,464 bytes of weights before any device-specific padding.

The fixture starts with a real token embedding and the layer's real
post-attention RMS weights. It checks all eight IDs, rejects an
out-of-range ID before upload, and compares all 2048 output floats
with an independent CPU calculation. The maximum absolute error is
9.32e-9 across Linux RTX 4070, Linux Lavapipe and Windows x64 under
Wine with Lavapipe. This validates the routed branch of one layer.
The shared expert, layer interconnect and full-model generation
remain open. The initial transfer path is synchronous.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/apex-experts
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
PYTHONPATH="$LLAMA_CPP/gguf-py" OPENBLAS_NUM_THREADS=1 \
    python3 "$tests/apex-experts-fixture.py" "$APEX_GGUF" "$out/experts.bin"
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 --apex-experts "$APEX_GGUF" "$out/experts.bin"
```

Fixture SHA-256 `6811e231e40508ccce7adeef291982f4df74dea49207525019a659467ee32563`.

### Complete Apex Vulkan trunk

The `--apex` path supports the pinned Q4_K_M tensor layout, all
40 trunk layers and greedy generation with MTP off. GDN uses
separate quantized alpha and beta projections and the Apex
RMS-based Q/K normalization. Attention rotates each 256-wide Q/K
head and each 64-wide V block before Q4_0 KV packing and applies
the inverse V transform before the attention gate. Each layer
adds the gated shared expert and the eight staged routed experts
to the residual. Only selected expert weights are staged per
layer. A new HTTP request clears both recurrent and KV state.

The first experimental run omitted the Q/K/V Walsh–Hadamard
transforms and returned a comma after `Paris` where CUDA returned
a period. The Vulkan path now matches the two saved CUDA
completions on Linux RTX 4070 (six generated IDs, exact text)
and the first result survives replay after the second request.
Both the promoted CUDA profile and the diagnostic CUDA run with
MMVQ, MMQ, dense/GDN/Flash MMA and Flash Vec disabled produce
the same fixture. The control uses context 96, batch/ubatch 64,
Q4_0 KV and MTP off. The reference executable SHA-256 is
`0ef188ef55f242ae548994ad793204fef6eb16b580aacb5c37093b5f27fda3ff`.

Windows x64 through Wine with NVIDIA's Vulkan ICD matches the two
generated IDs from the second request. The Linux Bonsai and Qwen2.5
HTTP regression fixtures pass on the same rebuilt executable.
These checks do not qualify native AMD, native Windows, long
sequences, memory or throughput.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/apex-model/rotated
bash tools/build-goose-vulkan.sh "$out/linux" linux qwen2
"$out/linux/frankenmoe-vulkan" "$APEX_GGUF" --apex --device-index 0 \
    --serve 29188 --ctx-size 96 --max-requests 5
# Run from a second shell.
python3 "$tests/apex-http-smoke.py" 29188
```

Tested executable SHA-256 values:

- Linux `0b70fee6b55b3271104b6689e574dd4060a01d1d9bc4430d4dd4e2d173a1b93d`.
- Windows `557d42b06bd466347b9d58d1b71c99399949a4326e74448d4a0290721695cc3f`.

### Inkling routed expert execution

The Inkling route ranks sigmoid probabilities with selection bias and
normalizes the selected pre-bias probabilities with model scale eight.
The portable Vulkan bank stages per-expert slices from the fused IQ4_XS
gate/up plane and the IQ4_NL down plane. SiTU uses the checkpoint's
gate cap four and up cap 25. The first-layer reference applies the real
layer RMS multiplier to a real token embedding and compares
all eight routed IDs and all 2304 output values against an independent
CPU calculation. Linux RTX 4070, Linux Lavapipe and Windows x64 under
Wine with Lavapipe all pass. The maximum absolute output error is
5.97e-6. The Apex routed fixture still passes on RTX 4070.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/inkling-experts
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
PYTHONPATH="$LLAMA_CPP/gguf-py" OPENBLAS_NUM_THREADS=1 \
    python3 "$tests/inkling-experts-fixture.py" "$INKLING_GGUF" "$out/experts.bin"
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 --inkling-experts "$INKLING_GGUF" "$out/experts.bin"
```

Fixture SHA-256 `cb65be95bbe1bd94f5213ee14edcd4c6cb654a602548936b7d54cbbe55b63b06`.

### Inkling Q4 relative and sliding attention

The Inkling-specific shader reads packed Q4_0 K/V through the
shared decoder and computes causal 24:8 grouped attention with
16-wide learned relative queries, learned denominator sinks,
and optional sliding cache positions. The learned relative
term vanishes at distances of 1024 or greater. The graph
validates the query, auxiliary table and packed cache extents.

The synthetic test writes the cache through the Vulkan Q4_0
packer and checks every output against FP64 CPU accumulation.
Full attention at seven keys, a sliding window after ring wrap,
and full attention at 1026 keys pass on Linux RTX 4070, Linux
Lavapipe and Windows x64 under Wine with Lavapipe. Maximum
absolute error is 1.63e-7 across the three configurations.

```bash
out=build/vulkan-bringup/inkling-attention
bash tools/build-goose-vulkan.sh "$out/linux" linux compute
"$out/linux/frankenmoe-vulkan-compute-test" 0 0 --inkling-attention
```

### Complete Inkling Vulkan model

The `--inkling` path uses the 24 physical layers and 48 visits
bound from `inkling2_loop` GGUF metadata. The reused Goose loop
and layer routines handle residual depth mixing, controllers,
clipping and short history. The Vulkan path executes the
projections, relative/sink attention, Q4_0 KV writes and selected
IQ4 experts. Each visit owns independent KV and short history.
Full attention uses the configured context and sliding layers
use a ring of at most 1024 positions. Each HTTP request resets
both types of state. GPT-4o pretokenization uses the tested local
Inkling scanner and direct-token lookup before BPE merges.
This path uses greedy generation and the unpadded vocabulary.

On Linux RTX 4070 the two five-token requests match the four
generated CUDA IDs and visible text exactly. Replaying the first
request after the second reproduces the first response. The
reference executable SHA-256 is
`739221fcb84cc52e91b817a465a598b5b82ce41da483c83f1a614e8b96299016`,
with Q4_0 KV, context 96 and batch/ubatch 64. The cold CUDA
reference request timed out at 40 seconds on the rotational
model drive. A sequential read populated the filesystem cache
for the paired correctness check. This was not a throughput
comparison. The Windows x64 text CLI under Wine with NVIDIA's
Vulkan ICD matches the first completion. These checks do not qualify
long sequences, memory, throughput, native Windows or AMD hardware.

```bash
tests=integration/goose-standalone/tests/vulkan
out=build/vulkan-bringup/inkling-model
bash tools/build-goose-vulkan.sh "$out/linux" linux qwen2
"$out/linux/frankenmoe-vulkan" "$INKLING_GGUF" --inkling --device-index 0 \
    --serve 29188 --ctx-size 96 --max-requests 5
# Run from a second shell.
python3 "$tests/inkling-http-smoke.py" 29188
```

Tested executable SHA-256 values:

- Linux `80855d8f37b3332f7ba05f07af7e051cc7a81c906b9c98e0d63f7175e2c2a743`.
- Windows `0cf0650b42a54d0fc4cea42361d77891fa81f473c1dedc0dc2d93d725cf04229`.

The Inkling text chat endpoint uses its own GGUF role and content
markers and sends the zero-effort instruction. A model can still
emit reasoning in that configuration. The HTTP chat outputs
separate `reasoning_content` from visible `content`, for JSON and
SSE. Raw completions preserve the model's original stream. A real
15-ID chat prompt on Linux RTX 4070 matches all five CUDA output
IDs. The same fixture verifies the JSON and SSE reasoning fields
and usage counters. Tool calls, multimodal content and prior
reasoning fields remain outside the text-chat contract.

## Model order

1. **Qwen2.5-0.5B-Instruct, Q4_0 GGUF** is the proposed first target. Choose
   The official Qwen artifact is pinned at revision
   `9217f5db79a29953eb74d5343926648285ec7e67`, SHA-256
   `7671c0c304e6ce5a7fc577bcb12aba01e2c155cc2efd29b2213c95b18edaf6ed`.
   A ranged metadata read found 291 tensors, consisting of 169 Q4_0,
   121 F32 and a Q8_0 output projection of shape [896, 151936].
   The embedding is a separate Q4_0 tensor. Each layer has F32 Q/K/V
   biases. The full weight file and one independent CPU raw-ID fixture have
   been verified. Linux RADV, Windows AMD ICD and the normal user-facing
   model entry points remain unqualified.
   It exercises GGUF, BPE, RMSNorm, RoPE, causal GQA, SwiGLU, quantized
   embedding/projections and sampling with modest VRAM. The existing
   `goose/model/qwen2.goose` and `base/{dense,trunk}.goose` are sources
   for graph contracts. Main's host audit does not establish a working
   standalone Qwen2 inference path.
   [Pinned model file](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/blob/9217f5db79a29953eb74d5343926648285ec7e67/qwen2.5-0.5b-instruct-q4_0.gguf)
2. **Bonsai 2.5 27B.** Add PQ2_0 unpacking and signed Hadamard, GDN with
   model-specific recurrence, Q4_0 KV and causal GQA 6:1. First qualify
   resident main-model inference with MTP off. Check a 12 GiB budget
   including weights, KV, recurrent state, staging and scratch, then
   validate 32K/512 against the frozen PrismML fixture. Add MTP as a
   separately gated stateful feature.
3. **Apex.** Reuse validated GDN and causal attention contracts after
   checking semantic differences. Implement grouped MoE, required K-quants,
   a backend-owned expert cache and pinned/staging transfers. Require
   32K/512 at context 262144 and 6290 MiB expert limit on the CUDA reference.
   Report any different Vulkan placement explicitly.
4. **Inkling-2-Mini.** Reuse the grouped MoE and Q4_0 KV paths when layouts
   agree. Add IQ4_XS/IQ4_NL and the observed mixed types, full and sliding
   attention with relative position behavior, depth-loop/bridge state and
   the valid missing expert plane. 2DSD remains separately gated. Compare
   32K/512 with accepted Inkling using matched placement where possible.

## Release checklist

- [x] Igor explicitly authorizes Vulkan implementation.
- [ ] Exact source/artifact/model references and CUDA regression baselines are frozen.
- [ ] Linux RADV and Windows AMD ICD pass initialization and lifetime tests.
- [ ] Reference kernels pass layout, precision, state and invalid-input fixtures.
- [ ] The first admitted model passes normal CLI and HTTP inference.
- [ ] Qualified AMD package works with CUDA absent. NVIDIA default and explicit
  selections work with existing launchers and unchanged CUDA kernels.
- [ ] Capability report and support matrix match tested devices and drivers.
- [ ] Selected model limitations and unsupported graph/type errors are documented.
- [ ] Peak VRAM/RAM, startup, TTFT, prefill, decode, and end-to-end results are
  recorded from unprofiled runs. Timestamp traces separately attribute CPU
  submission, dispatch, copies and wait time without counting overlaps twice.
- [ ] Every changed common component passes Apex/Inkling regression gates and
  applicable Bonsai comparisons. All arithmetic-changing variants pass the
  agreed logits, token selection, long-generation and task-quality gates.
- [ ] Subsequent model and optional acceleration support is claimed only after
  its own tests have passed on the relevant OS and hardware.
