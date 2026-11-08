#!/usr/bin/env python3
"""Verify the frozen compiler emitted the expected strong chat entry shape."""
import re
import sys
from pathlib import Path

src = Path(sys.argv[1] if len(sys.argv) > 1 else "generated/chat-control.c").read_text()
entry = None
for name, params in re.findall(r"static int64_t (gg_function_\d+)\(([^)]*)\)", src):
    if params.count("int64_t") == 6 and params.rstrip().endswith("base"):
        entry = name
        break
if entry is None:
    print("ENTRY_NOT_FOUND", file=sys.stderr)
    raise SystemExit(1)
if entry != "gg_function_1":
    print(f"ENTRY_ID_CHANGED {entry}", file=sys.stderr)
    raise SystemExit(1)
call = re.search(rf"{entry}\(([^;]*?),\s*base \+ 1\)", src)
if not call or call.group(1).count(",") != 4:
    print("ENTRY_CALL_SHAPE_INVALID", file=sys.stderr)
    raise SystemExit(1)
wrapper = Path("chat-final/chat-control-wrapper.c").read_text()
if "APX028_GENERATED_ENTRY gg_function_1" not in wrapper:
    print("WRAPPER_ENTRY_MISMATCH", file=sys.stderr)
    raise SystemExit(1)
print(f"CHAT_ENTRY_OK {entry} arity=5 base=hidden")
