# Build and package the standalone chat host

## Linux x86-64

Use the [cumulative Apex build recipe](../integration/goose-standalone/apex-checkpoint/README.md).
It pins the KeelWing ownership v2 Goose compiler, the pre-chat Goose unit,
the EXP-1805 Linux archive and the EXP-1786 build headers. Keep those
inputs together with their checksums. The recipe uses GCC 13 and replaces
the archived CUDA driver object with the cumulative driver built from this
tree.

Put the matching web assets in the output directory as `web/` and package
the bundle from the repository root:

```bash
bash tools/package-linux-chat-alpha.sh BUILD_DIR RELEASE_TAG
```

The package contains the native server, terminal launcher and dashboard,
web assets and licenses. See the [launch command](../README.md#run-the-linux-chat-server).

## Windows x64

See the [v0.12.0 build record](../experiments/EXP-2026-11-23-1805-route-prefetch/release/README.md)
for the MinGW build inputs and the [Windows guide](../integration/goose-standalone/windows/README.md)
for the launcher and troubleshooting. Linux EXP-1808 results apply to the
measured Linux checkpoint.

## Source checks

`bash tools/check.sh fast` composes and compiles the Goose unit.
The [test index](../tests/README.md) identifies the model, platform and
checkpoint dependencies of the other fixtures. Historical fixtures use their recorded source revisions.
