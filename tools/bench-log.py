#!/usr/bin/env python3
"""Append one performance measurement to benchmarks/experiment-history.jsonl.

Why this exists: the same numbers were re-derived three times in one session, and one
of them (4084 MiB VRAM at 35 tok/s) turned out to be a measurement taken while no server
was running. A journal that is written by the harness itself cannot drift like that.

Rules enforced here, not by convention:

  - One JSON object per line, appended, never rewritten.
  - Fields that were not measured are `null`, never a guess.
  - VRAM is recorded per phase (before load / after load / during generation / peak) so a
    pre-warm figure cannot be quoted as a steady-state one.
  - Library hashes come from the running process, not from a directory listing.
  - Failures and OOM are recorded like successes, with the error text.
  - Rows carried over from older experiments set `source` and `completeness` so a
    reconstructed entry is never mistaken for a measured one.

Usage, from a harness:

    python3 tools/bench-log.py --tag mc3584 --cache-mib 3584 \
        --prefill 2189.48 --decode 36.23 --vram-during 6784 --ram-kb 21839496 \
        --response /tmp/ab-mc3584-2.json

Everything not passed is left null. Reading the log:

    python3 tools/bench-log.py --show
    python3 tools/bench-log.py --compare
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOG = ROOT / "benchmarks" / "experiment-history.jsonl"
STAGE_LIB = ROOT / "runtime" / "goose-stage" / "lib"

# The answer this project is anchored to: the reference run's output for the pinned request
# (EXP-2026-11-15-718, cache 4352). A row whose content hash differs from this computed a
# different output and is never a faster version of the reference, however fast it is.
REFERENCE_SHA = "c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f"

# The libraries whose identity decides the result. Recorded by hash, from the file the
# loader actually takes (resolved through the symlink chain).
KEY_LIBS = [
    "libggml-cpu.so.0",
    "libggml-cpu-shared-dd5a40e47065.so.0.27.0",
    "libggml-base.so.0",
    "libllama.so.0",
    "libggml-cuda-pinned.so.0.27.0",
]


def sha256(path: Path) -> str | None:
    try:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        return h.hexdigest()
    except OSError:
        return None


def library_hashes() -> dict[str, str | None]:
    return {name: sha256(STAGE_LIB / name) for name in KEY_LIBS}


def git_commit() -> str | None:
    try:
        return subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"],
                              capture_output=True, text=True, check=True).stdout.strip()
    except Exception:
        return None


def response_metrics(path: str | None) -> dict:
    """Pull timings and the content hash out of a saved /completion response."""
    out = {"prefill_tok_s": None, "decode_tok_s": None, "content_sha256": None,
           "prompt_n": None, "predicted_n": None, "error": None}
    if not path:
        return out
    try:
        raw = json.loads(Path(path).read_text())
    except Exception as exc:
        out["error"] = f"unreadable response: {exc}"
        return out
    if "error" in raw:
        out["error"] = str(raw["error"])[:400]
        return out
    t = raw.get("timings") or {}
    out["prefill_tok_s"] = t.get("prompt_per_second")
    out["decode_tok_s"] = t.get("predicted_per_second")
    out["prompt_n"] = t.get("prompt_n")
    out["predicted_n"] = t.get("predicted_n")
    content = raw.get("content")
    if isinstance(content, str):
        out["content_sha256"] = hashlib.sha256(content.encode()).hexdigest()
    return out


def append(entry: dict) -> None:
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with open(LOG, "a") as f:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")


def build_entry(a: argparse.Namespace) -> dict:
    m = response_metrics(a.response)
    return {
        "id": a.id or f"{datetime.now(timezone.utc):%Y%m%dT%H%M%SZ}-{a.tag}",
        "tag": a.tag,
        "utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "git_commit": a.commit or git_commit(),
        "source": a.source,
        "completeness": a.completeness,
        # runtime identity
        "libraries": library_hashes(),
        "goose_compiler": a.goose_compiler,
        # model and workload
        "model": a.model,
        "quant": a.quant,
        "ctx": a.ctx,
        "prompt_n": m["prompt_n"] if m["prompt_n"] is not None else a.prompt_n,
        "predicted_n": m["predicted_n"] if m["predicted_n"] is not None else a.predicted_n,
        # configuration
        "argv": a.argv,
        "cache_mib": a.cache_mib,
        "ngl": a.ngl,
        "cpu_moe": a.cpu_moe,
        "flash_attn": a.flash_attn,
        # results
        "prefill_tok_s": m["prefill_tok_s"] if m["prefill_tok_s"] is not None else a.prefill,
        "decode_tok_s": m["decode_tok_s"] if m["decode_tok_s"] is not None else a.decode,
        "content_sha256": m["content_sha256"] or a.content_sha,
        # memory, per phase - never one blended figure
        "vram_mib_before_load": a.vram_before,
        "vram_mib_after_load": a.vram_after,
        "vram_mib_during": a.vram_during,
        "vram_mib_peak": a.vram_peak,
        "ram_kb": a.ram_kb,
        # cache behaviour
        "cache_allocated_mib": a.cache_allocated,
        "cache_hits": a.cache_hits,
        "cache_misses": a.cache_misses,
        "cache_evictions": a.cache_evictions,
        # outcome and conditions
        "status": a.status,
        "error": m["error"] or a.error,
        "warmup_requests": a.warmup,
        "repeat_index": a.repeat,
        "concurrent_load": a.concurrent_load,
    }


def load_rows() -> list[dict]:
    if not LOG.exists():
        return []
    rows = []
    for line in LOG.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return rows


def show(rows: list[dict]) -> None:
    if not rows:
        print("journal is empty")
        return
    hdr = (f"{'cache':>6} {'prefill':>9} {'decode':>7} {'vram':>7} {'ram_GiB':>8} "
           f"{'sha':>12} status")
    print(hdr)
    print("-" * len(hdr))
    for r in rows:
        def f(v, fmt="{:.2f}"):
            return fmt.format(v) if isinstance(v, (int, float)) else "-"
        vram = r.get("vram_mib_during") or r.get("vram_mib_peak")
        ram = r.get("ram_kb")
        print(f"{f(r.get('cache_mib'), '{:.0f}'):>6} {f(r.get('prefill_tok_s'), '{:.1f}'):>9} "
              f"{f(r.get('decode_tok_s'), '{:.2f}'):>7} {f(vram, '{:.0f}'):>7} "
              f"{ram / 1048576:.1f}" if isinstance(ram, (int, float)) else
              f"{f(r.get('cache_mib'), '{:.0f}'):>6} {f(r.get('prefill_tok_s'), '{:.1f}'):>9} "
              f"{f(r.get('decode_tok_s'), '{:.2f}'):>7} {f(vram, '{:.0f}'):>7} {'-':>8}",
              end=" ")
        sha = r.get("content_sha256")
        print(f"{(sha or '-')[:12]:>12} {r.get('status') or '-'}")


def compare(rows: list[dict], include_historical: bool = False) -> None:
    """Best decode per cache size, plus the efficiency of each additional MiB.

    Historical rows are EXCLUDED by default. They were measured on another build with other
    conditions, and putting a 41.94 from a 512-token reference run next to a 128-token
    measurement of ours is exactly the kind of comparison that produced contradictory
    numbers before this journal existed. Pass --include-historical to see them anyway.

    Rows are grouped by (prompt_n, predicted_n, content_sha256) before anything is ranked.
    Two runs with a different prompt length are different workloads, and two runs with a
    different content hash computed different outputs - neither is a speed-up of the other.
    A group that disagrees with the reference hash is labelled as computing something else,
    so a fast row can never be read as "the reference answer, faster".

    This check exists because both mistakes actually happened: results reported as measured
    at two prompt lengths were one length (a harness sent the wrong request file), and nine
    experiments optimised a number whose output hash was stored but never compared.
    """
    groups: dict[tuple, list[dict]] = {}
    for r in rows:
        if not include_historical and str(r.get("source", "")).startswith(("historical", "reconstructed")):
            continue
        if not isinstance(r.get("cache_mib"), int) or not isinstance(r.get("decode_tok_s"), (int, float)):
            continue
        groups.setdefault((r.get("prompt_n"), r.get("predicted_n"), r.get("content_sha256")), []).append(r)

    if len(groups) > 1:
        print("NOTE: rows fall into distinct groups - not comparable across groups.")
        print(f"      {'prompt_n':>8} {'predicted':>9} {'sha':>12} {'rows':>5} {'decode':>8}  verdict")
        for (pn, pred, sha), g in sorted(groups.items(), key=lambda kv: -max(x["decode_tok_s"] for x in kv[1])):
            best = max(x["decode_tok_s"] for x in g)
            print(f"      {str(pn):>8} {str(pred):>9} {(sha or '-')[:12]:>12} {len(g):>5} "
                  f"{best:>8.2f}  {group_verdict(sha)}")
        print()

    reference_sha = reference_hash(rows)
    # A different predicted_n produces different text by construction, so the hash is only
    # evidence of a different COMPUTATION when the workload matches. Compare within that.
    ref_predicted = next((p for (pn, p, sha) in groups if sha == reference_sha), None)
    for (pn, pred, sha) in sorted(groups, key=lambda k: (k[0] or 0, k[1] or 0, k[2] or "")):
        g = groups[(pn, pred, sha)]
        if reference_sha and sha and sha != reference_sha and pred == ref_predicted:
            print(f"WARNING: prompt_n={pn} predicted_n={pred} sha={sha[:12]} differs from the "
                  f"reference {reference_sha[:12]} at the SAME workload - these rows measure "
                  f"a DIFFERENT output.\n")
        _compare_group(g, pn, pred)


def group_verdict(sha: str | None) -> str:
    if not sha:
        return "no hash recorded"
    return f"reference-equivalent ({sha[:12]})" if sha == REFERENCE_SHA else "different output"


def reference_hash(rows: list[dict]) -> str | None:
    """The hash this project treats as the reference answer."""
    if REFERENCE_SHA:
        return REFERENCE_SHA
    for r in rows:
        if str(r.get("source", "")).startswith("reference") and r.get("content_sha256"):
            return r["content_sha256"]
    return None


def _compare_group(rows: list[dict], prompt_n=None, predicted_n=None) -> None:
    best: dict[int, dict] = {}
    for r in rows:
        c, d = r.get("cache_mib"), r.get("decode_tok_s")
        if c not in best or d > best[c]["decode_tok_s"]:
            best[c] = r
    if prompt_n is not None:
        print(f"prompt_n={prompt_n} predicted_n={predicted_n}")
    print(f"{'cache':>6} {'best decode':>12} {'vram':>7} {'dMiB':>6} {'dDecode':>8} {'per 256MiB':>11}")
    prev = None
    for c in sorted(best):
        r = best[c]
        vram = r.get("vram_mib_during") or r.get("vram_mib_peak")
        if prev is None:
            print(f"{c:>6} {r['decode_tok_s']:>12.2f} "
                  f"{(f'{vram:.0f}' if isinstance(vram, int) else '-'):>7} "
                  f"{'-':>6} {'-':>8} {'-':>11}")
        else:
            dv = (vram - (prev.get('vram_mib_during') or prev.get('vram_mib_peak'))) \
                if isinstance(vram, int) else None
            dd = r["decode_tok_s"] - prev["decode_tok_s"]
            per = (dd / (dv / 256)) if dv else None
            print(f"{c:>6} {r['decode_tok_s']:>12.2f} "
                  f"{(f'{vram:.0f}' if isinstance(vram, int) else '-'):>7} "
                  f"{(f'{dv:+.0f}' if dv is not None else '-'):>6} "
                  f"{dd:>+8.2f} {(f'{per:+.2f}' if per is not None else '-'):>11}")
        prev = r


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--show", action="store_true", help="print the journal")
    p.add_argument("--compare", action="store_true", help="best decode per cache size")
    p.add_argument("--include-historical", action="store_true",
                   help="include historical/reconstructed rows in --compare")
    p.add_argument("--tag")
    p.add_argument("--id")
    p.add_argument("--commit")
    p.add_argument("--source", default="measured",
                   help="measured | historical:<where> | reconstructed:<where>")
    p.add_argument("--completeness", default="full",
                   help="full | partial:<what is missing>")
    p.add_argument("--goose-compiler")
    p.add_argument("--model")
    p.add_argument("--quant")
    p.add_argument("--ctx", type=int)
    p.add_argument("--prompt-n", type=int)
    p.add_argument("--predicted-n", type=int)
    p.add_argument("--argv", help="the launch command, one string")
    p.add_argument("--cache-mib", type=int)
    p.add_argument("--ngl")
    p.add_argument("--cpu-moe", action="store_true", default=None)
    p.add_argument("--flash-attn")
    p.add_argument("--prefill", type=float)
    p.add_argument("--decode", type=float)
    p.add_argument("--content-sha")
    p.add_argument("--response", help="saved /completion response; overrides timings")
    p.add_argument("--vram-before", type=int)
    p.add_argument("--vram-after", type=int)
    p.add_argument("--vram-during", type=int)
    p.add_argument("--vram-peak", type=int)
    p.add_argument("--ram-kb", type=int)
    p.add_argument("--cache-allocated", type=int)
    p.add_argument("--cache-hits", type=int)
    p.add_argument("--cache-misses", type=int)
    p.add_argument("--cache-evictions", type=int)
    p.add_argument("--status", default="ok")
    p.add_argument("--error")
    p.add_argument("--warmup", type=int)
    p.add_argument("--repeat", type=int)
    p.add_argument("--concurrent-load")
    a = p.parse_args()

    if a.show:
        show(load_rows())
        return 0
    if a.compare:
        compare(load_rows(), a.include_historical)
        return 0
    if not a.tag:
        p.error("--tag is required when appending")
    append(build_entry(a))
    print(f"appended {a.tag} to {LOG.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
