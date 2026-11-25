# Inkling-2-Mini performance

Use **32K/512 as the sole primary Inkling performance workload**.
Shorter workloads are for numerical and functional diagnostics.

## Current FrankenMoE reference

[EXP-1969](../experiments/EXP-2026-11-25-1969-inkling-q8-packed-input/README.md)
established the current first-request baseline of **296.24 prefill /
22.03 decode tok/s** on the RTX 4070. It passed the warm Apex gate in
[EXP-1970](../experiments/EXP-2026-11-25-1970-apex-q8-regression-gate/README.md).
The executable is EXP-1969,
SHA-256 `739221fcb84cc52e91b817a465a598b5b82ce41da483c83f1a614e8b96299016`.
Use the **packed** arms in the EXP-1969 receipt.

Use the recorded Inkling-2-Mini IQ4_XS GGUF, exact 32768 input IDs
and 512-output request from the experiment receipt. Context
36864, logical batch/ubatch 4096, Q4_0 K/V, expert limit 6720 MiB,
threads 8/16 and CPU masks 0f0f/ffff. The reference enables the
joined attention/MoE bridge, LFU decode, parallel CPU softcap and
packed Q8 prefill input (`I2_Q8_PACKED_INPUT=1`).
Use the full argv/env in the receipt. Preserve the frozen reference
and its request when evaluating a candidate.

## Comparison protocol

- Compare each candidate with the latest accepted FrankenMoE
  reference. Execute arms sequentially with identical first-request
  conditions and cached model pages. Recompute the full prompt.
  Report prefill, decode, wall time, peak board VRAM, process RSS
  and swap separately.
- Check exact output IDs and text. Check intermediate state where
  the changed operation admits a fingerprint or equivalent reference
  comparison. Collect diagnostic traces separately from performance
  timings. Unset `I2_CHECK_STATE` in scored performance arms.
- Run pinned upstream checks periodically and before release.
  EXP-1967 reports the fixed upstream placement and binary revision.
  Update this reference link only after accepting the new candidate.
- Preserve Apex. Use its own 32K/512 request, context 262144,
  pinned expert profile and 6290 MiB cache. Measure the second
  request after one complete warm-up when comparing warm Apex
  throughput. Keep first-request and warm observations distinct.

The next target is **500 prefill / 25 decode tok/s** at 32K/512.
The current means imply about 45.08 s to remove from prefill and
2.75 s from decode. Reprofile the reference and prioritize the
largest exposed losses before selecting an implementation.
