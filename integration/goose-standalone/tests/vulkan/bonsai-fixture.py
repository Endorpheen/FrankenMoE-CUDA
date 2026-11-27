"""Extract the first Bonsai FFN's original packed weights for Vulkan checks."""

from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile


model, out = map(Path, sys.argv[1:])
root = Path(__file__).resolve().parents[4]
inventory = root / "experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/inventory.py"
with tempfile.TemporaryDirectory() as work:
    desc = Path(work) / "inventory.json"
    subprocess.run([sys.executable, inventory, model, desc], check=True)
    parsed = json.loads(desc.read_text())

meta = parsed["metadata"]
assert meta["prism.hadamard.sign_widths"] == [5120, 6144, 17408]
signs = meta["prism.hadamard.sign_values"]
assert len(signs) == 28672 and set(signs) == {-1, 1}
tensors = {item["name"]: item for item in parsed["tensors"]}

with model.open("rb") as src, out.open("wb") as target:
    target.write(struct.pack("<28672i", *signs))
    for name, width, rows in (("gate", 5120, 17408), ("up", 5120, 17408), ("down", 17408, 5120)):
        tensor = tensors[f"blk.0.ffn_{name}.weight"]
        assert tensor["type"] == 142 and tensor["shape"] == [width, rows]
        src.seek(parsed["data_offset"] + tensor["offset"])
        count = width // 128 * rows * 34
        weights = src.read(count)
        assert len(weights) == count
        target.write(weights)
