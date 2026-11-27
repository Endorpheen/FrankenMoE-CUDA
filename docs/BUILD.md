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

## Verified Linux candidate inputs

| Component | Checkpoint |
| --- | --- |
| Apex HTTP | EXP-1880 |
| Inkling HTTP | EXP-1969 |
| Bonsai HTTP | EXP-2019 |
| Bonsai greedy MTP CLI | EXP-2047 |

The Bonsai generations use the published Fried Goose KeelWing
compiler, GCC 13, Clang 18 for CUDA kernels and NVIDIA ptxas targeting
SM89. Follow the build instructions in each experiment before
assembling the package. Verify the input artifacts against the
experiment SHA-256 receipts.

```bash
GOOSEINFER_UI_PACKAGE=/path/to/verified/chat-package \
  bash tools/multimodel-release/stage-linux.sh build/release-linux-candidate
python3 tools/multimodel-release/smoke-linux.py \
  build/release-linux-candidate . build/release-smoke
```

The packaged HTTP runtimes require glibc 2.43. Python 3 is optional
for the terminal dashboard. The measured GPU is the RTX 4070 with
compute capability 8.9. See [model profiles and coverage](MULTIMODEL-ALPHA.md).

## Windows Bonsai cross-build

```bash
bash tools/multimodel-release/build-windows-bonsai.sh build/bonsai-windows
```

This uses MinGW-w64 and the same generated Goose C unit as the Linux
candidate. The Win32 adapter loads the NVIDIA driver from the system
directory and maps GGUF files read-only. The Bonsai CUDA image and
frozen PTX are supplied alongside the executable. The cross-build
is not a native Windows CUDA generation result.

See [release verification and publication](RELEASES.md) for the
distinction between artifact checks, Wine transport and native
NVIDIA tests.

## Windows multi-model host

The Windows multi-model link step uses the EXP-1969 Inkling COFF
checkpoint, a Windows common adapter checkpoint, and the v0.12.1 chat
host objects. Pass the directories explicitly. The linker isolates the
Inkling and Bonsai Goose entry points and uses the ordinary system NVIDIA
driver on native Windows. It also exposes a CUDA-free `--model-check`
path for checking real GGUF files and tokenization under Wine.

```bash
FRANKEN_MAIN=/path/to/checkout-with-win32-headers \
  bash tools/multimodel-release/link-windows-multimodel.sh \
  build/windows-multimodel /path/to/inkling-coff /path/to/common-coff /path/to/chat-coff
bash tools/multimodel-release/stage-windows.sh build/windows-staged \
  /path/to/extracted-apex-windows build/windows-multimodel/gooseinfer-multimodel.exe \
  /path/to/bonsai-mtp.exe /path/to/verified-linux-staging
```

The COFF inputs are build checkpoints, not Linux object files. Archive
them with their source and compiler provenance before reassembling a
release. This link command does not compile their upstream source units.
Use the [Release Gate](RELEASE-GATE.md) on the final archives.

## Renamed build outputs

Fresh Linux builds produce `gooseinfer-infer`, `gooseinfer-server`,
`gooseinfer-serve`, `gooseinfer-tokenize`, `libgooseinfer.a`, and
`libgooseinfer-tensor.a` in the corresponding builders. The old names
are compatibility aliases. New multi-model packages use `gooseinfer`
on Linux and `gooseinfer-multimodel.exe` on Windows.

New launchers prefer the `GOOSEINFER_*` environment settings and still
read their `FRANKENMOE_*` predecessors. They also forward the legacy
web-root setting to archived checkpoint binaries. Windows profile storage
keeps its existing registry key and directory so saved settings survive
the rename. Historical release asset names, checkpoint filenames and
checksums are retained.
