"""Check Bonsai GGUF and numeric operations using existing model bytes."""
import ctypes as ct
from hashlib import sha256
import json
import math
import mmap
from pathlib import Path
import random
import struct
import sys

root, inventory = Path(sys.argv[1]), json.loads(Path(sys.argv[2]).read_text())
model = Path(inventory['file'])
F, I, U, L, P, D, S = ct.c_float, ct.c_int32, ct.c_uint32, ct.c_int64, ct.c_void_p, ct.c_uint64, ct.c_size_t
goose = ct.CDLL(str((root / 'core.so').resolve()))
cuda = ct.CDLL('libcuda.so.1')


class Slice(ct.Structure):
    _fields_ = [('address', P), ('length', L)]


def span(array):
    return Slice(ct.addressof(array), len(array))


def call(name, *args):
    result = getattr(cuda, name)(*args)
    if result:
        raise RuntimeError(f'{name}: CUDA status {result}')


def allocate(size):
    ptr = D()
    call('cuMemAlloc_v2', ct.byref(ptr), S(size))
    return ptr


def upload(ptr, data):
    call('cuMemcpyHtoD_v2', ptr, data, S(ct.sizeof(data)))


def download(ptr, count):
    array = (F * count)()
    call('cuMemcpyDtoH_v2', array, ptr, S(ct.sizeof(array)))
    return array


def free_bytes():
    free, total = S(), S()
    call('cuMemGetInfo_v2', ct.byref(free), ct.byref(total))
    return free.value, total.value


def launch(module, name, grid, block, args):
    kernel = P()
    call('cuModuleGetFunction', ct.byref(kernel), module, name.encode())
    params = (P * len(args))(*(ct.addressof(a) for a in args))
    call('cuLaunchKernel', kernel, U(grid), U(1), U(1), U(block), U(1), U(1), U(0), P(), params, P())


def cpu_decode(encoded, width, kind=142):
    packed = (ct.c_uint8 * len(encoded)).from_buffer_copy(encoded)
    output = (F * width)()
    assert goose.qd_dequant(L(kind), span(packed), L(width), span(output)) == width
    return output


def cpu_rotate(values, signs, inverse=False, geometry=(0, 0, 1)):
    result = (F * len(values))()
    assert goose.bonsai_signed_fwht(span(values), span(signs), span(result),
                                  ct.c_uint8(inverse), *(L(n) for n in geometry)) == len(values)
    return result


