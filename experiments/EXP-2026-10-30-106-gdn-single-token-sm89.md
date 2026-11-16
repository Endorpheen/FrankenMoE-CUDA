# EXP-2026-10-30-106: single-token Gated Delta Net on sm89

Status: REJECTED by measured upper bound. No code was built.

## Measured upper bound (EXP084 decode trace)

- Decode window 4.418923 s for 127 measured tokens.
- `ggml_cuda_gated_delta_net_mma::gdn_single<16,128,1>` is absent from the
  decode top-kernel list; the smallest listed decode kernel is
  `rms_norm_f32` with 16.83 ms over the window.
- GDN decode time is therefore below 16.83 ms for the whole window,
  < 0.133 ms/token, < 0.4% of the ~33.7 ms/token decode.
- The 447.53 ms GDN figure in EXP084 belongs to cold prefill, not the warm
  single-response decode metric.

The pre-stated metric is warm single-response decode; even eliminating the
single-token GDN kernel completely would be below 0.4%, far under the 5% gate.
Prefix restore/branch correctness would also need re-validation, but the bound
already decides the direction.

## Verdict

REJECTED for the decode metric. The sm89 kernel may still matter for prefill,
but that is not this hypothesis and was not attempted. No sm90/sm100 assumption
was transferred.
