#!/usr/bin/env python3
"""Generate the anchored APX028 patch of inputs/native/common/chat.cpp.

Only two edits exist:
  1. include apx028-chat-native.h next to the sampling include;
  2. early dispatch at the top of common_chat_templates_apply_jinja.
The original body stays intact for APX028_CHAT=0 rollback.
"""
import hashlib
import json
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
SRC = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/inputs/native/common/chat.cpp")

INCLUDE_ANCHOR = '#include "sampling.h"\n'
FN_ANCHOR = (
    "static common_chat_params common_chat_templates_apply_jinja(const struct common_chat_templates *        tmpls,\n"
    "                                                            const struct common_chat_templates_inputs & inputs) {\n"
    "    autoparser::generation_params params;\n"
)
DISPATCH = (
    "    if (apx028_chat_enabled()) {\n"
    "        return apx028_chat_apply_templates(tmpls->template_default.get(),\n"
    "                                           tmpls->template_tool_use.get(),\n"
    "                                           tmpls->add_bos, tmpls->add_eos, inputs);\n"
    "    }\n"
)
INCLUDE_LINE = '#include "apx028-chat-native.h"\n'


def sha(text: str) -> str:
    return hashlib.sha256(text.encode()).hexdigest()


def main() -> int:
    src = SRC.read_text()
    if "apx028-chat-native.h" in src or "apx028_chat_enabled()" in src:
        print("SOURCE_ALREADY_PATCHED", file=sys.stderr)
        return 1
    if src.count(INCLUDE_ANCHOR) != 1:
        print("INCLUDE_ANCHOR_COUNT", src.count(INCLUDE_ANCHOR), file=sys.stderr)
        return 1
    if src.count(FN_ANCHOR) != 1:
        print("FUNCTION_ANCHOR_COUNT", src.count(FN_ANCHOR), file=sys.stderr)
        return 1

    out = src.replace(INCLUDE_ANCHOR, INCLUDE_ANCHOR + INCLUDE_LINE, 1)
    out = out.replace(FN_ANCHOR, FN_ANCHOR[: -len("    autoparser::generation_params params;\n")] +
                      DISPATCH + "    autoparser::generation_params params;\n", 1)

    patched_dir = BASE / "patched"
    patched_dir.mkdir(parents=True, exist_ok=True)
    patched = patched_dir / "chat-apx028.cpp"
    patched.write_text(out)

    import difflib
    diff = difflib.unified_diff(
        src.splitlines(True), out.splitlines(True),
        fromfile="inputs/native/common/chat.cpp",
        tofile="chat-final/patched/chat-apx028.cpp")
    (patched_dir / "chat-apx028.diff").write_text("".join(diff))

    deps = [
        SRC,
        Path("/inputs/native/common/chat.h"),
        Path("/inputs/native/common/chat-auto-parser.h"),
        Path("/inputs/native/common/chat-peg-parser.h"),
        Path("/inputs/native/common/peg-parser.h"),
        Path("/inputs/native/common/json.h"),
        Path("/inputs/native/common/jinja/lexer.cpp"),
        Path("/inputs/native/common/jinja/parser.cpp"),
        Path("/inputs/native/common/jinja/runtime.cpp"),
        Path("/inputs/native/common/jinja/value.cpp"),
        Path("/inputs/native/common/jinja/string.cpp"),
        Path("/inputs/native/common/jinja/caps.cpp"),
        Path("/inputs/native/common/json.cpp"),
        Path("/inputs/native/common/unicode.cpp"),
    ]
    dep_hashes = {}
    for d in deps:
        try:
            dep_hashes[str(d)] = sha(d.read_text())
        except FileNotFoundError:
            dep_hashes[str(d)] = "MISSING"
    lock = {
        "source": str(SRC),
        "source_sha256": sha(src),
        "patched": "chat-final/patched/chat-apx028.cpp",
        "patched_sha256": sha(out),
        "diff": "chat-final/patched/chat-apx028.diff",
        "include_anchor": INCLUDE_ANCHOR,
        "function_anchor": FN_ANCHOR.splitlines()[0],
        "dispatch": DISPATCH,
        "rollback": "APX028_CHAT=0 (or off/false/no) restores the original common_chat_templates_apply_jinja body",
        "dependencies": dep_hashes,
    }
    (BASE / "source-lock.json").write_text(json.dumps(lock, indent=2) + "\n")
    print(f"PATCH_OK {lock['source_sha256'][:16]} -> {lock['patched_sha256'][:16]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
