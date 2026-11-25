"""Compare the exact request kernels, excluding initialization and warmup."""
import json
import sqlite3
import sys
from pathlib import Path

prism, goose = map(Path, sys.argv[1:])


def read(path, kind):
    with sqlite3.connect(f"file:{path / 'trace.sqlite'}?mode=ro", uri=True) as db:
        rows = db.execute("""select s.value,k.gridX,k.start,k.end,d.value
            from CUPTI_ACTIVITY_KIND_KERNEL k
            join StringIds s on s.id=k.shortName join StringIds d on d.id=k.demangledName
            order by k.start""").fetchall()
    if kind == "prism":
        mmq = next(i for i, r in enumerate(rows) if r[0] == "quantize_mmq_q8_1")
        first = max(i for i, r in enumerate(rows[:mmq]) if r[0] == "k_get_rows")
        vocab, grid = "mul_mat_vec_q", 248320
    else:
        first = next(i for i, r in enumerate(rows) if r[0] == "bonsai_embed_batch")
        vocab, grid = "bonsai_pq2_matvec_rows2", 31040
    vocab_indices = [i for i in range(first, len(rows)) if rows[i][:2] == (vocab, grid)]
    assert len(vocab_indices) == 16, (kind, vocab_indices)
    boundary = vocab_indices[0] + 1
    result = json.loads((path / "response.json").read_text())
    report = {"timings": result["timings"], "ids": result["tokens"]}

    for label, phase in (("prefill", rows[first:boundary]), ("decode", rows[boundary:vocab_indices[-1]+1])):
        kernels = {}
        for name, _, lo, hi, instance in phase:
            entry = kernels.setdefault(name, {"calls": 0, "ns": 0, "instances": {}})
            entry["calls"] += 1
            entry["ns"] += hi - lo
            entry["instances"][instance] = entry["instances"].get(instance, 0) + 1
        report[label] = {"gpu_span_ns": phase[-1][3] - phase[0][2], "kernels": kernels}
    return report


r = {"prism": read(prism, "prism"), "goose": read(goose, "goose")}
assert r["prism"]["ids"] == r["goose"]["ids"]
print(json.dumps(r, indent=2))
