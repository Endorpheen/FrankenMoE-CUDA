"""Generate packed-weight reuse variants with unchanged half operands."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
for tokens in (64,128):
    name=f"bonsai_pq2_cached_64x{tokens}_k64"
    start=source.index(f"__kernel void {name}(")
    end=source.index("\n}\n",start)+3
    code=source[start:end]
    call="bonsai_ffn_unpack_cached((ushort)entry,codes)"
    assert code.count(call)==1
    for mode,symbol in (("lut","bonsai_pq2_quad"),("half2","bonsai_pq2_half2")):
        print(code.replace(name,f"bonsai_pq2_{mode}_64x{tokens}_k64")
              .replace(call,f"{symbol}((ushort)entry,codes)"))
