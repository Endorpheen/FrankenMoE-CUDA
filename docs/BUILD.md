# Build and package the standalone chat host

## Universal runtime from source

Run the following command from a GooseInfer checkout on Linux. Use a fresh
output directory for each target.

```bash
export GOOSE_BIN=/path/to/keelwing-preview/bin/goose
export GOOSE_CUDA_DRIVER_BIN=/path/to/fried-goose-v0.3.1-dev.1-keelwing/fgoose
export APEX_PTXAS=/path/to/cuda-12.4/bin/ptxas
export GOOSE_HOST_BIN=/path/to/keelwing-ownership-v2/bin/goose
export GOOSE_HOST_STDLIB=/path/to/keelwing/stdlib
export APEX_ROUTING_GOOSE=/path/to/goose-2026.3/goose
bash tools/build-gooseinfer-universal.sh linux build/universal-linux
bash tools/build-gooseinfer-universal.sh windows build/universal-windows
```

The Linux command writes `gooseinfer`. The Windows cross-build writes
`gooseinfer.exe`. Each output directory receives a `SHA256SUMS` file.
The script generates both backend images and all model objects from the
checkout. Model weights are needed only to run inference.

The v0.15.0 release uses the following pinned mixed toolchain. This release is grandfathered under the single-compiler policy for future builds.

| Tool | Version |
| --- | --- |
| `GOOSE_BIN` | Fried Goose v0.3.0 preview KeelWing |
| `GOOSE_CUDA_DRIVER_BIN` | Fried Goose v0.3.1-dev.1 KeelWing for the CUDA dispatcher, universal CPU module, GigaChat4 graph, and Bonsai session plans |
| `APEX_PTXAS` | CUDA 12.4.99 ptxas for the selected GPU images |
| `GOOSE_HOST_BIN` | KeelWing ownership v2, Fried Goose source revision `bb1452faa1e3f4e4ff4d715723c91a4fcf4e8c78` |
| `GOOSE_HOST_STDLIB` | KeelWing v0.3.0 preview standard library |
| `APEX_ROUTING_GOOSE` | Goose 2026.3 string compiler |
| `CUDA_HOME` | CUDA 12.6 toolkit, including its headers, libdevice and ptxas 12.6.85 |
| Clang | Clang 18.1.8 |
| Linux C compiler | GCC 13.4.0 for the host, GCC 15.2 for GigaChat4 and the Vulkan Goose unit |
| Windows cross-compiler | x86_64 MinGW GCC 13 and its binutils |
| Vulkan shader tools | glslang 16.2.0 and SPIRV-Tools 2026.1 |

The build also uses Bash, Python 3, Git, GNU binutils, `patch`, and Vulkan
headers. Set `CUDA_HOME` if CUDA is installed outside `/usr/local/cuda`.
If the Vulkan headers or shader tools are installed outside the usual search
paths, set `VULKAN_HEADERS`, `GLSLANG_VALIDATOR`, and `SPIRV_VAL`.

## Historical Linux x86-64 packaging

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