def main():
    goose.goose_init(0, P())
    for name in ('gr_parse','qd_dequant','bonsai_signed_fwht','bonsai_bind_layout'):
        getattr(goose, name).restype = L

    header_bytes = inventory['data_offset']
    with model.open('rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_COPY) as mapped:
        base = ct.addressof(ct.c_char.from_buffer(mapped))
        state, meta, tensors, keys, names = [(L*n)() for n in (8, 8*256, 12*1024, 512, 2048)]
        args = [Slice(base, header_bytes), L(len(mapped)), *map(span, (state, meta, tensors, keys, names))]
        status = goose.gr_parse(*args)
        assert status == 0, status
        assert state[3] == len(inventory['tensors']) and header_bytes == state[5]
        for index, record in enumerate(inventory['tensors']):
            item = tensors[12*index:12*index+12]
            assert item[2] == record['type'] and item[8] == record['offset']
            assert item[4:4+len(record['shape'])] == record['shape']
            shapes = {0:(1,4), 2:(32,18), 142:(128,34)}
            elements, block_bytes = shapes[item[2]]
            assert item[9] == math.prod(record['shape']) // elements * block_bytes
        assert goose.gr_parse(args[0], L(header_bytes+state[6]-1), *args[2:]) == -7
        assert goose.gr_parse(*args) == 0

        globals_, layers, draft = [(L*n)() for n in (3, 65*15, 4)]
        bound = goose.bonsai_bind_layout(args[0], *args[2:], *map(span,(globals_,layers,draft)))
        assert bound == 64, bound
        layer_kinds = [layers[l*15] for l in range(64)]
        assert layer_kinds.count(0) == 16 and layer_kinds.count(1) == 48
        assert layers[64*15] == 0
        used = list(globals_) + [layers[l*15+i] for l in range(65) for i in range(1,15)
                                  if layers[l*15+i] >= 0] + list(draft)
        assert set(used) == set(range(len(inventory['tensors']))) and len(used) == len(set(used))
        if '--vocabulary' in sys.argv[3:]:
            name_length_at = tensors[0] - 8
            header = bytearray(mapped[:name_length_at])
            struct.pack_into('<Q', header, 8, 0)
            header.extend(b'\x00' * (-len(header) % state[4]))
            (root/'vocabulary.gguf').write_bytes(header)
        if '--layout-only' in sys.argv[3:]:
            print(json.dumps({'validated_tensor_count':len(used),'attention_layers':16,'recurrent_layers':48,'draft_layers':1}))
            return

        table = {t['name']:t for t in inventory['tensors']}
        signs_by_width = {}
        off = 0
        signs_all = inventory['metadata']['prism.hadamard.sign_values']
        for width in inventory['metadata']['prism.hadamard.sign_widths']:
            signs_by_width[width] = (I * width)(*signs_all[off:off+width])
            off += width
        assert off == len(signs_all)
        width = 5120
        row_bytes = width // 128 * 34
        embedding = table['token_embd.weight']['offset']
        output_head = table['output.weight']['offset']
        selected = 248046
        start = header_bytes + embedding + selected * row_bytes
        encoded = mapped[start:start+row_bytes]
        unpacked = cpu_decode(encoded, width)
        scalar = []
        for pos in range(0, len(encoded), 34):
            scale = struct.unpack_from('<e', encoded, pos)[0]
            scalar.extend((float((byte >> bit & 3) - 1) * scale)
                          for byte in encoded[pos+2:pos+34] for bit in (0,2,4,6))
        assert bytes(unpacked)==bytes((F*width)(*scalar))
        legacy_q2 = b'\x00\x3c' + bytes(range(16))
        assert list(cpu_decode(legacy_q2, 64, kind=42)) == [
            float((byte >> bit & 3) - 1) for byte in range(16) for bit in (0,2,4,6)]
        restored = cpu_rotate(unpacked, signs_by_width[width], inverse=True)
        rng = random.Random(1973)

        # Full-size capacities for the intended first profile; only the
        # PQ2 test kernels execute in this process.
        context_tokens = 36864
        q4_kv = 16 * 2 * 1024 // 32 * 18 * context_tokens
        recurrent = 48 * (48 * 128 * 128 + 3 * 10240) * 4
        workspace = 1 << 30
        call('cuInit', U(0))
        device, context, module = I(), P(), P()
        call('cuDeviceGet', ct.byref(device), I(0))
        call('cuCtxCreate_v2', ct.byref(context), U(0), device)
        try:
            initial_free, total = free_bytes()
            call('cuModuleLoad', ct.byref(module), str(root/'bonsai.cubin').encode())
            payload = len(mapped)-header_bytes
            weights = allocate(payload)
            chunk = 32 << 20
            for offset in range(0, payload, chunk):
                call('cuMemcpyHtoD_v2', D(weights.value+offset), P(base+header_bytes+offset),
                     S(min(chunk,payload-offset)))
            for size in (q4_kv,recurrent,workspace):
                scratch = allocate(size)
                call('cuMemsetD8_v2', scratch, ct.c_uint8(0), S(size))
            remaining_free = free_bytes()[0]

            x, y, sign = (allocate(17408*4) for _ in range(3))
            launch(module, 'bonsai_pq2_row', (width+255)//256, 256,
                   [D(weights.value+embedding), x, U(width), U(selected)])
            assert bytes(download(x,width)) == bytes(unpacked)
            upload(sign, signs_by_width[width])
            launch(module,'bonsai_fwht1024',width//1024,256,
                   [x,sign,y,U(1),U(0),U(0),U(1)])
            assert bytes(download(y,width)) == bytes(restored)

            numerical = []
            for channels, geometry in ((5120,(0,0,1)),(6144,(128,16,3)),(17408,(0,0,1))):
                vector = (F*channels)(*(rng.uniform(-0.5,0.5) for _ in range(channels)))
                cpu = cpu_rotate(vector,signs_by_width[channels],geometry=geometry)
                upload(x,vector)
                upload(sign,signs_by_width[channels])
                launch(module,'bonsai_fwht1024',channels//1024,256,
                       [x,sign,y,U(0),*(U(n) for n in geometry)])
                assert bytes(cpu)==bytes(download(y,channels))
                numerical.append({'width':channels,'geometry':geometry,'fwht_sha256':sha256(bytes(cpu)).hexdigest()})

            upload(x, restored)
            upload(sign, signs_by_width[width])
            launch(module,'bonsai_fwht1024',width//1024,256,[x,sign,y,U(0),U(0),U(0),U(1)])
            reference = cpu_rotate(restored, signs_by_width[width])
            vocab = table['output.weight']['shape'][1]
            output = allocate(vocab*4)
            launch(module,'bonsai_pq2_matvec',(vocab+3)//4,128,
                   [D(weights.value+output_head),y,output,U(width),U(vocab)])
            scores = download(output,vocab)
            max_error=0
            sample_ids=[0,1,255,32768,selected,vocab-1]
            for row in sample_ids:
                begin=header_bytes + output_head + row*row_bytes
                row_values=cpu_decode(mapped[begin:begin+row_bytes],width)
                ref=math.fsum(a*b for a,b in zip(row_values,reference))
                max_error=max(max_error,abs(scores[row]-ref))
                assert abs(scores[row]-ref) < 2e-5, (row,scores[row],ref)
            assert all(math.isfinite(x) for x in scores)

            expansion = 17408
            gate, up, intermediate, ffn = (allocate(n*4) for n in (expansion,expansion,expansion,width))
            for suffix, dst in (('gate',gate),('up',up)):
                offset = table[f'blk.0.ffn_{suffix}.weight']['offset']
                launch(module,'bonsai_pq2_matvec',(expansion+3)//4,128,
                       [D(weights.value+offset),y,dst,U(width),U(expansion)])
            launch(module,'bonsai_swiglu',(expansion+255)//256,256,[gate,up,intermediate,U(expansion)])
            inter = download(intermediate,expansion)
            gate_values = download(gate,expansion)
            up_values = download(up,expansion)
            for channel in (0,1,255,expansion-1):
                g = gate_values[channel]
                expected = (g/(1+math.exp(-g)))*up_values[channel]
                assert abs(inter[channel]-expected) <= max(1e-7,abs(expected)*1e-6)
            upload(sign,signs_by_width[expansion])
            launch(module,'bonsai_fwht1024',expansion//1024,256,[intermediate,sign,y,U(0),U(0),U(0),U(1)])
            expected_input = cpu_rotate(inter,signs_by_width[expansion])
            offset = table['blk.0.ffn_down.weight']['offset']
            launch(module,'bonsai_pq2_matvec',(width+3)//4,128,
                   [D(weights.value+offset),y,ffn,U(expansion),U(width)])
            ffn_values = download(ffn,width)
            down_stride = expansion//128*34
            ffn_error=0.0
            for row in (0,31,255,width-1):
                begin=header_bytes + offset + row*down_stride
                row_values=cpu_decode(mapped[begin:begin+down_stride],expansion)
                expected=math.fsum(a*b for a,b in zip(row_values,expected_input))
                ffn_error=max(ffn_error,abs(ffn_values[row]-expected))
                assert abs(ffn_values[row]-expected)<2e-5
            assert all(math.isfinite(v) for v in ffn_values)
            print(json.dumps({'tensor_count':int(state[3]),'payload_bytes':payload,'short_file_rejected':True,'validated_tensor_bindings':len(used),
               'embedding_exact_cpu_cuda':True,'q2_0_reference_fixture':True,'ordered_fwht':numerical,
               'sampled_output_rows':sample_ids,'first_layer_ffn_max_sampled_error':ffn_error,'max_head_absolute_error':max_error,
               'device_total_bytes':total,'device_free_before_bytes':initial_free,
               'device_free_after_reservations_bytes':remaining_free,
               'reserved_q4_kv_bytes':q4_kv,'reserved_gdn_bytes':recurrent,'reserved_workspace_bytes':workspace},indent=2))
        finally:
            call('cuCtxDestroy_v2', context)


if __name__ == '__main__':
    main()
