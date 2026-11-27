from pathlib import Path
import re
import sys

source, output = map(Path, sys.argv[1:3])
code = source.read_text()
prefix, body = code.split("/* ---- prototypes ---- */\n", 1)
body = body.split("\nstatic void gs_init_globals(void)", 1)[0].rstrip()
types = "\n".join(re.findall(r"^typedef struct \{ [^}]* \} a\d+_f32;$", prefix, re.M))
output.write_text(f"\n{types}\n\n{body}\n\n")
