"""Generate an Inkling routed-expert oracle from the real GGUF."""

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
    norm = tensors["blk.0.ffn_norm.weight"].data
    bias = tensors["blk.0.exp_probs_b.bias"].data
    fused = tensors["blk.0.ffn_gate_up_exps.weight"]
    down = tensors["blk.0.ffn_down_exps.weight"]
    assert fused.tensor_type == Quant.IQ4_XS and down.tensor_type == Quant.IQ4_NL
    raw = dequantize(embedding.data[1511:1512], embedding.tensor_type)[0]
    wide = raw.astype(np.float64)
    x = wide * norm / np.sqrt(np.mean(wide**2) + 1e-6)
    probability = 1 / (1 + np.exp(-(router.data.astype(np.float64) @ x)))
    ids = np.argsort(-(probability + bias), kind="stable")[:8]
    weights = 8 * probability[ids] / probability[ids].sum()
    branches = []
    for index in ids:
        a = dequantize(fused.data[index], fused.tensor_type).astype(np.float64)
        out = dequantize(down.data[index], down.tensor_type).astype(np.float64)
        gate, up = np.split(a @ x, 2)
        activation = 4 * np.tanh(gate / 4) / (1 + np.exp(-gate)) * (25 * np.tanh(up / 25))
        branches.append(out @ activation)
    with output.open("wb") as file:
        file.write(b"FGMOEI1\0")
        for t in (fused, down):
            file.write(struct.pack("<QQ", t.data_offset, t.data[0].nbytes))
        file.write(struct.pack("<QQ", 0, 0))
        for array in (raw.astype("<f4"), norm.astype("<f4"), router.data.astype("<f4"),
                      bias.astype("<f4"), ids.astype("<u4"), (weights @ np.array(branches)).astype("<f4")):
            file.write(array.tobytes())


if __name__ == "__main__":
    main()
