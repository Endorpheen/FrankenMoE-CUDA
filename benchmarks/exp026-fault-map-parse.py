#!/usr/bin/env python3
# EXP-2026-09-05-026 offline major-fault attribution (clean rewrite).
# Replaces the earlier inline Perl pass whose // operator precedence
# mis-grouped the down+gate+up sum (it returned just the down count).
# Inputs: perf script records (field 5 = faulting file address), /proc maps
# snapshot (start, end, file offset, path), gguf tensor indexes for shard 2/3.
import json
import re
import sys
from collections import Counter

MAPS = "/tmp/exp026-server.maps"
FAULTS = "/tmp/exp026-major-faults.txt"
INDEXES = {
    1: None,  # shard 1 index was not retained; falls back to mapping stats
    2: "/tmp/exp026-shard2-gguf-index.txt",
    3: "/tmp/exp026-shard3-gguf-index.txt",
}
GGUF_DATA_OFFSET = 29632

MAP_RX = re.compile(r"^([0-9a-f]+)-([0-9a-f]+) (\S+) (\S+) \S+ \S+\s*(.*)$")
TENSOR_RX = re.compile(r"tensor\[(\d+)\]: name = (\S+), size = (\d+), offset = (\d+)")


def parse_maps():
    out = []
    for line in open(MAPS):
        m = MAP_RX.match(line.rstrip("\n"))
        if not m:
            continue
        lo, hi = int(m.group(1), 16), int(m.group(2), 16)
        file_off = int(m.group(4), 16)
        out.append((lo, hi, file_off, m.group(5).strip()))
    return out


def parse_indexes():
    out = {}
    for shard, path in INDEXES.items():
        if path is None:
            continue
        tensors = []
        for line in open(path):
            m = TENSOR_RX.search(line)
            if m:
                tensors.append((m.group(2), int(m.group(3)), int(m.group(4))))
        out[shard] = tensors
    return out


def classify(name):
    if "_exps" in name:
        if "ffn_down" in name:
            return "expert_down"
        if "ffn_gate" in name and "_inp" not in name:
            return "expert_gate"
        if "ffn_up" in name:
            return "expert_up"
        return "expert_other"
    return "model_other"


def classify_addr(addr, regions, index):
    for lo, hi, foff, path in regions:
        if lo <= addr < hi:
            if "gguf" not in path:
                return "outside_model_mapping", None
            if "MTP" in path:
                return "mtp_head", None
            if "-00002-of-00003" in path:
                shard = 2
            elif "-00003-of-00003" in path:
                shard = 3
            else:
                shard = 1
            if shard not in index:
                return "model_mapping_no_index", path
            d = (addr - lo) + foff - GGUF_DATA_OFFSET
            for name, size, toff in index[shard]:
                if toff <= d < toff + size:
                    return classify(name), name
            return "model_mapping_no_tensor", path
    return "outside_model_mapping", None
    return "outside_model_mapping", None


def main():
    regions = parse_maps()
    index = parse_indexes()
    total = 0
    by_class = Counter()
    by_shard = Counter()
    top_tensors = Counter()
    for line in open(FAULTS):
        parts = line.split()
        if len(parts) < 6:
            continue
        try:
            addr = int(parts[4], 16)
        except ValueError:
            continue
        total += 1
        cls, name = classify_addr(addr, regions, index)
        by_class[cls] += 1
        if name:
            top_tensors[name] += 1
    expert_total = sum(by_class[k] for k in ("expert_down", "expert_gate", "expert_up"))
    out = {
        "total_faults": total,
        "classes": dict(by_class),
        "expert_total": expert_total,
        "expert_pct": round(100.0 * expert_total / total, 2),
    }
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
