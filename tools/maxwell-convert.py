#!/usr/bin/env python3
"""Convert Maxwell-V1-Flash safetensors into a GGUF container for FrankenMoE.

The runtime is our own; llama.cpp and vLLM are not used for conversion. The
packing follows ggml's reference Q4_0 (ggml-quants.c:quantize_row_q4_0_ref), so
the result is read by the existing quantised kernels without a new dequant path.

Rules:
  - 2-D linear weights become Q4_0 when the row length is a multiple of 32;
  - every 1-D tensor (norms, expert bias, mHC alphas, hc_head parameters) stays
    F32: their spread is below bf16 resolution and routing depends on it;
  - embed_tokens and lm_head stay F16 by default (accuracy of the IO layers),
    Q4_0 with --quant-io;
  - tensor names keep the HF spelling; the arch builder maps them.

Usage:
  python3 tools/maxwell-convert.py <model-dir> <out.gguf> [--quant-io]
"""

import argparse
import io
import json
import os
import struct
import sys

import numpy as np

GGUF_MAGIC = 0x46554747  # "GGUF" little-endian
GGUF_VERSION = 3
ALIGN = 32

T_UINT32, T_INT32, T_FLOAT32, T_BOOL, T_STRING, T_ARRAY = 4, 5, 6, 7, 8, 9
T_UINT64, T_FLOAT64 = 10, 12

GGML_F32, GGML_F16, GGML_Q4_0 = 0, 1, 2
QK4_0 = 32


class Writer:
    def __init__(self, fh):
        self.fh = fh
        self.kv_count = 0

    def u32(self, v):
        self.fh.write(struct.pack("<I", v))

    def u64(self, v):
        self.fh.write(struct.pack("<Q", v))

    def i32(self, v):
        self.fh.write(struct.pack("<i", v))

    def f32(self, v):
        self.fh.write(struct.pack("<f", v))

    def f64(self, v):
        self.fh.write(struct.pack("<d", v))

    def string(self, s):
        b = s.encode()
        self.u64(len(b))
        self.fh.write(b)


def kv_scalar(w, key, typ, value):
    w.kv_count += 1
    w.string(key)
    w.u32(typ)
    if typ == T_UINT32:
        w.u32(int(value))
    elif typ == T_INT32:
        w.i32(int(value))
    elif typ == T_FLOAT32:
        w.f32(float(value))
    elif typ == T_BOOL:
        w.fh.write(struct.pack("<B", 1 if value else 0))
    elif typ == T_STRING:
        w.string(str(value))
    elif typ == T_UINT64:
        w.u64(int(value))
    elif typ == T_FLOAT64:
        w.f64(float(value))
    else:
        raise ValueError(f"scalar type {typ}")


def kv_array_u32(w, key, values):
    w.kv_count += 1
    w.string(key)
    w.u32(T_ARRAY)
    w.u32(T_UINT32)
    w.u64(len(values))
    for v in values:
        w.u32(int(v))


def kv_array_string(w, key, values):
    w.kv_count += 1
    w.string(key)
    w.u32(T_ARRAY)
    w.u32(T_STRING)
    w.u64(len(values))
    for v in values:
        w.string(str(v))


def read_safetensors_header(path):
    with open(path, "rb") as fh:
        raw = fh.read(8)
        if len(raw) != 8:
            raise ValueError(f"{path}: short header")
        (n,) = struct.unpack("<Q", raw)
        header = json.loads(fh.read(n))
    if "__metadata__" in header:
        header.pop("__metadata__")
    return header


def load_tensor(path, name, info):
    dtype = info["dtype"]
    shape = tuple(info["shape"])
    a, b = info["data_offsets"]
    with open(path, "rb") as fh:
        fh.seek(8 + _header_len(path) + a)
        raw = fh.read(b - a)
    if dtype == "F32":
        arr = np.frombuffer(raw, dtype="<f4")
    elif dtype == "F16":
        arr = np.frombuffer(raw, dtype="<f2").astype(np.float32)
    elif dtype == "BF16":
        u = np.frombuffer(raw, dtype="<u2").astype(np.uint32) << 16
        arr = u.view(np.float32)
    elif dtype in ("I64", "I32", "U32", "U8", "I8"):
        arr = np.frombuffer(raw, dtype=np.dtype(dtype).newbyteorder("<")).astype(np.float32)
    else:
        raise ValueError(f"{name}: unsupported dtype {dtype}")
    return arr.reshape(shape)


_HDRLEN = {}


def _header_len(path):
    if path not in _HDRLEN:
        with open(path, "rb") as fh:
            (n,) = struct.unpack("<Q", fh.read(8))
        _HDRLEN[path] = n
    return _HDRLEN[path]


