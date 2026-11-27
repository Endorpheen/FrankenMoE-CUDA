"""Generate an Apex router fixture from the GGUF's real weights and embeddings."""

from pathlib import Path
import sys

from gguf import GGUFReader, GGMLQuantizationType as Quant, dequantize
import numpy as np


def main():
    apex, output = map(Path, sys.argv[1:])
    reader = GGUFReader(apex)
    embedding = next(t for t in reader.tensors if t.name == "token_embd.weight")
    router = next(t for t in reader.tensors if t.name == "blk.0.ffn_gate_inp.weight")
    assert embedding.tensor_type == Quant.Q4_K and router.tensor_type == Quant.F32
    assert router.data.shape == (256, 2048)
    inputs = np.zeros((4, 2048), dtype="<f4")
    inputs[:2] = dequantize(embedding.data[[17, 1511]], embedding.tensor_type)
    inputs[3, 0] = np.nan

    weights = router.data.astype("<f4")
    scores = inputs[:3].astype(np.float64) @ weights.T.astype(np.float64)
    probs = np.exp(scores - scores.max(axis=1, keepdims=True))
    probs /= probs.sum(axis=1, keepdims=True)
    ids = np.full((4, 8), 0xffffffff, dtype="<u4")
    ids[:3] = np.argsort(-probs, axis=1, kind="stable")[:, :8]
    picked = np.take_along_axis(probs, ids[:3], axis=1)
    reference = np.zeros((4, 8), dtype="<f4")
    reference[:3] = (picked / np.maximum(picked.sum(axis=1, keepdims=True), 2**-14)) * 0.75
    with output.open("wb") as file:
        for array in (inputs, weights, ids, reference):
            file.write(array.tobytes())


if __name__ == "__main__":
    main()
