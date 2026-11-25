# EXP-2026-11-25-1983 — Share staged KV across Bonsai Flash tiles

**PROVEN for bit-identical attention and short prefill acceleration.**
Experimental mode pending decode and memory qualification. The accepted
Bonsai reference is unchanged.

The control expands each Q4_0 fragment inside every CTA. Candidate B
skews the shared Q/K and V strides to spread tensor fragment reads over
more banks. Candidate C also converts resident Q4_0 K/V once per layer
call into scratch buffers shared by all query-tile waves. K uses key-major
half pairs and V is transposed within each 32-key block. The resident KV
format and FP32 accumulation, softmax and reduction order stay the same.

## Component results

RTX 4070, seeded input, same independent frozen Q4_0 comparison as
EXP-1982. Each arm's mean is the average of two CUDA event brackets, in
A–B–C–C–B–A order. Staging and all tile/merge launches are included.
All five forms produced byte-identical attention output in the three
FP32 variants. Relative L2 against Q4 tiles remained below 0.00030.

| KV rows / query tokens | FP32 direct, ms | Bank skew, ms | Staged KV, ms |
| --- | ---: | ---: | ---: |
| 65 / 16 | 0.053 | 0.046 | 0.068 |
| 257 / 33 | 0.143 | 0.100 | 0.054 |
| 8192 / 128 | 15.187 | 13.653 | 6.034 |
| 32768 / 128 | 66.751 | 60.421 | 29.285 |
| 1024 / 1024 | 6.151 | 4.504 | 2.019 |

At 32768/128 the staged variant cuts the current FP32 component time by
56.1%. Dividing the previous 220.944 ms Q4 measurement on that fixture
by 29.285 ms gives about 7.54x, a cross-experiment component comparison.
EXP-1981's 3.63 ms diagnostic remains the speed target for this shape.
The unchanged long-shape relative L2 is 0.0002864.

## Short model A/B

RTX 4070, 4097/16, context 36864, ubatch 1024, Q4_0 K/V, same order
A–B–B–A as prior full-request testing. Both modes start from an empty
cache, use the first 4097 raw input IDs in EXP-1977, and retain EXP-1980
decode and the EXP-1982 FP32 tensor-core tile/merge. Greedy, EOS masked,
no MTP. A uses direct FP32 Flash and B uses the staged variant.

The candidate saved 962.961 and 786.977 ms prefill in the two pairs.
All 16 token IDs and the selected logits matched byte for byte. The
runtime allocates up to 144 MiB of shared K/V staging workspace at the
configured context, reused across layers, releasing it with the CUDA
context. It falls back to direct Flash beyond the staging capacity.
Decode was unchanged in the first pair within 0.3 ms and slower by
56.2 ms in the reverse pair. The cause of this timing variation is
unverified. Peak process RAM and device residency over time were not
sampled. No long model benchmark was started. Keep staged KV opt-in
until the decode and memory gates are reproduced.

A next kernel change should expand reuse across query rows and reduce
the current 41,216 B per-block shared allocation and register pressure.
Preserve the stage/compute accounting in the component comparison and
rerun both output gates. The compatibility, long-profile and +70% goal
against PrismML still remain open.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1983-bonsai-flash-reuse
bash "$exp/source/build.sh" build/bonsai-flash-reuse-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-reuse.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-flash-reuse-repro/flash-reuse.cubin build/bonsai-flash-reuse-repro/frozen-goose.ptx
```

Use `bonsai-flash-reuse` and `flash-reuse.cubin` with
`BONSAI_FLASH_F32=1 BONSAI_UBATCH=1024`. Select B with
`BONSAI_FLASH_REUSE=1` and A with `BONSAI_FLASH_REUSE=0`.
[Component data](component.json). [Short model data](results.json).