def quantize_q4_0(x):
    """ggml reference packing; x is 1-D float32, len % 32 == 0."""
    assert x.size % QK4_0 == 0
    nb = x.size // QK4_0
    out = np.empty(nb * 18, dtype=np.uint8)
    for i in range(nb):
        blk = x[i * QK4_0:(i + 1) * QK4_0]
        amax_idx = int(np.argmax(np.abs(blk)))
        mx = float(blk[amax_idx])
        d = mx / -8.0
        with np.errstate(divide="ignore"):
            ident = 1.0 / d if d != 0.0 else 0.0
        q = np.clip((blk * ident + 8.5).astype(np.int8), 0, 15).astype(np.uint8)
        base = i * 18
        out[base:base + 2] = np.frombuffer(np.float16(d).tobytes(), dtype=np.uint8)
        out[base + 2:base + 18] = q[:16] | (q[16:] << 4)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model_dir")
    ap.add_argument("out")
    ap.add_argument("--quant-io", action="store_true",
                    help="quantise embed_tokens and lm_head to Q4_0")
    args = ap.parse_args()

    cfg = json.load(open(os.path.join(args.model_dir, "config.json")))
    if cfg.get("architectures") != ["MaxwellMoeForCausalLM"]:
        raise SystemExit(f"unexpected architectures: {cfg.get('architectures')}")

    index_path = os.path.join(args.model_dir, "model.safetensors.index.json")
    if os.path.exists(index_path):
        weight_map = json.load(open(index_path))["weight_map"]
        shards = {}
        for name, shard in weight_map.items():
            shards.setdefault(shard, []).append(name)
    else:
        shards = {}
        for fn in sorted(os.listdir(args.model_dir)):
            if fn.endswith(".safetensors"):
                hdr = read_safetensors_header(os.path.join(args.model_dir, fn))
                shards[fn] = sorted(hdr)

    plan = []  # (name, file, info, ggml_type, dims)
    total_bytes = 0
    for shard, names in sorted(shards.items()):
        path = os.path.join(args.model_dir, shard)
        hdr = read_safetensors_header(path)
        for name in sorted(names):
            info = hdr[name]
            shape = [int(v) for v in info["shape"]]
            if len(shape) == 0:
                raise ValueError(f"{name}: scalar tensor")
            dims = list(reversed(shape))  # ggml ne[0] first
            quantise = (len(shape) == 2 and dims[0] % QK4_0 == 0 and
                        (args.quant_io or not name.endswith(("embed_tokens.weight",
                                                             "lm_head.weight"))))
            gt = GGML_Q4_0 if quantise else (
                GGML_F16 if len(shape) == 2 or info["dtype"] == "F16" else GGML_F32)
            row = dims[0]
            if gt == GGML_Q4_0:
                nbytes = (row // QK4_0) * 18 * (int(np.prod(dims)) // row)
            elif gt == GGML_F16:
                nbytes = int(np.prod(dims)) * 2
            else:
                nbytes = int(np.prod(dims)) * 4
            plan.append((name, path, info, gt, dims, nbytes))
            total_bytes += nbytes

    kvbuf = io.BytesIO()
    w = Writer(kvbuf)
    if True:

        kv_scalar(w, "general.architecture", T_STRING, "maxwell_moe")
        kv_scalar(w, "general.name", T_STRING, "Maxwell-V1-Flash")
        kv_scalar(w, "general.file_type", T_UINT32, 2)  # mostly Q4_0
        kv_scalar(w, "maxwell.block_count", T_UINT32, cfg["num_hidden_layers"])
        kv_scalar(w, "maxwell.embedding_length", T_UINT32, cfg["hidden_size"])
        kv_scalar(w, "maxwell.feed_forward_length", T_UINT32, cfg["intermediate_size"])
        kv_scalar(w, "maxwell.attention.head_count", T_UINT32, cfg["num_attention_heads"])
        kv_scalar(w, "maxwell.attention.head_count_kv", T_UINT32, cfg["num_key_value_heads"])
        kv_scalar(w, "maxwell.attention.head_dim", T_UINT32, cfg["attention_head_size"])
        kv_scalar(w, "maxwell.attention.layer_norm_rms_epsilon", T_FLOAT32, cfg["rms_norm_eps"])
        kv_scalar(w, "maxwell.rope.freq_base", T_FLOAT32, cfg["rope_theta"])
        kv_scalar(w, "maxwell.context_length", T_UINT32, cfg["max_position_embeddings"])
        kv_scalar(w, "maxwell.expert_count", T_UINT32, cfg["num_experts"])
        kv_scalar(w, "maxwell.expert_used_count", T_UINT32, cfg["num_experts_per_tok"])
        kv_scalar(w, "maxwell.expert_feed_forward_length", T_UINT32, cfg["moe_intermediate_size"])
        kv_scalar(w, "maxwell.expert_shared_feed_forward_length", T_UINT32,
                  cfg["shared_expert_intermediate_size"])
        kv_scalar(w, "maxwell.vocab_size", T_UINT32, cfg["vocab_size"])
        kv_scalar(w, "maxwell.qk_layernorm", T_BOOL, cfg["qk_layernorm"])
        kv_scalar(w, "maxwell.router.sigmoid", T_BOOL, cfg["router_score_function"] == "sigmoid")
        kv_scalar(w, "maxwell.router.expert_bias", T_BOOL, cfg["moe_router_enable_expert_bias"])
        kv_scalar(w, "maxwell.mtp.layer_count", T_UINT32, cfg["mtp_num_layers"])
        kv_array_u32(w, "maxwell.mlp_only_layers", cfg["mlp_only_layers"])
        kv_scalar(w, "maxwell.attention.variant", T_STRING, cfg["attention_variant"])
        kv_scalar(w, "maxwell.block_sparse_gqa.n_heads", T_UINT32,
                  cfg["block_sparse_gqa_indexer_n_heads"])
        kv_scalar(w, "maxwell.block_sparse_gqa.head_dim", T_UINT32,
                  cfg["block_sparse_gqa_indexer_head_dim"])
        kv_scalar(w, "maxwell.block_sparse_gqa.block_size", T_UINT32,
                  cfg["block_sparse_gqa_block_size"])
        kv_scalar(w, "maxwell.block_sparse_gqa.topk_blocks", T_UINT32,
                  cfg["block_sparse_gqa_topk_blocks"])
        kv_scalar(w, "maxwell.block_sparse_gqa.total_support_blocks", T_UINT32,
                  cfg["block_sparse_gqa_total_support_blocks"])
        kv_scalar(w, "maxwell.block_sparse_gqa.prefix_window_size", T_UINT32,
                  cfg["block_sparse_gqa_prefix_window_size"])
        kv_scalar(w, "maxwell.block_sparse_gqa.local_window_size", T_UINT32,
                  cfg["block_sparse_gqa_local_window_size"])
        kv_scalar(w, "maxwell.block_sparse_gqa.num_remote_blocks", T_UINT32,
                  cfg["block_sparse_gqa_num_remote_blocks"])
        kv_scalar(w, "maxwell.block_sparse_gqa.query_chunk_size", T_UINT32,
                  cfg["block_sparse_gqa_query_chunk_size"])
        kv_array_u32(w, "maxwell.block_sparse_gqa.layer_freq",
                     cfg["block_sparse_gqa_layer_freq"])
        kv_scalar(w, "maxwell.hyper_connections.enabled", T_BOOL,
                  cfg["enable_hyper_connections"])
        kv_scalar(w, "maxwell.hyper_connections.streams", T_UINT32,
                  cfg["num_residual_streams"])
        kv_scalar(w, "maxwell.hyper_connections.sinkhorn_iterations", T_UINT32,
                  cfg["mhc_sinkhorn_iterations"])
        kv_scalar(w, "maxwell.hyper_connections.init_gating_factor", T_FLOAT32,
                  cfg["mhc_init_gating_factor"])
        kv_scalar(w, "maxwell.tokenizer.bos_id", T_UINT32, cfg["bos_token_id"])
        kv_scalar(w, "maxwell.tokenizer.eos_id", T_UINT32, cfg["eos_token_id"])
        kv_scalar(w, "maxwell.tokenizer.pad_id", T_UINT32, cfg["pad_token_id"])

    n_kv = w.kv_count
    with open(args.out, "wb") as fh:
        w2 = Writer(fh)
        w2.u32(GGUF_MAGIC)
        w2.u32(GGUF_VERSION)
        w2.u64(len(plan))
        w2.u64(n_kv)
        fh.write(kvbuf.getvalue())
        w = Writer(fh)

        offset = 0
        for name, path, info, gt, dims, nbytes in plan:
            w.string(name)
            w.u32(len(dims))
            for d in dims:
                w.u64(d)
            w.u32(gt)
            w.u64(offset)
            offset += nbytes
            pad = (-offset) % ALIGN
            offset += pad
        pos = fh.tell()
        fh.write(b"\x00" * ((-pos) % ALIGN))

        written = 0
        for name, path, info, gt, dims, nbytes in plan:
            arr = load_tensor(path, name, info)
            flat = arr.reshape(-1)
            if gt == GGML_Q4_0:
                rows = flat.size // dims[0]
                parts = [quantize_q4_0(flat[r * dims[0]:(r + 1) * dims[0]])
                         for r in range(rows)]
                data = np.concatenate(parts) if parts else b""
            elif gt == GGML_F16:
                data = flat.astype("<f2").tobytes()
            else:
                data = flat.astype("<f4").tobytes()
            if len(data) != nbytes:
                raise RuntimeError(f"{name}: wrote {len(data)}, planned {nbytes}")
            fh.write(data)
            written += len(data)
            fh.write(b"\x00" * ((-written) % ALIGN))

    print(f"tensors={len(plan)} kv={n_kv} tensors_bytes={total_bytes} "
          f"file_bytes={os.path.getsize(args.out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
