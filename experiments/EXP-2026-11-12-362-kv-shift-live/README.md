# EXP362: continuing past a small context -- outputs match, but the shift port did not run

Two prompts generating far past a small context, so the cache must make room to continue.

| context | prompt | n_predict | native | candidate |
|---|---|---|---|---|
| 64 | `Hi` | 200 | `200 tokens, limit` | identical, full content compared |
| 64 | `The quick brown fox` | 400 | `242 tokens, limit` | identical |
| 128 | `Hi` | 200 | identical | identical |
| 128 | `The quick brown fox` | 400 | identical | identical |

Full content compared, not just the token count: zero differences across four shapes.

## But the port was not exercised

The claim I wanted was "the Goose KV shift agrees with native". It is not supported: the shift witness lives in the experiment's `bridge.c`, which is not
linked into this build, so no witness appeared. A `gdb` breakpoint on `fg230s_fill_shift` (the entry point that exists in the library) stayed
**pending** through a 150-token generation at `-c 64`, which means the call site was never reached.

So what this experiment shows is that continuing past a small context behaves identically between the builds. It does **not** show that the shift port
executes, and it is not evidence about that port's correctness. The earlier session note that shift ownership was deferred is consistent with this:
the native path is doing the work.

## Next step for P5-07

Find an input that actually reaches `llama_kv_cache` shifting -- the call site is guarded by `fg230s_enabled() && n_stream <= 64` and a `fits`
condition -- and only then compare. Until such an input exists, P5-07's shift claim stays where it is.

## Why the shift path is unreachable here

`set_input_k_shift` is only called when the graph carries a k-shift input, which happens when a context shift is actually applied. Two attempts to reach
it failed:

- **server**: generating 150-200 tokens past a 64-token context makes the server stop at the limit rather than shift, so the call site is never
  reached (breakpoint stayed pending);
- **CLI**: `llama-cli` is not present in `build-cuda/bin`, and gdb reported "No executable file specified", so a CLI run was not possible at all.

So with the binaries available, the shift port cannot be exercised. That is a statement about the tooling, not about the port: the port is neither
proven nor disproven, and P5-07's shift claim cannot move until something can drive a context shift -- a CLI build, or a server option that shifts
instead of stopping.

It also explains why the earlier session note deferred shift ownership: the code path is dormant in every configuration this project runs.
