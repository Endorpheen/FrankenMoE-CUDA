# Build

## Verified environment

| Component | Version |
| --- | --- |
| OS | Ubuntu 26.04 LTS, kernel 7.0 |
| CPU | AMD Ryzen 9 5950X |
| RAM | 64 GB installed, about 60 GiB visible |
| GPU | NVIDIA RTX 4070 12 GB, compute capability 8.9 |
| Driver | 580.173.02 |
| CUDA Toolkit | 12.4.131 |
| Compiler | GCC/G++ 13.4.0 |
| CMake / Ninja | 4.2.3 / 1.13.2 |

The machine had a CUDA 12.4 toolkit despite an initial expectation of CUDA 13.0. The build therefore pins GCC 13 and `sm_89` by default.

## Reproducible build

```bash
scripts/build.sh
```

Supported environment overrides:

```text
BUILD_DIR  JOBS  CC  CXX  CUDACXX  CUDA_ARCH
```

The script prepares the pinned upstream trees, applies the integration patch, configures CUDA, and builds the runtime and tests.

## Tests

```bash
scripts/test_small.sh
```

The test script runs 13 CTest cases, a resident-versus-streamed 64-token greedy comparison on CUDA, and an overlap smoke test. Set `KEEP_TMP=1` to preserve CLI logs and token streams.

The three upstream baselines were also built and tested before integration:

- official llama.cpp CUDA: 62/62;
- BigMoeOnEdge CPU: 12/12;
- expert-tier CUDA: 62/62.
