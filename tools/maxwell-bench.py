#!/usr/bin/env python3
"""Benchmark a running FrankenMoE server at three generation lengths.

Same prompt, same server, paired repeats; prints the median per length for
prefill and decode. No model is started here and no GPU is required by the
script itself.
"""
import argparse
import json
import statistics
import urllib.request


def request(url, prompt, n_predict, cache_prompt=False):
    body = {"prompt": prompt, "n_predict": n_predict, "temperature": 0,
            "cache_prompt": cache_prompt}
    req = urllib.request.Request(url + "/completion", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=3600) as r:
        return json.load(r)["timings"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:9101")
    ap.add_argument("--chars", type=int, default=2400, help="prompt length in characters")
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--lengths", default="128,256,512")
    args = ap.parse_args()

    prompt = "The history of computing spans several decades of invention. " * (args.chars // 61 + 1)
    prompt = prompt[:args.chars]
    print(f"prompt_chars={len(prompt)} repeats={args.repeats}")
    rows = []
    for n in [int(x) for x in args.lengths.split(",")]:
        pf, dec = [], []
        for _ in range(args.repeats):
            t = request(args.url, prompt, n)
            pf.append(t["prompt_per_second"])
            dec.append(t["predicted_per_second"])
        rows.append((n, statistics.median(pf), statistics.median(dec), t["prompt_n"]))
        print(f"n={n:4d} prefill={statistics.median(pf):8.2f} tok/s "
              f"decode={statistics.median(dec):7.2f} tok/s "
              f"(ranges {min(pf):.1f}-{max(pf):.1f} / {min(dec):.1f}-{max(dec):.1f})")
    print("median table:")
    for n, pf, dec, pn in rows:
        print(f"  {pn:5d} prompt / {n:3d} generated: prefill {pf:.2f}, decode {dec:.2f}")


if __name__ == "__main__":
    main()
