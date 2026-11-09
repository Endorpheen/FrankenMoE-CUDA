#!/usr/bin/env python3
"""Check an Apex run against the permanent acceptance criteria.

Owner directive 2026-11-08: every remaining stage must clear
    prefill >= 2500 tok/s
    decode  >=  25 tok/s
and must not regress more than 2% against the previously accepted stage.

Both conditions apply at once. Clearing the floors but regressing >2% is a reject.
Being within 2% but under a floor is a reject, because the floor is absolute.

Correctness is a gate ahead of speed and is checked first: exact token IDs, exact
saved probabilities, identical content hash. A faster run that changes any of
them is a failure.

Usage:
    check_acceptance.py --run <summary.json> [--previous <summary.json>]
                        [--reference <summary.json>] [--json]

Exit code 0 = accepted, 1 = rejected, 2 = input error.
"""

import argparse
import hashlib
import json
import sys

PREFILL_FLOOR = 2500.0
DECODE_FLOOR = 25.0
MAX_REGRESSION_PCT = 2.0


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def metrics(doc):
    """Pull throughput out of a summary, whether timings nest it or not."""
    t = doc.get("timings") or {}
    pp = t.get("prompt_per_second")
    dc = t.get("predicted_per_second")
    if pp is None or dc is None:
        raise ValueError("summary has no timings.prompt_per_second / predicted_per_second")
    return float(pp), float(dc)


def correctness(doc):
    """Return (ok, list of failures). Only fields that are present are judged."""
    failures = []
    for key, human in (
        ("tokens_equal", "token IDs differ from the reference"),
        ("completion_probabilities_equal", "saved probabilities differ from the reference"),
        ("content_equal", "output text differs from the reference"),
    ):
        if key in doc and doc[key] is not True:
            failures.append(human)
    return (not failures), failures


def pct_delta(new, old):
    if old == 0:
        return None
    return (new - old) / old * 100.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run", required=True)
    ap.add_argument("--previous", help="previously accepted stage for the 2%% rule")
    ap.add_argument("--reference", help="wholly-native reference, informational")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    try:
        run = load(args.run)
        pp, dc = metrics(run)
        prev = load(args.previous) if args.previous else None
        ref = load(args.reference) if args.reference else None
        prev_pp, prev_dc = metrics(prev) if prev else (None, None)
        ref_pp, ref_dc = metrics(ref) if ref else (None, None)
    except (OSError, ValueError, KeyError) as e:
        print(f"input error: {e}", file=sys.stderr)
        return 2

    rows = []
    rejected = []

    ok_corr, corr_failures = correctness(run)
    if not ok_corr:
        rejected.extend(corr_failures)

    if pp < PREFILL_FLOOR:
        rejected.append(f"prefill {pp:.2f} below floor {PREFILL_FLOOR:.0f}")
    rows.append(("prefill", pp, f">= {PREFILL_FLOOR:.0f}", pp >= PREFILL_FLOOR))

    if dc < DECODE_FLOOR:
        rejected.append(f"decode {dc:.2f} below floor {DECODE_FLOOR:.0f}")
    rows.append(("decode", dc, f">= {DECODE_FLOOR:.0f}", dc >= DECODE_FLOOR))

    if prev is not None:
        d_pp = pct_delta(pp, prev_pp)
        d_dc = pct_delta(dc, prev_dc)
        if d_pp is not None:
            bad = d_pp < -MAX_REGRESSION_PCT
            if bad:
                rejected.append(f"prefill regressed {d_pp:+.2f}% vs previous")
            rows.append(("prefill vs previous", pp, f"delta {d_pp:+.2f}% > -{MAX_REGRESSION_PCT:.0f}%", not bad))
        if d_dc is not None:
            bad = d_dc < -MAX_REGRESSION_PCT
            if bad:
                rejected.append(f"decode regressed {d_dc:+.2f}% vs previous")
            rows.append(("decode vs previous", dc, f"delta {d_dc:+.2f}% > -{MAX_REGRESSION_PCT:.0f}%", not bad))

    verdict = "ACCEPTED" if not rejected else "REJECTED"

    if args.json:
        out = {
            "verdict": verdict,
            "reasons": rejected,
            "run": {"prefill": pp, "decode": dc},
            "previous": None if prev is None else {"prefill": prev_pp, "decode": prev_dc},
            "reference": None if ref is None else {"prefill": ref_pp, "decode": ref_dc},
            "floors": {"prefill": PREFILL_FLOOR, "decode": DECODE_FLOOR},
            "max_regression_pct": MAX_REGRESSION_PCT,
            "correctness_ok": ok_corr,
        }
        print(json.dumps(out, indent=2))
        return 0 if verdict == "ACCEPTED" else 1

    print(f"{'check':<22}{'value':>12}{'requirement':>26}   result")
    print("-" * 74)
    for name, value, req, good in rows:
        print(f"{name:<22}{value:>12.2f}{req:>26}   {'ok' if good else 'FAIL'}")
    if ref is not None:
        print(f"{'reference pp/decode':<22}{ref_pp:>12.2f}{ref_dc:>26.2f}   informational")
    print()
    print(f"correctness: {'ok' if ok_corr else 'FAIL'}")
    print(f"verdict: {verdict}")
    for r in rejected:
        print(f"  - {r}")
    return 0 if verdict == "ACCEPTED" else 1


if __name__ == "__main__":
    sys.exit(main())
