#!/usr/bin/env python3
"""Generate the tiny hybrid GGUF from a flat name list and a shape table (both edited by driver2.py)."""
import sys, json, re, numpy as np, gguf
from pathlib import Path
E = Path(__file__).resolve().parent
names = [l.strip() for l in (E/"names-mutant.txt").read_text().splitlines() if l.strip()]
import os
NLAY = int(os.environ.get('TINY_LAYERS', '2'))
REC  = [b == '1' for b in os.environ.get('TINY_RECUR', '10')]
if NLAY > 2:
    # replicate layer 1's (attention) tensors for the extra layers, and make the last one recurrent if REC says so
    extra = []
    for n in names:
        if n.startswith('blk.1.'):
            for il in range(2, NLAY):
                extra.append(n.replace('blk.1.', f'blk.{il}.'))
    names = names + extra
    if len(REC) >= NLAY and REC[-1]:
        # a recurrent extra layer needs layer-0's ssm/qkv tensors under its own index
        for n in [x for x in names if x.startswith('blk.0.')]:
            suff = n.split('.', 2)[2]
            for il in range(2, NLAY):
                if REC[il] and f'blk.{il}.{suff}' not in names:
                    names.append(f'blk.{il}.{suff}')

shapes = json.loads((E/"shapes-mutant.json").read_text())
w = gguf.GGUFWriter(str(E/"tiny-mutant.gguf"), "qwen35moe")
w.add_block_count(NLAY); w.add_context_length(256); w.add_embedding_length(32)
w.add_head_count(2); w.add_head_count_kv(1); w.add_key_length(16); w.add_value_length(16)
w.add_feed_forward_length(16); w.add_expert_count(4); w.add_expert_used_count(2)
w.add_expert_feed_forward_length(16); w.add_expert_shared_feed_forward_length(16)
w.add_layer_norm_rms_eps(1e-6); w.add_rope_freq_base(10000.0); w.add_rope_dimension_count(16)
w.add_array("qwen35moe.rope.dimension_sections", [6,6,4,0])
w.add_uint32("qwen35moe.nextn_predict_layers", 0)
w.add_uint32("qwen35moe.full_attention_interval", 2)
w.add_uint32("qwen35moe.ssm.conv_kernel", 4)
w.add_uint32("qwen35moe.ssm.state_size", 8)
w.add_uint32("qwen35moe.ssm.group_count", 2)
w.add_uint32("qwen35moe.ssm.time_step_rank", 4)
w.add_uint32("qwen35moe.ssm.inner_size", 32)
w.add_array("qwen35moe.attention.recurrent_layers", REC)
w.add_tokenizer_model("gpt2"); w.add_tokenizer_pre("qwen35")
toks=[f"t{i}" for i in range(64)]
w.add_token_list(toks); w.add_token_types([1]*64); w.add_token_merges(["t 0"])
w.add_eos_token_id(2); w.add_pad_token_id(1)
for n in names:
    key = re.sub(r'^blk\.\d+\.', 'blk.0.', n) if False else n
    dims = tuple(shapes.get(n, shapes.get(re.sub(r'blk\.\d+\.', 'blk.0.', n), [1])))
    if n.startswith('blk.') and n.split('.')[1] != '0':
        alt = re.sub(r'blk\.\d+\.', 'blk.1.', n)
        dims = tuple(shapes.get(n, shapes.get(alt, shapes.get(re.sub(r'blk\.\d+\.', 'blk.0.', n), [1]))))
    w.add_tensor(n, np.zeros(tuple(reversed(dims)), dtype=np.float32))
w.write_header_to_file(); w.write_kv_data_to_file(); w.write_tensors_to_file(); w.close()
print("written", len(names))
