"""Forward the existing generation phase to the Bonsai session only."""
from pathlib import Path
import sys

source, out = map(Path, sys.argv[1:])
s = source.read_text()
header = '#include "session.h"'
step = "bonsai_session_step(H.bonsai, (const uint32_t *)exchange, (unsigned)tokens.len, row)"
assert s.count(header) == s.count(step) == 1
s = s.replace(header, header + "\nint bonsai_session_step_phase(struct bonsai_session *, const uint32_t *, unsigned, float *, int);")
s = s.replace(step, "bonsai_session_step_phase(H.bonsai, (const uint32_t *)exchange, (unsigned)tokens.len, row, !prefill)")
out.write_text(s)
