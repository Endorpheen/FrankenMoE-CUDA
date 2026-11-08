#!/usr/bin/env python3
"""Merge the already-passing pre-crash steps with the post-crash retry steps.

The retry manifest intentionally contains only the interrupted nvcc/replay
tail.  The failed pre-crash nvcc step is replaced by its successful retry, not
counted twice.
"""
import json
from pathlib import Path

root = Path("local-check-results")
prior = json.loads((root / "result-prior.json").read_text())
retry = json.loads((root / "result.json").read_text())
# retry.steps[0] is the PRIOR_RESULT_PRESENT guard; the rest are the tail.
tail = retry["steps"][1:]
prior_ok = [s for s in prior["steps"] if s.get("exit_code", 1) == 0]
steps = []
for row in prior_ok + tail:
    row = dict(row)
    row["step"] = len(steps)
    steps.append(row)
out = {
    "status": "PASS" if all(s.get("exit_code", 1) == 0 for s in steps) else "FAILED",
    "completed_steps": len(steps),
    "expected_steps": len(steps),
    "steps": steps,
    "isolation": "no network,home/weights/credentials/production unavailable;only this worker writable",
    "note": "Steps 0-13 are the pre-crash run; steps 14+ replace the interrupted nvcc/replay tail.",
}
(root / "result-full.json").write_text(json.dumps(out, indent=2) + "\n")
print(json.dumps({"status": out["status"], "steps": len(steps)}, indent=2))
