# FrankenMoE Pure Goose — Windows Alpha

Extract the ZIP and double-click **start.bat**. Select your Apex GGUF
in the file picker. FrankenMoE opens **http://localhost:8080/** after
the model server reports ready. Keep the console open. The model
location is remembered per Windows user.

The alpha targets Windows 10/11 x64, NVIDIA RTX 30-series or newer,
a CUDA 12.4-capable NVIDIA driver, and at least 8 GiB of free VRAM.
Loading the Apex 35B Q4_K_M model also requires enough system RAM
for its mapped weights and runtime. The launcher checks the
driver, GPU, VRAM, GGUF header, web files, and loopback port.
If a driver update is needed, follow the message to NVIDIA's
driver page and retry. Administrator rights are unnecessary for
extracting, selecting the model, and running FrankenMoE.

This package uses the independent Goose host and prebuilt CUDA PTX.
The NVIDIA driver provides `nvcuda.dll`. Application support libraries
are linked into the executables. Windows system libraries supply
networking, the file dialog, and opening the default browser.

Download the supported file **Apex-35B-V1.0-Q4_K_M.gguf** (21.71 GB)
from the [official Apex GGUF repository](https://huggingface.co/Apexintelligence-AI/Apex-35B-V1.0-GGUF/blob/c585fe8de7387925133ab7a573916c2d7070d714/Apex-35B-V1.0-Q4_K_M.gguf).
This is the only model variant targeted for Windows Alpha. The file
used for the Linux benchmarks matches the publisher's SHA-256:
`c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355`.

## Hardware settings and command-line launch

- **start.bat** uses the Automatic profile unless you choose another.
  It sizes the context, prefill batches, expert cache and CPU workers
  using the currently available resources.
- **settings.bat** opens a native settings dialog. Choose Automatic,
  Economy or Performance and save, or select a different model. The
  dialog previews the numeric settings. Performance still obeys
  the detected memory budget. Restart the server to apply changes.
- In the browser, supported generation settings such as temperature,
  top-p and output length apply to individual requests.

For direct control, open PowerShell in the extracted folder and run:

```powershell
.\frankenmoe-server.exe --ctx-size 32768 --batch-size 2048 --ubatch-size 2048 --cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib 5632 --threads 6 --threads-batch 12 "E:\Models\Apex-35B-V1.0-Q4_K_M.gguf" --serve 8080
```

Adjust the numeric settings to fit your hardware. The direct CLI serves
the same `web` directory beside the executable. Open `http://localhost:8080/`
after the console says it is listening. To stop either launch mode, close
its server console.

## API covered by the alpha

- `GET /health`, `GET /v1/models`, and the native `POST /completion`.
- Text-only `POST /v1/chat/completions` using the Apex/Qwen3.5 chat format.
- Simple OpenAI JSON and SSE framing. The SSE content arrives after
  generation, in one chunk. The alpha does not implement tools,
  multimodal messages, or structured output.

## Reporting an issue

Double-click **collect-diagnostics.bat**. Review
`frankenmoe-diagnostics.txt` and attach it to your issue along with
what you clicked and whether the browser opened. The report has fixed
status fields for the Windows runtime, CUDA, memory, and the last
startup result. It contains neither the GGUF path nor chat content.
Logs are saved locally and never uploaded from the application.

Use **settings.bat** to replace an already saved model selection.
The package has no GGUF weights.

## Testing status

This alpha is cross-built with MinGW. The software-only transport
gate uses Wine and a fixture evaluator. Real CUDA inference and
one-click startup on physical Windows with an NVIDIA GPU require
community testing.
