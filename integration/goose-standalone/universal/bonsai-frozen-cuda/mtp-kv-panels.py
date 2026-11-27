"""Map the generated shared arena onto CUDA launch-time shared memory."""
from pathlib import Path
import re
import sys

p = Path(sys.argv[1])
s = p.read_text()
pat = r"^\s*\.shared \.align 16 \.b8 (bonsai_flash_kv_(?:panel|lean)[24]_\$_panel_scratch)\[(?:61952|50176)\];$"
names = re.findall(pat, s, re.M)
assert len(names) == 4, [line for line in s.splitlines() if "panel_scratch" in line][:12]
s = re.sub(pat, "", s, flags=re.M)
at = re.search(r"^\.address_size 64$", s, re.M)
assert at
s = s[:at.end()] + "\n\n.extern .shared .align 16 .b8 bonsai_flash_shared[];" + s[at.end():]
for name in names:
    s = s.replace(name, "bonsai_flash_shared")
p.write_text(s)
