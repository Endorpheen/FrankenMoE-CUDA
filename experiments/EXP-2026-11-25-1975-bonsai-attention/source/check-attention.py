"""Validate Bonsai attention and Q4 cache against a CPU layer reference."""
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

experiment = Path(__file__).resolve().parents[2]
recurrent_path = experiment / 'EXP-2026-11-25-1974-bonsai-recurrent/source/check-layer.py'
spec = importlib.util.spec_from_file_location('recurrent', recurrent_path)
rec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rec)
gpu = rec.gpu
F, U, D, P, S = gpu.F, gpu.U, gpu.D, gpu.P, gpu.S
HIDDEN, FFN, EPS = rec.HIDDEN, rec.FFN, rec.EPS
HEAD_WIDTH, Q_HEADS, KV_HEADS = 256, 24, 4
QV, KV = HEAD_WIDTH * Q_HEADS, HEAD_WIDTH * KV_HEADS
ROW_BYTES = HEAD_WIDTH // 32 * 18
KV_BYTES = KV_HEADS * ROW_BYTES
ROPE_PAIRS, ROPE_BASE = 32, 1e7


def f32_word(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def argmax_q4(x):
    x = x.astype(np.float32).reshape(-1, 32)
    largest = x[np.arange(len(x)), np.abs(x).argmax(axis=1)]
    scale = largest / np.float32(-8)
    inv = np.divide(1, scale, out=np.zeros_like(scale), where=scale != 0)
    code = np.minimum(15, (x * inv[:, None] + np.float32(8.5)).astype(np.uint8))
    payload = code[:, :16] | (code[:, 16:] << 4)
    return np.column_stack((scale.astype('<f2').view(np.uint8).reshape(-1, 2), payload)).tobytes()


def unpack_q4(data):
    row = np.frombuffer(data, dtype=np.uint8).reshape(-1, 18)
    scale = row[:, :2].copy().view('<f2').astype(np.float64)
    packed = row[:, 2:]
    values = np.column_stack((packed & 15, packed >> 4)).astype(np.float64) - 8
    return (scale * values).reshape(KV_HEADS, HEAD_WIDTH)


def rope(x, position):
    phases = position * ROPE_BASE ** (-np.arange(ROPE_PAIRS, dtype=np.float64) / ROPE_PAIRS)
    a, b = x[:, :32].copy(), x[:, 32:64].copy()
    x[:, :32] = a * np.cos(phases) - b * np.sin(phases)
    x[:, 32:64] = a * np.sin(phases) + b * np.cos(phases)
    return x


def reference(x, position, weights, signs, cached):
    def rms(value, weight):
        return value * weight / np.sqrt(np.mean(value * value, axis=-1, keepdims=True) + EPS)

    h = rec.rotation(rms(x, weights['attn_norm.weight']), signs)
    qg = (weights['attn_q.weight'] @ h).reshape(Q_HEADS, 2, HEAD_WIDTH)
    q = rope(rms(qg[:, 0], weights['attn_q_norm.weight']), position)
    k = rope(rms((weights['attn_k.weight'] @ h).reshape(KV_HEADS, HEAD_WIDTH),
                 weights['attn_k_norm.weight']), position)
    v = (weights['attn_v.weight'] @ h).reshape(KV_HEADS, HEAD_WIDTH)
    cache_line = (argmax_q4(k), argmax_q4(v))
    output = reference_finish(x, q, qg[:, 1], [*cached, cache_line], weights, signs, rms)
    return output, cache_line


def reference_finish(x, q, gate, cache, weights, signs, rms):
    keys = np.stack([unpack_q4(key) for key, _ in cache])
    values = np.stack([unpack_q4(value) for _, value in cache])
    groups = np.arange(Q_HEADS) // (Q_HEADS // KV_HEADS)
    score = np.einsum('hd,thd->ht', q, keys[:, groups]) / math.sqrt(HEAD_WIDTH)
    prob = np.exp(score - score.max(axis=1, keepdims=True))
    prob /= prob.sum(axis=1, keepdims=True)
    attended = np.einsum('ht,thd->hd', prob, values[:, groups]) / (1 + np.exp(-gate))
    residual = x + weights['attn_output.weight'] @ rec.rotation(attended.ravel(), signs)
    r = rec.rotation(rms(residual, weights['post_attention_norm.weight']), signs)
    inner = rec.silu(weights['ffn_gate.weight'] @ r) * (weights['ffn_up.weight'] @ r)
    return residual + weights['ffn_down.weight'] @ rec.rotation(inner, signs)


class Attention:
    def __init__(self, module, image, encoded, signs, capacity):
        self.module = rec.Layer.load(module)
        self.frozen = rec.Layer.load(image)
        self.w = {name: rec.Layer.stage(data) for name, data in encoded.items()}
        self.signs = {n: rec.Layer.stage(x.astype('<i4').tobytes()) for n, x in signs.items()}
        self.cache = [gpu.allocate(capacity * KV_BYTES) for _ in range(2)]
        self.capacity = capacity
        self.count = 0
        self.attention_config = gpu.allocate(22 * 8)
        self.rope_config = gpu.allocate(27 * 8)
        self.positions = gpu.allocate(4 * 4)
        tiles = (capacity + 255) // 256
        self.partial = gpu.allocate(Q_HEADS * tiles * (HEAD_WIDTH + 2) * 4)
        shapes = dict(x=HIDDEN, h=HIDDEN, hr=HIDDEN, qg=2*QV, q=QV, qnorm=QV, qr=QV, gate=QV,
                      k=KV, knorm=KV, kr=KV, v=KV, att=QV, gated=QV, rot=QV, out=HIDDEN,
                      residual=HIDDEN, ffn_gate=FFN, ffn_up=FFN, ffn=FFN, ffn_rot=FFN)
        self.b = {name: gpu.allocate(n * 4) for name, n in shapes.items()}

    def reset(self):
        for buffer in self.cache:
            gpu.call('cuMemsetD8_v2', buffer, ct.c_uint8(0), S(KV_BYTES * self.capacity))
        self.count = 0

    def run(self, embedding_row, position):
        b = self.b

        def launch(name, grid, lanes, *args):
            gpu.launch(self.module, 'bonsai_' + name, grid, lanes, args)

        def rotate(src, dst, width, inverse=0):
            launch('fwht1024', width//1024, 256, b[src], self.signs[width], b[dst],
                   U(inverse), U(0), U(0), U(1))

        def norm(src, weight, dst, width, heads=1):
            launch('norm', heads, 32, b[src], self.w[weight + '.weight'], D(0), b[dst], U(width), F(EPS), U(0))

        def mat(name, src, dst, cols, rows):
            launch('pq2_matvec', rows//4, 128, self.w[name + '.weight'], b[src], b[dst], U(cols), U(rows))

        def rotate_positions(src, dst, heads):
            stride = HEAD_WIDTH * 4
            config = (gpu.L * 27)(
                2, heads, 1, 1, HEAD_WIDTH * 4, HEAD_WIDTH, heads, 1, 1, 64, 40, 0,
                11, 11, 10, 0, 4, stride, heads*stride, heads*stride, 1,
                f32_word(ROPE_BASE ** (-1 / ROPE_PAIRS)), f32_word(1), 0, f32_word(1), 0, 0,
            )
            gpu.upload(self.rope_config, config)
            gpu.launch(self.frozen, 'goose_rope_decode', heads, 128,
                       (b[src], self.positions, D(0), b[dst], self.rope_config))

        launch('pq2_row', HIDDEN//256, 256, self.w['token_embd.weight'], b['h'], U(HIDDEN), U(embedding_row))
        rotate('h', 'x', HIDDEN, inverse=1)
        norm('x', 'attn_norm', 'h', HIDDEN)
        rotate('h', 'hr', HIDDEN)
        mat('attn_q', 'hr', 'qg', HIDDEN, 2*QV)
        mat('attn_k', 'hr', 'k', HIDDEN, KV)
        mat('attn_v', 'hr', 'v', HIDDEN, KV)
        launch('split_q_gate', Q_HEADS, HEAD_WIDTH, b['qg'], b['q'], b['gate'])
        norm('q', 'attn_q_norm', 'qnorm', HEAD_WIDTH, Q_HEADS)
        norm('k', 'attn_k_norm', 'knorm', HEAD_WIDTH, KV_HEADS)
        gpu.upload(self.positions, (gpu.I * 4)(position, position, position, 0))
        rotate_positions('qnorm', 'qr', Q_HEADS)
        rotate_positions('knorm', 'kr', KV_HEADS)

        assert self.count < self.capacity
        cache_line_ptr = [D(cache.value + self.count * KV_BYTES) for cache in self.cache]
        blocks = KV // 32
        for src, dst in zip(('kr', 'v'), cache_line_ptr):
            gpu.launch(self.frozen, 'goose_set_q4_rows', (blocks + 127)//128, 128,
                       (b[src], dst, D(KV * 4), U(blocks), D(blocks)))
        self.count += 1
        self.attend()
        launch('attention_gate', Q_HEADS, 256, b['att'], b['gate'], b['gated'])
        rotate('gated', 'rot', QV)
        mat('attn_output', 'rot', 'out', QV, HIDDEN)
        launch('residual', HIDDEN//256, 256, b['x'], b['out'], b['residual'])
        norm('residual', 'post_attention_norm', 'h', HIDDEN)
        rotate('h', 'hr', HIDDEN)
        mat('ffn_gate', 'hr', 'ffn_gate', HIDDEN, FFN)
        mat('ffn_up', 'hr', 'ffn_up', HIDDEN, FFN)
        launch('swiglu', FFN//256, 256, b['ffn_gate'], b['ffn_up'], b['ffn'], U(FFN))
        rotate('ffn', 'ffn_rot', FFN)
        mat('ffn_down', 'ffn_rot', 'out', FFN, HIDDEN)
        launch('residual', HIDDEN//256, 256, b['residual'], b['out'], b['out'])

        cache_line = []
        for ptr in cache_line_ptr:
            raw = (ct.c_uint8 * KV_BYTES)()
            gpu.call('cuMemcpyDtoH_v2', raw, ptr, S(KV_BYTES))
            cache_line.append(bytes(raw))
        return np.ctypeslib.as_array(gpu.download(b['out'], HIDDEN)), tuple(cache_line)

    def attend(self):
        b = self.b
        tiles = (self.count + 255)//256
        per_head = tiles * (HEAD_WIDTH + 2) * 4
        # The query and output have one position and 24 contiguous heads.
        # KV rows are token-major, with four adjacent quantized heads per row.
        config = (gpu.L * 22)(
            HEAD_WIDTH, HEAD_WIDTH, 1, Q_HEADS, KV_HEADS, KV_HEADS, self.count,
            QV*4, HEAD_WIDTH*4, KV_BYTES, ROW_BYTES, KV_BYTES, ROW_BYTES,
            0, HEAD_WIDTH*4, QV*4, 256, tiles, Q_HEADS, Q_HEADS, Q_HEADS*per_head, self.count,
        )
        gpu.upload(self.attention_config, config)
        kernel = P()
        gpu.call('cuModuleGetFunction', ct.byref(kernel), self.frozen, b'goose_q4_attention_tiles')
        args = (b['qr'], *self.cache, D(0), self.partial, self.attention_config, F(1/16), gpu.I(0))
        gpu.call('cuLaunchKernel', kernel, U(Q_HEADS), U(tiles), U(1), U(32), U(1), U(1), U(0),
                 P(), (P * len(args))(*(ct.addressof(a) for a in args)), P())
        gpu.launch(self.frozen, 'goose_q4_attention_merge', Q_HEADS, 32,
                   (self.partial, b['att'], self.attention_config, gpu.I(0)))


def main():
    kernel_file, frozen_header = map(Path, sys.argv[3:5])
    ids = [1511, 17, 248046, 1511]
    positions = [4093, 4094, 4095, 4096]
    tensors = gpu.inventory['tensors']
    embedding = next(t for t in tensors if t['name'] == 'token_embd.weight')
    row_bytes = HIDDEN // 128 * 34
    signs, at = {}, 0
    all_signs = gpu.inventory['metadata']['prism.hadamard.sign_values']
    for width in gpu.inventory['metadata']['prism.hadamard.sign_widths']:
        signs[width] = np.asarray(all_signs[at:at+width], dtype=np.float32)
        at += width
    encoded, weights = {}, {}
    with gpu.model.open('rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
        onset = gpu.inventory['data_offset']
        start = onset + embedding['offset']
        encoded['token_embd.weight'] = b''.join(mapped[start+i*row_bytes:start+(i+1)*row_bytes] for i in ids)
        embeddings = rec.unpack(encoded['token_embd.weight'], len(ids), HIDDEN)
        for t in tensors:
            if not t['name'].startswith('blk.3.'):
                continue
            name, shape = t['name'][6:], t['shape']
            start = onset + t['offset']
            if t['type'] == 142:
                encoded[name] = mapped[start:start + math.prod(shape)//128*34]
                weights[name] = rec.unpack(encoded[name], shape[1], shape[0])
            else:
                assert t['type'] == 0
                encoded[name] = mapped[start:start + math.prod(shape)*4]
                weights[name] = np.frombuffer(encoded[name], dtype='<f4').reshape(shape[::-1])

    device, context = gpu.I(), P()
    gpu.call('cuInit', U(0))
    gpu.call('cuDeviceGet', ct.byref(device), gpu.I(0))
    gpu.call('cuCtxCreate_v2', ct.byref(context), U(0), device)
    try:
        array = re.search(rb'goose_cuda_mmq_image\[\] = \{(.*?)\};', frozen_header.read_bytes(), re.S)
        assert array
        image = bytes(int(item, 0) for item in array[1].split(b',') if item.strip())
        cubin = kernel_file.read_bytes()
        layer = Attention(cubin, image, encoded, signs, len(ids))
        layer.reset()
        cached, report, outputs = [], [], []
        for row, pos in enumerate(positions):
            x = rec.rotation(embeddings[row].astype(np.float64), {HIDDEN: np.ones(HIDDEN)}) * signs[HIDDEN]
            expected, cache_line = reference(x, pos, weights, signs, cached)
            out, observed_cache = layer.run(row, pos)
            for field, name in enumerate(('kr', 'v')):
                projected = np.ctypeslib.as_array(gpu.download(layer.b[name], KV))
                assert observed_cache[field] == argmax_q4(projected), name
                assert observed_cache[field] == cache_line[field], name
            np.testing.assert_allclose(out, expected, atol=2e-4, rtol=5e-4)
            cached.append(cache_line)
            outputs.append((out.tobytes(), observed_cache))
            report.append(dict(token=ids[row], position=pos, cache_rows=len(cached),
                               max_absolute_error=float(np.max(np.abs(out - expected)))))
        layer.reset()
        for row, pos in enumerate(positions):
            out, cache = layer.run(row, pos)
            assert outputs[row] == (out.tobytes(), cache)
        print(json.dumps(dict(layer=3, tokens=report, bit_exact_cache=True, deterministic_reset=True,
                              frozen_cuda_image_sha256=sha256(image).hexdigest(),
                              candidate_cuda_image_sha256=sha256(cubin).hexdigest()), indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2', context)


if __name__ == '__main__':
    main()
