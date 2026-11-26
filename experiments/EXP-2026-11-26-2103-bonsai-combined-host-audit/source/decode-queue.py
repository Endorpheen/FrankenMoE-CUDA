"""Separate observed decode gaps by the next GPU operation's host API window."""

from collections import Counter
import json
from pathlib import Path
import sqlite3
from statistics import median
import sys


def describe(values):
    return {"count": len(values), "total_ns": sum(values),
            "median_ns": median(values), "max_ns": max(values)}


def main():
    directory = Path(sys.argv[1])
    path = (directory / "trace.sqlite").resolve()
    with sqlite3.connect(path.as_uri() + "?mode=ro", uri=True) as db:
        epoch, = db.execute("SELECT utcEpochNs FROM TARGET_INFO_SESSION_START_TIME").fetchone()
        start, end = [int((directory / marker).read_text()) - epoch
                      for marker in ("request-start-ns", "request-end-ns")]
        readbacks = db.execute("""
            SELECT m.start, m.end, r.start, r.end
            FROM CUPTI_ACTIVITY_KIND_MEMCPY m JOIN CUPTI_ACTIVITY_KIND_RUNTIME r
                ON m.correlationId=r.correlationId
            JOIN StringIds s ON s.id=r.nameId
            WHERE m.copyKind=2 AND m.bytes=12 AND s.value='cuMemcpyDtoH_v2'
              AND m.start >= ? AND m.end <= ? ORDER BY m.start""", (start, end)).fetchall()
        choices = db.execute("""
            SELECT k.end FROM CUPTI_ACTIVITY_KIND_KERNEL k
             JOIN StringIds s ON s.id=k.shortName
            WHERE s.value='bonsai_choice_final' AND k.start >= ? AND k.end <= ? ORDER BY k.end
            """, (start, end)).fetchall()
        assert len(readbacks) == len(choices) == 511
        full, = db.execute("""
            SELECT end FROM CUPTI_ACTIVITY_KIND_MEMCPY
            WHERE copyKind=2 AND bytes=993280 AND start >= ? AND end <= ?""", (start, end)).fetchall()
        intervals = {"api_entry_to_choice_end": [], "choice_end_to_copy_start": [],
                     "copy_service": [], "copy_end_to_api_return": []}
        for (copy_start, copy_end, api_start, api_end), (choice_end,) in zip(readbacks, choices, strict=True):
            assert api_start <= choice_end <= copy_start <= copy_end <= api_end
            for values, duration in zip(intervals.values(),
                    (choice_end-api_start, copy_start-choice_end, copy_end-copy_start, api_end-copy_end), strict=True):
                values.append(duration)

        decode_bounds = full[0], readbacks[-1][1]
        operations = db.execute("""
            WITH ops(start, end, correlation, name) AS (
                SELECT k.start, k.end, k.correlationId, s.value FROM CUPTI_ACTIVITY_KIND_KERNEL k
                    JOIN StringIds s ON s.id=k.shortName
                    WHERE k.start >= :lo AND k.end <= :hi
                UNION ALL
                SELECT m.start, m.end, m.correlationId, 'copy_' || m.copyKind
                    FROM CUPTI_ACTIVITY_KIND_MEMCPY m WHERE m.start >= :lo AND m.end <= :hi
                UNION ALL
                SELECT m.start, m.end, m.correlationId, 'memset'
                    FROM CUPTI_ACTIVITY_KIND_MEMSET m WHERE m.start >= :lo AND m.end <= :hi
            ),
            host AS (
                SELECT correlationId, min(start) AS api_start, max(end) AS api_end
                    FROM CUPTI_ACTIVITY_KIND_RUNTIME WHERE end > :lo AND start < :hi
                    GROUP BY correlationId
            )
            SELECT ops.start, ops.end, ops.name, host.api_start, host.api_end FROM ops
                LEFT JOIN host ON ops.correlation=host.correlationId
            ORDER BY ops.start, ops.end""", {"lo": decode_bounds[0], "hi": decode_bounds[1]})
        bins, gaps, next_operation, transitions = Counter(), Counter(), Counter(), Counter()
        tail, previous = decode_bounds[0], "first_full_readback"
        for lo, hi, name, api_start, api_end in operations:
            if lo > tail:
                gap = lo - tail
                if api_start is None:
                    label = "no_matching_host_api"
                elif api_end <= tail:
                    label = "host_api_returned_before_gap"
                elif api_start > tail:
                    label = "host_api_started_after_gap_start"
                    bins["before_late_host_api_entry"] += min(api_start, lo) - tail
                    next_operation[name] += min(api_start, lo) - tail
                else:
                    label = "host_api_spans_gap_start"
                bins[label] += gap
                gaps[label] += 1
                transitions[f"{previous} -> {name}"] += gap
            if hi > tail:
                tail = hi
                previous = name
        assert tail == decode_bounds[1]
    result = {
        "readback_intervals": {key: describe(values) for key, values in intervals.items()},
        "gap_durations_ns": dict(bins), "gap_counts": dict(gaps),
        "largest_transitions_gap_ns": dict(transitions.most_common(20)),
        "largest_next_ops_by_gap_before_late_api_ns": dict(next_operation.most_common(20)),
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
