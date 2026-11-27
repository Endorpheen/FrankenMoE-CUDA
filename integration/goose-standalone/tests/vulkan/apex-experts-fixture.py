"""Generate a first-layer Apex routed-expert oracle from the real GGUF."""

from pathlib import Path
import struct
import sys

from gguf import GGUFReader, GGMLQuantizationType as Quant, dequantize
import numpy as np


def main():
    model, output = map(Path, sys.argv[1:])
    reader = GGUFReader(model)
    tensors = {t.name: t for t in reader.tensors}
    embedding = tensors["token_embd.weight"]
    router = tensors["blk.0.ffn_gate_inp.weight"]
    norm = tensors["blk.0.post_attention_norm.weight"].data
    planes = [tensors[f"blk.0.ffn_{part}_exps.weight"] for part in ("gate", "up", "down")]
    assert [t.tensor_type for t in planes] == [Quant.Q4_K, Quant.Q4_K, Quant.Q6_K]

    raw = dequantize(embedding.data[1511:1512], embedding.tensor_type)[0]
    wide = raw.astype(np.float64)
    x = wide * norm / np.sqrt(np.mean(wide**2) + 1e-6)
    scores = router.data.astype(np.float64) @ x
    probs = np.exp(scores - scores.max())
    ids = np.argsort(-probs, kind="stable")[:8]
    weights = probs[ids] / probs[ids].sum()
    branches = []
    for index in ids:
        gate, up, down = [dequantize(t.data[index], t.tensor_type).astype(np.float64) for t in planes]
        logits = gate @ x
        branches.append(down @ ((logits / (1 + np.exp(-logits))) * (up @ x)))
    expected = weights @ np.array(branches)
    with output.open("wb") as file:
        file.write(b"FGMOEE2\0")
        for t in planes:
            file.write(struct.pack("<QQ", t.data_offset, t.data[0].nbytes))
        for array in (raw.astype("<f4"), norm.astype("<f4"), router.data.astype("<f4"),
                      ids.astype("<u4"), expected.astype("<f4")):
            file.write(array.tobytes())


if __name__ == "__main__":
    main()
