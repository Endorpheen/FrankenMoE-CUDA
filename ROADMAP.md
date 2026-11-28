# GooseInfer roadmap

The [current state](docs/CURRENT_STATE.md) records the release, cumulative
runtime and qualification status.

## Required work sequence

1. **Restore and improve model performance.** Work in order on Inkling 2
   Mini, GigaChat4, then Bonsai. Compare each migrated runtime against its
   strongest reproducible pre-migration checkpoint under identical hardware,
   model, settings, workload, and cache conditions. Restore and then exceed
   the recorded prefill and decode performance by at least 25% in both phases
   while protecting correctness, stability, and memory. Inkling acceptance
   uses EXP-2128 with 32K/512, at least 545.73 prefill tok/s and an unrounded
   decode floor of 44.99365101086784 tok/s. Use the [experiment registry](experiments/EXPERIMENTS.md)
   and [optimization registry](docs/goose-migration/FRIED-GOOSE-OPTIMIZATION-REGISTRY.md)
   to combine verified improvements and transfer reusable mechanisms. Rotate
   approaches when profiling or repeated trials show limited headroom. Keep
   the Apex EXP-2411 checkpoint and evidence available for regression checks
   and later optimization.
2. **Complete release acceptance.** Cover every supported model, mode, and
   compute backend on the relevant platforms. Verify functionality,
   correctness, stability, performance, and memory against the architecture
   gates and the [release gate](docs/RELEASE-GATE.md).
3. **Publish the Linux and Windows release.** Publish after acceptance.
   Include current usage documentation, qualification results, and archive
   checksums. Label every configuration that could not be verified.
4. **Resolve the remaining technical debt after publication.** Maintain the
   [source inventory](docs/goose-migration/TECH-DEBT.md#remaining-non-goose-runtime-inputs)
   during performance work. The verified identical Bonsai prefixes were
   consolidated ahead of release as a separately scoped exception. After
   publication, remove remaining duplication, reuse generic Goose kernels
   wherever applicable, and port the unique compute paths to Goose.
5. **Verify full runtime independence.** Audit the repository, build system,
   and every executable path. The runtime implementation must be 100% Goose,
   with zero C, C++, OpenCL C, or other implementation languages and zero hidden
   external compute or fallback implementations. Retain only the required OS
   and hardware driver interfaces. Confirm functionality, correctness,
   stability, and performance across the supported configurations.

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
- Bonsai work focuses on full prefill against the PrismML 32K/512 reference.
  Preserve the measured exact MTP decode checkpoint while optimizing prefill.
- [Vulkan backend roadmap](docs/roadmap/VULKAN-BACKEND.md) is a deferred design.
