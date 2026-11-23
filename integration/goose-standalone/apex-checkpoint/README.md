# Apex cumulative Linux checkpoint

`cuda-driver.c` is the C host driver from the recorded EXP-1808 cumulative
checkpoint `37ac0f1d570dbec705f82c3b51e2437e9eec655f0da6bb69fcdf61bb8a1cf5e8`.
The source SHA-256 is
`4f736afe0c2aac644d7f7150720340e703f7129158fb05514c01669e5c70d631`.
It combines EXP-1805 route prefetch, EXP-1806 early decode submission and the
EXP-1808 job-pack upload for packets above 4096 bytes. The Linux chat entry
defaults to `GOOSE_DECODE_SPLIT=4` and `GOOSE_DECODE_SPLIT_COUNT=1`.
`GOOSE_DECODE_SPLIT=0`, `GOOSE_JOBS_ASYNC=0` and `GOOSE_EXPERT_PREFETCH=0`
disable the corresponding paths.

## Build inputs

- The pre-chat `main.goose` from the EXP-1786 source reproduction. The build
  composes it with this tree's chat HTTP unit.
- The EXP-1805 `libfranken-inference.a` from
  [the accepted checkpoint](../../../experiments/EXP-2026-11-23-1805-route-prefetch/README.md).
  The builder checks its SHA-256, copies the archive, and replaces only the
  CUDA driver object before linking the newer composed chat host.
- The matching EXP-1786 `build-inputs/` headers, including the embedded
  device images. The separately versioned EXP-1805 predictor image is in
  this repository.
- Fried Goose KeelWing ownership v2, SHA-256
  `139373305b88833a289ea44cfb7027e6327d039d1916821c06dfeb7064c58a2e`,
  with its matching standard library. The reference compiler for the
  C host is GCC 13.4.0.

```bash
GOOSE=/path/to/keelwing-ownership-v2/goose CC=gcc-13 \
  bash tools/build-linux-chat-alpha.sh /path/to/new-build \
    /path/to/exp1786-source-repro/host \
    /path/to/exp1805-route-prefetch \
    /path/to/exp1786-split-planes/build-inputs
```

The checked-in source needs these external build inputs. The archived
benchmark executable was `frankenmoe-infer`; the build above produces a newer
chat host, so the executable hash and benchmark results apply to the archived
checkpoint. The Windows build uses its existing platform-specific path.
