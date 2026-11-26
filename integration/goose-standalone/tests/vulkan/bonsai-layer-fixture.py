"""Write a real first-layer Bonsai Vulkan fixture and independent CPU outputs."""

from pathlib import Path
import json
import subprocess
import sys
import tempfile

import numpy as np


TOKENS = (1511, 17, 248046, 1511)
HIDDEN, WIDTH, HEADS, KEY_HEADS, FFN = 5120, 128, 48, 16, 17408
VALUES, MIX = HEADS * WIDTH, (HEADS + 2 * KEY_HEADS) * WIDTH
EPS = 1e-6


def unpack(raw, width, rows):
    blocks = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 34)
    scale = blocks[:, :2].copy().view("<f2").astype(np.float32)
    code = (blocks[:, 2:, None] >> np.array([0, 2, 4, 6], dtype=np.uint8)) & 3
    return ((code.reshape(-1, 128).astype(np.float32) - 1) * scale).reshape(rows, width)


def rotate(value, signs, inverse=False, grouped=False):
    if grouped:
        value = value.reshape(3, KEY_HEADS, WIDTH).transpose(1, 0, 2).ravel()
    out = (value.copy() if inverse else value * signs).reshape(-1, 1024)
    for stage in range(10):
        span = 1 << stage
        group = out.reshape(-1, 2, span)
        a, b = group[:, 0].copy(), group[:, 1].copy()
        group[:, 0], group[:, 1] = a + b, a - b
    out = out.ravel() / 32
    return out * signs if inverse else out


def rms(x, weights, axis=None):
    return x * weights / np.sqrt((x * x).mean(axis=axis, keepdims=True) + EPS)


def silu(x):
    return x / (1 + np.exp(-x))


def step(x, w, sign, conv, state):
    h = rms(x, w["attn_norm.weight"])
    rotated = rotate(h, sign[HIDDEN])
    mixed = w["attn_qkv.weight"] @ rotated
    z = w["attn_gate.weight"] @ rotated
    alpha = w["ssm_a"] * np.logaddexp(np.float32(0), w["ssm_alpha.weight"] @ h + w["ssm_dt.bias"])
    beta = 1 / (1 + np.exp(-(w["ssm_beta.weight"] @ h)))
    window = np.vstack((conv, mixed))
    qkv = silu(np.einsum("tc,ct->c", window, w["ssm_conv1d.weight"])).reshape(-1, WIDTH)
    qk = qkv[:2 * KEY_HEADS]
    qk /= np.maximum(np.linalg.norm(qk, axis=1, keepdims=True), EPS)
    q, k = (np.tile(group, (3, 1)) for group in np.split(qk, 2))
    v = qkv[2 * KEY_HEADS:]
    decayed = state * np.exp(alpha)[:, None, None]
    delta = (v - np.einsum("hvk,hk->hv", decayed, k)) * beta[:, None]
    state = decayed + delta[:, :, None] * k[:, None, :]
    score = np.einsum("hvk,hk->hv", state, q) / np.float32(np.sqrt(WIDTH))
    gated = rms(score, w["ssm_norm.weight"], axis=1) * silu(z.reshape(HEADS, WIDTH))
    residual = x + w["ssm_out.weight"] @ rotate(gated.ravel(), sign[VALUES], grouped=True)
    normal = rotate(rms(residual, w["post_attention_norm.weight"]), sign[HIDDEN])
    value = silu(w["ffn_gate.weight"] @ normal) * (w["ffn_up.weight"] @ normal)
    return residual + w["ffn_down.weight"] @ rotate(value, sign[FFN]), window[1:], state


def main():
    model, path = map(Path, sys.argv[1:])
    root = Path(__file__).resolve().parents[4]
    reader = root / "experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/inventory.py"
    with tempfile.TemporaryDirectory() as work:
        inventory = Path(work) / "inventory.json"
        subprocess.run([sys.executable, reader, model, inventory], check=True)
        parsed = json.loads(inventory.read_text())
    meta = parsed["metadata"]
    assert meta["prism.hadamard.sign_widths"] == [HIDDEN, VALUES, FFN]
    sign_rows = np.split(np.array(meta["prism.hadamard.sign_values"], dtype="<i4"), [HIDDEN, HIDDEN + VALUES])
    assert np.float32(meta["qwen35.attention.layer_norm_rms_epsilon"]) == np.float32(EPS)
    assert all(np.isin(row, [-1, 1]).all() for row in sign_rows)
    signs = {len(row): row.astype(np.float32) for row in sign_rows}
    tensors = {item["name"]: item for item in parsed["tensors"]}
    layout = (
        ("attn_norm.weight", HIDDEN, 1, 0), ("attn_qkv.weight", HIDDEN, MIX, 142),
        ("attn_gate.weight", HIDDEN, VALUES, 142), ("ssm_alpha.weight", HIDDEN, HEADS, 0),
        ("ssm_beta.weight", HIDDEN, HEADS, 0), ("ssm_dt.bias", HEADS, 1, 0), ("ssm_a", HEADS, 1, 0),
        ("ssm_conv1d.weight", 4, MIX, 0), ("ssm_norm.weight", WIDTH, 1, 0),
        ("ssm_out.weight", VALUES, HIDDEN, 142), ("post_attention_norm.weight", HIDDEN, 1, 0),
        ("ffn_gate.weight", HIDDEN, FFN, 142), ("ffn_up.weight", HIDDEN, FFN, 142),
        ("ffn_down.weight", FFN, HIDDEN, 142),
    )
    weights = {}
    with model.open("rb") as file, path.open("wb") as out:
        for sign in sign_rows:
            out.write(sign.tobytes())
        for name, width, rows, kind in layout:
            tensor = tensors[f"blk.0.{name}"]
            assert tensor["type"] == kind and tensor["shape"] == ([width] if rows == 1 else [width, rows])
            file.seek(parsed["data_offset"] + tensor["offset"])
            nbytes = width * rows * 4 if not kind else width // 128 * rows * 34
            raw = file.read(nbytes)
            assert len(raw) == nbytes
            out.write(raw)
            weights[name] = (unpack(raw, width, rows) if kind else
                             np.frombuffer(raw, dtype="<f4").reshape((rows, width) if rows > 1 else (width,)))
        embedding = tensors["token_embd.weight"]
        assert embedding["type"] == 142 and embedding["shape"] == [HIDDEN, 248320]
        row_bytes = HIDDEN // 128 * 34
        history = np.zeros((3, MIX), dtype=np.float32)
        state = np.zeros((HEADS, WIDTH, WIDTH), dtype=np.float32)
        for token in TOKENS:
            file.seek(parsed["data_offset"] + embedding["offset"] + token * row_bytes)
            value = rotate(unpack(file.read(row_bytes), HIDDEN, 1).ravel(), signs[HIDDEN], inverse=True)
            result, history, state = step(value, weights, signs, history, state)
            for part in (value, result, history, state):
                out.write(part.astype("<f4", copy=False).tobytes())


if __name__ == "__main__":
    main()
