"""Compare identical MTP replay inputs and isolate the EH arithmetic difference."""
import json
import mmap
from pathlib import Path
import sys

import numpy as np


WIDTH = 5120
VOCAB = 248320
LAYER = "blk.64.nextn."
IDS = (1812, 5020, 220, 15)


def stages(path):
    data = np.fromfile(path, dtype="<f4").reshape(len(IDS), VOCAB + 3 * WIDTH)
    assert np.isfinite(data).all()
    return dict(zip(("logits", "eh", "attention_residual", "head_norm"),
                    np.split(data, [VOCAB, VOCAB + WIDTH, VOCAB + 2 * WIDTH], axis=1)))


def errors(actual, expected):
    delta = actual.astype(np.float64) - expected
    return {
        "max_abs": np.max(abs(delta), axis=1).tolist(),
        "relative_l2": (np.linalg.norm(delta, axis=1) / np.linalg.norm(expected, axis=1)).tolist(),
    }


def eh_oracles(inventory, hidden):
    tensors = {tensor["name"]: tensor for tensor in inventory["tensors"]}
    epsilon = inventory["metadata"]["qwen35.attention.layer_norm_rms_epsilon"]
    signs = np.array(inventory["metadata"]["prism.hadamard.sign_values"][:WIDTH])

    with Path(inventory["file"]).open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
        def packed(name, shape, dtype, offset=0):
            return np.ndarray(shape, dtype=dtype, buffer=weights,
                              offset=inventory["data_offset"] + tensors[name]["offset"] + offset)

        def norm(rows, name):
            return rows / np.sqrt(np.mean(rows**2, axis=1, keepdims=True) + epsilon) * packed(
                LAYER + name + ".weight", (WIDTH,), "<f4")

        stride = WIDTH // 128 * 34
        rows = np.stack([packed("token_embd.weight", (WIDTH // 128, 34), "u1", token * stride)
                         for token in IDS])
        scale = rows[:, :, :2].copy().view("<f2").astype(np.float64)
        codes = (rows[:, :, 2:, None] >> np.array([0, 2, 4, 6], dtype=np.uint8)) & 3
        transformed = ((codes.reshape(len(IDS), -1, 128).astype(np.float64) - 1) * scale).reshape(-1, 1024)
        for size in (1, 2, 4, 8, 16, 32, 64, 128, 256, 512):
            view = transformed.reshape(-1, 2, size)
            a, b = view[:, 0].copy(), view[:, 1].copy()
            view[:, 0], view[:, 1] = a + b, a - b
        e = transformed.reshape(len(IDS), WIDTH) * signs / 32
        x = np.concatenate((norm(e, "enorm"), norm(hidden, "hnorm")), axis=1).reshape(len(IDS), 320, 32)

        d = np.max(abs(x), axis=2, keepdims=True) / 127
        ratio = np.divide(x, d, out=np.zeros_like(x), where=d != 0)
        q = np.copysign(np.floor(abs(ratio) + 0.5), ratio).astype(np.int32)
        d8 = d.squeeze(2).astype(np.float16).astype(np.float64)
        s8 = x.sum(axis=2).astype(np.float16).astype(np.float64)

        raw = packed(LAYER + "eh_proj.weight", (WIDTH, 320, 18), "u1")
        fp = np.empty((len(IDS), WIDTH), dtype=np.float64)
        q8 = np.empty_like(fp)
        for start in range(0, WIDTH, 64):
            block = raw[start:start + 64]
            scales = block[:, :, :2].copy().view("<f2").astype(np.float64).squeeze(2)
            nibbles = np.concatenate((block[:, :, 2:] & 15, block[:, :, 2:] >> 4), axis=2)
            fp[:, start:start + len(block)] = x.reshape(len(IDS), -1) @ (
                (nibbles.astype(np.float64) - 8) * scales[:, :, None]).reshape(len(block), -1).T
            dot = np.einsum("ibk,rbk->irb", q, nibbles, dtype=np.int32)
            q8[:, start:start + len(block)] = np.sum(
                scales[None, :, :] * (dot * d8[:, None, :] - 8 * s8[:, None, :]), axis=2)

        return fp, q8


out = Path(sys.argv[1])
inventory = json.loads(Path(sys.argv[2]).read_text())
goose, prism, unrotated = (stages(out / (name + ".bin"))
                          for name in ("goose", "prism", "prism-unrotated"))
hidden = np.fromfile(out / "hidden.bin", dtype="<f4").reshape(len(IDS), WIDTH).astype(np.float64)
assert not np.any(hidden[0])
fp, q8 = eh_oracles(inventory, hidden)
assert prism["eh"].tobytes() == unrotated["eh"].tobytes()
print(json.dumps({
    "input_ids": IDS,
    "goose_draft_ids": np.argmax(goose["logits"], axis=1).tolist(),
    "prism_draft_ids": np.argmax(prism["logits"], axis=1).tolist(),
    "prism_unrotated_draft_ids": np.argmax(unrotated["logits"], axis=1).tolist(),
    "default_prism": {name: errors(goose[name], values) for name, values in prism.items()},
    "unrotated_prism": {name: errors(goose[name], values) for name, values in unrotated.items()},
    "eh_fp_oracle_vs_goose": errors(goose["eh"], fp),
    "eh_fp_oracle_vs_prism": errors(prism["eh"], fp),
    "eh_q8_oracle_vs_prism": errors(prism["eh"], q8),
}, indent=2))
