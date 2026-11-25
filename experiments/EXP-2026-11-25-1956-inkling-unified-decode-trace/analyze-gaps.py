"""Group disjoint GPU gaps by their neighboring activities in the decode tail."""

from collections import Counter
import json
from pathlib import Path
import sqlite3
import sys


def copy_name(direction: int, size: int) -> str:
    if direction == 1 and size in (2820096, 1492992):
        return "expert H2D"
    if direction == 1 and size == 9216:
        return "activation H2D"
    if direction == 2:
        return {
            32: "routing ID D2H",
            9216: "activation D2H",
            804096: "logits D2H",
        }.get(size, "other D2H")
    return f"other copy ({direction})"


def decode_gaps(database: Path, steps: int = 128) -> dict:
    with sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True) as db:
        logits = [row[0] for row in db.execute(
            "SELECT end FROM CUPTI_ACTIVITY_KIND_MEMCPY "
            "WHERE copyKind=2 AND bytes=804096 ORDER BY end"
        )]
        if len(logits) < steps + 1:
            raise ValueError("Too few logits readbacks for the selected decode tail")
        lo, hi = logits[-steps - 1], logits[-1]
        spans = [(start, end, copy_name(direction, size)) for start, end, direction, size in db.execute(
            "SELECT start, end, copyKind, bytes FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start<? AND end>?",
            (hi, lo),
        )]
        spans.extend(db.execute(
            "SELECT k.start, k.end, s.value FROM CUPTI_ACTIVITY_KIND_KERNEL k "
            "JOIN StringIds s ON s.id=k.shortName WHERE k.start<? AND k.end>?",
            (hi, lo),
        ))

    transitions = Counter()
    calls = Counter()
    last_end, last_name = lo, "logits D2H"
    for start, end, name in sorted(spans):
        if start > last_end:
            transitions[last_name, name] += start - last_end
            calls[last_name, name] += 1
        if end > last_end:
            last_end, last_name = end, name
    assert last_end == hi
    return {
        "steps": steps,
        "captured_logit_copies": len(logits),
        "idle_ms_per_step": sum(transitions.values()) / steps / 1e6,
        "idle_by_transition": [
            {"after": a, "before": b, "calls_per_step": calls[a, b] / steps,
             "ms_per_step": ns / steps / 1e6}
            for (a, b), ns in transitions.most_common()
        ],
    }


if __name__ == "__main__":
    print(json.dumps(decode_gaps(Path(sys.argv[1])), indent=2))
