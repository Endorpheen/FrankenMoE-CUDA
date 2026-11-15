# EXP-2026-11-15-667 — P1-12 integration: the CPU MoE block dots route to the Goose SIMD kernels

Base: `7bcfb638`. Build and gate only. No model, no server, no GPU.

## 1. Hypothesis

The accepted delivery (EXP665) exposes four single-block kernels over a C ABI.
If they are put behind one runtime switch on the CPU MoE path, with the native
kernels as the OFF arm, then

> every routed call yields a bit-identical fp32 result and the same integer
> accumulator as the native kernel, the switch can be taken back at run time
> without a rebuild, and a multi-block row is never handed to a one-block kernel.

## 2. The one hard limit, and why the guard is not an optimisation

The delivered entry points take **one block** per call: 256 elements for the
K-quants, 32 for `q8_0`/`q4_0`. ggml calls a row kernel with `n = nb * 256` and
accumulates across blocks in a fixed single-precision order.

A one-block fast path is therefore only valid when `nb == 1`. For `nb > 1` the
native loop runs unchanged, which is what preserves both the accumulation order
and the fp32 result bit for bit. Forwarding a multi-block row to a one-block
kernel would silently drop every block after the first — a wrong answer, not a
slow one — so the bridge refuses `nb != 1` and the gate asserts the refusal.

This is the honest scope of the delivery: it makes the single-block MoE dots
Goose-owned, and leaves the multi-block row loop native until the compiler
surface can express the row loop. `ACCEPTANCE.json` of the delivery says the
same thing from the compiler side.

## 3. What was added

| file | role |
|---|---|
| `integration/p1-12/src/fg-goose-cpu-simd.h` | the switch contract: `fg_goose_dot_{q4_k_q8_k,q6_k_q8_k,q8_0_q8_0,q4_0_q8_0}`, `fg_goose_cpu_simd_{active,available,calls}` |
| `integration/p1-12/src/fg-goose-cpu-simd.cpp` | the switch: weak references to the delivered object, `dlopen("libfg-cpu-simd.so")` fallback, `FG_GOOSE_CPU_SIMD_NATIVE` as the only disable |
| `integration/p1-12/tests/switch_gate.cpp` | differential gate against `ggml_vec_dot_*_generic` |
| `integration/p1-12/tests/run_switch.sh` | builds and runs both arms |

Rule 10 is applied as everywhere else: unset means the Goose kernel answers, and
the environment variable exists only to take the native arm back. The value is
read once per process, so a request never pays for `getenv`.

Failure policy: the delivered object is referenced weakly. A build that does not
link it stays on the native arm and reports `available=0`; a missing kernel can
never become a zeroed dot product.

## 4. Result

| arm | cases | failures | `goose_calls` |
|---|---:|---:|---:|
| 128 iterations, switch active | 513 | 0 | 512 |
| 512 iterations, switch active | 2 049 | 0 | 2 048 |
| 1 024 iterations, switch active | 4 097 | 0 | 4 096 |
| 2 048 iterations, switch active | 8 193 | 0 | 8 192 |
| any size, `FG_GOOSE_CPU_SIMD_NATIVE=1` | 1 | 0 | 0 |
| multi-block row (`nb=4`) | — | 0 | refused, output untouched |

Every routed case compares the bridge's fp32 output with
`ggml_vec_dot_*_generic` bit for bit. The native arm runs the same binary with
the switch disabled and reports `goose_calls=0`, which proves the routing is real
and not a coincidence of equal outputs.

## 5. Why this is the correct acceptance shape

The compiler owner proved the kernels; EXP665 proved them again against this
repository's native sources; this experiment proves the *switch*, which is the
part the runtime owns. The three proofs are disjoint: a defect in the kernel, in
the comparison, or in the routing each fails a different one.

The gate links the shipped bridge object and the delivered object as they are.
It does not compile a copy of the wrapper and does not stub a kernel.

## 6. Scope and remaining P1-12 work

Closed here: the four single-block dots are switchable and default to Goose.

Still open, and not claimed:
1. The multi-block row loop stays native. Covering it needs a row-level Goose
   entry point, which the delivery does not have.
2. NEON/SVE/AVX512/VNNI are not delivered; the scalar fallback is.
3. No performance measurement exists, so no speedup or regression is claimed.
   The A/B that would measure it needs Igor's approval.
4. The GPU MoE path is untouched; this is the CPU backend only.

## 7. Reproduce

```
bash integration/p1-12/tests/run_switch.sh 1024
```

The script refuses a `blocks.o` whose SHA-256 differs from the accepted
delivery, then prints both arms.
