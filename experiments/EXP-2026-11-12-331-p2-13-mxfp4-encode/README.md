# EXP331: MXFP4 encode in Goose -- first encoder gated against the artifact, not against a copy of the source

This is the first encoder slice in P2-13 that does not need `make_qkx3_quants` or `make_qp_quants`, and therefore the first one
that could be finished while that helper is parked. It is also the first encoder whose oracle is the **executed library**:
`quantize_row_mxfp4_ref` is exported, so the gate compares encoded bytes against it directly and no source copy is involved.

## What is ported

Per block: the absolute-maximum scan, `e = floor(log2(amax)) - 2 + 127` (zero when the block is all zeros), the halved E8M0
scale, then each of the sixteen output bytes packing two indices from the strict-less absolute-error search over the doubled e2m1
table -- strict less, so the first index wins ties, exactly as `best_index_mxfp4` does.

## Gate

    EXP331_MXFP4_ENC checks=3944 failures=0

Byte-for-byte comparison of the whole encoded block array against `quantize_row_mxfp4_ref`, over 256/512/2560/4096 elements,
with an all-zero trailing block and a block containing +/-1e-30 denormal inputs.

Mutation: changing the exponent bias from `- 2` to `- 1` produces 3910 divergences out of 3944; the edit was verified present in
the source first, and the gate was restored to zero failures afterwards.

## Why this matters for the parked helper

The parked signed-zero problem in `make_qkx3_quants` only matters if it changes bytes. This experiment demonstrates the gate style
that answers that question the same way for the K-quant encoder later: compare against the exported reference, not against a
transcription.
