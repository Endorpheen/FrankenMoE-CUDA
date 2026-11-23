# Runtime records

This directory preserves compiler records and original build assets.
[Standalone integration](../integration/README.md) and the
[build guide](../docs/BUILD.md) describe the active chat runtime.

- `compiler/` records compiler pins for the builds that used them.
- [`checkpoints/`](checkpoints/README.md) preserves older artifacts and receipts.
- `reconstructed/`, `goose-stage/` and the CMake entry points document
  the earlier hybrid runtime. Reproduce those configurations with their
  recorded source revisions and libraries.
