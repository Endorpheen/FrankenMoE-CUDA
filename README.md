# FrankenMoE-CUDA

FrankenMoE runs local LLM inference with Goose model code and native
adapters to the operating system and NVIDIA CUDA driver. Learn about
the language in the [original Goose repository](https://github.com/aardappel/goose).

![Goose on Fire terminal dashboard with Apex startup logs](docs/assets/goose-on-fire.png)

## Multi-model alpha

The Linux package has been checked on an RTX 4070 12 GB with the
following GGUF models. Detailed requirements, launch profiles and
platform coverage are in the [multi-model guide](docs/MULTIMODEL-ALPHA.md).

| Model | Interface | Prefill, tok/s | Decode, tok/s |
| --- | --- | ---: | ---: |
| Apex 35B V1.0 Q4_K_M | HTTP | 3505.42 | 82.10 |
| Inkling 2 Mini AD-IQ4_XS | HTTP | 296.24 | 22.03 |
| Ternary Bonsai 2.5 27B PQ2_0 | HTTP | 648.79 | 32.70 |
| Ternary Bonsai 2.5 27B PQ2_0 | Greedy MTP CLI | 618.10 | 72.12 |

These are single-response 32K/512 measurements on the named
checkpoints. Linux package smoke tests check short generation
separately. The [v0.13.0-alpha release](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.13.0-alpha)
includes the Linux package and an experimental Windows package with Apex
HTTP and the Bonsai CUDA CLI. Windows Inkling is unavailable and native
Windows CUDA inference remains untested.

## Run on Linux

Run one model per GPU with the appropriate memory profile:

```bash
./frankenmoe apex /path/to/Apex-35B-V1.0-Q4_K_M.gguf
./frankenmoe inkling /path/to/Inkling-2-Mini-AD-IQ4_XS.gguf
./frankenmoe bonsai /path/to/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
```

Open the dashboard at the port printed by the server, or use its
OpenAI-compatible `/v1` endpoint. Python 3 enables the interactive
terminal dashboard. Set `FRANKENMOE_CONSOLE=0` for plain logs.

## Build and evidence

- [Build guide](docs/BUILD.md)
- [Release and packaging guide](docs/RELEASES.md)
- [Experiment registry](experiments/EXPERIMENTS.md)
- [Goose sources](goose/README.md)
- [License](LICENSE)
