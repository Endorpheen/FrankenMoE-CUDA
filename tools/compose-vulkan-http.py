"""Reuse the standalone HTTP framing and JSON helpers in the Vulkan unit."""

from pathlib import Path
import sys


def section(path, start, end):
    text = path.read_text()
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


root, out = map(Path, sys.argv[1:])
server = root / "goose/server"
inference = server / "inference-http.goose"
reader = section(inference, "fn ih_line_end(", "struct IhCompletion")
anchor = "route != 36 && !list_tools"
if reader.count(anchor) != 1:
    raise ValueError("Unexpected standalone HTTP route admission")
# Native Vulkan also serves the tokenizer route.
reader = reader.replace(anchor, "route != 25 && route != 36 && !list_tools")
helpers = section(server / "http-listen.goose", "fn hl_lower(", "fn hl_put_addr(")
depth = section(inference, "fn ih_chat_depth_ok(", "fn ih_chat_json(")
fmt = section(root / "goose/cli/cli-protocol.goose", "const PJ_K_NULL:", "fn cp_pow10(")

out.write_text("\n".join((helpers, reader, depth, fmt)))
