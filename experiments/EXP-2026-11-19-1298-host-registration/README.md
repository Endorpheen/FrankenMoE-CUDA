# EXP-2026-11-19-1298: the missing ten tokens per second were an accepted component absent from the compiled tree

## The question

The historical Apex reference decodes at 55.52 tok/s; the candidate measured 45.2. Was that a Goose
regression, or something else?

## The answer, from the artefact rather than the build

```
strings <reference libllama> | grep "mapped expert ranges"   -> 1
strings <candidate libllama> | grep "mapped expert ranges"   -> 0
```

The reference binary (EXP906) was built from `runtime/reconstructed/src/llama-model.cpp`, which registers the
resident mmap expert ranges with the backend. Every build under `builds/` compiles `vendor/flint/src/llama-model.cpp`,
which contains **no registration code at all**. Both compile cleanly; only one has the feature. Nothing in the Goose
port caused the difference.

That also explains an earlier registry line, "decode -0.7 % versus EXP906": it compared two builds of the **same**
composition, both missing the feature. Two equally slow builds agreeing with each other is not parity.

## What was done

The existing implementation was **connected, not rewritten**: `llama-mmap-expert-pin.h` copied verbatim from
`runtime/reconstructed/src/`, `pin_mmap_experts` moved with its body unchanged onto the vendored model class, the
three members added to its impl, and the call site re-established in the vendored context initialiser under the same
`host_buffer` capability guard. The reconstructed tree's own switch, `GGML_MMAP_EXPERT_PIN=0`, still disables it.

## Measured, on identical conditions and the same acceptance hash

```
feature ON  (default)   decode 62.42 62.90 62.79   prefill 2687 2952 2953   vram 9511 MiB  rss 20837548 kB
feature OFF (switch)    decode 42.63 43.91 44.15   prefill 1945 2190 2175   vram 9475 MiB  rss 20837332 kB
previous candidate      decode 45.04 45.99 45.06   prefill 1962 2265 2236   vram 9447 MiB  rss 20835956 kB
```

All four axes against the switch-off control:

| axis | off | on | change |
|---|---:|---:|---:|
| decode tok/s (median of three) | 43.91 | 62.79 | **+43.0 %** |
| prefill tok/s (median) | 2189.7 | 2952.3 | **+34.8 %** |
| VRAM | 9475 MiB | 9511 MiB | +36 MiB (+0.38 %) |
| RSS | 20837332 kB | 20837548 kB | +216 kB (+0.001 %) |

Against the historical reference (55.52 decode, 2625 prefill) the candidate is now **+13.1 %** decode and
**+12.5 %** prefill, with the response hash unchanged at `c2ff42cc1219`.

## Why the log line was not the evidence

The registration's `LLAMA_LOG_INFO` does not appear in the server log at its default verbosity, so the first check
was inconclusive and could have misled: the run that gained 17 tok/s had no message saying why. The switch A/B is
what proves causation, and it is why the feature is verified by an env-var A/B and by the presence check rather than
by a log message.

## What now enforces this

`tools/check-accepted-components.py` reads the built libraries and fails when an accepted component is absent. It
reported `host-registration` as the single missing one before this change and reports all five present after it. Rule
33 in `AGENTS.md` records the requirement: one growing cumulative build, connect what exists, and check the
artefact - not the compile.
