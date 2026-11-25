"""Generate bounds-free K64 kernels for full prompt tiles."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected exactly one source anchor: {old!r}")
    return text.replace(old, new)


def kernel_variant(control, width):
    name = f"bonsai_pq2_full_64x128_k64_{width}"
    code = replace(control, "bonsai_pq2_cached_64x128_k64", name)
    code = replace(code, "const uint blocks=width/128;", f"const uint blocks={width // 128};")
    code = replace(
        code,
        """                uint bits=0,low=0,high=0;
                if (row<rows) {
                    const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                    bits=*(__global const ushort *)q;
                    low=q[2+quad];
                    high=q[18+quad];
                }""",
        """                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];""",
    )
    code = replace(
        code,
        """            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);""",
        f"""            const ulong source=(ulong)token*{width // 2}+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);""",
    )
    return replace(
        code,
        "    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);",
        """    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }""",
    )


def configure_host(host):
    host = replace(host, "MMQ_CACHE128, KERNEL_COUNT", "MMQ_CACHE128, MMQ_FULL_5120, MMQ_FULL_17408, KERNEL_COUNT")
    host = replace(host, "flash_panel, mmq_cached;", "flash_panel, mmq_cached, mmq_full;")
    host = replace(
        host, "    const char *cached = getenv(",
        '    const char *full = getenv("BONSAI_MMQ_FULL_TILE");\n'
        '    m->mmq_full = !full || strcmp(full, "0") != 0;\n'
        '    const char *cached = getenv(',
    )
    host = replace(host, "const enum kernel projection = k64", "enum kernel projection = k64")
    host = replace(
        host,
        "            return launch(m, projection, (out_width+tile-1)/tile,",
        """            if (m->mmq_full && projection == MMQ_CACHE128 &&
                    out_width % 64 == 0 && count % 128 == 0) {
                if (in_width == 5120) projection = MMQ_FULL_5120;
                if (in_width == 17408) projection = MMQ_FULL_17408;
            }
            return launch(m, projection, (out_width+tile-1)/tile,""",
    )
    return replace(
        host,
        '"bonsai_pq2_cached_64x128_k64",',
        '"bonsai_pq2_cached_64x128_k64",\n'
        '        "bonsai_pq2_full_64x128_k64_5120", "bonsai_pq2_full_64x128_k64_17408",',
    )


def main():
    image, host, out = map(Path, sys.argv[1:])
    source = image.read_text()
    start = source.index("__kernel void bonsai_pq2_cached_64x128_k64(")
    end = source.index("\n}\n", start) + 3
    variants = [kernel_variant(source[start:end], width) for width in (5120, 17408)]
    (out / "pipeline.cl").write_text(source + "\n".join(variants))
    (out / "backend.c").write_text(configure_host(host.read_text()))


if __name__ == "__main__":
    main()
