# EXP-2026-11-18-1192: fixed seed and logprobs on a real model

Three requests with `seed=12345`, `temp=0.9`, `top_k=40`, `top_p=0.95`, `min_p=0.05`,
`n_predict=48`, on Apex-35B Q4_K_M, each in a fresh server:

```
ON  run1: 1c96db449bb8ce64 179 ' Paris. The Eiffel Tower, one of the mos'
ON  run2: 1c96db449bb8ce64 179 ' Paris. The Eiffel Tower, one of the mos'
OFF run1: 1c96db449bb8ce64 179 ' Paris. The Eiffel Tower, one of the mos'
```

The hash is SHA-256 of the completion text, first 16 hex digits. Two things at once:

- **reproducible**: two separate processes with the chain on produce the same bytes;
- **equivalent**: the same seed with the chain off produces the same bytes as with it on.

## Logprobs, compared field by field

With `n_probs=3` the response carries `completion_probabilities`; every entry's `id`, `logprob`
and `top_logprobs` were compared:

```
entries: 8 8 identical: True
first entry ON:  {'id': 11751, 'logprob': -0.018532}  top: [(11751, -0.018532), (264, -5.627563), (198, -5.69518)]
first entry OFF: {'id': 11751, 'logprob': -0.018532}
```

## A harness defect on the way, the same kind as before

My first extraction looked for `tok_str` and `probs`; the server's fields are `token`,
`logprob` and `top_logprobs`. The comparison printed two lists of empty entries and **looked
green** - an empty result agreeing with an empty result. That is the "empty output is never
proof" lesson arriving again from a new direction, and it was caught only by dumping the JSON keys
instead of trusting the shape I assumed.

Real calls in the same run, from the server's own log:

```
FG171_TOP_P goose calls=1 kept=1
FG171_DIST  goose calls=1 sel=0
```
