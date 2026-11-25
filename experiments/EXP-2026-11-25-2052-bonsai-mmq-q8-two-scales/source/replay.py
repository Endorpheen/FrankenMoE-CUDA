"""Compare full logits at the first response divergence on the same tokens."""

import json
from pathlib import Path
import sys


case = Path(sys.argv[3])
ids = [[json.loads(line)["token"] for line in (case / f"{mode}.jsonl").read_text().splitlines()]
       for mode in ("f16", "int8")]
first = next(i for i, (a, b) in enumerate(zip(*ids)) if a != b)
sys.argv.extend((str(first), str(ids[1][first])))

experiments = Path(__file__).resolve().parents[2]
replay = experiments / "EXP-2026-11-25-1996-bonsai-pq2-q8-residual/source/check-history.py"
source = replay.read_text()
anchor = "'BONSAI_PQ2_MMQ_INT8':mode"
assert source.count(anchor) == 1
source = source.replace(anchor, "'BONSAI_FFN_Q8_64':mode")
exec(compile(source, str(replay), "exec"), {"__file__": str(replay), "__name__": "__main__"})
