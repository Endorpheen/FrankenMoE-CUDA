"""Write a two-layer GGUF for CPU graph and chunked-generation checks."""

import math
import struct
import sys
from pathlib import Path

def string(value):
    data = value.encode()
    return struct.pack('<Q', len(data)) + data

metadata = []
def key(name, dtype, value):
    metadata.append(string(name) + struct.pack('<I', dtype) + value)
def u32(name, value):
    key(name, 4, struct.pack('<I', value))

key('general.architecture', 8, string('qwen35moe'))
for name, value in {
    'block_count': 2, 'embedding_length': 32, 'attention.head_count': 1,
    'attention.head_count_kv': 1, 'attention.key_length': 32,
    'ssm.state_size': 32, 'ssm.group_count': 1, 'ssm.time_step_rank': 1,
    'ssm.conv_kernel': 2, 'ssm.inner_size': 32, 'expert_count': 2,
    'expert_used_count': 1, 'expert_feed_forward_length': 32,
    'expert_shared_feed_forward_length': 32, 'context_length': 262144,
    'rope.dimension_count': 32,
}.items():
    u32('qwen35moe.' + name, value)
key('qwen35moe.attention.layer_norm_rms_epsilon', 6, struct.pack('<f', 1e-5))
key('qwen35moe.rope.freq_base', 6, struct.pack('<f', 10000))
key('qwen35moe.rope.dimension_sections', 9, struct.pack('<IQ4i', 5, 4, 16, 0, 0, 0))
key('qwen35moe.attention.recurrent_layers', 9, struct.pack('<IQ2B', 7, 2, 0, 1))
key('tokenizer.ggml.tokens', 9,
    struct.pack('<IQ', 8, 4) + b''.join(map(string, ['a', 'b', 'ab', '<|endoftext|>'])))
key('tokenizer.ggml.merges', 9, struct.pack('<IQ', 8, 1) + string('a b'))
key('tokenizer.ggml.token_type', 9, struct.pack('<IQ4i', 5, 4, 1, 1, 1, 3))
u32('tokenizer.ggml.eos_token_id', 3)

tensors = [
    ('token_embd.weight', (32, 4)),
    ('output_norm.weight', (32,)),
]
attention = {
    'attn_norm.weight': (32,), 'attn_q.weight': (32, 64),
    'attn_k.weight': (32,32), 'attn_v.weight': (32,32),
    'attn_output.weight': (32,32), 'attn_q_norm.weight': (32,),
    'attn_k_norm.weight': (32,), 'post_attention_norm.weight': (32,),
}
recurrent = {
    'attn_norm.weight': (32,), 'attn_qkv.weight': (32,96), 'attn_gate.weight': (32,32),
    'ssm_beta.weight': (32,1), 'ssm_alpha.weight': (32,1), 'ssm_dt.bias': (1,),
    'ssm_a': (1,), 'ssm_conv1d.weight': (2,96), 'ssm_norm.weight': (32,),
    'ssm_out.weight': (32,32), 'post_attention_norm.weight': (32,),
}
moe = {
    'ffn_gate_inp.weight': (32,2), 'ffn_up_exps.weight': (32,32,2),
    'ffn_gate_exps.weight': (32,32,2), 'ffn_down_exps.weight': (32,32,2),
    'ffn_up_shexp.weight': (32,32), 'ffn_gate_shexp.weight': (32,32),
    'ffn_down_shexp.weight': (32,32), 'ffn_gate_inp_shexp.weight': (32,),
}
for layer, branch in enumerate((attention,recurrent)):
    tensors.extend((f'blk.{layer}.{name}', shape) for name, shape in (branch | moe).items())
payload = bytearray()
records = []
stateful = sys.argv[2:] == ['--stateful']
for name, shape in tensors:
    offset = len(payload)
    records.append(string(name) + struct.pack('<I', len(shape)) + struct.pack('<'+'Q'*len(shape), *shape)
                   + struct.pack('<IQ', 0, offset))
    values = [0.0] * math.prod(shape)
    if name == 'token_embd.weight':
        values[64] = 1.0
        if stateful:
            values[0] = 0.5
            values[33] = 0.75
            values[65] = -0.25
    elif name == 'output_norm.weight':
        values = [1.0] * len(values)
    elif stateful and 'norm.weight' in name:
        values = [1.0] * len(values)
    elif stateful and any(word in name for word in ('attn_', 'ssm_')):
        values = [((11*i + i//7) % 17 - 8) / 128 for i in range(len(values))]
    payload += struct.pack('<' + 'f'*len(values), *values)
    payload += bytes((-len(payload)) % 32)
header = struct.pack('<4sIQQ', b'GGUF', 3, len(records), len(metadata))
header += b''.join(metadata) + b''.join(records)
Path(sys.argv[1]).write_bytes(header + bytes((-len(header)) % 32) + payload)
