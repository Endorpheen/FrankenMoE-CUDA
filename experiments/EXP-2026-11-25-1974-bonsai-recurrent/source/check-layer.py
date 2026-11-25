"""Compare a resident Bonsai recurrent layer with independent CPU arithmetic."""
import ctypes as ct
from hashlib import sha256
import importlib.util
import json
import math
import mmap
from pathlib import Path
import re
import struct
import sys

import numpy as np

here = Path(__file__).resolve().parent
previous = here.parent.parent / 'EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source'
spec = importlib.util.spec_from_file_location('bootstrap', previous / 'probe.py')
gpu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gpu)
F, U, D, P, S = gpu.F, gpu.U, gpu.D, gpu.P, gpu.S
HIDDEN, WIDTH, HEADS, KEY_HEADS = 5120, 128, 48, 16
MIXED, VALUES, FFN = 10240, WIDTH * HEADS, 17408
STATE = VALUES * WIDTH
EPS = gpu.inventory['metadata']['qwen35.attention.layer_norm_rms_epsilon']


def silu(x):
    return x / (1 + np.exp(-x))


def rotation(x, signs, grouped=False):
    if grouped:
        x = x.reshape(3, KEY_HEADS, WIDTH).transpose(1, 0, 2).ravel()
    x = (x * signs[len(x)]).reshape(-1, 1024)
    span = 1
    while span < 1024:
        pairs = x.reshape(-1, 2, span)
        a, b = pairs[:, 0].copy(), pairs[:, 1].copy()
        pairs[:, 0], pairs[:, 1] = a + b, a - b
        span *= 2
    return x.ravel() / 32


def unpack(encoded, rows, width):
    blocks = np.frombuffer(encoded, dtype=np.uint8).reshape(-1, 34)
    scales = blocks[:, :2].copy().view('<f2').astype(np.float32)
    codes = ((blocks[:, 2:, None] >> np.array([0, 2, 4, 6], dtype=np.uint8)) & 3).astype(np.float32)
    return ((codes.reshape(-1, 128) - 1) * scales).reshape(rows, width)


def reference(x, weights, signs, conv, state):
    def rms(value, weight, axis=None):
        return value * weight / np.sqrt((value * value).mean(axis=axis, keepdims=True) + EPS)

    h = rms(x, weights['attn_norm.weight'])
    hr = rotation(h, signs)
    mix = weights['attn_qkv.weight'] @ hr
    z = weights['attn_gate.weight'] @ hr
    gate = weights['ssm_a'] * np.logaddexp(0, weights['ssm_alpha.weight'] @ h + weights['ssm_dt.bias'])
    beta = 1 / (1 + np.exp(-(weights['ssm_beta.weight'] @ h)))
    window = np.vstack((conv, mix))
    qkv = silu(np.einsum('tc,ct->c', window, weights['ssm_conv1d.weight'])).reshape(-1, WIDTH)
    qk = qkv[:2 * KEY_HEADS]
    qk /= np.maximum(np.linalg.norm(qk, axis=1, keepdims=True), EPS)
    q, k = (np.tile(group, (3, 1)) for group in np.split(qk, 2))
    v = qkv[2 * KEY_HEADS:]
    old = state * np.exp(gate)[:, None, None]
    delta = (v - np.einsum('hvk,hk->hv', old, k)) * beta[:, None]
    state = old + delta[:, :, None] * k[:, None, :]
    gdn = np.einsum('hvk,hk->hv', state, q) / math.sqrt(WIDTH)
    gated = rms(gdn, weights['ssm_norm.weight'], axis=1) * silu(z.reshape(HEADS, WIDTH))
    a = x + weights['ssm_out.weight'] @ rotation(gated.ravel(), signs, grouped=True)
    ar = rotation(rms(a, weights['post_attention_norm.weight']), signs)
    expanded = silu(weights['ffn_gate.weight'] @ ar) * (weights['ffn_up.weight'] @ ar)
    return a + weights['ffn_down.weight'] @ rotation(expanded, signs), window[1:], state


def recurrent_packet(q, k, v, gate, mix, old, score, new):
    q_stride = (1, WIDTH, KEY_HEADS * WIDTH, KEY_HEADS * WIDTH)
    config = [*q_stride, *q_stride, 1, WIDTH, VALUES, VALUES,
              1, 1, HEADS, HEADS, 1, 1, HEADS, HEADS, 1, WIDTH, WIDTH * WIDTH, STATE,
              WIDTH, HEADS, 1, 1, 0, 0, 1, 0, 0, 0, 8, 32, 4, 32, 1,
              STATE, 0, struct.unpack('<I', struct.pack('<f', 1 / math.sqrt(WIDTH)))[0],
              1, 4, KEY_HEADS, 1, 0, 1, VALUES]
    assert len(config) == 49
    return (D * 57)(*(p.value for p in (q, k, v, gate, mix, old, score, new)), *config)


