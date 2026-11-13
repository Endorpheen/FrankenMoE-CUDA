# Full kernel census from the archived trace

The previously used census listed 8 kernels per phase because `analyze_trace.py` ends its query
with `LIMIT 8`. The underlying database is present, so the full list can be read directly:

    sqlite3 runtime/exp175/results/census/apex-census.sqlite \
      "SELECT s.value, COUNT(*), SUM(k.end-k.start) FROM CUPTI_ACTIVITY_KIND_KERNEL k
       JOIN StringIds s ON s.id=k.demangledName GROUP BY s.value ORDER BY 3 DESC;"

**61 distinct kernels**, not 10. By GPU time:

| share | launches | kernel | arm |
|---|---:|---|---|
| 30.3% | 2448 | `mul_mat_q` |  |
| 19.6% | 80 | `fg175m_kernel` | goose |
| 9.7% | 464 | `mul_mat_q` |  |
| 8.9% | 240 | `fg174m_kernel` | goose |
| 4.5% | 300 | `concat_non_cont` |  |
| 3.8% | 1032 | `fg166_kernel` | goose |
| 3.7% | 431 | `moe_weighted_reduction_f32` |  |
| 3.4% | 2304 | `quantize_mmq_q8_1` |  |
| 2.6% | 240 | `fg170s_kernel` | goose |
| 1.8% | 624 | `quantize_mmq_q8_1` |  |
| 1.8% | 936 | `mm_ids_helper` |  |
| 1.2% | 330 | `fg168_kernel` | goose |
| 1.1% | 431 | `fg167_kernel` | goose |
| 0.9% | 874 | `fg168_kernel` | goose |
| 0.8% | 660 | `fg168_kernel` | goose |
| 0.7% | 723 | `fg167_kernel` | goose |
| 0.6% | 312 | `cutlass::Kernel2` |  |
| 0.5% | 464 | `quantize_mmq_q8_1` |  |
| 0.5% | 110 | `fg166_kernel` | goose |
| 0.4% | 440 | `cpy_scalar` |  |
| 0.4% | 731 | `fg167_kernel` | goose |
| 0.3% | 395 | `mul_mat_vec_f` |  |
| 0.3% | 220 | `fg168_kernel` | goose |
| 0.3% | 220 | `fwht_cuda` |  |
| 0.3% | 180 | `fg175q_contig` | goose |
| 0.3% | 220 | `fwht_cuda` |  |
| 0.2% | 480 | `mul_mat_q` |  |
| 0.2% | 220 | `fg171r_kernel` | goose |
| 0.1% | 432 | `fg169t_kernel` | goose |
| 0.1% | 220 | `fg176r_kernel` | goose |
| 0.1% | 80 | `fg175f_general` | goose |
| 0.1% | 80 | `fg175f_mask` | goose |
| 0.1% | 90 | `fg174s_kernel` | goose |
| 0.0% | 480 | `mul_mat_q_stream_k_fixup` |  |
| 0.0% | 330 | `k_get_rows_float_vec` |  |
| 0.0% | 22 | `mul_mat_vec_q` |  |
| 0.0% | 10 | `fg175v_kernel` | goose |
| 0.0% | 268 | `mul_mat_vec_q` |  |
| 0.0% | 268 | `mul_mat_vec_q` |  |
| 0.0% | 10 | `fg175m_kernel` | goose |
| 0.0% | 131 | `mul_mat_vec_q` |  |
| 0.0% | 761 | `fg166_kernel` | goose |
| 0.0% | 888 | `quantize_q8_1` |  |
| 0.0% | 336 | `k_get_rows_float` |  |
| 0.0% | 80 | `mul_mat_vec_q` |  |
| 0.0% | 39 | `mul_mat_vec_q` |  |
| 0.0% | 39 | `mul_mat_vec_q` |  |
| 0.0% | 120 | `fg166_kernel` | goose |
| 0.0% | 300 | `fg166_kernel` | goose |
| 0.0% | 39 | `mul_mat_f` |  |
| 0.0% | 78 | `mul_mat_vec_f` |  |
| 0.0% | 10 | `fg175v_kernel` | goose |
| 0.0% | 90 | `fg170s_kernel` | goose |
| 0.0% | 21 | `mul_mat_vec_q` |  |
| 0.0% | 20 | `mul_mat_vec_q` |  |
| 0.0% | 10 | `fg175f_uniform` | goose |
| 0.0% | 39 | `dot_kernel` |  |
| 0.0% | 39 | `reduce_1Block_kernel` |  |
| 0.0% | 30 | `concat_cont` |  |
| 0.0% | 30 | `fg166_kernel` | goose |
| 0.0% | 10 | `fg175v_combine` | goose |

## What this changes

The earlier documents described "ten kernels" and treated that as the complete set of what Apex
executes. It was the top of a truncated list. Two consequences:

1. The blocked families are understated. `mul_mat_q` (30.3% + 9.7%) and `quantize_mmq_q8_1`
   (3.4% + 1.8%) together are over 45% of kernel time, and `mm_ids_helper` at 1.8% is another
   native path that exists here.
2. Most of the remaining 61 are small. Rows below the top sixteen are each under 0.7%, and many
   are the same kernel at different template parameters — `mq` variants, `fg168_kernel` at four
   instantiations, and so on.

The coverage table in `final-coverage.md` should be read with this in hand: it enumerates the
kernels that matter by share, which is the useful set, but it is not the whole list.

## Provenance

Trace: `runtime/exp175/results/census/apex-census.nsys-rep` with its `.sqlite` export, recorded
2026-11-08. This trace postdates EXP084 and is closer to the current chain, though it is still not
the active runtime — the two census attempts on the active runtime are documented in
`census-status.md` and both failed for reasons unrelated to capture.
