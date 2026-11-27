"""Extract model rows and compute an independent CPU oracle with llama.cpp gguf-py."""

from pathlib import Path
import struct
import sys

from gguf import GGUFReader, GGMLQuantizationType as Quant, dequantize
import numpy as np


def main():
    apex, inkling, output = map(Path, sys.argv[1:])
    width, rows = 512, 7
    batch_input = ((np.arange(3 * width).reshape(3, width) % 31 - 15) / 8).astype("<f4")
    with output.open("wb") as file:
        file.write(b"FGMOEQ1\0")
        for model, cases in (
            (apex, ((Quant.Q4_K, "token_embd.weight"), (Quant.Q6_K, "blk.0.ffn_down_exps.weight"))),
            (inkling, ((Quant.IQ4_NL, None), (Quant.IQ4_XS, None), (Quant.Q8_0, "token_embd.weight"))),
        ):
            reader = GGUFReader(model)
            for quant, name in cases:
                tensor = next(t for t in reader.tensors if t.tensor_type == quant and
                              (t.name == name if name else len(t.shape) == 3))
                bytes_per_row = tensor.data.shape[-1] * width // int(tensor.shape[0])
                raw = tensor.data.reshape(-1, tensor.data.shape[-1])[:rows, :bytes_per_row].copy()
                dense = dequantize(raw, quant)
                dot = (batch_input.astype(np.float64) @ dense.T.astype(np.float64)).astype("<f4")
                file.write(struct.pack("<I", len(raw.tobytes())))
                file.write(raw.tobytes())
                file.write(dot.tobytes())
                if name == "token_embd.weight":
                    gathered = np.stack((dense[-1], dense[0], np.zeros(width, dtype=np.float32)))
                    file.write(gathered.astype("<f4").tobytes())
                print(f"{model.name} {tensor.name} {quant.name}: {rows} rows x {width}")


if __name__ == "__main__":
    main()
