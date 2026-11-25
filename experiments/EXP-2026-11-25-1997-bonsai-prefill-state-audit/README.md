# EXP-2026-11-25-1997 — Locate Q4 cache effects in the Bonsai residual divergence

**DIAGNOSTIC, no runtime promotion.** The FP16 control and EXP-1996
Q8 residual candidate reproduce the first divergent ID at output
position 196 of the code-trace question. Q4 cache rows differ as
early as the first full-attention layer. Intervention establishes
that prefill K/V payloads contribute to the selection difference.
The two payloads interact and do not explain the entire logit delta.

## Conditions and capture

RTX 4070, 440 prompt IDs from the exact EXP-1996 chat fixture, context
36864, ubatch 1024, Q4_0 K/V, EOG enabled, fixed 195-token FP16 output
history. Each runtime evaluates the 439-token bulk prefill and a
final scalar prompt token. The audit executable reads FP32 K and V
inputs and actual Q4_0 payload bytes for both cases. It samples four
evenly spaced hidden rows per layer and eight state columns from
three GDN heads. It does not capture every hidden or GDN element.
The final same-history vocabulary vectors contain all 248320 logits.
Audit adds copies and synchronization, so times are not scored.

The two paths use separate fresh contexts. The three interventions
run the residual path with no other arithmetic change. Immediately
after each prefill Q4 writer and before the next consumer, they
supply selected K/V bytes from the FP16 trace at the exact same
position and layer. A per-record header validates kind, position,
count, layer and length. Decode cache writes and the recurrent
state follow the original candidate. These interventions depend
on reference data and are diagnostic only.

## Measured state differences

For the bulk prefill, there are 224,768 Q4 blocks in each of K and V
across the 16 full-attention layers. K changes 187,581 blocks and
243,199 packed codes. V changes 193,034 blocks and 338,352 packed
codes. A changed block can have a changed stored scale, a changed
code, or both. Across 7,192,576 coefficients this is about 3.38%
changed K codes and 4.70% changed V codes. The count covers only this
request and microbatch.

The first code changes appear at full-attention layer 3, zero-based.
One K element at token 3, head 1, block 7, lane index 31 has input
0.219808 versus 0.220352. Using the writer's signed peak scale on
each corresponding block, its F32 pretruncation operands are
8.999076 and 9.000335. The data capture shows nibble 8 versus 9,
with the same stored F16 scale. In V at layer 3, token 0, head 0,
block 1, lane index 26, pretruncation operands are 12.998262 and
13.000405 and the captured nibble is 12 versus 13. These comparisons
establish discrete changes at the Q4 boundary. They do not establish
that this is the first numerical perturbation anywhere in the model.

Layer 0 sampled hidden-row relative L2 is at most 4.46e-5. In layer 3
it reaches 3.42e-4 on the sampled rows. At layer 47 the largest of
those four row differences is 9.98e-2. Layer 63 reaches 9.63e-2.
These are sample maxima, not a global average or a claim that the
error grows monotonically. The raw per-layer measurements include
the sampled GDN outputs as well.

## Interventions at output position 196

| Input path | Selected ID | Logit of 16 | Logit of 17 | Max absolute delta vs FP16 | Centered relative L2 |
| --- | ---: | ---: | ---: | ---: | ---: |
| FP16 | 16 | 18.9089 | 18.8187 | 0 | 0 |
| Residual | 17 | 18.7016 | 18.8476 | 0.2823 | 0.02853 |
| Residual + reference K | 17 | 18.7994 | 18.8033 | 0.1400 | 0.01463 |
| Residual + reference V | 16 | 19.0056 | 18.8620 | 0.2209 | 0.02403 |
| Residual + reference K and V | 17 | 18.8742 | 18.8834 | 0.1215 | 0.01513 |

Replacing only V restores the selected ID on this history. Replacing
both lowers the maximum error further and still leaves a different
winner. K/V and later state therefore matter jointly, and the token
decision cannot be predicted from one norm. Recovering that single
ID with an external reference does not establish an equivalent
standalone INT8 path. The FP16 default remains in place. There are
no changes to shared Apex or Inkling sources or device images.

## Reproduction

Use a built EXP-1996 directory and its saved code-trace fixture with
the measured fixed-history boundary.

```sh
exp=experiments/EXP-2026-11-25-1997-bonsai-prefill-state-audit
base=build/bonsai-mmq-residual-1996-runtime
out=build/bonsai-state-audit-repro
model="$HOME/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf"
bash "$exp/source/build.sh" "$base"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-audit.py" \
  "$base" "$model" "$base/quality/code-trace" "$out" fp16 residual both k v
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/analyze.py" "$out" > "$out/analysis.json"
```

The raw state, reference bytes and full logit arrays remain local at
`build/bonsai-state-audit-1997`. The raw bytes have hashes in
[evidence.sha256](evidence.sha256). [Per-layer and intervention data](analysis.json).
