"""Read the GGUF v3 inventory without reading or rewriting the weights."""
import json
from pathlib import Path
import struct
import sys

model, output = map(Path, sys.argv[1:3])
formats = {0:'B',1:'b',2:'H',3:'h',4:'I',5:'i',6:'f',7:'?',10:'Q',11:'q',12:'d'}


def read_inventory(model):
    with model.open('rb') as file:
        def read(size):
            data = file.read(size)
            if len(data) != size:
                raise ValueError('Truncated GGUF header')
            return data

        def scalar(kind):
            fmt = '<' + kind
            return struct.unpack(fmt, read(struct.calcsize(fmt)))[0]

        def string():
            size = scalar('Q')
            if size > 64 << 20:
                raise ValueError('Invalid GGUF string length')
            return read(size).decode()

        def value(kind, keep):
            if kind == 8:
                return string()
            if kind != 9:
                return scalar(formats[kind])
            entry, count = scalar('I'), scalar('Q')
            if entry == 9 or count > 1 << 24:
                raise ValueError('Invalid GGUF array')
            if keep:
                return [value(entry, True) for _ in range(count)]
            if entry in formats:
                read(count * struct.calcsize('<' + formats[entry]))
            else:
                for _ in range(count):
                    value(entry, False)
            return {'array_type': entry, 'count': count}

        assert read(4) == b'GGUF'
        version, tensor_count, key_count = scalar('I'), scalar('Q'), scalar('Q')
        assert version == 3 and tensor_count < 1024 and key_count < 256
        metadata = {}
        for _ in range(key_count):
            name = string()
            metadata[name] = value(scalar('I'), not name.startswith('tokenizer.'))
        tensors = []
        for _ in range(tensor_count):
            name = string()
            dims = scalar('I')
            assert 1 <= dims <= 4
            tensors.append({'name': name, 'shape': [scalar('Q') for _ in range(dims)],
                            'type': scalar('I'), 'offset': scalar('Q')})
        alignment = metadata.get('general.alignment', 32)
        assert alignment > 0 and alignment & (alignment - 1) == 0
        start = (file.tell() + alignment - 1) & -alignment
    return {'file': str(model), 'size': model.stat().st_size, 'version': version,
            'data_offset': start, 'metadata': metadata, 'tensors': tensors}


output.write_text(json.dumps(read_inventory(model), indent=2) + '\n')
