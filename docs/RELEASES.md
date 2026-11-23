# Standalone chat releases

The published [v0.12.0-chat-alpha](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.12.0-chat-alpha)
is the EXP-1805 Linux and Windows chat release. Its
[build record](../experiments/EXP-2026-11-23-1805-route-prefetch/release/README.md)
documents the platform checks and input artifacts. The cumulative Linux
source and its newer checkpoint are documented in the [build guide](BUILD.md).

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
