# What in the candidate is actually Goose

Written after the P7-08 finding, because that finding generalises. Every "parity" result is only evidence about the migration when the two
sides are different implementations; when the component is native on both sides, parity is identity measured against itself.

The test is mechanical: `gg_function_N` symbols exist only in Goose-emitted objects, and `fg1xx` references exist only where generated code
calls a hand-written bridge. Measured on the shipped stage, with symlinks resolved:

| component | size | `gg_function_` | `fg1xx` refs | reading |
|---|---:|---:|---:|---|
| `libggml-cuda` | 220.7 MB | 56 | 1584 | mostly native kernels, Goose integration around them |
| `libllama-common` | 6.5 MB | **0** | **0** | entirely native: chat templates, minja, sampler helpers, console |
| `libllama` | 5.5 MB | 35 | 34 | Goose in 35 functions plus bridges |
| `libggml-cpu` | 1.9 MB | 149 | 310 | large Goose share of the CPU backend |
| `libggml-base` | 1.2 MB | 95 | 314 | large Goose share, five native primitives |
| `llama-server` | 17 KB | 0 | 0 | thin wrapper; its logic lives in `libllama-common` |

## What this changes about earlier claims

- **Sampler differential (EXP351)**: the sampler chain is inside `libllama`/`libllama-common`; the sampler object carries no generated code,
  so identical sampling is identity, not evidence of a port.
- **Template rendering (EXP352)**: `libllama-common` has zero Goose symbols, so every rendering equality is the same native Jinja engine.
- **The "matmul matches native" argument in P1-07** was already declared identity; the same reasoning now applies to two more surfaces that
  had been described as parity.

## What it does not change

`libggml-base`, `libggml-cpu` and `libllama` do contain substantial Goose code -- 95, 149 and 35 generated functions -- so the differentials
that run through those paths are genuine. The Apex milestone, the expert/cache work, the tokenizer and Unicode bridges, and the CPU operator
contracts all live there.

## The two large native masses

`libggml-cuda` is 220 MB of mostly native kernels (P3-20's census counted 144 native `.cu.o` objects plus one generated module), and
`libllama-common` is 6.5 MB of native chat/sampler support with **no** Goose code at all. Those are the two places where "port it" means a
project rather than an experiment, and neither has been started.
