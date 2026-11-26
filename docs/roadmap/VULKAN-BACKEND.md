# Vulkan backend for FrankenMoE

**Status: Vulkan bring-up and first compute graph.** Device selection,
transfer, and a short RMSNorm → Q4_0 matmul → SwiGLU graph are implemented.
Model execution and release qualification are open. Keep the existing
CUDA inference path unchanged during bring-up.

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

```bash
# Point GLSLANG_VALIDATOR and SPIRV_VAL to the tools if they are outside PATH.
bash tools/build-goose-vulkan.sh build/vulkan-bringup/linux linux compute
bash tools/build-goose-vulkan.sh build/vulkan-bringup/windows windows compute
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json \
    build/vulkan-bringup/linux/frankenmoe-vulkan-compute-test 0 1
```

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

## Model order

1. **Qwen2.5-0.5B-Instruct, Q4_0 GGUF** is the proposed first target. Choose
   and record an exact GGUF revision after inspecting its tensor inventory.
   It exercises GGUF, BPE, RMSNorm, RoPE, causal GQA, SwiGLU, quantized
   embedding/projections and sampling with modest VRAM. The existing
   `goose/model/qwen2.goose` and `base/{dense,trunk}.goose` are sources
   for graph contracts. Main's host audit does not establish a working
   standalone Qwen2 inference path. Pin an independent CPU reference before
   changing code.
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