class Layer:
    def __init__(self, kernel_file, frozen_header, encoded, signs):
        self.module = self.load(kernel_file.read_bytes())
        array = re.search(rb'goose_cuda_mmq_image\[\] = \{(.*?)\};', frozen_header.read_bytes(), re.S)
        assert array, 'Frozen GDN image missing'
        image = bytes(int(value, 0) for value in array[1].split(b',') if value.strip())
        self.image_hash = sha256(image).hexdigest()
        gdn_module = self.load(image)
        self.gdn = P()
        gpu.call('cuModuleGetFunction', ct.byref(self.gdn), gdn_module, b'goose_gdn_decode')
        self.weights = {name: self.stage(data) for name, data in encoded.items()}
        self.signs = {width: self.stage(sign.astype('<i4').tobytes()) for width, sign in signs.items()}
        self.history = gpu.allocate(3 * MIXED * 4)
        self.states = [gpu.allocate(STATE * 4), gpu.allocate(STATE * 4)]
        self.x, self.h, self.hr, self.out, self.residual = [gpu.allocate(HIDDEN * 4) for _ in range(5)]
        self.mixed, self.conv = [gpu.allocate(MIXED * 4) for _ in range(2)]
        self.z, self.score, self.gated, self.rotated = [gpu.allocate(VALUES * 4) for _ in range(4)]
        self.gate, self.mix = [gpu.allocate(HEADS * 4) for _ in range(2)]
        self.qk = gpu.allocate(2 * KEY_HEADS * WIDTH * 4)
        self.ffn_gate, self.ffn_up, self.ffn_value, self.ffn_rotated = [gpu.allocate(FFN * 4) for _ in range(4)]

    @staticmethod
    def load(data):
        module = P()
        gpu.call('cuModuleLoadData', ct.byref(module), ct.create_string_buffer(data))
        return module

    @staticmethod
    def stage(data):
        pointer = gpu.allocate(len(data))
        gpu.upload(pointer, (ct.c_uint8 * len(data)).from_buffer_copy(data))
        return pointer

    def reset(self):
        for pointer, size in ((self.history, 3 * MIXED), *((p, STATE) for p in self.states)):
            gpu.call('cuMemsetD32_v2', pointer, U(0), S(size))

    def run(self, row):
        def run(name, grid, block, *args):
            gpu.launch(self.module, 'bonsai_' + name, grid, block, args)

        def mat(name, x, out, width, rows):
            run('pq2_matvec', rows // 4, 128, self.weights[name + '.weight'], x, out, U(width), U(rows))

        def rotate(x, out, width, inverse=0, grouped=False):
            geometry = (WIDTH, KEY_HEADS, 3) if grouped else (0, 0, 1)
            run('fwht1024', width // 1024, 256, x, self.signs[width], out, U(inverse), *map(U, geometry))

        def norm(x, weight, gate, out, width, rows, mode=0):
            w = D(0) if weight is None else self.weights[weight + '.weight']
            run('norm', rows, 32, x, w, gate, out, U(width), F(EPS), U(mode))

        run('pq2_row', HIDDEN // 256, 256, self.weights['token_embd.weight'], self.h, U(HIDDEN), U(row))
        rotate(self.h, self.x, HIDDEN, inverse=1)
        norm(self.x, 'attn_norm', D(0), self.h, HIDDEN, 1)
        rotate(self.h, self.hr, HIDDEN)
        mat('attn_qkv', self.hr, self.mixed, HIDDEN, MIXED)
        mat('attn_gate', self.hr, self.z, HIDDEN, VALUES)
        run('decay', HEADS, 32, self.h, *(self.weights[name] for name in
            ('ssm_alpha.weight', 'ssm_beta.weight', 'ssm_dt.bias', 'ssm_a')), self.gate, self.mix)
        run('conv_step', MIXED // 256, 256, self.mixed, self.weights['ssm_conv1d.weight'], self.history, self.conv)
        norm(self.conv, None, D(0), self.qk, WIDTH, 2 * KEY_HEADS, mode=1)
        k = D(self.qk.value + KEY_HEADS * WIDTH * 4)
        v = D(self.conv.value + 2 * KEY_HEADS * WIDTH * 4)
        packet = recurrent_packet(self.qk, k, v, self.gate, self.mix, self.states[0], self.score, self.states[1])
        gpu.call('cuLaunchKernel', self.gdn, U(HEADS), U(1), U(8), U(WIDTH), U(1), U(1),
                 U(0), P(), (P * 1)(ct.addressof(packet)), P())
        self.states.reverse()
        norm(self.score, 'ssm_norm', self.z, self.gated, WIDTH, HEADS, mode=2)
        rotate(self.gated, self.rotated, VALUES, grouped=True)
        mat('ssm_out', self.rotated, self.out, VALUES, HIDDEN)
        run('residual', HIDDEN // 256, 256, self.x, self.out, self.residual)
        norm(self.residual, 'post_attention_norm', D(0), self.h, HIDDEN, 1)
        rotate(self.h, self.hr, HIDDEN)
        mat('ffn_gate', self.hr, self.ffn_gate, HIDDEN, FFN)
        mat('ffn_up', self.hr, self.ffn_up, HIDDEN, FFN)
        run('swiglu', FFN // 256, 256, self.ffn_gate, self.ffn_up, self.ffn_value, U(FFN))
        rotate(self.ffn_value, self.ffn_rotated, FFN)
        mat('ffn_down', self.ffn_rotated, self.out, FFN, HIDDEN)
        run('residual', HIDDEN // 256, 256, self.residual, self.out, self.out)
        return tuple(np.ctypeslib.as_array(gpu.download(pointer, size)) for pointer, size in
                     ((self.out, HIDDEN), (self.history, 3 * MIXED), (self.states[0], STATE)))


def main():
    kernel_file, frozen_header = map(Path, sys.argv[3:5])
    ids = [1511, 17, 248046, 1511]
    tensors = gpu.inventory['tensors']
    embedding = next(t for t in tensors if t['name'] == 'token_embd.weight')
    row_bytes = HIDDEN // 128 * 34
    signs, at = {}, 0
    for width in gpu.inventory['metadata']['prism.hadamard.sign_widths']:
        signs[width] = np.array(gpu.inventory['metadata']['prism.hadamard.sign_values'][at:at+width], dtype=np.float32)
        at += width
    encoded, weights = {}, {}
    with gpu.model.open('rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
        offset = gpu.inventory['data_offset']
        start = offset + embedding['offset']
        encoded['token_embd.weight'] = b''.join(mapped[start + i * row_bytes:start + (i + 1) * row_bytes] for i in ids)
        embeddings = unpack(encoded['token_embd.weight'], len(ids), HIDDEN)
        for t in tensors:
            if not t['name'].startswith('blk.0.'):
                continue
            name = t['name'][6:]
            shape = t['shape']
            start = offset + t['offset']
            if t['type'] == 142:
                encoded[name] = mapped[start:start + math.prod(shape) // 128 * 34]
                weights[name] = unpack(encoded[name], shape[1], shape[0])
            else:
                assert t['type'] == 0
                encoded[name] = mapped[start:start + math.prod(shape) * 4]
                weights[name] = np.frombuffer(encoded[name], dtype='<f4').reshape(shape[::-1])

    device, context = gpu.I(), P()
    gpu.call('cuInit', U(0))
    gpu.call('cuDeviceGet', ct.byref(device), gpu.I(0))
    gpu.call('cuCtxCreate_v2', ct.byref(context), U(0), device)
    try:
        layer = Layer(kernel_file, frozen_header, encoded, signs)
        layer.reset()
        conv = np.zeros((3, MIXED), dtype=np.float64)
        state = np.zeros((HEADS, WIDTH, WIDTH), dtype=np.float64)
        refs, runs, rows = [], [], []
        for index, token in enumerate(ids):
            x = rotation(embeddings[index].astype(np.float64), {HIDDEN: np.ones(HIDDEN)}) * signs[HIDDEN]
            out, conv, state = reference(x, weights, signs, conv, state)
            expected = (out, conv.ravel(), state.ravel())
            observed = layer.run(index)
            errors = [float(np.max(np.abs(a - b))) for a, b in zip(observed, expected)]
            for a, b in zip(observed, expected):
                np.testing.assert_allclose(a, b, atol=2e-5, rtol=5e-4, err_msg=f'token {index}')
            runs.append(tuple(a.tobytes() for a in observed))
            refs.append(out)
            rows.append({'token': token, 'max_absolute_error_output_history_state': errors})
        assert not np.allclose(refs[0], refs[-1], atol=1e-6), 'Fixture must exercise prior state'
        layer.reset()
        for index, result in enumerate(runs):
            assert tuple(a.tobytes() for a in layer.run(index)) == result
        print(json.dumps({'layer': 0, 'tokens': rows, 'deterministic_reset': True,
                          'frozen_cuda_image_sha256': layer.image_hash,
                          'candidate_cuda_image_sha256': sha256(kernel_file.read_bytes()).hexdigest()}, indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2', context)


if __name__ == '__main__':
    main()
