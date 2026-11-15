# EXP-2026-11-15-722 — the first real kernel profile: Goose owns 55% of GPU time

Base: `f66096eb`. `perf` is blocked (`perf_event_paranoid=4`), but `nsys` works. Profiled one
32K/512 request end to end with `--trace cuda` and queried the CUPTI tables directly rather than
parsing the text report.

## 1. GPU time by family

| family | kernels | seconds | share |
|---|---:|---:|---:|
| **Goose `fg*` kernels** | 16,331 | 4.314 | **55.3%** |
| ggml `mul_mat_q` (quantised matmul) | 3,872 | 3.110 | 39.9% |
| ggml `quantize_mmq_q8_1` | 3,392 | 0.457 | 5.9% |
| cutlass gemm | 312 | 0.054 | 0.7% |
| `mul_mat_vec_q*` family | ~1,000 | 0.020 | 0.3% |

**Goose is the majority of GPU time, not a layer on top of it.** That is the opposite of what the
"llama.cpp plus Goose" summary suggests, and it is the most useful fact measured this session.

## 2. The top kernels individually

| seconds | calls | avg ms | kernel |
|---:|---:|---:|---|
| 2.345 | 2,448 | 0.96 | `mul_mat_q<type12, 128, ...>` — ggml quantised matmul |
| 1.419 | 80 | **17.74** | `fg175m_kernel<8>` — Goose flash attention |
| 0.744 | 464 | 1.60 | `mul_mat_q<type14, 128, ...>` |
| 0.641 | 240 | 2.67 | `fg174m_kernel` — Goose GDN MMA |
| 0.548 | 936 | 0.59 | `fg181m_kernel` — Goose MM ids |
| 0.256 | 2,304 | 0.11 | `quantize_mmq_q8_1` |
| 0.246 | 1,216 | 0.20 | `fg166_kernel` — Goose broadcast |
| 0.229 | 469 | 0.49 | `fg179w_kernel` — Goose MoE weighted |

`fg175m` is the single largest kernel after the main matmul: 17.74 ms per call, 80 calls. It is the
Goose port of `flash_attn_ext_f16` for the fixed Ada instantiation, and its header states that the
numerical work is delegated to the Goose process module with no C++ math callback. So this cost is
**Goose's own flash attention**, and it is 33% of all Goose kernel time.

## 3. What this changes about where to optimise

The plan was "optimise the quantised arithmetic". The profile says that is 39.9% of the time, split
between two `mul_mat_q` instantiations (types 12 and 14 - Q4_K and Q6_K), and they are **native
ggml kernels that already use MMQ with 128-wide tiles**. There is no obvious cheap win there: they
are the substrate's tuned path.

The other 55% is Goose code that we own and can change. Within it, `fg175m` alone is 33%.

So the next optimisation should target **`fg175m`**, not the matmul:
- 17.74 ms average with a 9.6 ms standard deviation - a very wide spread, which often means tail
  effects (stream-K partitioning) rather than a uniformly slow kernel;
- 80 calls for 512 tokens suggests it runs once per attention layer per prefill chunk, not per
  token, so its cost is dominated by prefill, not decode;
- it is Goose code, so improving it moves the migration and the speed at the same time.

## 4. What is NOT claimed

That `fg175m` is slow *relative to native*. The native flash attention may cost the same. What is
measured is its share of the total, not its quality. Comparing it against the native kernel on the
same work is the next measurement, and it needs the native path to be callable - which the
integrated runtime has.

## 5. Reproducibility

```
/usr/local/cuda/bin/nsys profile -f true -o /tmp/apex-dec --trace cuda,nvtx --sample none \
    <the pinned run-apex.sh argv>
sqlite3 /tmp/apex-dec.sqlite "SELECT ... FROM CUPTI_ACTIVITY_KIND_KERNEL ..."
```

`top-kernels.txt` holds the query output. The `.nsys-rep` (13.6 MB) and `.sqlite` are not committed:
they are reproducible from the pinned configuration.
