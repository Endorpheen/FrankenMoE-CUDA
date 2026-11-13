# EXP400: the slot file is byte-identical across two runs

## Question

Do the Goose-owned metadata, layout and payload paths write the same bytes every time, on the same input?

## Method

Two ordinary runs of the reconstructed tree on the fixed profile, each saving the slot, and a comparison
of the file digests. No switch changes between runs.

## Result

| run | slot size | sha256 |
|---|---|---|
| 1 | 387 989 920 | `3e216c49941d30998115a8b41909a59619dd934bc94ea4d6db07b724f1b6f8a1` |
| 2 | 387 989 920 | `3e216c49941d30998115a8b41909a59619dd934bc94ea4d6db07b724f1b6f8a1` |

**Identical.** Both runs also produced completion `e68df05b` with equal probabilities and an exact
restore.

## Why it matters for the three items

A per-span offset error, a different sequence-selection rule or a different delegation order can each
produce a file of the same size with different bytes. Byte identity across independent runs means the
whole write path — KV metadata selection (EXP393), KV layout (EXP394), recurrent payload spans (EXP396)
and the hybrid delegation order (EXP397) — is deterministic and reproducible, not merely "restores fine".

## Stronger: the file does not depend on our switches

The same digest `3e216c49941d30998115a8b41909a59619dd934bc94ea4d6db07b724f1b6f8a1` appears in four separate
directories produced under different switch settings:

| directory | switches |
|---|---|
| `build/run-restore` | everything on |
| `build/run-apex-save` | everything on |
| `build/run-ours-off` | our earlier routes at 0 |
| `build/run-disabled` (`good.bin`) | earlier disable-contract run |

So turning the Goose routes off does not change a single byte of the state file. That is a stronger
statement than run-to-run determinism: it means the ported decisions reproduce the native file exactly,
because the native path and the Goose path agree.

## The accepted binary writes the same file

The blocker below was solved, not accepted. The accepted DSO only needs the goose ggml build that lives in
`runtime/run`, which its own run directory does not ship:

    LD_LIBRARY_PATH=/…/runtime/run:/…/runtime/exp244/measured-apex-or244-or-goose/run \
    LD_PRELOAD=/…/…/run/libllama.so.0  llama-server … --slot-save-path …

With that, the **accepted runtime** writes

| writer | size | sha256 |
|---|---|---|
| reconstructed tree (Goose routes on) | 387 989 920 | `3e216c49…` |
| accepted runtime `libllama.so.0` | 387 989 920 | `3e216c49…` |

**Byte-identical.** Both also answer `e68df05b` and restore exactly. So the Goose metadata selection,
layout arithmetic, recurrent payload spans and hybrid delegation reproduce the accepted binary's file
bit for bit, not merely "restore fine".

## The blocker as it stood



Byte identity against a file written by the **accepted runtime binary** is still not measured. I tried:
`runtime/exp244/measured-apex-or244-or-goose/run/libllama.so.0` does not start in that directory —

    symbol lookup error: libllama.so.0: undefined symbol: ggml_backend_sched_set_copy_callback

and that symbol is defined by none of the `libggml-*.so` files present there. So the accepted binary needs
a ggml build that is not in its own run directory. Until that library is found or rebuilt, the comparison
against the accepted file cannot be made, and I record it as open rather than pretending the on/off
agreement covers it.
