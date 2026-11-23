from collections import OrderedDict
from pathlib import Path
import json
import struct
import sys

def read_trace(path):
    data = path.read_bytes()
    version, group_words, layer_count, state_words = struct.unpack_from("<4Q", data)
    assert version == 1 and group_words % 9 == 0 and layer_count == 40
    pos = 32
    groups = struct.unpack_from(f"<{group_words}q", data, pos)
    pos += group_words * 8
    layers = struct.unpack_from(f"<{layer_count}q", data, pos)
    pos += layer_count * 8
    state = struct.unpack_from(f"<{state_words}i", data, pos)
    pos += state_words * 4
    records = list(struct.iter_unpack("<4I8B", data[pos:]))
    assert len(records) == 511 * 40 * 3
    return [groups[i:i+9] for i in range(0, len(groups), 9)], layers, state, records

class CacheGroup:
    def __init__(self, layout, state, protected_pct):
        self.width = layout[3]
        self.capacity = layout[5]
        self.cost = sum(layout[:3])
        self.protected_limit = self.capacity * protected_pct // 100
        slots = self.capacity
        start = layout[7]
        self.keys = list(state[start:start + slots])
        self.active = {key: slot for slot, key in enumerate(self.keys) if key >= 0}
        following = state[start + 2*slots:start + 3*slots]
        self.lists = [OrderedDict(), OrderedDict()]
        for segment in (0, 1):
            slot = state[start + 4*slots + segment]
            while slot != -1:
                self.lists[segment][slot] = None
                slot = following[slot]
        assert len(self.lists[0]) + len(self.lists[1]) == slots
        self.balance()

    def balance(self):
        while len(self.lists[1]) > self.protected_limit:
            slot, _ = self.lists[1].popitem(last=False)
            self.lists[0][slot] = None

    def apply(self, layer, ids):
        selected = [layer * self.width + expert for expert in ids]
        live = set(selected)
        for key in selected:
            slot = self.active.get(key)
            if slot is not None:
                if slot in self.lists[0]:
                    self.lists[0].pop(slot)
                else:
                    self.lists[1].pop(slot)
                self.lists[1][slot] = None
                self.balance()
        fills = evictions = 0
        for key in selected:
            if key in self.active:
                continue
            for queue in self.lists:
                slot = next((slot for slot in queue if self.keys[slot] not in live), None)
                if slot is not None:
                    queue.pop(slot)
                    break
            else:
                raise ValueError("No eviction candidate")
            old = self.keys[slot]
            if old >= 0:
                del self.active[old]
                evictions += 1
            self.keys[slot] = key
            self.active[key] = slot
            self.lists[0][slot] = None
            fills += 1
        return fills, evictions

