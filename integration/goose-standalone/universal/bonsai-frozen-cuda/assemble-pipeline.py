from pathlib import Path
import re
import sys

source, scratch = map(Path, sys.argv[1:])
entry_pattern = re.compile(r'^\.visible \.entry (\w+)\(.*?^}', re.M | re.S)
entries = {}
for module in ("main", "flash", "unpack"):
    ptx = (scratch / f"{module}.ptx").read_text()
    entries[module] = {match[1]: match[0] for match in entry_pattern.finditer(ptx)}

flash = (scratch / "flash.ptx").read_text()
header = "\n".join(line for line in flash.splitlines()
                   if line.startswith((".version ", ".target ", ".address_size ", ".extern .shared ")))
sections = [header]
for line in (source / "pipeline-entries.txt").read_text().splitlines():
    module, symbol = line.split()
    sections.append(entries[module][symbol])
(scratch / "pipeline.ptx").write_text("\n\n".join(sections) + "\n")
