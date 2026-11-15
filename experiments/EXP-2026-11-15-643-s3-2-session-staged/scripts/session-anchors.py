#!/usr/bin/env python3
"""Provenance check for the staged S3.2 transcription.

The gate's oracle is a transcription of runtime/core/src/engine/{session,runtime}.cpp, not the
compiled engine (which needs llama.cpp, a KV cache and a GPU). A transcription that drifts from
the file it claims to reproduce would still pass a differential against itself, so every anchor
below is checked against reference/session.cpp and reference/runtime.cpp: exact line, exact
substring.

Usage: python3 scripts/session-anchors.py [reference dir]
Exit 0 when every anchor is present, 1 otherwise.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_REF = HERE.parent / "reference"

# (file, line, substring the line must contain, what the transcription does with it)
ANCHORS = [
    ("session.cpp", 64, "if (bytes_per_slot == 0)", "hot store: no expert tensors"),
    ("session.cpp", 70, "const int sentinels = std::max(8, top_k);", "sentinels"),
    ("session.cpp", 74, "sentinel slots alone require about", "sentinel MiB message"),
    ("session.cpp", 671, "constexpr uint64_t reserve_mib = 4096;", "preflight reserve"),
    ("session.cpp", 674, "return fail(\"VRAM preflight failed: only", "preflight refusal"),
    ("session.cpp", 677, "const int safe_mib = (int) (free_mib - reserve_mib);", "preflight clamp"),
    ("session.cpp", 691, "hot_slots_from_budget(meta(), effective_vram_cache_mb", "hot store call"),
    ("session.cpp", 728, "mparams.load_mtp = cfg.spec.is_mtp();", "load_mtp"),
    ("session.cpp", 765, "if (cfg.spec.is_mtp() && im.n_layer_nextn <= 0)", "MTP head admission"),
    ("session.cpp", 802, "n_layer_streamed = im.n_layer", "streamed span"),
    ("session.cpp", 817, "cparams.n_ubatch = cfg.n_ubatch > 0", "target ubatch"),
    ("session.cpp", 843, "cparams.n_rs_seq = (uint32_t) cfg.spec.draft_max;", "n_rs_seq"),
    ("session.cpp", 868, "margin_mib = 256 + (cfg.spec.is_mtp() ? 512 : 0);", "probe margin"),
    ("session.cpp", 869, "probe_budget = free_mib > margin_mib", "probe budget"),
    ("session.cpp", 874, "probe_budget > 0 ? hot_slots_from_budget", "probe re-derive"),
    ("session.cpp", 889, "expert_hot_slots = 0;", "probe disable"),
    ("session.cpp", 926, "dparams.n_ubatch = (uint32_t) std::max(cfg.spec.draft_max + 1, mtp_draft_ubatch);",
     "draft context width"),
    ("session.cpp", 927, "if (dparams.n_ubatch > cparams.n_ubatch)", "draft width clamp"),
    ("session.cpp", 983, "llama_batch_init(std::max(cfg.n_batch, cfg.spec.draft_max + 1)",
     "verify batch"),
    ("runtime.cpp", 14, "sc.n_batch = cfg.n_ctx;", "n_batch = n_ctx"),
    ("runtime.cpp", 38, "ValidationResult v = validate(cfg);", "run() step 1"),
    ("runtime.cpp", 39, "if (!v) {", "run() early return 1"),
    ("runtime.cpp", 45, "const SessionConfig sc = session_config_from(cfg);", "run() step 2"),
    ("runtime.cpp", 48, "Session::open(sc, error", "run() step 3"),
    ("runtime.cpp", 49, "if (!session) {", "run() early return 2"),
    ("runtime.cpp", 59, "req.clear_kv = true;", "request: clear_kv"),
    ("runtime.cpp", 63, "req.render_text = cfg.progress;", "request: render_text"),
    ("runtime.cpp", 67, "return session->generate(req, on_token, sink);", "run() step 5"),
]


def main() -> int:
    ref = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_REF
    cache: dict[str, list[str]] = {}
    missing = 0
    for name, line, needle, what in ANCHORS:
        path = ref / name
        if name not in cache:
            if not path.is_file():
                print(f"MISSING FILE {path}")
                return 1
            cache[name] = path.read_text(encoding="utf-8").splitlines()
        lines = cache[name]
        if line < 1 or line > len(lines) or needle not in lines[line - 1]:
            print(f"MISS {name}:{line} ({what}) — expected {needle!r}")
            if 0 < line <= len(lines):
                print(f"     actual: {lines[line - 1].strip()!r}")
            missing += 1
    print(f"anchors={len(ANCHORS)} missing={missing}")
    return 0 if missing == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
