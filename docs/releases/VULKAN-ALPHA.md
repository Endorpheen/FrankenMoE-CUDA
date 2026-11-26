# FrankenMoE Vulkan multi-model alpha

The Linux and Windows x64 archives include a Vulkan runtime for Apex,
Inkling, Bonsai and the Qwen2.5 control model. The executable loads the
system Vulkan driver. The model weights are distributed separately.

| Mode | GGUF | API model ID |
| --- | --- | --- |
| `--apex` | `Apex-35B-V1.0-Q4_K_M.gguf` | `apex-35b` |
| `--inkling` | `Inkling-2-Mini-AD-IQ4_XS.gguf` | `inkling-2-mini` |
| `--bonsai` | `Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf` | `bonsai-2.5-27b` |
| No model flag | `qwen2.5-0.5b-instruct-q4_0.gguf` | `qwen2.5-0.5b-instruct` |

## Requirements and launch

Use a Vulkan 1.3 compute driver with synchronization2 and timeline
semaphores. The Linux x64 build requires glibc 2.43. The default device
is the first eligible AMD GPU. An explicit `--device-index` selects
another Vulkan device for diagnostics. Configure the system Vulkan
driver before launching.

Linux, from the extracted archive:

```bash
./vulkan/frankenmoe-vulkan /path/to/Apex-35B-V1.0-Q4_K_M.gguf --apex --serve 9104 --ctx-size 512
./vulkan/frankenmoe-vulkan /path/to/Inkling-2-Mini-AD-IQ4_XS.gguf --inkling --serve 9105 --ctx-size 512
./vulkan/frankenmoe-vulkan /path/to/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf --bonsai --serve 9106 --ctx-size 512
```

Windows, from the extracted archive:

```powershell
.\vulkan\frankenmoe-vulkan.exe "C:\Models\Apex-35B-V1.0-Q4_K_M.gguf" --apex --serve 9104 --ctx-size 512
.\vulkan\frankenmoe-vulkan.exe "C:\Models\Inkling-2-Mini-AD-IQ4_XS.gguf" --inkling --serve 9105 --ctx-size 512
.\vulkan\frankenmoe-vulkan.exe "C:\Models\Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf" --bonsai --serve 9106 --ctx-size 512
```

Start one model at a time for initial validation. Apex and Inkling load
only the selected experts into a reusable Vulkan buffer. An SSD or a
warm filesystem cache avoids repeated cold reads from a rotational drive.
Use `--prompt TEXT` or `--prompt-file UTF8_FILE` with `--n-predict` for CLI
inference.

The HTTP server listens on loopback and handles one request at a time.
It provides `/health`, `/v1/models`, `/tokenize`, `/completion`,
`/v1/completions`, and text-only `/v1/chat/completions`. The two
OpenAI-style completion endpoints support SSE.

```bash
curl -fsS http://127.0.0.1:9105/v1/chat/completions \
    -H 'Content-Type: application/json' \
    -d '{"model":"inkling-2-mini","messages":[{"role":"user","content":"Paris?"}],"max_tokens":12,"temperature":0,"reasoning_effort":"none"}'
```

## Verification and limits

Short real-model generation was verified on Linux with the RTX 4070
Vulkan driver and with the Windows x64 executable under Wine. Native AMD
and native Windows Vulkan are **NOT TESTED**. Check the Release Gate
receipt for the per-model results.

Apex, Inkling and Bonsai use Q4_0 KV and greedy sampling. This alpha
uses token-at-a-time prefill. MTP, tool calls, multimodal inputs and
non-greedy sampler options are outside this build. Inkling text chat
requests zero reasoning effort and separates any emitted reasoning from
the answer. The current chat parser accepts text role/content messages.
The short fixtures do not establish long-context performance or quality
equivalence.

The original Qwen2.5 control uses the
[pinned Q4_0 GGUF](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/blob/9217f5db79a29953eb74d5343926648285ec7e67/qwen2.5-0.5b-instruct-q4_0.gguf)
with SHA-256 `7671c0c304e6ce5a7fc577bcb12aba01e2c155cc2efd29b2213c95b18edaf6ed`.
