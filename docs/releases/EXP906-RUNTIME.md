# EXP906 runtime release (accepted Apex reference)

- GitHub release: `exp906-runtime-v1.0` in `Endorpheen/FrankenMoE-CUDA`
- asset: `exp906-runtime-linux-x86_64.tar.gz`
- archive sha256: `05f7b1b75d6fc15b65543b29a4751ff5066d3da08fa04787d18a6499ec1ce18f`
- accepted numbers: decode 55.517645 tok/s, prefill 2625.206607 tok/s
  (32768/512, RTX 4070 12 GB, MoE cache 4992 MiB)
- CUDA library: `17e79510f37817400c1b268e401d16eb2f392d1ef7f3da979d5ccf975cb32240`

The archive is a byte copy of
`/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run/` plus a launcher,
README and SHA256SUMS. Weights are not included.

Verify before use:

    bash tools/verify-apex-reference.sh            # local reference directory
    gh release download exp906-runtime-v1.0 --repo Endorpheen/FrankenMoE-CUDA
    sha256sum -c SHA256SUMS                        # inside the unpacked archive
