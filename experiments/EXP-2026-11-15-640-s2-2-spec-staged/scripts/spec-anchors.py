#!/usr/bin/env python3
"""Anchor check for the staged S2.2 slice (speculative-decoding decisions).

The carrier in common/fg-spec-decide.cpp transcribes decision policy out of
runtime/core/src/engine/session.cpp. That file is copied here as reference/session.cpp
because it cannot be compiled without the engine; the transcription is therefore only as
good as its provenance. This script pins the provenance: each anchor is a line range plus
the substrings the transcription depends on. If the reference copy is replaced by another
revision of session.cpp, a missing anchor fails here instead of silently invalidating the
oracle.

The draft source is not transcribed at all: runtime/core/src/engine/ngram_draft.cpp is
compiled unmodified, so its anchor only records that the copied file is the shipped one.

    python3 scripts/spec-anchors.py [--session PATH] [--ngram PATH]

Defaults are reference/session.cpp and runtime/core/src/engine/ngram_draft.cpp.
Exit status 0 when every anchor is present, 1 otherwise.
"""
from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# file, label, first line, last line, substrings the carrier's transcription relies on
ANCHORS = [
    ("session", "draft budget",   1675, 1679,
     ["int n_draft = 0;", "const int room = req.n_predict - n_gen - 1;", "if (spec_on && room > 0) {"]),
    ("session", "mtp resize",     1691, 1692,
     ["im.draft_buf.resize((size_t) room);", "n_draft = (int) im.draft_buf.size();"]),
    ("session", "ngram call",     1707, 1708,
     ["n_draft = ngram_draft(mtp_ctx, tok, std::min(im.cfg.spec.draft_max, room),"]),
    ("session", "verify width",   1723, 1723,
     ["const bool wide = spec_on && n_draft > 0;"]),
    ("session", "accept loop",    1772, 1800,
     ["confirmed.clear();", "int n_acc = 0;", "if (want != im.draft_buf[i]) break;",
      "++n_acc;", "if (llama_vocab_is_eog(im.vocab, want)) {", "confirmed.push_back(want);"]),
    ("session", "rollback",       1821, 1826,
     ["if (n_acc < n_draft) {", "const llama_pos keep = n_past + 1 + n_acc;"]),
    ("session", "emit bound",     1848, 1848,
     ["for (size_t e = 0; e < confirmed.size() && n_gen < req.n_predict; ++e)"]),
    ("session", "advance",        1888, 1889,
     ["n_past += 1 + n_acc;", "const int32_t row = wide ? n_acc : -1;"]),
    ("session", "tail trim",      1899, 1912,
     ["const llama_pos emitted_end = (llama_pos) n_prompt + n_gen;",
      "llama_memory_clear(llama_get_memory(ctx), true);"]),
    ("ngram",   "draft source",   1, 20,
     ["#include \"bmoe/ngram_draft.h\"", "int ngram_draft(const std::vector<int32_t> & ctx,"]),
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--session", default=str(ROOT / "reference" / "session.cpp"))
    ap.add_argument("--ngram", default=str(ROOT / "runtime/core/src/engine/ngram_draft.cpp"))
    a = ap.parse_args()
    files = {"session": Path(a.session), "ngram": Path(a.ngram)}

    missing_any = 0
    for key, path in files.items():
        if not path.exists():
            print(f"missing: {path}")
            return 1
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        print(f"FG642_ANCHORS {key} {path} lines={len(lines)}")
        for f, label, lo, hi, needles in ANCHORS:
            if f != key:
                continue
            window = "\n".join(lines[max(0, lo - 1):hi])
            missing = [n for n in needles if n not in window]
            if missing:
                missing_any += 1
                print(f"[MISS] {label} ({lo}-{hi}): {missing}")
            else:
                print(f"[ ok ] {label} ({lo}-{hi})")
    print(f"FG642_ANCHORS anchors={len(ANCHORS)} missing={missing_any}")
    return 0 if missing_any == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
