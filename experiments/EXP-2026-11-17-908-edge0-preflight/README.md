# EXP-2026-11-17-908: Edge0 pack integrity and runtime preflight

Status: **PREPARATION ACCEPTED; INFERENCE NOT IMPLEMENTED**.
Base: `82381b54` (EXP907), branch `codex/edge0-120b-preflight-20261117`.
No model/server was launched and the immutable Apex bundle was not modified.

## Hypothesis and acceptance

The two .part files may have reached their published sizes without being
complete. Before touching the loader, verify their entire byte content against
Hugging Face and identify the actual architecture, pack layout and memory
requirements. Accept preparation only when the pinned revision and all required
file hashes are known, structural checks pass, and missing execution paths are
explicit. This is not acceptance of Edge0 inference or a performance claim.

## Download integrity

Hugging Face main inspected on 2026-11-17:
`ad5ea45a67ab6cd6761c364793bebb363a69720c`. The local D/U projection files
match the older published revision
`dab5ce727d494e2018626fe8896bfdc774215b17`, not current main. The complete
68-file text-pack inventory **matches that older revision**, including the
tokenizer, scales, profile, MTP files and all weights. No corruption is claimed.
The revision and expected LFS SHA256 / Git-blob SHA1 values are pinned in
`tools/edge0-pack-manifest.json`.

| File | Bytes | Verified SHA256 |
|---|---:|---|
| experts.bin | 28626124800 | ffa87de634deb4d0a65b82b06748aef2cbe649838e20f239730cc62a0e3c7c80 |
| ple-fp8.gguf | 51200246112 | c7615c59b060277ddf6f84729fa8f7469cfc82c7fb983d6e7eab5a2f7446f170 |
| model-skeleton.gguf | 5991569248 | ab49fbe7590cdcda45d7a2bcae4be0bd42c6a70b951d594a00eb158d7df0b72f |
| dense.bin | 1538625024 | f9bcc7f36eac4e59a7b1cb1962e038ba12dc831a41fb193761f1d1e72657fb62 |
| latent_dense_q4.bin, local revision | 1415577600 | 4824fb68c5dceb94c33bae0fda3801b1f26f65eb8b9ea0dfac0450fc2bfa41b5 |
| latent_dense_q8.bin, local revision | 2673868800 | fec3adc0b7b3b0798127550951e541823a0c96ca136911332ee050e39c302c91 |

Current-main D/U SHA256 values are respectively
`9ec1974b83bd4bcf90f8bc71ac529ced61d40ca934952b72d65de8515a3fd50c` and
`1e82f0e02545734f3f5915aaf29b47e91b75f7080edca1d5e89ab437637a5521`.
Their sizes did not change. The HF commit titles only say upload; the reason for
the replacement is UNKNOWN. Do not mix current D/U into the pinned pack without
a separate compatibility check. Updating only Q4 would require about 1.32 GiB,
not redownloading the whole model, if later justified.

One sequential streaming hash pass took 91.3 s and used an 8 MiB read buffer.
92 locally present HF files were considered: 88 matched current main; README
and .gitattributes differed in size, and both D/U files differed in hash. The
same recorded digests then matched all 68 required files of the pinned original
revision; no repeat read of the large weight files was needed.

After full SHA verification and checking unchanged sizes/mtimes, the two .part
files were renamed to their canonical names without copying or overwriting an
existing destination. Their inodes were preserved. No weights were downloaded.

## Architecture: verified metadata, not the model name

The skeleton is GGUF v3, **general.architecture=qwen4exp**, 1079 tensors,
data offset 11012448. Its actual types are BF16 (484 tensors), F32 (292), Q8_0
(302) and F16 (one). The historical general.name string "Nvfp4 Src" does not
describe these tensor types. The external expert pack is separately mixed Q3_K
gate/up and IQ4_NL down. No evidence establishes an architecture named QN4.

Metadata: 48 blocks, hidden width 2560, 512 experts, top-10 routed plus shared,
24 attention heads / 2 KV heads, head dimensions 256, HC count 4 / low rank 320,
indexer 4 heads x128 / top-k 2048, full attention every fourth layer (12 full,
36 recurrent), recurrent state size 128 / rank 48 / inner size 6144. PLE is at
layer 1, ngram 3 / 8 heads per ngram / head width 160 / conv kernel 4.

The PLE GGUF has architecture `strata-ple`, one tensor
`per_layer_token_embd.weight [160, 320001536]`, data offset 352 and storage type
24 (`GGML_TYPE_I8`). Its bytes encode **FP8 E4M3**, not signed integers. A generic
I8 gather would have the wrong numerical interpretation; external FP8 decoding
and bounded row streaming are required. Expanding all rows to F32 is prohibited
by the available memory. Metadata was read without tensor payload loading.

The skeleton contains neither the routed expert matrices nor the external PLE
table. A plain skeleton launch would not exercise the intended model.

## Existing support and missing implementation

