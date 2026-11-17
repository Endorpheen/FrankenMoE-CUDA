#!/usr/bin/env python3
"""Round-trip gate for tools/maxwell-convert.py.

Builds a synthetic Maxwell-shaped safetensors set, converts it, then parses the
GGUF independently (header, metadata, tensor descriptors) and dequantises the
Q4_0 payload with a separate transcription of ggml's reference formula. No
weights are needed and none are read.
"""
import json
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
CONVERT = os.path.join(ROOT, "tools", "maxwell-convert.py")
QK = 32
fails = []
checks = 0


def check(cond, msg):
    global checks
    checks += 1
    if not cond:
        fails.append(msg)
        print("FAIL", msg)


def write_safetensors(path, tensors):
    header = {}
    blobs = []
    off = 0
    for name, arr in tensors:
        raw = arr.tobytes()
        header[name] = {
            "dtype": {"float32": "F32", "float16": "F16"}[str(arr.dtype)],
            "shape": list(arr.shape),
            "data_offsets": [off, off + len(raw)],
        }
        blobs.append(raw)
        off += len(raw)
    hj = json.dumps(header).encode()
    with open(path, "wb") as fh:
        fh.write(struct.pack("<Q", len(hj)))
        fh.write(hj)
        for b in blobs:
            fh.write(b)


def bf16_bytes(x):
    u = np.asarray(x, dtype=np.float32).view(np.uint32)
    return (u >> 16).astype("<u2").tobytes()


def write_safetensors_bf16(path, tensors):
    header, blobs, off = {}, [], 0
    for name, arr in tensors:
        raw = bf16_bytes(arr)
        header[name] = {"dtype": "BF16", "shape": list(arr.shape),
                        "data_offsets": [off, off + len(raw)]}
        blobs.append(raw)
        off += len(raw)
    hj = json.dumps(header).encode()
    with open(path, "wb") as fh:
        fh.write(struct.pack("<Q", len(hj)))
        fh.write(hj)
        for b in blobs:
            fh.write(b)


