# FrankenMoE roadmap

The [current state](docs/CURRENT_STATE.md) records the release, cumulative
runtime and qualification status.

- **Apex performance.** Use Apex 32K/512, a 262144-token context and a 6290 MiB
  expert cache on RTX 4070 for comparisons against the recorded checkpoint.
  Keep the archived checkpoint and paired correctness, prefill, decode and
  memory evidence with each accepted change.
- **Reproducible builds.** Record the Goose compiler, unit, device image
  inputs and archive used to assemble the standalone Linux chat host. The
  [current recipe](docs/BUILD.md) identifies the inputs still supplied by
  checkpoints.
- **Chat and platforms.** Qualify the web UI and OpenCode against the
  [protocol checklist](docs/OPENCODE-SMOKE-TEST.md). Capture Windows NVIDIA
  evidence separately from Wine or HTTP fixture checks.
- **Model compatibility.** Use the
  [architecture registry](docs/goose-migration/MODEL-ARCHITECTURES.md) for
  per-model evidence and the acceptance gates.

The earlier experiment queue is preserved in the Git
history at `4c4ad0aef:ROADMAP.md`. The
[Goose migration record](GOOSE_MIGRATION_PLAN.md) indexes the older work.

## Current model work

- [Inkling performance evidence](docs/INKLING_PERFORMANCE.md) and supported
  launch settings are recorded with the multi-model checkpoints.
- Model optimization is paused at the
  [Bonsai CUDA handoff](experiments/EXP-2026-11-26-2109-bonsai-mtp-handoff/README.md).
  Preserve the stable CUDA release and both experimental checkpoints.
- Next is [AMD/Vulkan on Linux and Windows](docs/roadmap/VULKAN-BACKEND.md),
  with CUDA regression checks and the shared Release Gate. Mark all
  unverified AMD configurations experimental in the two release archives.

## Planned model after AMD/Vulkan

[Surogate 3.7 35B-A3B](https://huggingface.co/surogate/Surogate3.7-35B-A3B)
is a Qwen3.5 MoE derivative with an MTP head. Keep this model deferred.
After the AMD/Vulkan releases, pause FrankenMoE development until Igor
resumes it. The next development priority is Fried Goose IR, compiler
backend and machine-code generation.

For the future integration audit, verify the GGUF metadata and tensor
mapping, quantization support, and whether the chosen GGUF preserves
the MTP tensors. Check which existing Qwen3.5 MoE routing, expert,
attention, GDN and MTP kernels can be reused with the model's actual
geometry and tensor formats. Compatibility is pending that audit.