| Area | Existing | Required before an honest first inference |
|---|---|---|
| Backbone | native qwen4exp model, HC, indexer, recurrent/full attention, ordinary PLE hash/state logic | preserve and validate this path with Edge0-specific data |
| Goose | qwen4exp shape/decision module, EXP511 pack index/native-expert parser | external tensor descriptors and actual latent execution; a decision module is not a complete engine |
| Quant/CUDA | Q3_K, IQ4_NL, Q4_0, Q8_0 primitives and ggml CUDA backend | correct strided expert records, per-group D/U, scale placement and bounded residency |
| Pack loader | GGUF loader and small pack-table validator | bind index/dense/experts/labels/scales/D-U/profile into model-owned tensor/provider objects without duplicating the pack |
| Routed MoE | ordinary full-width qwen4exp expert FFN | grouped 2560 ->1280 ->640 ->1280 ->2560 latent path, routing and shared-expert combination |
| External PLE | ordinary tensor-based PLE in qwen4exp | FP8 row decode, bounded I/O cache, sequence hash/conv state with clear/restore |
| MTP | generic qwen4exp MTP code and downloaded draft files | pack-specific draft binding; first diagnostic should disable speculation |
| Server | current shared HTTP/server infrastructure | select this pack provider explicitly; keep Apex unchanged |

Native expert record arithmetic: gate 352000 B + up 352000 B + down 460800 B
= 1164800 B; 512 records/layer ->596377600 B; 48 layers ->28626124800 B.
The D/U files hold, per layer, D for eight groups followed by U for eight groups.
Q4_0 uses 18 B per 32 weights, Q8_0 uses 34 B. Group labels and column scales
are part of the mathematical contract, not optional tuning.

## Memory and disk feasibility

Observed machine state: RTX4070 total 12282 MiB, used 3002 MiB, free 8871 MiB;
MemTotal 63638224 KiB, MemAvailable 41979144 KiB. Port 9101 was free. Existing
services (including an unrelated small llama server) were left running.
These are snapshots, not a guarantee of resources for a later test.

The entire local folder occupies 92281883361 bytes (85.94 GiB). Free disk was
about 35.57 GB (33.13 GiB); duplicating or converting the entire pack would not
fit and was not attempted.

ESTIMATE: naively making experts + PLE + skeleton + dense + Q4 D/U resident
would require **82.68 GiB** before caches/workspaces, exceeding physical RAM.
That does not prove the model cannot run: bounded PLE streaming and bounded
expert caching are necessary. At 4096 context the raw F16 attention K/V payload
is approximately 96 MiB; at 262144 it is 6 GiB. Recurrent F32 matrix state is
about 108 MiB plus 4.22 MiB conv state. Indexer/HC/allocator/graph/workspace and
additional caches are NOT included in those lower bounds.

A provisional implementation target is a short 4096-context run with bounded
CPU expert/PLE caches and partial GPU placement, leaving desktop/headroom.
Exact peak RAM/VRAM, compatible cache quotas and throughput remain UNKNOWN.
The developer's measurements with 16 GiB VRAM are not measurements of this
12 GiB card. No promise is made that their numbers or default context will fit.

## Validator fixes and gates

The old checker did not add a failure for .part files and merely printed
hashes instead of comparing them. It could therefore report READY incorrectly.
The checker now uses the revision-pinned file manifest, rejects unfinalized
files, compares expected hashes, detects a file changing during hashing, adds
missing tokenizer/scales/config inventory, checks duplicate native layer IDs,
and distinguishes structural success from integrity/inference acceptance.
`--hash --file <path>` permits selective rechecks without re-reading 80 GB.

Five focused unittest checks passed: full-size .part rejection, same-size hash
corruption rejection, SHA256/Git-blob verification, selective hash checking,
and duplicate-layer rejection. Real pack structure passed: 1367 index tensors,
1065 in dense.bin, pool 1538625024, alignment 64, 48 expert layers.

Model launches: **0**. Prefill, decode, request correctness, peak RSS and peak
VRAM: **not measured**. Current baseline remains Apex EXP906, 55.52 /2625.21.
No runtime source or binary changed. Next work is a bounded Goose external-pack
descriptor/FP8-row capability gate, then loader/graph integration; it must not
be mislabeled as a working Edge0 server.

Evidence: ignored `Backup/edge0-20261117/` contains HF metadata for both
revisions, the one-pass hash receipt, independent GGUF metadata/tensor tables,
rename receipt, structural output and checker gate logs. These are reused while
their inputs remain unchanged.

## Primary sources

- [Pinned local pack](https://huggingface.co/Edge0/Edge0-120B-q3k/tree/dab5ce727d494e2018626fe8896bfdc774215b17)
- [Current model card](https://huggingface.co/Edge0/Edge0-120B-q3k)
- [Developer pack/options documentation](https://github.com/Edge0-AI/Edge0/blob/main/edge0-infer/docs/edge0-120b.md)
- [D/U record layout](https://github.com/Edge0-AI/Edge0/blob/main/edge0-strata/include/strata/core/latent_dense.hpp)

The external engine was read for its format contract; it was not built,
launched or substituted for FrankenMoE.