class Reader:
    def __init__(self, path):
        self.fh = open(path, "rb")
        magic, version, n_tensors, n_kv = struct.unpack("<IIQQ", self.fh.read(24))
        assert magic == 0x46554747 and version == 3, (hex(magic), version)
        self.n_tensors = n_tensors
        self.kv = {}
        for _ in range(n_kv):
            key = self.string()
            self.kv[key] = self.value()
        self.tensors = {}
        for _ in range(n_tensors):
            name = self.string()
            nd = self.u32()
            dims = [self.u64() for _ in range(nd)]
            typ = self.u32()
            off = self.u64()
            self.tensors[name] = {"dims": dims, "type": typ, "offset": off}
        pos = self.fh.tell()
        align = self.kv.get("general.alignment", 32)
        if isinstance(align, int) and align > 0:
            self.fh.seek((pos + align - 1) // align * align)
        self.data_start = self.fh.tell()

    def u32(self):
        return struct.unpack("<I", self.fh.read(4))[0]

    def u64(self):
        return struct.unpack("<Q", self.fh.read(8))[0]

    def string(self):
        n = self.u64()
        return self.fh.read(n).decode()

    def value(self):
        t = self.u32()
        if t == 4:
            return self.u32()
        if t == 5:
            return struct.unpack("<i", self.fh.read(4))[0]
        if t == 6:
            return struct.unpack("<f", self.fh.read(4))[0]
        if t == 7:
            return bool(struct.unpack("<B", self.fh.read(1))[0])
        if t == 8:
            return self.string()
        if t == 9:
            et, n = self.u32(), self.u64()
            return [self._elem(et) for _ in range(n)]
        raise ValueError(f"kv type {t}")

    def _elem(self, t):
        if t == 4:
            return self.u32()
        if t == 8:
            return self.string()
        raise ValueError(f"array elem {t}")

    def raw(self, name):
        t = self.tensors[name]
        n = int(np.prod(t["dims"]))
        row = t["dims"][0]
        if t["type"] == 2:
            nbytes = (row // QK) * 18 * (n // row)
        elif t["type"] == 1:
            nbytes = n * 2
        else:
            nbytes = n * 4
        self.fh.seek(self.data_start + t["offset"])
        return self.fh.read(nbytes), row, n


def ref_q4_0(x):
    nb = x.size // QK
    out = bytearray(nb * 18)
    for i in range(nb):
        blk = x[i * QK:(i + 1) * QK]
        mx = blk[int(np.argmax(np.abs(blk)))]
        d = float(mx) / -8.0
        ident = 1.0 / d if d != 0.0 else 0.0
        q = np.clip((blk * ident + 8.5).astype(np.int8), 0, 15).astype(np.uint8)
        out[i * 18:i * 18 + 2] = np.float16(d).tobytes()
        out[i * 18 + 2:i * 18 + 18] = bytes(a | (b << 4) for a, b in zip(q[:16], q[16:]))
    return bytes(out)


def deq_q4_0(raw, row, n):
    blocks = len(raw) // 18
    out = np.empty(blocks * QK, dtype=np.float32)
    for i in range(blocks):
        d = np.frombuffer(raw[i * 18:i * 18 + 2], dtype="<f2")[0].astype(np.float32)
        qs = np.frombuffer(raw[i * 18 + 2:i * 18 + 18], dtype=np.uint8)
        q = np.concatenate([qs & 0xF, qs >> 4]).astype(np.float32)
        out[i * QK:(i + 1) * QK] = (q - 8.0) * d
    return out.reshape(n // row, row)


def main():
    cfg = {
        "architectures": ["MaxwellMoeForCausalLM"],
        "num_hidden_layers": 2, "hidden_size": 64, "intermediate_size": 128,
        "num_attention_heads": 4, "num_key_value_heads": 1, "attention_head_size": 16,
        "rms_norm_eps": 1e-6, "rope_theta": 20000000, "max_position_embeddings": 4096,
        "num_experts": 8, "num_experts_per_tok": 2, "moe_intermediate_size": 32,
        "shared_expert_intermediate_size": 64, "vocab_size": 256,
        "qk_layernorm": True, "router_score_function": "sigmoid",
        "moe_router_enable_expert_bias": True, "mtp_num_layers": 1,
        "mlp_only_layers": [0], "attention_variant": "block_sparse_gqa",
        "block_sparse_gqa_indexer_n_heads": 1, "block_sparse_gqa_indexer_head_dim": 16,
        "block_sparse_gqa_block_size": 8, "block_sparse_gqa_topk_blocks": 2,
        "block_sparse_gqa_total_support_blocks": 4,
        "block_sparse_gqa_prefix_window_size": 16,
        "block_sparse_gqa_local_window_size": 16, "block_sparse_gqa_num_remote_blocks": 2,
        "block_sparse_gqa_query_chunk_size": 4,
        "block_sparse_gqa_layer_freq": [1, 1],
        "enable_hyper_connections": True, "num_residual_streams": 4,
        "mhc_sinkhorn_iterations": 20, "mhc_init_gating_factor": 0.01,
        "bos_token_id": 1, "eos_token_id": 2, "pad_token_id": 3,
    }
    rng = np.random.default_rng(7)
    w2 = (rng.standard_normal((64, 64)) * 0.02).astype(np.float32)
    w3 = (rng.standard_normal((32, 64)) * 0.02).astype(np.float32)
    n1 = (rng.standard_normal(64) * 0.5).astype(np.float32)
    bias = (rng.standard_normal(8) * 0.001).astype(np.float32)
    emb = (rng.standard_normal((256, 64)) * 0.02).astype(np.float32)

    with tempfile.TemporaryDirectory() as td:
        write_safetensors(os.path.join(td, "model-00001.safetensors"),
                          [("model.layers.0.self_attn.q_proj.weight", w2)])
        write_safetensors_bf16(os.path.join(td, "model-00002.safetensors"),
                               [("model.layers.0.block_sparse_moe.experts.0.w1.weight", w3)])
        write_safetensors(os.path.join(td, "model-00003.safetensors"),
                          [("model.layers.0.input_layernorm.weight", n1),
                           ("model.layers.0.block_sparse_moe.expert_bias", bias),
                           ("model.embed_tokens.weight", emb)])
        json.dump(cfg, open(os.path.join(td, "config.json"), "w"))
        # Index is optional for the converter; build it from the headers it wrote.
        idx = {"weight_map": {}}
        for fn, names in (("model-00001.safetensors", ["model.layers.0.self_attn.q_proj.weight"]),
                          ("model-00002.safetensors", ["model.layers.0.block_sparse_moe.experts.0.w1.weight"]),
                          ("model-00003.safetensors", ["model.layers.0.input_layernorm.weight",
                                                       "model.layers.0.block_sparse_moe.expert_bias",
                                                       "model.embed_tokens.weight"])):
            for n in names:
                idx["weight_map"][n] = fn
        json.dump(idx, open(os.path.join(td, "model.safetensors.index.json"), "w"))

        out = os.path.join(td, "maxwell.gguf")
        r = subprocess.run([sys.executable, CONVERT, td, out], capture_output=True, text=True)
        check(r.returncode == 0, f"convert rc={r.returncode} {r.stderr[-300:]}")
        if r.returncode != 0:
            return report()

        rd = Reader(out)
        check(rd.kv.get("general.architecture") == "maxwell_moe", "architecture")
        check(rd.kv.get("maxwell.block_count") == 2, "block_count")
        check(rd.kv.get("maxwell.expert_count") == 8, "expert_count")
        check(rd.kv.get("maxwell.mlp_only_layers") == [0], "mlp_only_layers")
        check(rd.kv.get("maxwell.hyper_connections.streams") == 4, "hc streams")
        check(abs(rd.kv.get("maxwell.rope.freq_base") - 20000000.0) < 1.0, "rope base")
        check(rd.kv.get("maxwell.block_sparse_gqa.block_size") == 8, "block size")
        check(len(rd.tensors) == 5, f"tensor count {len(rd.tensors)}")

        t = rd.tensors["model.layers.0.self_attn.q_proj.weight"]
        check(t["type"] == 2 and t["dims"] == [64, 64], f"q_proj {t}")
        raw, row, n = rd.raw("model.layers.0.self_attn.q_proj.weight")
        check(raw == ref_q4_0(w2.reshape(-1)), "q_proj Q4_0 bytes differ from reference")
        got = deq_q4_0(raw, row, n).reshape(w2.shape)
        err = np.abs(got - w2)
        check(err.max() < 0.01, f"q_proj dequant max err {err.max():.5f}")

        t = rd.tensors["model.layers.0.input_layernorm.weight"]
        check(t["type"] == 0 and t["dims"] == [64], f"norm {t}")
        raw, _, _ = rd.raw("model.layers.0.input_layernorm.weight")
        check(np.array_equal(np.frombuffer(raw, dtype="<f4"), n1), "F32 norm not exact")

        t = rd.tensors["model.layers.0.block_sparse_moe.expert_bias"]
        check(t["type"] == 0 and t["dims"] == [8], f"bias {t}")
        raw, _, _ = rd.raw("model.layers.0.block_sparse_moe.expert_bias")
        check(np.array_equal(np.frombuffer(raw, dtype="<f4"), bias), "expert bias not exact")

        t = rd.tensors["model.embed_tokens.weight"]
        check(t["type"] == 1 and t["dims"] == [64, 256], f"embed {t}")
        raw, _, _ = rd.raw("model.embed_tokens.weight")
        check(np.allclose(np.frombuffer(raw, dtype="<f2").reshape(256, 64), emb,
                          atol=1e-4), "embed F16 round-trip")

        t = rd.tensors["model.layers.0.block_sparse_moe.experts.0.w1.weight"]
        check(t["type"] == 2 and t["dims"] == [64, 32], f"bf16 expert {t}")
        raw, row, n = rd.raw("model.layers.0.block_sparse_moe.experts.0.w1.weight")
        got = deq_q4_0(raw, row, n).reshape(w3.shape)
        check(np.abs(got - w3).max() < 0.01, "bf16 expert dequant")

    return report()


def report():
    print(f"checks={checks} failures={len(fails)}")
    if not fails:
        print("MAXWELL-CONVERT OK")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
