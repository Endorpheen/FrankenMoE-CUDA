# EXP-2026-11-28-2443 — Batch Inkling verifier Q8 output projection

Project all 16 exact verifier rows together through the existing
batch Q8 matrix path. Keep per-row preparation, softcap and padded
output layout. Reuse host scratch after the layer walk and preserve
the final padded logits row in the decoder. The other head formats
and verification sizes retain their existing dispatch. Disable the
batched path with `I2_VERIFY_BATCH_LOGITS=0`.

The preceding CUDA trace recorded 384 separate Q8 head launches
and 0.418 s of GPU time after the first logits readback. The
trace ended before the complete 512-token window.

## Full first-request comparison

Inkling 2 Mini AD-IQ4_XS, RTX 4070, 32768 prompt and 512 output
tokens, context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert
cache 6720 MiB. Fresh processes used a warmed model file cache
and evaluated the full prompt. The same binary ran both arms.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 716.82 | 56.38 | 55.100 | 13541380 | 9822 |
| Control | 720.06 | 54.75 | 55.166 | 13543084 | 9822 |
| Candidate 2 | 721.71 | 56.26 | 54.809 | 13541132 | 9822 |
| Candidate mean | 719.27 | 56.32 | 54.954 | | |

Decode improves 2.86%, prefill changes by −0.11%, and the full
request saves 0.21 s. All 512 IDs, text and stop type match EXP-2128.
The 32 draft groups, zero rejects and 27819417600 expert bytes match
the control. No scored disk reads, process swap or swap-out occur.

The host backend was built with Fried Goose `d682b183` and GCC 13.
All CUDA images are unchanged.

| Artifact | SHA-256 |
| --- | --- |
| Executable | `24b0ed53397b6dd3865b5764fce77a1ae9457b7e220d028dc6b4e9480f2b8841` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `d7cf16c3d14b7ed4ee8cc619b9e8014f13d41aacda90d54495380db96d3e5ac3` |
| Control response | `1d9a81d4b4f37decfbf74cf1f8e3889c9f490f4f0f6e206c3f1bf2f0b0dc8a68` |
| Candidate 2 response | `018db600d39487aacd9b1b656875ebb0b2ff715012f4da0f5a6635f5dc8424fa` |
