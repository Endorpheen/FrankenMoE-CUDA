# EXP-2026-11-25-1955: Reproduce strong upstream placement on Inkling 8192/256

**Verdict: retain this upstream placement as the decode reference.**
The combined Goose build is faster for the full request and still
trails llama.cpp in decode. No runtime change was tested.

Pin the upstream executable at commit
`4c92c5bfb7cab395af9fc0fe0939e34ae5083345`. Auto-fit in EXP-1944
shifted several GiB between weights and temporary buffers on the
same nominal settings. Reconstruct the EXP-1927 stronger CUDA
placement from the recorded fit strategy (43 layers, 31 partial,
first partial layer with a CPU down projection). Disable fit and
use this fixed placement on the single RTX 4070:

```bash
--fit off --n-gpu-layers 43 \
  --override-tensor '^blk\.18\.ffn_down.*=CPU,^blk\.(19|[234][0-9])\.ffn_(up|down|gate_up|gate)_(ch|)exps=CPU'
```

It reproduced exactly 7675.91 MiB CUDA model weights and 968.43 MiB
CUDA compute scratch from EXP-1927. The first startup expired before
tensor loading completed. Reading the 14996735968-byte model file
from its local drive into the OS page cache took 286.49 s. The retry
loaded, then measured one fresh request per engine sequentially.
The request file and its 8192 input IDs, 256-output budget, seed,
sampling, context 36864, Q4_0 K/V and logical batch 4096 were shared.
Microbatch and weight placement are runtime-specific.

| Runtime | Ubatch | Prefill, tok/s | Decode, tok/s | Full request, s | Peak board, MiB | Peak RSS, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| upstream llama.cpp | 128 | 155.29 | 24.29 | 63.255 | 11278 | 15083244 |
| Combined Goose EXP-1953 | 4096 | 277.65 | 19.83 | 42.657 | 11706 | 14112060 |

All 256 output IDs and text match. Neither process swapped and
system swap-out was zero from before each launch through the end
of its request. During the request upstream recorded 20783104
bytes of storage reads and Goose recorded zero. This is a single
observation per engine, with separate server processes.

The Goose decode gap is 2359.19 ms over the request, or about
9.25 ms per inter-token step across 255 steps. Goose moved
27944497152 expert bytes across 6479 fills during decode, inferred
from total counters minus the after-prefill snapshot. That is about
109.59 MB per inter-token step. It measures traffic, with copy and
compute overlap still to be resolved on the critical path. The
next decode budget should measure the exposed copies, graph waits
and kernel spans for the combined binary at this workload.

See `results.json` for argv, env, SHA-256, fixture and resource
measurements. The raw directories are under
`build/inkling-apex-gate/upstream-1955`. The fixed command uses
the already built upstream binary under `/home/end0/AI/LlamaServer/`.
No Apex binary, host source or benchmark profile was changed.
