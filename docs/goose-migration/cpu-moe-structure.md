# CPU MoE: Goose owns the control, native owns the dot products

Checked by symbol/disassembly rather than inferred from the witness counts, because "op=100
calls=42560" says a Goose entry point ran, not what it did.

## Call chain, traced in the shipped library

`runtime/exp196/run/libggml-cpu.so.0.27.0`, disassembly:

    ggml_compute_forward_mul_mat
      -> fg143_ctl_leaf_mul_mat_id        (Goose control leaf)
           -> fg143_try                   (Goose)
           -> fg140_dispatch              (native fallback)

`fg143_compute`, the generated Goose body, is called from exactly two places in the whole
library — `fg143_try.part.0` and `fg143_apply_fused` — and `fg143_try` is reached 90 times from
the control leaves. So the dispatch policy, task count, scratch arithmetic and per-worker graph
lifecycle are Goose, which is what the `FG143_CONTROL plans=5346 dispatches=170378` witness
counts.

## Where the arithmetic is

The quantized dot products are still native:

    T ggml_vec_dot_q4_K_q8_K          0xf5140
    T ggml_vec_dot_q4_K_q8_K_generic  0x681c0
    T ggml_vec_dot_q6_K_q8_K          0xf5900
    T ggml_vec_dot_q6_K_q8_K_generic  0x68b30

They are defined in the library, and the Goose-generated CPU body contains no `vec_dot`,
`q4_K` or dot loop of its own — it selects and calls. That matches the census entry: the CPU dot
products cannot move because Goose has no SIMD, and the host's `ggml_vec_dot_q4_K_q8_K` carries
58 SIMD instructions against 91 for Q6_K.

## What this means for P1

P1-12 asks to close the CPU MoE subsystem with native compute disabled. That is not reachable
here for the same reason as the CUDA matmul families: the compute itself is the thing that
cannot move. What is ported is the orchestration around it, and that is measured, not assumed —
this note exists because the witness numbers alone would not distinguish "Goose computes" from
"Goose dispatches", and the difference is the whole question.

No code was changed in this pass.
