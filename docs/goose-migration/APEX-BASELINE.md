# Apex performance baseline

## Current canonical baseline: EXP-2026-11-19-1298

Registered 2026-11-19. Any performance comparison is made against this entry, on identical conditions.

```
profile        Apex-35B-V1.0-Q4_K_M, RTX 4070 12 GiB
request        32768-token input, 512-token output, seed 123, one sequence (np 1)
context        262144
batch/ubatch   4096 / 4096
ngl            all, -fit off, --cpu-moe, --moe-cache-mib 4992
quantisation   -ctk q4_0 -ctv q4_0, flash attention on
threads        -t 8 -tb 16, --cpu-mask 0f0f, --cpu-mask-batch ffff, strict
server         llama-server from builds/flint-acc/bin, port 9101
hash           c2ff42cc1219 (response content and 512-token sequence)
```

### Measurements, three sequential runs of three requests

```
run 1   decode 62.29 62.73 62.89   prefill 2678.4 2953.3 2948.0
run 2   decode 62.53 62.77 62.99   prefill 2702.5 2948.7 2962.5
run 3   decode 62.56 63.45 62.48   prefill 2662.4 2968.7 2904.4
median of the nine: decode 62.73   prefill 2948.0
```

The registration run quoted in the experiment card gave decode 62.79 / prefill 2952.3 / VRAM 9511 MiB /
RSS 20837548 kB; the three-run medians above are the number to compare against, because they are the same
quantity measured nine times rather than three times once.

### Control for the baseline's own key feature

Same build, same conditions, `GGML_MMAP_EXPERT_PIN=0`:

```
decode 42.63 43.91 44.15   prefill 1945 2190 2175   VRAM 9475 MiB   RSS 20837332 kB
```

Everything in the new baseline depends on the resident mmap expert ranges being registered. A build that
loses that registration silently returns to this level, which is why
`tools/check-accepted-components.py` runs as a pre-flight of every benchmark.

### Published release

```
tag      v0.9.0-exp1298   (annotated, on e5a725033a0d65ea7d3d6da5446e573ee0318151)
release  https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.9.0-exp1298   (Latest)
asset    frankenmoe-apex-exp1298-linux-x86_64.tar.gz
sha256   834a5808aaafd8109d81cfbd593968425263d5eca601f2cb05b8d23a3c751177
```

Downloaded after publication to a separate directory, checksum verified, extracted (12 of 12 files verified) and run:
decode 62.56 / 62.86 / 62.38, prefill 2701.2 / 2956.0 / 2954.1, hash `c2ff42cc1219`, accepted-component pre-flight
5 of 5. Earlier releases including EXP906 are untouched.

### Provenance

| item | value |
|---|---|
| git revision | see `builds/checkpoint-20261119-EXP1298/REVISION` |
| checkpoint | `builds/checkpoint-20261119-EXP1298/` with `SHA256SUMS` |
| headline experiment | `experiments/EXP-2026-11-19-1298-host-registration/` |
| launch command | `tools/apex-test.sh <tag> 512 4096 4096 4992` (records argv, checksums, deps per run) |

## Historical baseline: EXP-2026-11-16-906 (kept, not superseded in the record)

```
decode 55.5176   prefill 2625.2066   (median of six responses, A-B-B-A block)
artifact: /home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run
```

Its own control, without host registration, decoded at 42.59 / prefilled at 2019.28. EXP1298 is +13.1 % decode
and +12.5 % prefill against EXP906, and EXP906 remains in the project history with its artefacts untouched.

## Rule for future records

A new confirmed record becomes the current baseline; every previous baseline is preserved in this file and
its artefacts are never deleted, overwritten or declared invalid. If a comparison is made under conditions
that differ from the table above, the difference is stated with the numbers rather than left implicit.
