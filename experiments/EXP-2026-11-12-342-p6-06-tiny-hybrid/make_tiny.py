#!/usr/bin/env python3
"""Generate a minimal hybrid qwen35moe GGUF: two layers, one recurrent and one full-attention."""
import sys, gguf, numpy as np
from gguf import GGUFWriter

out = sys.argv[1] if len(sys.argv) > 1 else "tiny-hybrid.gguf"
w = GGUFWriter(out, "qwen35moe")

n_layer, n_embd, n_vocab = 2, 32, 64
n_head, n_head_kv = 2, 1
n_ff, n_expert, n_used = 16, 4, 2
n_embd_head = 16

w.add_block_count(n_layer)
w.add_context_length(256)
w.add_embedding_length(n_embd)
w.add_head_count(n_head)
w.add_head_count_kv(n_head_kv)
w.add_key_length(n_embd_head)
w.add_value_length(n_embd_head)
w.add_feed_forward_length(n_ff)
w.add_expert_count(n_expert)
w.add_expert_used_count(n_used)
w.add_expert_feed_forward_length(n_ff)
w.add_expert_shared_feed_forward_length(n_ff)
w.add_layer_norm_rms_eps(1e-6)
w.add_rope_freq_base(10000.0)
w.add_rope_dimension_count(n_embd_head)
w.add_uint32("qwen35moe.rope.dimension_sections", 0)
w.add_array("qwen35moe.rope.dimension_sections", [0,0,0,0])
w.add_uint32("qwen35moe.nextn_predict_layers", 0)
w.add_uint32("qwen35moe.full_attention_interval", 2)
w.add_uint32("qwen35moe.ssm.conv_kernel", 4)
w.add_uint32("qwen35moe.ssm.state_size", 8)
w.add_uint32("qwen35moe.ssm.group_count", 2)
w.add_uint32("qwen35moe.ssm.time_step_rank", 4)
w.add_uint32("qwen35moe.ssm.inner_size", 32)
w.add_array("qwen35moe.attention.recurrent_layers", [True, False])
w.add_tokenizer_model("gpt2")
w.add_tokenizer_pre("qwen35")
toks = [f"t{i}" for i in range(n_vocab)]
w.add_token_list(toks)
w.add_token_types([1]*n_vocab)
w.add_token_merges(["t 0"])
w.add_eos_token_id(0)
w.add_pad_token_id(1)

_seen = set()
def T(name, shape, dtype=np.float32):
    if name in _seen:
        print("DUPLICATE", name)
    _seen.add(name)
    w.add_tensor(name, np.zeros(tuple(reversed(shape)), dtype=dtype))

T("token_embd.weight", (n_embd, n_vocab))
T("output_norm.weight", (n_embd,))
T("output.weight", (n_embd, n_vocab))
for il in range(n_layer):
    rec = (il == 0)
    pre = f"blk.{il}."
    T(pre+"attn_norm.weight", (n_embd,))
    if not rec:
        T(pre+"attn_q.weight", (32, 64))
        T(pre+"attn_k.weight", (n_embd, n_head_kv*n_embd_head))
        T(pre+"attn_v.weight", (n_embd, n_head_kv*n_embd_head))
        T(pre+"attn_output.weight", (n_head*n_embd_head, n_embd))
    T(pre+"post_attention_norm.weight", (n_embd,))
    T(pre+"attn_qkv.weight", (32, 96 if not rec else 64))
    T(pre+"attn_gate.weight", (n_embd, n_embd))
    if rec:
        T(pre+"ssm_conv1d.weight", (4, 64))
        T(pre+"ssm_dt.bias", (4,))
        T(pre+"ssm_a", (4,))
        T(pre+"ssm_d", (n_embd,))
        T(pre+"ssm_norm.weight", (8,))
        T(pre+"ssm_out.weight", (n_embd, n_embd))
        T(pre+"ssm_alpha.weight", (32, 4))
        T(pre+"ssm_beta.weight", (32, 4))
    T(pre+"ffn_gate_exps.weight", (32, 16, 4))
    T(pre+"ffn_gate_inp.weight", (32, 4))
    T(pre+"ffn_up_exps.weight", (32, 16, 4))
    T(pre+"ffn_down_exps.weight", (16, 32, 4))
    T(pre+"ffn_gate_shexp.weight", (n_embd, n_ff))
    T(pre+"ffn_up_shexp.weight", (n_embd, n_ff))
    T(pre+"ffn_gate_inp_shexp.weight", (n_embd,))
    T(pre+"attn_q_norm.weight", (16,))
    T(pre+"attn_k_norm.weight", (16,))
    T(pre+"ffn_down_shexp.weight", (n_ff, n_embd))
w.write_header_to_file(); w.write_kv_data_to_file(); w.write_tensors_to_file(); w.close()
print("written", out)
