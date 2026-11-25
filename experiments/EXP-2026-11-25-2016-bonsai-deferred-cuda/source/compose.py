"""Bind Bonsai CUDA calls lazily without changing the common runtime ABI."""
from pathlib import Path
import re
import sys

source, out = map(Path, sys.argv[1:])
src = source.read_text()
loader = Path(__file__).with_name("cuda-loader.inc").read_text()
names = set(re.findall(r"^    X\((cu\w+)\)", loader, re.M))
calls = set(re.findall(r"\b(cu[A-Z]\w*)\(", src))
assert names == calls, (names - calls, calls - names)
src = re.sub(r"\b(cu[A-Z]\w*)\(", lambda m: f"bonsai_cuda.fn_{m[1]}(", src)
anchor = "#include <time.h>\n"
assert src.count(anchor) == 1
src = src.replace(anchor, anchor + "\n" + loader)
anchor = "    if (context < CONTEXT_MIN || context > POSITIONS_MAX) return NULL;"
assert src.count(anchor) == 1
src = src.replace(anchor, anchor + "\n    if (!bonsai_cuda_ready()) return NULL;")
out.write_text(src)
