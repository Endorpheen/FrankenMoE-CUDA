# Experimental Vulkan backend

The experimental Vulkan distribution runs the pinned
Qwen2.5-0.5B-Instruct Q4_0 GGUF. The executable loads the system
driver at startup and has no CUDA dependency. Obtain the NVIDIA CUDA
packages for Apex, Inkling and Bonsai from the separate multi-model
release.

Use a 64-bit OS and a Vulkan 1.3 compute driver with synchronization2
and timeline semaphores. The default Vulkan device is the first
eligible AMD GPU. Native AMD hardware and native Windows AMD drivers
are unverified in this alpha. Configure the system Vulkan driver
before launching. Model weights are provided separately.

- [Pinned GGUF](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/blob/9217f5db79a29953eb74d5343926648285ec7e67/qwen2.5-0.5b-instruct-q4_0.gguf)
- GGUF SHA-256 `7671c0c304e6ce5a7fc577bcb12aba01e2c155cc2efd29b2213c95b18edaf6ed`

Linux, from the extracted archive:
```bash
./vulkan/frankenmoe-vulkan /path/to/qwen2.5-0.5b-instruct-q4_0.gguf \
    --serve 9104 --ctx-size 512
```

Windows, from the extracted archive:
```powershell
.\vulkan\frankenmoe-vulkan.exe "C:\Models\qwen2.5-0.5b-instruct-q4_0.gguf" --serve 9104 --ctx-size 512
```

Then use the loopback API:
```bash
curl -fsS http://127.0.0.1:9104/health
curl -fsS http://127.0.0.1:9104/v1/chat/completions \
    -H 'Content-Type: application/json' \
    -d '{"model":"qwen2.5-0.5b-instruct","messages":[{"role":"user","content":"What is the capital of France?"}],"max_tokens":12,"temperature":0}'
```

The same executable accepts `--prompt TEXT` and `--prompt-file UTF8_FILE`
for CLI use. The API supports greedy text completion, ChatML chat, SSE
on OpenAI-style routes, tokenization and usage. The listener binds to
loopback and processes one request at a time. This Vulkan alpha supports
only the documented Qwen2.5 variant. Apex, Inkling and Bonsai use CUDA.
Tool calls, non-greedy sampler options and Vulkan performance claims
are outside this package's verified scope.
