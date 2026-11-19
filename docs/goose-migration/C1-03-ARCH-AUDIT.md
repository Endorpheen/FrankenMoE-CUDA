# C1-03 audit: what Qwen reuses, what it decides, and one part that does not exist

Measured 2026-11-19 from the modules themselves. The requirement closes on evidence, not on a count.

## The three parts of the requirement

### 1. Quantized precision - reused, not forked

The Qwen modules declare no quantisation of their own:

| module | imports | exports | what it decides |
|---|---|---|---|
| `model/q4m-metadata.goose` | 0 | 19 | what the GGUF must carry (key names, model-type rules, layer counts from `qwen4exp.cpp:103-106`) |
| `model/q4t-tensor-map.goose` | 0 | 10 | which tensors exist and where the expert tensors live |
| `model/q4e-expert-layout.goose` | 1 | 15 | whether the routed experts and the attention projection are fused or split |
| `model/qwen4exp.goose` | 2 | 4 | the family constants and abort name |

None of them implements a quantisation kernel, a dequantiser or a matmul. `grep` finds no `q4_k`, `q6_k`
or `iq2` handling in them: the accuracy of the arithmetic stays with the shared ggml path and the shared
Goose quant modules. That is what "reuse common quant/math/controller modules" asks for, and it is
visible in the import counts.

### 2. Explicit fused verification windows - present

`q4e-expert-layout.goose` states the window in its own header: `src/models/qwen4exp.cpp:240-243` is the
fused attention QKV call, and the module decides - per model - whether the projection is fused
(`attn_qkv`) or split (`attn_q`/`attn_k`/`attn_v`), because "both are 'try the fused name first, fall
back'" in the native code. The decision is explicit and named, not implicit in a fallback chain.

### 3. Cold-bank - no counterpart in the native tree

`grep -riE "cold.?bank"` over `vendor/flint`, `runtime`, `integration` and `goose` returns **nothing**.
The native Qwen path this migration replaces has no cold-bank, so there is nothing to preserve and
nothing to port. It is recorded as non-existent rather than invented, which is the only honest option:
porting a feature that does not exist would create a branch nothing calls.

## No fork without a demonstrated differing contract

Nothing in the four modules forks the shared runtime: they are constants and decisions, they call no
kernel of their own, and they are compiled into the same single unit as everything else (156 modules,
0 duplicate globals). The one place Qwen genuinely differs - fused versus split attention naming - is
expressed as a decision with the native line numbers beside it.

## What this audit does not claim

It does not claim the Qwen path is *qualified*: no Qwen GGUF was loaded, no tensor shape was compared,
no inference was run. That is C1-05, and it remains open. C1-03 asks whether the integration forks the
common runtime to get Qwen working; the answer, with evidence above, is that it does not.
