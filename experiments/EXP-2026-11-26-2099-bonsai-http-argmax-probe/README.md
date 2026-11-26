# EXP-2026-11-26-2099 — Device greedy choice probe for Bonsai HTTP decode

**Component verified.** EXP-2034's existing two-stage device argmax
was appended to EXP-2096's device image in an isolated build.
The current HTTP generation interface and retained checkpoint
were unchanged.

EXP-2098 identified CPU greedy choice and index checks in the
GPU gaps. The current native eval ABI requires a full logits
buffer before the Goose sampler checks its request settings and
selects a token. The result of device choice must have a separate
path in that ABI. Fabricating a sparse logits buffer would change
the contract for probability output and non-greedy samplers.

## Component evidence

RTX 4070, SM89. The original EXP-2034 edge fixture passes.
The extended probe reads 36 real full-vocabulary rows saved
from EXP-2096's CodeTrace control (FP16 FFN down) and adds three
finite-tie, EOG or nonfinite rows. Each test checks both masked
and unmasked selection. Results include 76 exact finite choices
with winner logit bits matching the host oracle, and two
nonfinite fallback flags. The EOG-mask test includes a synthetic
row in which the unmasked winner is blocked. Exact first-ID
tie breaking covers ties across CTA boundaries and signed zero.

CUDA event observations cover the full 993,280 B synchronous
readback to a pageable buffer, versus the two CUDA reductions
and a 12 B synchronous readback. The input is already resident
on device. Include both reductions and data transfer in each
candidate observation. The CPU scan is outside this timing.

| Resident-logit component | Median |
| --- | ---: |
| Full readback | 107.04 µs |
| GPU choice and record readback | 19.89 µs |

The isolated difference is 87.15 µs. It must not be multiplied
into a request gain without measuring the actual queue, CUDA
wait and CPU scan behavior. No tuned model run was performed.
The available comparison stays at 1094.19 prefill / 37.82
decode tok/s for EXP-2096 and 1095.12 / 48.08 for PrismML.

## Runtime integration requirements

Use request-level eligibility in the Goose generation loop:
one beam, greedy without typical filtering or repeat penalties,
and no requested probability rows. Pass EOS masking explicitly.
A native Bonsai choice call can return a distinct unavailable
result to preserve the existing full-logit path for other
architectures and requests. For an eligible final prefill or
decode step, execute the original graph and launch choice on its
resident vocabulary output before any host logits copy. If the
device flags nonfinite logits or no valid token, copy the real
logits and use the exact legacy sampler, including its error
semantics. Never advance the model state twice on fallback.

Preserve the existing completed-evaluation position and
request timing boundaries. First test ties, EOS on/off,
probability-reporting fallback and non-greedy fallback through
the HTTP path. Then compare a cold 32K/512 on an isolated
Bonsai package with the same host and image in both arms. Shape,
raw prompt IDs, sampling and the full runtime graphs are
required evidence for that subsequent experiment. Apex and
Inkling must retain their current packaged artifacts.

## Reproduce

Use the EXP-2096 build directory, Clang 18 and CUDA ptxas for SM89.
The probe accepts the EXP-1973 bootstrap directory and GGUF
inventory, plus the saved EXP-2096 control CodeTrace logits.

```bash
exp=experiments/EXP-2026-11-26-2099-bonsai-http-argmax-probe
out=build/bonsai-http-argmax-probe-2099
bash "$exp/source/build.sh" "$out" build/bonsai-ffn-down-fused-q8-2096
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" CORE_DIRECTORY INVENTORY "$out/pipeline.cubin"  build/bonsai-ffn-down-fused-q8-2096/code-trace/code-trace/f16-logits.bin
```

[Measured observations](data/component.json), [original edge tests](data/edge-cases.json),
and [build and fixture hashes](data/build.json).
