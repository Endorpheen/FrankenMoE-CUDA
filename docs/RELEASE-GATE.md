# Release Gate

Every FrankenMoE package release uses the same
[architecture and backend matrix](../tools/multimodel-release/release-matrix.json)
and [runner](../tools/multimodel-release/release-gate.py). Extend the existing
matrix when a model or backend enters release scope. Each Vulkan model has
separate native AMD and diagnostic rows. Results from explicit Vulkan
device selection or Wine do not qualify native AMD hardware.

## Checklist

- Freeze the exact source checkpoint, compiler, image, model quant and launch
  profile. Build in fresh directories. Record dependency versions and the
  command used for each operating system. Preserve accepted checkpoints.
- Stage Linux x64 and Windows x64, including every matrix entry and its
  runtime assets. Keep machine-local aliases and weights outside the package.
- Verify archive SHA-256, extract, verify all contents and inspect standalone
  executable format and imports. Compare build outputs against packaged hashes.
- Exercise every model independently on real weights. Confirm startup,
  readiness, short generation, golden IDs/text and process RAM/VRAM.
  Run Linux CUDA on the available RTX 4070 and native Windows CUDA on a
  Windows/NVIDIA host when accessible. Wine is a separate CPU/transport
  check with CUDA explicitly disabled.
- Review and ship the generated JSON and Markdown reports with the original
  archive SHA-256 receipt. Check the names and hashes of published artifacts.
- Fix missing declared artifacts, build or dependency errors and correctness
  failures before publication. Report unavailable hardware or unimplemented
  CPU paths explicitly. Publish a qualified alpha with unavailable hardware
  marked as such, without advertising that combination as verified.

## Run

```bash
python3 tools/multimodel-release/release-gate.py \
  --linux path/to/linux-x64.tar.gz --windows path/to/windows-x64.zip \
  --sha256 path/to/SHA256SUMS --output build/release-gate-vNEXT \
  --weights /path/to/gguf-directory --wine-prefix /path/to/isolated-wine-prefix \
  --build-plan path/to/local-build-plan.json
```

Omit `--weights` for an archive-only report. Omit `--wine-prefix` when no
isolated prefix is available. The gate never downloads weights or runs
benchmarks. It serializes generation, uses short model fixtures on GPU and one token on
Wine CPU, and bounds startup and request waits. Warm file-cache residency
ahead of the live block if a model profile requires it. On a native Windows
host, run the same gate against the same archives with weights and PE
`objdump` available. Live checks run on the current host only.

Use the default distribution for any archive containing CUDA executables.
For independent Vulkan-only archives, add `--distribution vulkan`. This
mode verifies that every file belongs to the Vulkan distribution, then
evaluates the Vulkan rows in the same registry. Use
`--vulkan-device-index N` to exercise a separate explicit-device Linux
diagnostic and `--vulkan-wine-icd PATH` with an isolated Wine prefix to
exercise the Windows executable through the selected host ICD. Real AMD tests select an eligible
AMD device using the native loader. The full CUDA+Vulkan matrix is still
mandatory for combined archives.

A local build plan maps each platform to a trusted command, a timeout and
the mapping of archive-relative executable names to built files. One command
can use the project's platform build scripts and stage the resulting binaries.

```json
{
  "linux-x64": {
    "argv": ["bash", "path/to/build-all-linux.sh"],
    "timeout_s": 300,
    "outputs": {"bin/apex-server": "path/to/built/apex-server"}
  }
}
```

Include all entry points in a real build plan. Preserve logs and use a
fresh report directory for each pass. Use `--only OS/BACKEND/MODEL` to
repeat a failing row. Use a full matrix report for publication. Do not copy
a previous PASS to a different binary. Exit 0 means every applicable check passed, 1 means a
check failed and 2 means applicable checks are still untested.

| Status | Meaning |
| --- | --- |
| PASS | This exact check ran and met its criterion. |
| FAIL | The check ran and found an error, or a required artifact is missing. |
| NOT TESTED | Hardware, tooling, a build plan or a reference was unavailable. |
| NOT APPLICABLE | The architecture has no such path, as declared in the matrix. |

Memory checks use peak process RSS and process-attributed VRAM sampled after
generation. They are smoke limits, not performance regression measurements.
If process-attributed VRAM is unavailable, record it as
NOT TESTED. A short regression gate does not replace the per-model
performance baselines in the optimization registry.
