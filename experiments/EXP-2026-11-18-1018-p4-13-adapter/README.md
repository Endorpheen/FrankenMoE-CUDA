# EXP-2026-11-18-1018 — P4-13 adapter, Goose side (compiled skeleton)

`src/adapter.goose` declares the five unified operations on top of the existing
allocator primitives (`fg161_alloc_buffer`, `fg161_free_buffer`,
`fg161_reset_buffer`, `fg161_usage`, `fg161_tensor_alloc`) and carries the
generation guard from `P4-13-CONTRACT.md` §8.

State: it compiles with the qualified compiler `8b7a56bb…`
(`build_adapter.c`, 45 884 bytes). The generation itself still has to live in
the host record: `gen_of` is a placeholder and `place` therefore accepts
generation 0 for any buffer. The host unit and the real generation store are
the next step, followed by the differential gate against
`experiments/EXP-2026-11-18-1014-p4-13-alloc-gate/reference-native*.txt`.

## Host unit (EXP-2026-11-18-1020)

`host/fg161_host.c` implements the five primitives the Goose adapter imports —
`fg161_alloc_buffer`, `fg161_free_buffer`, `fg161_reset_buffer`,
`fg161_set_usage`, `fg161_tensor_alloc` — over the real ggml buffer API, with a
per-buffer generation and a live table. It compiles clean with `-Wall -Wextra`
and its functional test (`host/host_test.c`, output `host/host-reference.txt`)
gives: alloc handle 1 gen 0 live 1; reset bumps gen to 1; free drops live to 0
and the handle's generation query returns 0 (dead); re-allocation reuses the
handle. Next: bind the compiled Goose skeleton to this unit and run the
differential gate against `evidence-.../reference-native*.txt`.
