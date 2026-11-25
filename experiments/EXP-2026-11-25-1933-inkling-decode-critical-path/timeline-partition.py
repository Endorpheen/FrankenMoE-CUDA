from collections import Counter, defaultdict
from pathlib import Path
import json
import sqlite3

ROOT = Path(__file__).resolve().parent
LABEL = {1: "H2D", 2: "D2H", 8: "D2D"}


def partition(a, b, spans):
    events = defaultdict(Counter)
    for kind, lo, hi in spans:
        lo, hi = max(a, lo), min(b, hi)
        if lo < hi:
            events[lo][kind] += 1
            events[hi][kind] -= 1
    active = Counter()
    output = Counter()
    for t0, t1 in zip(sorted({a, b, *events}), sorted({a, b, *events})[1:]):
        active.update(events[t0])
        live = tuple(sorted(k for k, n in active.items() if n > 0))
        key = live[0] if len(live) == 1 else "overlap" if live else "no GPU activity"
        output[key] += t1 - t0
    return {k: ns / 1e6 for k, ns in output.items()}
