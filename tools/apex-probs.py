#!/usr/bin/env python3
"""Probability parity: the same request on the shipped path and with the Goose sampler doors switched off.

The generated text can differ for a legitimate reason - two correct summations of the same probabilities can cross a
top-p threshold on different sides - so this compares the **model distribution** instead: greedy decoding with the top
probabilities reported per position. At temperature zero the chain is bypassed, so what is compared is the model's own
distribution, and its hash must be identical.

usage: apex-probs.py <out-dir> [--label name]
"""
import hashlib
import json
import sys
import urllib.request


def ask(base, n_predict=24, n_probs=5):
    prompt = ("The runtime keeps dense weights resident while experts stream from RAM. "
              "Explain in one paragraph why that trade is worth its cost.") * 3
    body = json.dumps({
        "prompt": prompt,
        "n_predict": n_predict,
        "temperature": 0.0,
        "seed": 11,
        "n_probs": n_probs,
        "cache_prompt": False,
    }).encode()
    req = urllib.request.Request(f"{base}/completion", data=body, method="POST",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=900) as r:
        return json.load(r)


def digest(resp):
    """A stable digest of every reported (token, probability) pair, in order."""
    h = hashlib.sha256()
    rows = []
    for entry in resp.get("completion_probabilities") or []:
        top = entry.get("top_probs") or []
        row = []
        for t in top:
            tok, prob = t.get("tok_str", t.get("id")), round(float(t.get("prob", 0.0)), 6)
            h.update(f"{tok}:{prob};".encode())
            row.append((tok, prob))
        rows.append(row)
    h.update(resp.get("content", "").encode())
    return h.hexdigest()[:16], rows


def main():
    out = sys.argv[1]
    label = sys.argv[sys.argv.index("--label") + 1] if "--label" in sys.argv else "run"
    base = sys.argv[sys.argv.index("--base") + 1] if "--base" in sys.argv else "http://127.0.0.1:9101"
    resp = ask(base)
    digest_hex, rows = digest(resp)
    payload = {
        "label": label,
        "digest": digest_hex,
        "positions": len(rows),
        "content_len": len(resp.get("content", "")),
        "first_positions": [[[t[0], t[1]] for t in r] for r in rows[:3]],
        "timings": resp.get("timings"),
    }
    with open(f"{out}/{label}.json", "w") as f:
        json.dump(payload, f, indent=1)
    print(f"{label}: digest={digest_hex} positions={payload['positions']} content_len={payload['content_len']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
