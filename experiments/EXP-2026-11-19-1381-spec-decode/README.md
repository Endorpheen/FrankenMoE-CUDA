# EXP-2026-11-19-1381: speculative decoding measured, and a decode difference that is not about speculation

## The first two configurations

Same model, same request, one 512-token generation each, temperature 0, seed 1234:

```
base-none   decode 77.97 tok/s   vram 9518 MiB   rss 20765256 kB
ngram       decode 74.38 tok/s   vram 9521 MiB   rss 20765924 kB
```

N-gram drafting is **4.6 % slower** on this request. The workload is a fresh paragraph with almost no repetition, which
is the case ngram drafting cannot help: it pays for proposals and accepts few of them.

That is one request per configuration, so it is a direction, not a verdict - the card says so rather than promoting it.

## The number that matters more

Both of these ran at `-c 4096 -b 2048 -ub 2048`, and gave **77.97 tok/s**. The acceptance profile runs at
`-c 262144 -b 4096 -ub 4096` and gives **62.7-63.2 tok/s**. Same model, same hardware, same 512 generated tokens, a
**24 % difference** that has nothing to do with speculative decoding.

Two candidate causes, and they are separable by one run each:

1. the context size itself - a 262 144-slot KV cache is allocated and touched differently from a 4096-slot one;
2. the batch and micro-batch (4096/4096 against 2048/2048), which changes how decode batches are formed.

The next two runs isolate them: the first is this harness with `-c 262144`, the second with `-b 4096 -ub 4096` and the
original context. Whichever reproduces the loss names the cause.

## Why this is the right question

If a decode rate of ~78 tok/s is reachable at a large context, that is a 24 % gain on the record baseline without
touching speculative decoding at all - and it would change what a speculative gain is measured against. If the loss is
inherent to the 262 144 context, then the acceptance profile is already at its ceiling and speculation is the only
remaining lever.

## Not claimed

Nothing here is an improvement yet. Both runs are one request, the prefill figure (57 tok/s) is a twenty-token prompt
and is not comparable with the acceptance prefill, and no correctness check was made in this script - the hash check
belongs to the acceptance profile, which these runs deliberately do not use.
