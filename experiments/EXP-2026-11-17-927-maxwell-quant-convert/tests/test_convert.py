#!/usr/bin/env python3
"""Round-trip gate for tools/maxwell-convert.py.

Builds a synthetic Maxwell-shaped safetensors set with the real HF names,
converts it, then parses the GGUF independently (header, metadata, tensor
descriptors) and dequantises the Q4_0 payload with a separate transcription of
ggml's reference formula. No real weights are read.
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
    header, blobs, off = {}, [], 0
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
        if t == 5:
            return struct.unpack("<i", self.fh.read(4))[0]
        if t == 6:
            return struct.unpack("<f", self.fh.read(4))[0]
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
        mx = float(blk[int(np.argmax(np.abs(blk)))])
        d = mx / -8.0
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


CFG = {
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
    "block_sparse_gqa_query_chunk_size": 4, "block_sparse_gqa_layer_freq": [1, 1],
    "enable_hyper_connections": True, "num_residual_streams": 4,
    "mhc_sinkhorn_iterations": 20, "mhc_init_gating_factor": 0.01,
    "bos_token_id": 1, "eos_token_id": 2, "pad_token_id": 3,
}


def build_tensors(rng, vocab=256, n_layer=2, dense=(0,)):
    E, hc, C = CFG["num_experts"], CFG["num_residual_streams"], CFG["hidden_size"]
    mix = (2 + hc)*hc
    t = []
    t.append(("model.embed_tokens.weight", (rng.standard_normal((vocab, C))*0.02).astype(np.float32)))
    t.append(("lm_head.weight", (rng.standard_normal((vocab, C))*0.02).astype(np.float32)))
    t.append(("model.norm.weight", np.ones(C, dtype=np.float32)))
    t.append(("model.hc_head.hc_head_fn", (rng.standard_normal((hc, hc*C))*0.02).astype(np.float32)))
    t.append(("model.hc_head.hc_head_base", np.zeros(hc, dtype=np.float32)))
    t.append(("model.hc_head.hc_head_scale", np.ones(1, dtype=np.float32)))
    for i in range(n_layer):
        p = f"model.layers.{i}"
        t += [
            (f"{p}.self_attn.q_proj.weight", (rng.standard_normal((C, C))*0.02).astype(np.float32)),
            (f"{p}.self_attn.k_proj.weight", (rng.standard_normal((16, C))*0.02).astype(np.float32)),
            (f"{p}.self_attn.v_proj.weight", (rng.standard_normal((16, C))*0.02).astype(np.float32)),
            (f"{p}.self_attn.o_proj.weight", (rng.standard_normal((C, C))*0.02).astype(np.float32)),
            (f"{p}.self_attn.q_layernorm.weight", np.ones(16, dtype=np.float32)),
            (f"{p}.self_attn.k_layernorm.weight", np.ones(16, dtype=np.float32)),
            (f"{p}.input_layernorm.weight", np.ones(C, dtype=np.float32)),
            (f"{p}.post_attention_layernorm.weight", np.ones(C, dtype=np.float32)),
        ]
        for tag in ("self_attn_hyper_connection", "mlp_hyper_connection"):
            t += [
                (f"{p}.{tag}.mapping_proj.weight",
                 (rng.standard_normal((mix, hc*C))*0.02).astype(np.float32)),
                (f"{p}.{tag}.bias", (rng.standard_normal(mix)*0.01).astype(np.float32)),
                (f"{p}.{tag}.alpha_pre", np.array([0.01], dtype=np.float32)),
                (f"{p}.{tag}.alpha_post", np.array([0.01], dtype=np.float32)),
                (f"{p}.{tag}.alpha_res", np.array([0.02], dtype=np.float32)),
            ]
        if i in dense:
            t += [
                (f"{p}.block_sparse_moe.w1.weight", (rng.standard_normal((128, C))*0.02).astype(np.float32)),
                (f"{p}.block_sparse_moe.w3.weight", (rng.standard_normal((128, C))*0.02).astype(np.float32)),
                (f"{p}.block_sparse_moe.w2.weight", (rng.standard_normal((C, 128))*0.02).astype(np.float32)),
            ]
        else:
            t.append((f"{p}.block_sparse_moe.gate.weight",
                      (rng.standard_normal((E, C))*0.02).astype(np.float32)))
            t.append((f"{p}.block_sparse_moe.expert_bias",
                      (rng.standard_normal(E)*0.001).astype(np.float32)))
            for e in range(E):
                t += [
                    (f"{p}.block_sparse_moe.experts.{e}.w1.weight",
                     (rng.standard_normal((32, C))*0.02).astype(np.float32)),
                    (f"{p}.block_sparse_moe.experts.{e}.w3.weight",
                     (rng.standard_normal((32, C))*0.02).astype(np.float32)),
                    (f"{p}.block_sparse_moe.experts.{e}.w2.weight",
                     (rng.standard_normal((C, 32))*0.02).astype(np.float32)),
                ]
            for nm, shape in (("w1", (64, C)), ("w3", (64, C)), ("w2", (C, 64))):
                t.append((f"{p}.block_sparse_moe.shared_expert.{nm}.weight",
                          (rng.standard_normal(shape)*0.02).astype(np.float32)))
    return t


def main():
    rng = np.random.default_rng(7)
    tensors = build_tensors(rng)
    by_name = dict(tensors)

    with tempfile.TemporaryDirectory() as td:
        json.dump(CFG, open(os.path.join(td, "config.json"), "w"))
        shards = {"model-00001.safetensors": [], "model-00002.safetensors": [],
                  "model-00003.safetensors": []}
        for k, (name, arr) in enumerate(tensors):
            shards[list(shards)[k % 3]].append((name, arr))
        idx = {"weight_map": {}}
        for fn, items in shards.items():
            write_safetensors(os.path.join(td, fn), items)
            for name, _ in items:
                idx["weight_map"][name] = fn
        json.dump(idx, open(os.path.join(td, "model.safetensors.index.json"), "w"))

        out = os.path.join(td, "maxwell.gguf")
        r = subprocess.run([sys.executable, CONVERT, td, out, "--stub-vocab"], capture_output=True, text=True)
        check(r.returncode == 0, f"convert rc={r.returncode} {r.stderr[-400:]}")
        if r.returncode != 0:
            return report()

        rd = Reader(out)
        check(rd.kv.get("general.architecture") == "maxwell_moe", "architecture")
        check(rd.kv.get("maxwell_moe.block_count") == 2, "block_count")
        check(rd.kv.get("maxwell_moe.attention.key_length") == 16, "key_length")
        check(rd.kv.get("maxwell_moe.leading_dense_block_count") == 1, "dense lead")
        check(rd.kv.get("maxwell_moe.expert_gating_func") == 2, "gating sigmoid")
        check(rd.kv.get("maxwell_moe.expert_weights_norm") is True, "weights norm")
        check(rd.kv.get("maxwell_moe.hyper_connection.count") == 4, "hc count")
        check(rd.kv.get("maxwell_moe.hyper_connection.sinkhorn_iterations") == 20, "hc iters")
        check(abs(rd.kv.get("maxwell_moe.hyper_connection.epsilon") - 1e-6) < 1e-9, "hc eps")
        check(abs(rd.kv.get("maxwell_moe.rope.freq_base") - 20000000.0) < 1.0, "rope base")

        # Every planned output exists and no HF spelling leaked through.
        for name in rd.tensors:
            check(not name.startswith(("model.", "lm_head.")), f"HF name leaked: {name}")

        t = rd.tensors["blk.0.attn_q.weight"]
        check(t["type"] == 2 and t["dims"] == [64, 64], f"attn_q {t}")
        raw, row, n = rd.raw("blk.0.attn_q.weight")
        check(raw == ref_q4_0(by_name["model.layers.0.self_attn.q_proj.weight"].reshape(-1)),
              "attn_q Q4_0 differs from reference")

        t = rd.tensors["blk.0.attn_norm.weight"]
        check(t["type"] == 0 and t["dims"] == [64], f"attn_norm {t}")
        raw, _, _ = rd.raw("blk.0.attn_norm.weight")
        check(np.array_equal(np.frombuffer(raw, dtype="<f4"), np.ones(64, np.float32)),
              "F32 norm not exact")

        t = rd.tensors["blk.0.hc_attn_fn.weight"]
        check(t["type"] == 1 and t["dims"] == [256, 24], f"hc_attn_fn {t}")
        t = rd.tensors["blk.0.hc_attn_base.weight"]
        check(t["type"] == 0 and t["dims"] == [24], f"hc_attn_base {t}")
        raw, _, _ = rd.raw("blk.0.hc_attn_base.weight")
        check(np.array_equal(np.frombuffer(raw, dtype="<f4"),
                             by_name["model.layers.0.self_attn_hyper_connection.bias"]),
              "hc base not exact")
        t = rd.tensors["blk.0.hc_attn_scale.weight"]
        check(t["type"] == 0 and t["dims"] == [3], f"hc_attn_scale {t}")
        raw, _, _ = rd.raw("blk.0.hc_attn_scale.weight")
        check(np.allclose(np.frombuffer(raw, dtype="<f4"), [0.01, 0.01, 0.02], atol=1e-9),
              "hc scale values")

        t = rd.tensors["blk.0.ffn_gate.weight"]
        check(t["type"] == 2 and t["dims"] == [64, 128], f"dense gate {t}")

        t = rd.tensors["blk.1.ffn_gate_exps.weight"]
        check(t["type"] == 2 and t["dims"] == [64, 32, 8], f"gate_exps {t}")
        raw, row, n = rd.raw("blk.1.ffn_gate_exps.weight")
        stack = np.concatenate([by_name[f"model.layers.1.block_sparse_moe.experts.{e}.w1.weight"]
                                for e in range(8)], axis=0).reshape(-1)
        check(raw == ref_q4_0(stack), "gate_exps Q4_0 differs from aggregated reference")
        got = deq_q4_0(raw, row, n).reshape(8*32, 64)
        want = stack.reshape(8*32, 64)
        check(np.abs(got - want).max() < 0.01, "gate_exps dequant")

        t = rd.tensors["blk.1.ffn_down_exps.weight"]
        check(t["type"] == 2 and t["dims"] == [32, 64, 8], f"down_exps {t}")
        t = rd.tensors["blk.1.exp_probs_b.weight"]
        check(t["type"] == 0 and t["dims"] == [8], f"exp_probs_b {t}")
        raw, _, _ = rd.raw("blk.1.exp_probs_b.weight")
        check(np.array_equal(np.frombuffer(raw, dtype="<f4"),
                             by_name["model.layers.1.block_sparse_moe.expert_bias"]),
              "expert bias not exact")

        t = rd.tensors["output_hc_fn.weight"] if "output_hc_fn.weight" in rd.tensors else rd.tensors["output_hc_fn"]
        check(t["type"] == 1 and t["dims"] == [256, 4], f"hc head {t}")
        t = rd.tensors["token_embd.weight"]
        check(t["type"] == 1 and t["dims"] == [64, 256], f"embed {t}")
        t = rd.tensors["output.weight"]
        check(t["type"] == 1 and t["dims"] == [64, 256], f"lm_head {t}")

    return report()


def report():
    print(f"checks={checks} failures={len(fails)}")
    if not fails:
        print("MAXWELL-CONVERT OK")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
