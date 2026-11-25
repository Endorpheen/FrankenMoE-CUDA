# Build and package FrankenMoE

Goose model code is linked with native C host adapters and NVIDIA
CUDA images. The [experiment registry](../experiments/EXPERIMENTS.md)
records the source, compiler, image and kernel choices for each
checkpoint.

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
FRANKENMOE_UI_PACKAGE=/path/to/verified/chat-package \
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
  /path/to/extracted-apex-windows build/windows-multimodel/frankenmoe-multimodel.exe \
  /path/to/bonsai-mtp.exe /path/to/verified-linux-staging
```

The COFF inputs are build checkpoints, not Linux object files. Archive
them with their source and compiler provenance before reassembling a
release. This link command does not compile their upstream source units.
Use the [Release Gate](RELEASE-GATE.md) on the final archives.
