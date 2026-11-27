# GooseInfer multi-model alpha

Published FrankenMoE archives retain their original executable names.

This package uses Goose for model execution and native C adapters for
the operating system and NVIDIA CUDA driver. Model GGUF files are
distributed separately.

## Linux x86-64

The staged server binaries require glibc 2.43. Linux inference was
checked on the NVIDIA RTX 4070, compute capability 8.9, with 12 GB
VRAM and 64 GB system RAM. This is the tested system configuration.
Bonsai CUDA images are built for SM89. No other GPU architecture was
qualified for this package.

Run one server at a time from the extracted directory:

```bash
./gooseinfer apex /path/to/Apex-35B-V1.0-Q4_K_M.gguf
./gooseinfer inkling /path/to/Inkling-2-Mini-AD-IQ4_XS.gguf
./gooseinfer bonsai /path/to/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
```

The default loopback ports are 9101, 9102 and 9103, respectively.
Pass a different port after the model path if needed. The dashboard
uses Python 3 when stdout is an interactive terminal. Set
`FRANKENMOE_CONSOLE=0` for plain output, or set `FRANKENMOE_LOG`
to a writable log location. The OpenAI-compatible base URL ends in
`/v1`. Native completion requests go to `/completion`.

The bundled profiles preserve Apex context 262144 with a 6290 MiB
expert cache, and Inkling context 36864 with a 6720 MiB expert cache.
Both released profiles use Q4_0 K/V and batch/ubatch 4096/4096.
These recorded CPU masks target the tested 16-thread placement.
On a different CPU, review the profile flags before launching.
If system memory pressure evicts mapped weight pages, read the
unchanged GGUF once to restore host residency before measuring.

Bonsai HTTP uses context 36864, batch/ubatch 4096/1024 and Q4_0 K/V.
Its experimental Q8 MTP path is a separate CLI with token-ID input:

```bash
set -a
source ./profiles/bonsai-mtp.env
set +a
taskset 0f0f ./bin/bonsai-mtp /path/to/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf \
  ./cuda/bonsai-mtp.cubin ./cuda/frozen-goose.ptx 36864 16 1812 5020 220 15
```

The three model servers each returned matching token IDs and text in
two 129-input/4-output requests on the test system. The packaged
Bonsai CLI matches the saved 129/64 MTP output. Historical performance
below refers to the matching checkpoint and exact 32K/512 workload,
not the short package smoke tests.

| Model and mode | Prefill, tok/s | Decode, tok/s | Evidence |
| --- | ---: | ---: | --- |
| Apex HTTP, EXP-1880 | 3505.42 | 82.10 | Accepted Linux checkpoint |
| Inkling HTTP, EXP-1969 | 296.24 | 22.03 | Accepted packed-Q8 Linux checkpoint |
| Bonsai HTTP, EXP-2019 | 648.79 | 32.70 | Exact 512-ID HTTP gate |
| Bonsai greedy MTP CLI, EXP-2047 | 618.10 | 72.12 | Exact 512-ID and selected-logit gate |

All figures are for one response, with prefill and decode measured
separately. The Bonsai MTP rate applies to the CLI. This package
does not route HTTP requests through the MTP path.

Verify the downloaded archive against the adjacent SHA-256 receipt:

```bash
sha256sum -c SHA256SUMS
```

## Windows x64 coverage

| Component | Wine results |
| --- | --- |
| Apex HTTP from v0.12.1 | Real GGUF loaded in CPU mode, /health returned HTTP 200. One-token request exceeded the 25-second client timeout. |
| Bonsai MTP CLI | PE32+ x64 build reached the CUDA driver loader, which could not load `nvcuda.dll`. The CLI requires CUDA. |
| Inkling | Linux only in this release. The Windows build did not complete linking. |

The shipped executables import Windows system libraries. Native Windows
CUDA inference was not tested. The Linux benchmark table applies to the
RTX 4070 on Linux.

## Source and checkpoints

The Linux bundle retains the exact EXP-1880, EXP-1969, EXP-2019 and
EXP-2047 executables. The experiment journal records the original
source, compiler and image provenance. The archive manifest records
each bundled file's SHA-256.
