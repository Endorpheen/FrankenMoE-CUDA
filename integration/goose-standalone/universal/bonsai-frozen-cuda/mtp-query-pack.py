"""Use the common opt-in shared memory symbol for the query-pack kernels."""
from pathlib import Path
import re
import sys


path = Path(sys.argv[1])
text = path.read_text()
pattern = r"^\s*\.shared \.align 16 \.b8 (bonsai_flash_q_(?:packed|direct|linear)_panel_\$_panel_scratch)\[(?:50176|33280)\];$"
names = re.findall(pattern, text, re.M)
assert len(names) == 3, names
text = re.sub(pattern, "", text, flags=re.M)
for name in names:
    text = text.replace(name, "bonsai_flash_shared")
path.write_text(text)
