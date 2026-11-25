"""Check the captured MTP FFN and selected output logits against GGUF math."""
import json
import mmap
from pathlib import Path
import sys

import numpy as np


WIDTH = 5120
VOCAB = 248320
LAYER = "blk.64."


def check_capture(inventory, trace):
    model = json.loads(inventory.read_text())
    tensors = {tensor["name"]: tensor for tensor in model["tensors"]}
    signs = np.array(model["metadata"]["prism.hadamard.sign_values"][:WIDTH])
    epsilon = model["metadata"]["qwen35.attention.layer_norm_rms_epsilon"]
    captured = np.fromfile(trace, dtype="<f4").reshape(-1, VOCAB + 2 * WIDTH)
    logits, residual, head = np.split(captured, [VOCAB, VOCAB + WIDTH], axis=1)
    assert np.isfinite(captured).all()

    with Path(model["file"]).open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
        def packed(name, shape, dtype, offset=0):
            return np.ndarray(shape, dtype=dtype, buffer=weights,
                              offset=model["data_offset"] + tensors[name]["offset"] + offset)

        def norm(x, name):
            scale = packed(LAYER + name + ".weight", (WIDTH,), "<f4")
            return x / np.sqrt(np.mean(x * x, axis=1, keepdims=True) + epsilon) * scale

        def q4_matrix(name, x):
            name = LAYER + name + ".weight"
            width, rows = tensors[name]["shape"]
            assert tensors[name]["type"] == 2 and x.shape[1] == width
            raw = packed(name, (rows, width // 32, 18), "u1")
            output = np.empty((x.shape[0], rows), dtype=np.float64)
            for start in range(0, rows, 128):
                chunk = raw[start:start + 128]
                scale = chunk[:, :, :2].copy().view("<f2").astype(np.float64)
                values = np.concatenate((chunk[:, :, 2:] & 15, chunk[:, :, 2:] >> 4), axis=2)
                dense = ((values.astype(np.float64) - 8) * scale).reshape(-1, width)
                output[:, start:start + len(chunk)] = x @ dense.T
            return output

        normed = norm(residual.astype(np.float64), "post_attention_norm")
        gate = q4_matrix("ffn_gate", normed)
        up = q4_matrix("ffn_up", normed)
        inner = gate / (1 + np.exp(-gate)) * up
        expected_head = norm(residual + q4_matrix("ffn_down", inner), "nextn.shared_head_norm")
        np.testing.assert_allclose(head, expected_head, rtol=5e-5, atol=8e-5)

        rotated = (head.astype(np.float64) * signs).reshape(-1, 1024)
        for size in (1, 2, 4, 8, 16, 32, 64, 128, 256, 512):
            view = rotated.reshape(-1, 2, size)
            a, b = view[:, 0].copy(), view[:, 1].copy()
            view[:, 0], view[:, 1] = a + b, a - b
        rotated = rotated.reshape(-1, WIDTH) / 32
        assert tensors["output.weight"]["type"] == 142
        probes = np.arange(0, VOCAB, 8191)
        for row, observed in enumerate(logits):
            top = np.argsort(observed)[-8:]
            ids = np.union1d(probes, top)
            expected = []
            for token in ids:
                stride = WIDTH // 128 * 34
                quant = packed("output.weight", (WIDTH // 128, 34), "u1", int(token) * stride)
                scale = quant[:, :2].copy().view("<f2").astype(np.float64)
                codes = (quant[:, 2:, None] >> np.array([0, 2, 4, 6], dtype=np.uint8)) & 3
                dense = ((codes.reshape(-1, 128).astype(np.float64) - 1) * scale).ravel()
                expected.append(np.dot(rotated[row], dense))
            expected = np.asarray(expected)
            np.testing.assert_allclose(observed[ids], expected, rtol=5e-5, atol=5e-5)
            assert ids[np.argmax(expected)] == np.argmax(observed)
            print(json.dumps({
                "position": row,
                "ffn_norm_max_abs": float(np.max(np.abs(head[row] - expected_head[row]))),
                "ffn_norm_relative_l2": float(np.linalg.norm(head[row] - expected_head[row]) /
                                              np.linalg.norm(expected_head[row])),
                "checked_logits": len(ids),
                "logit_max_abs": float(np.max(np.abs(observed[ids] - expected))),
                "gpu_top_id": int(np.argmax(observed)),
            }))


if __name__ == "__main__":
    check_capture(*map(Path, sys.argv[1:]))
