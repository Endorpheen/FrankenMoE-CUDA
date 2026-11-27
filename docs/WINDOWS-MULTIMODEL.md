# GooseInfer Windows x64 multi-model package

Published FrankenMoE archives retain their original executable names.

The package includes the existing Apex HTTP server, the multi-model HTTP
server for Inkling and Bonsai, and a separate Bonsai MTP command-line build.
Model weights are separate. The NVIDIA driver and CUDA-capable hardware are
required for the Inkling and Bonsai execution paths. The supplied Bonsai CUDA
images target SM89. Review the Release Gate report for actual host coverage.

Extract the whole package. Run one model per GPU from PowerShell:

```powershell
.\Start-Model.ps1 -Model apex -Weights 'C:\Models\Apex-35B-V1.0-Q4_K_M.gguf'
.\Start-Model.ps1 -Model inkling -Weights 'C:\Models\Inkling-2-Mini-AD-IQ4_XS.gguf'
.\Start-Model.ps1 -Model bonsai -Weights 'C:\Models\Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf'
```

The server listens on loopback. Use `-Port` to change the default 9101.
For Inkling and Bonsai use `/health` and raw `/completion`. See the gate
report for request parameters. Apex's graphical launcher is covered in
[README-Apex.md](README-Apex.md). A separate Bonsai MTP token-ID probe:

```powershell
Set-Location .\bonsai
.\bonsai-mtp.exe 'C:\Models\Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf' bonsai-mtp.cubin frozen-goose.ptx 1024 2 1812 5020 220 15
```

The multi-model server has a file and tokenizer check that can run with
CUDA absent. It reads GGUF metadata and prints the recognized architecture,
vocabulary size and token IDs after verifying the text round trip:

```powershell
.\gooseinfer-multimodel.exe --model-check 'C:\Models\Inkling-2-Mini-AD-IQ4_XS.gguf' 'Hello Goose'
```

A model check and Wine transport check are separate from native GPU generation.
