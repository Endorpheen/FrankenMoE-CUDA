# Independent runtime recovery, 2026-11-16

## Checkpoint 0

HEAD: 7c4f8ae37193890820f09686faadb85a9949e78e. Initial tracked diff and restored model sources were preserved before edits. Reference manifests: 13/13 files each; independent set: 1/1; reference-45 set: 33/33. All hashes match. Reference artifacts are read-only inputs to the audit, never link inputs.

## Diagnostic 1: existing independent runtime

Core: 906f92ddd2f43c33ded158d2fa08ccc9c40a5d1dfad032d357b6fe4ec33e95b1. The fixed 262144/4992 profile did not reach readiness. One core remained busy, process swap was zero, GPU usage 9805 MiB. External debugger attachment was denied. The process was terminated and the port released. No performance claim.

## Diagnostic 2: in-process stack capture

The same core, 100% GGUF residency, unchanged model/profile, diagnostic preload only. The main thread was in slru_plan + 0x1f3, called by llama_moe_cache::impl::copy during warmup; gs_fault_handler appeared above the repeated fault. The stale experiment build/slru-policy.c exports slice arguments, while the native caller and canonical Goose module use raw int32 addresses. A source-generated policy exists in the old build directory but the independent recipe selects the stale experiment output instead. Symbol presence and dlopen could not detect this ABI mismatch.

## Source correction and local gates

Generate the SLRU policy/runtime/header into each candidate build. Include the generated header from the C++ caller instead of redeclaring the ABI by hand. Differential gate: 400/400 sequences, seed 99, protected fraction 7/8, including final cache state. Negative compile gate: the stale slice header is rejected. Preserve all existing optimization switches and defaults.

## Build diagnostics

The unfinished full model restoration hit duplicate enums at lfm2.cpp because native and vendor headers were mixed. Vendor translation units now use vendor headers consistently. Model dispatch/source restoration remains preserved. Required extra objects and modules are compiled from repository sources, missing inputs fail explicitly. Build outputs are isolated under LlamaServer; no reference objects are consumed. Full backend/server support sources were imported with a hash manifest to replace inherited binary dependencies. Acceptance is pending real generation and controlled measurements.
