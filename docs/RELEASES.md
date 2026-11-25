# FrankenMoE releases

The [v0.13.0-alpha release](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.13.0-alpha)
provides Linux and Windows x64 archives and SHA-256 checksums. See the
[multi-model guide](MULTIMODEL-ALPHA.md) for verified model profiles and
platform coverage.

## Candidate assembly and verification

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
