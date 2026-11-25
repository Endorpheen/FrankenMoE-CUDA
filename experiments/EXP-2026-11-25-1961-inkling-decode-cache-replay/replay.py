"""Replay a complete Inkling decode route trace without running CUDA."""

from collections import defaultdict
from pathlib import Path
import argparse
import json
import re

EXPERTS_PER_LAYER = 128
VISITS_PER_TOKEN = 48
EXPERT_BYTES = 2820096 + 1492992
INFINITY = 1 << 31
ROUTE = re.compile(r"I2 trace route token=(\d+) visit=(\d+) layer=(\d+) mask=([\da-f]{32})")


def read_trace(path):
    initial, groups, start_token = None, [], None
    for line in path.read_text().splitlines():
        if line.startswith("I2 trace initial slots="):
            if initial is not None:
                raise ValueError("Expected one request")
            head, *values = line.split()
            del head
            slots, *keys = values[2:]
            capacity = int(slots.split("=")[1])
            initial = {int(key) for key in keys if int(key) >= 0}
            assert len(keys) == capacity and len(initial) <= capacity
        elif line.startswith("I2 trace route "):
            match = ROUTE.fullmatch(line)
            assert match
            token, visit, layer = map(int, match.groups()[:3])
            if start_token is None:
                start_token = token
            assert visit == len(groups) % VISITS_PER_TOKEN
            assert token - start_token == len(groups) // VISITS_PER_TOKEN
            active = int(match[4], 16)
            group = tuple(layer * EXPERTS_PER_LAYER + i for i in range(EXPERTS_PER_LAYER) if active >> i & 1)
            assert len(group) == 8
            groups.append(group)
    assert initial is not None and groups and len(groups) % VISITS_PER_TOKEN == 0
    visits = {key for group in groups for key in group}
    assert max(visits) < 24 * EXPERTS_PER_LAYER
    return capacity, initial, groups


def replay(capacity, initial, groups, policy):
    if capacity < max(map(len, groups)) or capacity < len(initial):
        raise ValueError("Cache cannot hold the recorded initial contents or active group")
    cache = set(initial)
    last = defaultdict(lambda: -1)
    frequency = defaultdict(int)
    decay = int(policy.split("-", 1)[1]) if policy.startswith("frequency-") else None
    if policy == "offline":
        future = {}
        next_keys = []
        for i in range(len(groups) - 1, -1, -1):
            group = groups[i]
            next_keys.append({key: future.get(key, INFINITY) for key in group})
            future.update(dict.fromkeys(group, i))
        next_keys.reverse()

    fills_by_token = []
    recent_visit = defaultdict(list)
    predicted = {}
    for i, group in enumerate(groups):
        if i % VISITS_PER_TOKEN == 0:
            fills_by_token.append(0)
            if policy == "previous-token":
                predicted, recent_visit = recent_visit, defaultdict(list)
            if decay and i and i // VISITS_PER_TOKEN % decay == 0:
                for key in frequency:
                    frequency[key] //= 2
        needed = set(group)
        admits = needed - cache
        fills_by_token[-1] += len(admits)
        evict = max(0, len(cache) + len(admits) - capacity)
        if policy == "offline":
            victims = sorted(cache - needed, key=lambda key: (-future.get(key, INFINITY), key))[:evict] if evict else ()
            future.update(next_keys[i])
        elif policy == "previous-token":
            visit = i % VISITS_PER_TOKEN
            def predicted_use(key):
                guesses = predicted.get(key, ())
                return next((v - visit for v in guesses if v > visit),
                            VISITS_PER_TOKEN + guesses[0] - visit if guesses else INFINITY)
            victims = sorted(cache - needed, key=lambda key: (-predicted_use(key), last[key], key))[:evict] if evict else ()
            for key in needed:
                recent_visit[key].append(visit)
        elif decay is not None:
            victims = sorted(cache - needed, key=lambda key: (frequency[key], last[key], key))[:evict] if evict else ()
            for key in needed:
                frequency[key] += 1
        else:
            victims = sorted(cache - needed, key=lambda key: (last[key], key))[:evict] if evict else ()
        cache.difference_update(victims)
        cache.update(needed)
        last.update(dict.fromkeys(needed, i))

    fills = sum(fills_by_token)
    return {"fills": fills, "upload_bytes": fills * EXPERT_BYTES,
            "fills_per_token": fills_by_token}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server_log", type=Path)
    parser.add_argument("--measured-fills", type=int, required=True)
    args = parser.parse_args()
    capacity, initial, groups = read_trace(args.server_log)
    result = {"slots": capacity, "initial_resident": len(initial),
              "decode_steps": len(groups) // VISITS_PER_TOKEN, "route_groups": len(groups),
              "distinct_experts": len({key for group in groups for key in group}),
              "bytes_per_fill": EXPERT_BYTES,
              "measured": {"fills": args.measured_fills, "upload_bytes": args.measured_fills * EXPERT_BYTES},
              "replay": {}}
    for policy in ("lru", "previous-token", "frequency-0", "frequency-2", "frequency-8", "frequency-32", "offline"):
        result["replay"][policy] = replay(capacity, initial, groups, policy)
    for slots in (capacity, capacity + 128, capacity + 256, capacity + 384):
        item = replay(slots, initial, groups, "offline")
        result.setdefault("oracle_capacity_sweep", {})[slots] = item["fills"]
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()
