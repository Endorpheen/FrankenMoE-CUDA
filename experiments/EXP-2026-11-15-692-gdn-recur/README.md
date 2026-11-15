# EXP-2026-11-15-692 — the recurrent state is written and read back

Base: `e24c2730`. No model run, no GPU.

## 1. Result

```
token 0: conv n=8192 differs=0 | tail n=24576 differs=0 | slot_off=0 ok=1
token 1: conv n=8192 differs=0 | tail n=24576 differs=0 | slot_off=0 ok=1
FG_GDN_RECUR tokens=2 state_elems=24576 state_differs=0 state_nonzero=10120 PASS
```

Two tokens, where token 1's conv input is built from the state token 0 wrote. This is
the check a single-token gate cannot be: with a zeroed state the state path is an
identity, so every node can be bit-exact and the layer still not recurrent.

`state_nonzero=10120` is the part that matters. Equal-but-zero states would pass a
plain comparison while proving nothing; the state has to actually carry values
forward.

## 2. The bug this found, and why only this gate could find it

`gdn_conv_tail` computed the tail stride as `elem * channels`. In `conv_input` the
shape is `[kernel-1+T, channels]`, so **axis 0 is the kernel position and axis 1 is the
channel**: `nb[1]` is `elem * ne0`, not `elem * channels`.

The wrong view read a different 24576 elements. Every other node still matched - the
conv output at token 0 was bit-exact - because the conv output does not depend on the
tail view at all. Only comparing the tail, and then feeding it to a second token,
exposed it:

```
token 0: conv differs=0 | tail differs=11565     <- the tail was wrong
token 1: conv differs=7035                       <- and it poisoned token 1
```

After the fix, both are zero. The stagger is the point: the tail comparison catches it
at token 0, the state comparison catches the cascade at token 1.

## 3. What moved into Goose

| export | decision |
|---|---|
| `gdn_conv_input` | transpose the mixed output, concat the state at the front, axis 0 |
| `gdn_conv_tail` | which kernel rows become the next state, and the stride of the channel axis |
| `gdn_conv_slot` | `(slot * mem_size + kv_head) * row_size` into the flat state tensor |
| `gdn_conv_store` | that the tail goes into the slot at all |

`gdn_conv_tail` reads `ne(conv_input, 0)` for the row count instead of taking it as a
parameter, so the axis cannot be confused again by a caller passing the wrong number.

## 4. Two gate defects, neither in the module

1. `ggml_cpy` creates a node with `view_src` set to the destination. With a
   host-allocated context the copy lands in the node's own buffer and the state is
   never touched; with the backend allocator it aliases correctly, but tensors created
   after `ggml_backend_alloc_ctx_tensors` have no buffer and fault. The gate therefore
   compares the tail and writes it into the state directly, which is exactly what `cpy`
   does, and leaves the allocator's aliasing out of the claim.
2. Missing `extern` declarations for the Goose exports made the compiler assume `int`
   returns, truncating 64-bit handles - an immediate segfault on first dereference.

## 5. What is still not proven

A state that survives **between calls**. This gate keeps the state in the driver's
buffer and hands it back each token; the runtime still builds its graph with fresh
tensors per call. That is runtime structure, and it is the same shape as the attention
KV cache - so both caches should be one mechanism, as `docs/goose-migration/STATE-BUFFERS.md`
already says.

## 6. Cost

Two arch-unit builds in this session (the exports, then the tail fix). The first was
one build for four new exports; the second was justified by a correctness defect, not
by preference. Gate compiles are seconds.
