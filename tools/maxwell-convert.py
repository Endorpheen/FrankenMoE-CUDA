#!/usr/bin/env python3
"""Convert Maxwell-V1-Flash safetensors into a GGUF container for FrankenMoE.

The runtime is our own; llama.cpp and vLLM are not used for conversion. Packing
follows ggml's reference Q4_0 (ggml-quants.c:quantize_row_q4_0_ref), so the
existing quantised kernels read the result without a new dequant path.

Tensor naming follows the runtime's `blk.%d.*` convention, because that is what
`llm_tensor` resolves. The mapping, the expert aggregation and the metadata
keys are the contract with src/models/maxwell.cpp.

Rules:
  - 2-D linear weights become Q4_0 when the row length is a multiple of 32;
  - mHC tensors (hc_*) stay F16: they are small and their gates are sensitive;
  - every 1-D tensor (norms, expert bias, mHC alphas, hc_head parameters) stays
    F32: the expert-bias spread is below bf16 resolution;
  - embed_tokens and lm_head stay F16 by default, Q4_0 with --quant-io;
  - routed experts of one projection are aggregated into one 3-D tensor
    [n_embd, n_ff, n_expert] ([n_ff, n_embd, n_expert] for down);
  - the block-sparse indexer and the MTP head are not loaded yet (the first
    build runs dense causal attention up to the 4096-token support window).

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

GGUF_MAGIC = 0x46554747
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
        b = s.encode() if isinstance(s, str) else s
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


def kv_array_i32(w, key, values):
    w.kv_count += 1
    w.string(key)
    w.u32(T_ARRAY)
    w.u32(T_INT32)
    w.u64(len(values))
    for v in values:
        w.i32(int(v))


def kv_array_f32(w, key, values):
    w.kv_count += 1
    w.string(key)
    w.u32(T_ARRAY)
    w.u32(T_FLOAT32)
    w.u64(len(values))
    for v in values:
        w.f32(float(v))


def kv_array_string(w, key, values):
    w.kv_count += 1
    w.string(key)
    w.u32(T_ARRAY)
    w.u32(T_STRING)
    w.u64(len(values))
    for v in values:
        w.string(v)


def _split_patterns(pre, out):
    if not isinstance(pre, dict):
        return
    if pre.get("type") == "Split":
        out.append(json.dumps(pre.get("pattern") or ""))
    for part in pre.get("pretokenizers", []) if pre.get("type") == "Sequence" else []:
        _split_patterns(part, out)


def hf_pre_type(tj):
    """Map the HF pre-tokenizer onto the closest llama.cpp pre type."""
    pre = tj.get("pre_tokenizer")
    pats = []
    _split_patterns(pre, pats)
    pat = " ".join(pats)
    has_bytelevel = "ByteLevel" in json.dumps(pre or {})
    # Maxwell/Qwen split on \p{L}\p{M} runs before byte-level BPE; qwen35 is
    # the closest built-in. Its Han/Thai splits and \p{N}{1,3} remain a known
    # difference (documented in the experiment card).
    if "p{M}" in pat:
        return "qwen35"
    if "p{L}" in pat or "p{N}" in pat:
        return "qwen2"
    if has_bytelevel:
        return "gpt-2"
    return "default"


def read_hf_tokenizer(path, n_vocab):
    """Return (tokens, types, scores, merges, pre) for a GGUF vocabulary."""
    tj = json.load(open(path))
    model = tj["model"]
    if model.get("type") != "BPE":
        raise SystemExit(f"unsupported tokenizer type: {model.get('type')}")
    vocab = model["vocab"]
    tokens = [None]*n_vocab
    types = [1]*n_vocab
    for tok, idx in vocab.items():
        if 0 <= idx < n_vocab:
            tokens[idx] = tok
    for a in tj.get("added_tokens", []):
        idx = int(a["id"])
        if 0 <= idx < n_vocab:
            tokens[idx] = a["content"]
            types[idx] = 3 if a.get("special") else 1
    missing = [i for i, t in enumerate(tokens) if t is None]
    if missing:
        raise SystemExit(f"tokenizer leaves {len(missing)} ids undefined, first {missing[0]}")
    merges = model.get("merges", [])
    merges = [" ".join(m) if isinstance(m, list) else m for m in merges]
    scores = [0.0]*n_vocab
    return tokens, types, scores, merges, hf_pre_type(tj)


def read_eos_ids(model_dir, fallback):
    """HF generation_config may list several EOS ids; the runtime uses
    eos/eot/eom and puts all three into the end-of-generation set."""
    p = os.path.join(model_dir, "generation_config.json")
    if not os.path.exists(p):
        return [fallback]
    v = json.load(open(p)).get("eos_token_id", fallback)
    if isinstance(v, list):
        return [int(x) for x in v if x is not None]
    return [int(v)]


def write_hf_vocab(w, path, n_vocab, bos, eos, pad, unk=0, eot=None, eom=None):
    tokens, types, scores, merges, pre = read_hf_tokenizer(path, n_vocab)
    kv_scalar(w, "tokenizer.ggml.model", T_STRING, "gpt2")
    kv_scalar(w, "tokenizer.ggml.pre", T_STRING, pre)
    kv_array_string(w, "tokenizer.ggml.tokens", tokens)
    kv_array_f32(w, "tokenizer.ggml.scores", scores)
    kv_array_i32(w, "tokenizer.ggml.token_type", types)
    kv_array_string(w, "tokenizer.ggml.merges", merges)
    kv_scalar(w, "tokenizer.ggml.bos_token_id", T_UINT32, bos)
    kv_scalar(w, "tokenizer.ggml.eos_token_id", T_UINT32, eos)
    kv_scalar(w, "tokenizer.ggml.padding_token_id", T_UINT32, pad)
    kv_scalar(w, "tokenizer.ggml.unknown_token_id", T_UINT32, unk)
    if eot is not None:
        kv_scalar(w, "tokenizer.ggml.eot_token_id", T_UINT32, eot)
    if eom is not None:
        kv_scalar(w, "tokenizer.ggml.eom_token_id", T_UINT32, eom)
    return pre, len(merges)


def write_stub_vocab(w, n_vocab, eot=None, eom=None):
    """A test-only vocabulary: enough for the loader, meaningless for text."""
    tokens = ["<unk>", "<s>", "</s>", "<pad>"] + [f"tok{i}" for i in range(4, n_vocab)]
    tokens = tokens[:n_vocab]
    types = [2, 3, 3, 3] + [1]*(n_vocab - 4)
    kv_scalar(w, "tokenizer.ggml.model", T_STRING, "gpt2")
    kv_array_string(w, "tokenizer.ggml.tokens", tokens)
    kv_array_f32(w, "tokenizer.ggml.scores", [0.0]*n_vocab)
    kv_array_i32(w, "tokenizer.ggml.token_type", types)
    kv_array_string(w, "tokenizer.ggml.merges", [])
    kv_scalar(w, "tokenizer.ggml.unknown_token_id", T_UINT32, 0)
    kv_scalar(w, "tokenizer.ggml.pre", T_STRING, "default")
    if eot is not None:
        kv_scalar(w, "tokenizer.ggml.eot_token_id", T_UINT32, eot)
    if eom is not None:
        kv_scalar(w, "tokenizer.ggml.eom_token_id", T_UINT32, eom)


def read_safetensors_header(path):
    with open(path, "rb") as fh:
        (n,) = struct.unpack("<Q", fh.read(8))
        header = json.loads(fh.read(n))
    header.pop("__metadata__", None)
    return header


_HDRLEN = {}


def header_len(path):
    if path not in _HDRLEN:
        with open(path, "rb") as fh:
            (_HDRLEN[path],) = struct.unpack("<Q", fh.read(8))
    return _HDRLEN[path]


def load_tensor(path, name, info):
    dtype = info["dtype"]
    shape = tuple(info["shape"])
    a, b = info["data_offsets"]
    with open(path, "rb") as fh:
        fh.seek(8 + header_len(path) + a)
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


def quantize_q4_0(x):
    """ggml reference packing, vectorised over 32-value blocks.

    d = max/-8 where max is the signed value of largest magnitude; each weight
    becomes min(15, (int8)(x/d + 8.5)) and pairs (j, j+16) share one byte.
    """
    x = np.asarray(x, dtype=np.float32).reshape(-1, QK4_0)
    amax = np.argmax(np.abs(x), axis=1)
    mx = x[np.arange(x.shape[0]), amax]
    d = (mx / -8.0).astype(np.float32)
    ident = np.where(d != 0.0, 1.0/np.where(d != 0.0, d, 1.0), 0.0).astype(np.float32)
    q = np.clip((x * ident[:, None] + 8.5).astype(np.int8), 0, 15).astype(np.uint8)
    out = np.empty((x.shape[0], 18), dtype=np.uint8)
    out[:, :2] = d.astype(np.float16).view(np.uint8).reshape(-1, 2)
    out[:, 2:] = q[:, :16] | (q[:, 16:] << 4)
    return out.reshape(-1)


class Model:
    def __init__(self, model_dir, quant_io):
        self.dir = model_dir
        self.cfg = json.load(open(os.path.join(model_dir, "config.json")))
        self.quant_io = quant_io
        index = os.path.join(model_dir, "model.safetensors.index.json")
        if os.path.exists(index):
            self.weight_map = json.load(open(index))["weight_map"]
        else:
            self.weight_map = {}
            for fn in sorted(os.listdir(model_dir)):
                if not fn.endswith(".safetensors"):
                    continue
                for name in read_safetensors_header(os.path.join(model_dir, fn)):
                    self.weight_map[name] = fn
        self._hdr = {fn: read_safetensors_header(os.path.join(model_dir, fn))
                     for fn in sorted(set(self.weight_map.values()))}

    def take(self, name):
        if name not in self.weight_map:
            raise KeyError(f"missing tensor {name}")
        shard = self.weight_map[name]
        return load_tensor(os.path.join(self.dir, shard), name, self._hdr[shard][name])

    def shape(self, name):
        return [int(v) for v in self._hdr[self.weight_map[name]][name]["shape"]]


def out_plan(model):
    """Return (name, dims, kind, loader) for every output tensor."""
    cfg = model.cfg
    n_layer, n_embd = cfg["num_hidden_layers"], cfg["hidden_size"]
    n_ff_exp = cfg["moe_intermediate_size"]
    n_expert, hc = cfg["num_experts"], cfg["num_residual_streams"]
    dense = set(cfg["mlp_only_layers"])
    mix = (2 + hc)*hc

    plan = []

    def add(name, dims, loader, kind):
        plan.append((name, list(dims), kind, loader))

    def weight_kind(dims):
        return "q4" if len(dims) == 2 and dims[0] % QK4_0 == 0 else "f16"

    def plain(hf, name):
        dims = list(reversed(model.shape(hf)))
        add(name, dims, lambda hf=hf: model.take(hf).reshape(-1), weight_kind(dims))

    def one_d(hf, name):
        add(name, [int(np.prod(model.shape(hf)))],
            lambda hf=hf: model.take(hf).astype(np.float32).reshape(-1), "f32")

    def io(hf, name):
        dims = list(reversed(model.shape(hf)))
        add(name, dims, lambda hf=hf: model.take(hf).reshape(-1),
            "q4" if model.quant_io else "f16")

    io("model.embed_tokens.weight", "token_embd.weight")
    one_d("model.norm.weight", "output_norm.weight")
    io("lm_head.weight", "output.weight")

    hc_fn = model.take("model.hc_head.hc_head_fn").astype(np.float32)
    add("output_hc_fn.weight", [hc*n_embd, hc], lambda hc_fn=hc_fn: hc_fn.reshape(-1), "f16")
    one_d("model.hc_head.hc_head_base", "output_hc_base.weight")
    one_d("model.hc_head.hc_head_scale", "output_hc_scale.weight")

    for i in range(n_layer):
        p = f"model.layers.{i}"
        plain(f"{p}.self_attn.q_proj.weight", f"blk.{i}.attn_q.weight")
        plain(f"{p}.self_attn.k_proj.weight", f"blk.{i}.attn_k.weight")
        plain(f"{p}.self_attn.v_proj.weight", f"blk.{i}.attn_v.weight")
        plain(f"{p}.self_attn.o_proj.weight", f"blk.{i}.attn_output.weight")
        one_d(f"{p}.self_attn.q_layernorm.weight", f"blk.{i}.attn_q_norm.weight")
        if f"{p}.self_attn.indexer.linear_q_proj.weight" in model.weight_map:
            plain(f"{p}.self_attn.indexer.linear_q_proj.weight", f"blk.{i}.indexer.q_proj.weight")
            plain(f"{p}.self_attn.indexer.linear_k_proj.weight", f"blk.{i}.indexer.k_proj.weight")
            one_d(f"{p}.self_attn.indexer.q_layernorm.weight", f"blk.{i}.indexer.q_norm.weight")
            one_d(f"{p}.self_attn.indexer.k_layernorm.weight", f"blk.{i}.indexer.k_norm.weight")
        one_d(f"{p}.self_attn.k_layernorm.weight", f"blk.{i}.attn_k_norm.weight")
        one_d(f"{p}.input_layernorm.weight", f"blk.{i}.attn_norm.weight")
        one_d(f"{p}.post_attention_layernorm.weight", f"blk.{i}.ffn_norm.weight")

        for tag, prefix in (("self_attn_hyper_connection", "hc_attn"),
                            ("mlp_hyper_connection", "hc_ffn")):
            fn = model.take(f"{p}.{tag}.mapping_proj.weight").astype(np.float32)
            add(f"blk.{i}.{prefix}_fn.weight", [hc*n_embd, mix],
                lambda fn=fn: fn.reshape(-1), "f16")
            one_d(f"{p}.{tag}.bias", f"blk.{i}.{prefix}_base.weight")
            sc = np.array([float(model.take(f"{p}.{tag}.alpha_pre").reshape(-1)[0]),
                           float(model.take(f"{p}.{tag}.alpha_post").reshape(-1)[0]),
                           float(model.take(f"{p}.{tag}.alpha_res").reshape(-1)[0])],
                          dtype=np.float32)
            add(f"blk.{i}.{prefix}_scale.weight", [3], lambda sc=sc: sc, "f32")

        if i in dense:
            plain(f"{p}.block_sparse_moe.w1.weight", f"blk.{i}.ffn_gate.weight")
            plain(f"{p}.block_sparse_moe.w3.weight", f"blk.{i}.ffn_up.weight")
            plain(f"{p}.block_sparse_moe.w2.weight", f"blk.{i}.ffn_down.weight")
            continue

        plain(f"{p}.block_sparse_moe.gate.weight", f"blk.{i}.ffn_gate_inp.weight")
        one_d(f"{p}.block_sparse_moe.expert_bias", f"blk.{i}.exp_probs_b.weight")

        for hf, name, out_dims in (
                ("w1", "ffn_gate_exps", [n_embd, n_ff_exp, n_expert]),
                ("w3", "ffn_up_exps",   [n_embd, n_ff_exp, n_expert]),
                ("w2", "ffn_down_exps", [n_ff_exp, n_embd, n_expert])):
            def load(hf=hf, p=p):
                mats = [model.take(f"{p}.block_sparse_moe.experts.{e}.{hf}.weight")
                        .astype(np.float32).reshape(n_ff_exp, n_embd)
                        for e in range(n_expert)]
                return np.concatenate(mats, axis=0).reshape(-1)
            add(f"blk.{i}.{name}.weight", out_dims, load, "q4")

        plain(f"{p}.block_sparse_moe.shared_expert.w1.weight", f"blk.{i}.ffn_gate_shexp.weight")
        plain(f"{p}.block_sparse_moe.shared_expert.w3.weight", f"blk.{i}.ffn_up_shexp.weight")
        plain(f"{p}.block_sparse_moe.shared_expert.w2.weight", f"blk.{i}.ffn_down_shexp.weight")

    return plan


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model_dir")
    ap.add_argument("out")
    ap.add_argument("--quant-io", action="store_true")
    ap.add_argument("--stub-vocab", action="store_true",
                    help="write a placeholder vocabulary for loader tests only")
    ap.add_argument("--chat-template", choices=["simple", "hf"], default="simple",
                    help="simple: PEG-compatible template; hf: the upstream Jinja")
    ap.add_argument("--f32", action="store_true",
                    help="write every tensor as F32 (numeric gate only)")
    args = ap.parse_args()

    model = Model(args.model_dir, args.quant_io)
    cfg = model.cfg
    if cfg.get("architectures") != ["MaxwellMoeForCausalLM"]:
        raise SystemExit(f"unexpected architectures: {cfg.get('architectures')}")

    dense = cfg["mlp_only_layers"]
    if dense != list(range(len(dense))):
        raise SystemExit(f"mlp_only_layers must be a leading prefix, got {dense}")

    plan = out_plan(model)
    if args.f32:
        plan = [(n, d, "f32", l) for n, d, _k, l in plan]

    kvbuf = io.BytesIO()
    w = Writer(kvbuf)
    kv_scalar(w, "general.architecture", T_STRING, "maxwell_moe")
    kv_scalar(w, "general.name", T_STRING, "Maxwell-V1-Flash")
    kv_scalar(w, "general.file_type", T_UINT32, 2)
    kv_scalar(w, "maxwell_moe.block_count", T_UINT32, cfg["num_hidden_layers"])
    kv_scalar(w, "maxwell_moe.embedding_length", T_UINT32, cfg["hidden_size"])
    kv_scalar(w, "maxwell_moe.feed_forward_length", T_UINT32, cfg["intermediate_size"])
    kv_scalar(w, "maxwell_moe.attention.head_count", T_UINT32, cfg["num_attention_heads"])
    kv_scalar(w, "maxwell_moe.attention.head_count_kv", T_UINT32, cfg["num_key_value_heads"])
    kv_scalar(w, "maxwell_moe.attention.key_length", T_UINT32, cfg["attention_head_size"])
    kv_scalar(w, "maxwell_moe.attention.value_length", T_UINT32, cfg["attention_head_size"])
    kv_scalar(w, "maxwell_moe.attention.layer_norm_rms_epsilon", T_FLOAT32, cfg["rms_norm_eps"])
    kv_scalar(w, "maxwell_moe.rope.freq_base", T_FLOAT32, cfg["rope_theta"])
    kv_scalar(w, "maxwell_moe.rope.dimension_count", T_UINT32, cfg["attention_head_size"])
    kv_scalar(w, "maxwell_moe.context_length", T_UINT32, cfg["max_position_embeddings"])
    kv_scalar(w, "maxwell_moe.expert_count", T_UINT32, cfg["num_experts"])
    kv_scalar(w, "maxwell_moe.expert_used_count", T_UINT32, cfg["num_experts_per_tok"])
    kv_scalar(w, "maxwell_moe.expert_feed_forward_length", T_UINT32, cfg["moe_intermediate_size"])
    kv_scalar(w, "maxwell_moe.expert_shared_feed_forward_length", T_UINT32,
              cfg["shared_expert_intermediate_size"])
    kv_scalar(w, "maxwell_moe.leading_dense_block_count", T_UINT32, len(dense))
    kv_scalar(w, "maxwell_moe.expert_weights_norm", T_BOOL, True)
    kv_scalar(w, "maxwell_moe.expert_weights_scale", T_FLOAT32, 1.0)
    kv_scalar(w, "maxwell_moe.expert_gating_func", T_UINT32, 2)  # sigmoid
    kv_scalar(w, "maxwell_moe.hyper_connection.count", T_UINT32, cfg["num_residual_streams"])
    kv_scalar(w, "maxwell_moe.hyper_connection.sinkhorn_iterations", T_UINT32,
              cfg["mhc_sinkhorn_iterations"])
    kv_scalar(w, "maxwell_moe.hyper_connection.epsilon", T_FLOAT32, 1e-6)
    kv_scalar(w, "maxwell_moe.vocab_size", T_UINT32, cfg["vocab_size"])
    kv_scalar(w, "maxwell_moe.attention.variant", T_STRING, cfg["attention_variant"])
    kv_scalar(w, "maxwell_moe.block_sparse_gqa.block_size", T_UINT32,
              cfg["block_sparse_gqa_block_size"])
    kv_scalar(w, "maxwell_moe.block_sparse_gqa.prefix_window_size", T_UINT32,
              cfg["block_sparse_gqa_prefix_window_size"])
    kv_scalar(w, "maxwell_moe.block_sparse_gqa.local_window_size", T_UINT32,
              cfg["block_sparse_gqa_local_window_size"])
    kv_scalar(w, "maxwell_moe.block_sparse_gqa.num_remote_blocks", T_UINT32,
              cfg["block_sparse_gqa_num_remote_blocks"])
    kv_scalar(w, "maxwell_moe.block_sparse_gqa.query_chunk_size", T_UINT32,
              cfg["block_sparse_gqa_query_chunk_size"])
    kv_scalar(w, "maxwell_moe.block_sparse_gqa.total_support_blocks", T_UINT32,
              cfg["block_sparse_gqa_total_support_blocks"])
    kv_scalar(w, "maxwell_moe.attention.indexer.head_count", T_UINT32,
              cfg["block_sparse_gqa_indexer_n_heads"])
    kv_scalar(w, "maxwell_moe.attention.indexer.key_length", T_UINT32,
              cfg["block_sparse_gqa_indexer_head_dim"])
    kv_scalar(w, "maxwell_moe.attention.indexer.top_k", T_UINT32,
              cfg["block_sparse_gqa_num_remote_blocks"])
    kv_scalar(w, "maxwell_moe.attention.indexer.block_size", T_UINT32,
              cfg["block_sparse_gqa_block_size"])
    kv_scalar(w, "maxwell_moe.attention.indexer.local_blocks", T_UINT32,
              cfg["block_sparse_gqa_local_window_size"] // cfg["block_sparse_gqa_block_size"])
    # The upstream Jinja template uses namespace(), split(), raise_exception and
    # tojson; the server's PEG auto-parser cannot turn that into a grammar, and
    # every parse fails. Write the equivalent plain form (same tags, same roles,
    # same reasoning placement); the upstream file stays in the model directory.
    simple_template = (
        "{% for m in messages %}"
        "{{ '<|maxwell_start|>' + m.role + '\n' + m.content + '<|maxwell_end|>\n' }}"
        "{% endfor %}"
        "{% if add_generation_prompt %}{{ '<|maxwell_start|>assistant\n' }}{% endif %}"
    )
    chat_tpl = os.path.join(args.model_dir, "chat_template.jinja")
    if args.chat_template == "hf" and os.path.exists(chat_tpl):
        kv_scalar(w, "tokenizer.chat_template", T_STRING, open(chat_tpl, encoding="utf-8").read())
    else:
        kv_scalar(w, "tokenizer.chat_template", T_STRING, simple_template)

    tok_json = os.path.join(args.model_dir, "tokenizer.json")
    if args.stub_vocab:
        eos_ids = read_eos_ids(args.model_dir, cfg["eos_token_id"])
        write_stub_vocab(w, cfg["vocab_size"],
                         eot=eos_ids[1] if len(eos_ids) > 1 else None,
                         eom=eos_ids[2] if len(eos_ids) > 2 else None)
        pre, n_merges = "default", 0
        extra_eos = eos_ids[1:]
    else:
        if not os.path.exists(tok_json):
            raise SystemExit(f"{tok_json} is missing; pass --stub-vocab only for loader tests")
        eos_ids = read_eos_ids(args.model_dir, cfg["eos_token_id"])
        pre, n_merges = write_hf_vocab(w, tok_json, cfg["vocab_size"],
                                       cfg["bos_token_id"], eos_ids[0],
                                       cfg["pad_token_id"],
                                       eot=eos_ids[1] if len(eos_ids) > 1 else None,
                                       eom=eos_ids[2] if len(eos_ids) > 2 else None)
        extra_eos = eos_ids[1:]
    n_kv = w.kv_count

    sizes = []
    for name, dims, kind, _ in plan:
        n = int(np.prod(dims))
        if kind == "q4":
            assert dims[0] % QK4_0 == 0, (name, dims)
            nb = (dims[0] // QK4_0) * 18 * (n // dims[0])
        elif kind == "f16":
            nb = n * 2
        else:
            nb = n * 4
        sizes.append(nb)

    with open(args.out, "wb") as fh:
        w2 = Writer(fh)
        w2.u32(GGUF_MAGIC)
        w2.u32(GGUF_VERSION)
        w2.u64(len(plan))
        w2.u64(n_kv)
        fh.write(kvbuf.getvalue())

        w = Writer(fh)
        offset = 0
        for (name, dims, kind, _), nb in zip(plan, sizes):
            w.string(name)
            w.u32(len(dims))
            for d in dims:
                w.u64(d)
            w.u32({"q4": GGML_Q4_0, "f16": GGML_F16, "f32": GGML_F32}[kind])
            w.u64(offset)
            offset += nb
            offset += (-offset) % ALIGN
        pos = fh.tell()
        fh.write(b"\x00" * ((-pos) % ALIGN))

        written = 0
        for (name, dims, kind, loader), nb in zip(plan, sizes):
            flat = np.asarray(loader(), dtype=np.float32).reshape(-1)
            if kind == "q4":
                row = dims[0]
                rows = flat.size // row
                data = b"".join(quantize_q4_0(flat[r*row:(r+1)*row]).tobytes()
                                for r in range(rows))
            elif kind == "f16":
                data = flat.astype("<f2").tobytes()
            else:
                data = flat.astype("<f4").tobytes()
            if len(data) != nb:
                raise RuntimeError(f"{name}: wrote {len(data)}, planned {nb}")
            fh.write(data)
            written += len(data)
            pad = (-written) % ALIGN
            fh.write(b"\x00" * pad)
            written += pad

    print(f"tensors={len(plan)} kv={n_kv} pre={pre} merges={n_merges} "
          f"eos_extra={extra_eos} bytes={os.path.getsize(args.out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
