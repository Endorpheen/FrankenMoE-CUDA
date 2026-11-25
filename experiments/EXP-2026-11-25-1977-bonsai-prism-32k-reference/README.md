# EXP-2026-11-25-1977 — PrismML llama.cpp reference for Bonsai 32K/512

**Measured reference, one scored request.** RTX 4070 12 GiB, driver 580.178.04,
CUDA 12.4.131, PrismML commit `910442b0ce1128ef85d7730cbcae835cce60cdd4`.
The benchmark uses the 7,492,338,944-byte local
`Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf`. The model card requires the
[PrismML fork](https://github.com/PrismML-Eng/llama.cpp) for the PQ2_0 encoding.
See the [model card](https://huggingface.co/prism-ml/Ternary-Bonsai-2.5-27B-GGUF).

| Metric | Result |
| --- | ---: |
| Prefill, 32,768 evaluated input tokens | 1093.209 tok/s (29.974 s) |
| Decode, 512 returned output IDs | 47.966 tok/s (10.653 s) |
| Non-streaming client elapsed | 40.636 s |
| Peak process VRAM, NVML via nvidia-smi | 8288 MiB |
| Peak whole-board VRAM, including other processes | 10223 MiB |
| Peak process RSS, including initialization and file-backed RSS | 7,653,328 KiB |
| RSS after the scored request | 1,069,660 KiB |
| Process swap / system swap-out growth | 0 / 0 |

Configuration: context 36864, batch 4096, ubatch 1024, Q4_0 K/V, Flash
Attention on, one stream, full GPU offload including an explicit token
embedding override, MTP off, greedy sampling, seed 123, EOS masked to
ensure the complete output budget, no prompt reuse and no context shift.
Client submits fixed raw token IDs with no chat templating or inserted BOS.
The model's HTTP tokenizer and an independent llama-tokenize binary agreed
on all 85,275 source-corpus IDs. The request used the first 32,768.

A separate verbose startup using the same options reported all 66 layer
slots offloaded, with 6906.74 MiB of model tensors on CUDA, 648.00 MiB for
K/V, 149.62 MiB for recurrent state and 356.31 MiB for the CUDA compute
buffer. The disabled MTP layer's tensors were skipped by this build. For
the scored run the board already held 1935 MiB before launch. Separate
sampled process and board peaks are reported to distinguish the runtime
from other GPU consumers.

## Reproduction

Build in the permitted isolated llama.cpp directory. The historical run
used host C++ GNU 15.2.0 and nvcc with GNU 13.4.0 as CUDA host compiler.
The embedded/prebuilt Web UI was disabled due to an asset-format mismatch.
The inference tree and CUDA compilation were unchanged.

```sh
cd ~/AI/LlamaServer
git clone --depth 1 --single-branch --branch prism https://github.com/PrismML-Eng/llama.cpp.git bonsai-prism-reference-20261125
cd bonsai-prism-reference-20261125
git fetch --depth 1 origin 910442b0ce1128ef85d7730cbcae835cce60cdd4
git switch --detach FETCH_HEAD
cmake -S . -B build-sm89 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89 \
  -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_TOOLS=ON \
  -DLLAMA_BUILD_SERVER=ON -DLLAMA_OPENSSL=OFF \
  -DLLAMA_BUILD_UI=OFF -DLLAMA_USE_PREBUILT_UI=OFF
cmake --build build-sm89 --target llama-server -j 10
```

```sh
model="$HOME/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf"
binary="$HOME/AI/LlamaServer/bonsai-prism-reference-20261125/build-sm89/bin/llama-server"
env -i PATH=/usr/bin:/bin HOME="$HOME" LANG=C CUDA_VISIBLE_DEVICES=0 \
  "$binary" -m "$model" --host 127.0.0.1 --port 9101 \
  --ctx-size 36864 --batch-size 4096 --ubatch-size 1024 \
  --cache-type-k q4_0 --cache-type-v q4_0 --flash-attn on --n-gpu-layers 99 \
  --override-tensor '^token_embd\.weight$=CUDA0' --fit off --parallel 1 \
  --cache-ram 0 --spec-type none --no-context-shift \
  --threads 8 --threads-batch 16 --cpu-mask 0f0f --cpu-mask-batch ffff \
  --cpu-strict 1 --cpu-strict-batch 1
```

From this experiment directory, after the server reports healthy:

```sh
curl --fail-with-body --silent --show-error \
  -H 'Content-Type: application/json' --data-binary @request.json \
  -w '%{time_total}\n' -o response.json http://127.0.0.1:9101/completion
```

GPU memory was sampled every 250 ms using both
`nvidia-smi --query-compute-apps=pid,used_gpu_memory` and a board trace.
Measure `VmHWM`, `VmRSS` and `VmSwap` in `/proc/<pid>/status` before
stopping the server. Raw local evidence is retained in
`build/bonsai-prism-32k-reference`.

The matching short generation fixture and end-of-generation handling are in
[EXP-1976](../EXP-2026-11-25-1976-bonsai-trunk/README.md). The full retained
[request](request.json) and [results](results.json) include the input hash,
output IDs, placement evidence, timings and build hashes. Use this exact
request again when comparing FrankenMoE on the same profile.
