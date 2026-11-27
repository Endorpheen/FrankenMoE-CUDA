import re
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
s, version = re.subn(r"^\.version 8\.3$", ".version 8.4", s, flags=re.M)
assert version == 1
name = "bonsai_flash_kv_fused_ordered_$_panel_scratch"
s, count = re.subn(r"^\s*\.shared \.align 16 \.b8 " + re.escape(name) + r"\[50176\];$", "", s, flags=re.M)
assert count == 1
s = s.replace(name, "bonsai_flash_shared")
name = "bonsai_flash_fp8_qk_$_panel_scratch"
s, count = re.subn(r"^\s*\.shared \.align 16 \.b8 " + re.escape(name) + r"\[41472\];$", "", s, flags=re.M)
assert count == 1
s = s.replace(name, "bonsai_flash_shared")
name = "bonsai_flash_fp8_qkpv_$_panel_scratch"
s, count = re.subn(r"^\s*\.shared \.align 16 \.b8 " + re.escape(name) + r"\[41472\];$", "", s, flags=re.M)
assert count == 1
p.write_text(s.replace(name, "bonsai_flash_shared"))
