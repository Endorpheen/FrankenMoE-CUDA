#!/usr/bin/env python3
"""Check that every accepted component is present in the shipped library.

A successful compile says nothing about which source tree was compiled. The candidate once compiled
`vendor/flint/src/llama-model.cpp`, which contains no host-registration code, while the reference binary
was built from `runtime/reconstructed/src/llama-model.cpp`, which does: both compiled, one had the
feature, and about ten tokens per second went with it. This check reads the artefact instead.

Each entry is (name, kind, needle, why). `strings` catches a feature whose only trace is a log line or a
flag name; `nm` catches an exported door.
"""
import subprocess
import sys
import pathlib

BIN = sys.argv[1] if len(sys.argv) > 1 else "builds/flint-acc/bin"

COMPONENTS = [
    ("host-registration", "strings", "mapped expert ranges",
     "EXP-2026-11-16-906: registers the resident mmap expert ranges and removes a transfer step"),
    ("moe-cache", "strings", "moe-cache",
     "the GPU expert cache the fixed Apex profile sets to 4992 MiB"),
    ("goose-sampler-chain", "nm", "fg_sampler_chain",
     "the ported sampling chain"),
    ("goose-q4-k", "nm", "fg_cpu_simd_mmid_tiled_q4_k",
     "the accepted tiled Q4_K kernel bridge"),
    ("goose-shim-marker", "strings", "FG171_",
     "the shim's own diagnostics prove the door table was compiled in"),
    # Added as the migration accepts each piece: an entry here is a promise the artefact must keep.
    ("goose-grammar", "nm", "fg_grammar_reject",
     "the ported grammar automaton, correct on 1 736 513 candidates"),
    ("goose-spec-prefix", "nm", "fg_sp_prefix",
     "the speculative prefix-agreement door"),
    ("goose-dry-limit", "nm", "fg_dry_rep_limit_entry",
     "DRY step 1, whose context order was a real defect and is fixed"),
    ("goose-dry-apply", "nm", "fg_sampler_dry_call",
     "DRY steps 2-4 through the shipped door"),
    ("goose-bulk-bridge", "nm", "fg161_bulk",
     "the accepted bulk bridge the ggml base library links against"),
    ("gguf-reader-call-path", "strings", "FG_GGUF_PARSE",
     "the loader calls the ported reader and refuses the load on disagreement (FG_GGUF_DIAG)"),
    ("goose-spec-synth", "strings", "FG_SP_SYNTH goose",
     "the synthetic acceptance door reached on the real model (EXP-2026-11-19-1366: it had been added to a "
     "server-context.cpp this build does not compile, so the artefact carried nothing)"),
    ("expert-cache", "strings", "moe-cache",
     "the SLRU expert cache the profile sets to 4992 MiB"),
]

def shell(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True).stdout

def main():
    # the features live in different libraries of the same build: the sampler doors and the model loader
    # in libllama, the tiled Q4_K bridge in libggml-cpu. Searching only one of them reported a present
    # component as missing.
    libs = sorted(str(p) for p in pathlib.Path(BIN).glob("*.so*"))
    if not libs:
        print(f"check-accepted-components: no libraries under {BIN}")
        return 2
    strings = "".join(shell(f"strings {p}") for p in libs)
    symbols = "".join(shell(f"nm -D {p}") for p in libs)
    missing = []
    for name, kind, needle, why in COMPONENTS:
        hay = strings if kind == "strings" else symbols
        if needle in hay:
            print(f"  present  {name}")
        else:
            print(f"  MISSING  {name}  <- {why}")
            missing.append(name)
    if missing:
        print(f"check-accepted-components: FAIL, {len(missing)} accepted component(s) absent: {', '.join(missing)}")
        return 1
    print("check-accepted-components: ok, every accepted component is in the shipped library")
    return 0

if __name__ == "__main__":
    sys.exit(main())
