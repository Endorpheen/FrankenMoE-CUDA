# EXP-2026-11-19-1400: the GGUF check now compares floats and booleans, not only integers

## What was missing

The reader verification compared integer keys, string keys and every array element, but **stopped at integers**:
`GGUF_TYPE_FLOAT32`, `GGUF_TYPE_FLOAT64` and `GGUF_TYPE_BOOL` fell through a `default: continue`. So the value classes the
runtime actually reads for sampling defaults, epsilon and the recurrent-layer mask were never compared at load.

## What was added

Floats are compared **bit for bit** through `gr_meta_f32_bits`, not by value: two different NaN payloads or a sign of zero
would pass a `==` comparison and fail a bit comparison, and the runtime's behaviour follows the bits. FLOAT64 is narrowed
to float32 first, because that is the accessor's type, and compared in the same way. Booleans are compared as 0/1.

## The result on Apex

```
FG_GGUF_VALUES  integer=34/34        <- was 26/26
FG_GGUF_ARRAYS  elements=744231/744231
```

Eight more values are now verified before use, all equal, and the harness completes normally afterwards.

## Why this matters beyond the count

`qwen35moe.attention.layer_norm_rms_epsilon` and `qwen35moe.attention.recurrent_layers` are float and bool arrays
respectively - the second one decides which layers take the recurrent path. A reader that disagreed on those would change
what the model computes while every integer key still matched.

## Shipped path

The check runs under `FG_GGUF_DIAG`; with it unset nothing changes. Acceptance afterwards: decode 62.74 / 63.18 / 63.28, prefill 2690.3 / 2947.6 / 2949.4, hash `c2ff42cc1219`.
