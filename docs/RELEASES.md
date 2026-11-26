# FrankenMoE releases

The [v0.13.0-alpha release](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.13.0-alpha)
provides Linux and Windows x64 archives and SHA-256 checksums. See the
[multi-model guide](MULTIMODEL-ALPHA.md) for verified model profiles and
platform coverage.

The [v0.14.0-vulkan-alpha release](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.14.0-vulkan-alpha)
provides a separate experimental distribution for Qwen2.5-0.5B Q4_0
on Vulkan. It includes both x64 platforms, SHA-256 and the Release
Gate reports. See the [Vulkan setup guide](releases/VULKAN-ALPHA.md).
The combined CUDA+Vulkan candidate remains unpublished after a
Windows Apex CPU one-token generation timeout under Wine. Linux
Apex and Inkling residency checks in that combined gate were also
inconclusive. Existing CUDA archives and checkpoints are preserved.

## Candidate assembly and verification

Run the mandatory [Release Gate](RELEASE-GATE.md) for every candidate.

Use the exact checkpoint inputs recorded in the experiment registry.
Treat the original paths as immutable. Assemble into a fresh directory:

```bash
FRANKENMOE_UI_PACKAGE=/path/to/verified/chat-package \
  bash tools/multimodel-release/stage-linux.sh build/release-linux-candidate
python3 tools/multimodel-release/smoke-linux.py \
  build/release-linux-candidate . build/release-smoke
```

Record GGUF name and quantization, compiler and CUDA image provenance,
the binary checksums, context, KV profile and measured GPU model.
Record native GPU generation, Wine startup, artifact format checks
and historical performance separately.

Package the tested directory without weights, local shell configuration
or local launch aliases. Verify every archive after extraction and
include the archive SHA-256 receipt with the download. Publish only
with an accurate platform and architecture matrix.

## Standalone chat release history

The published [v0.12.1-chat-alpha](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.12.1-chat-alpha)
adds OpenCode tool continuation and separate reasoning accounting on Linux
and Windows. The [v0.12.0 build record](../experiments/EXP-2026-11-23-1805-route-prefetch/release/README.md)
documents the Windows CUDA host inputs. The cumulative Linux checkpoint
is documented in the [build guide](BUILD.md).

To prepare a package, use `tools/package-linux-chat-alpha.sh` or
`tools/package-windows-alpha.sh` with the corresponding built server and web
assets. Publish the versioned package and its `SHA256SUMS`. The model GGUF is
distributed separately.

Release notes should identify the source revision, compiler and runtime inputs,
supported model and profile, measured checkpoint and platform coverage.
Distinguish the native GPU inference result from the HTTP fixture, Wine
startup, and archived benchmark results.

The older `llama-server` package format and retention procedure are preserved
in Git as `4c4ad0aef:docs/RELEASES.md`.
