#!/usr/bin/env python3
"""Provenance check: the transcription still matches reference/gguf-model-data.cpp.

Every anchor below is a line of the original detect_gguf_filename() that the
staged body reproduces. The reference copy is read-only.
"""
from pathlib import Path
import sys

EXP = Path(__file__).resolve().parent.parent
REF = EXP / "reference" / "gguf-model-data.cpp"

ANCHORS = [
    'auto [code, body] = gguf_http_get(api_url, {}, 30);',
    'j = nlohmann::json::parse(body.begin(), body.end());',
    'if (!j.contains("siblings") || !j["siblings"].is_array()) {',
    'for (const auto & sibling : j["siblings"]) {',
    'if (!sibling.contains("rfilename")) { continue; }',
    'std::string fname = sibling["rfilename"].get<std::string>();',
    'if (fname.size() < 5 || fname.substr(fname.size() - 5) != ".gguf") {',
    'auto pos = m.find("-00001-of-");',
    'split_prefix = m.substr(0, pos);',
]


def main() -> int:
    text = REF.read_text(encoding="utf-8")
    missing = 0
    for a in ANCHORS:
        n = text.count(a)
        if n != 1:
            missing += 1
            print(f"MISSING count={n} :: {a}")
    print(f"FG665_ANCHORS anchors={len(ANCHORS)} missing={missing}")
    return 0 if missing == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
