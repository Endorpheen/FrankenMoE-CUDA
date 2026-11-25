"""Extract and validate model JSONL from an Nsight combined process output."""
import json
from pathlib import Path
import re
import sys

log, oracle, output = map(Path, sys.argv[1:])
text = log.read_text()
text = re.sub(r"(?:mtp_blocks|profile_decode_cpu_ms|prompt_n)=[^\n]*\n", "", text)
rows = re.findall(r'^\{"step":\d+,"token":\d+,"logit":[^\n]+?\}$', text, re.M)
assert [json.loads(row)["step"] for row in rows] == list(range(512))
result = "\n".join(rows) + "\n"
assert result == oracle.read_text()
output.write_text(result)
