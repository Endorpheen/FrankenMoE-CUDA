"""Compose compact Flash variants with tile-local Q4 unpacking."""
from pathlib import Path
import sys

s = Path(sys.argv[1]).read_text()
a = s.index("static float2 flash_q4_pair(")
b = s.index("// One CTA", a)
helper = (s[a:b].replace("__global", "__local")
          .replace("flash_q4_pair", "local_flash_q4_pair")
          .replace("flash_pack_q4_pair", "local_flash_pack_q4_pair"))
print(helper)
print("""static inline void flash_copy_q4_tile(__local uint *packed, __global const uchar *kv,
    uint head, uint first, uint rows, uint thread) {
    for (uint task = thread; task < 32 * 9; task += 128) {
        const uint row = first + task / 9;
        const uint offset = task % 9;
        const uint bytes = row < rows ? 16 : 0;
        const size_t src = bytes ? (size_t)row * 576 + head * 144 + offset * 16 : 0;
        bonsai_copy16(packed + task * 4, (__global const uint *)(kv + src), bytes);
    }
    bonsai_copy_complete();
    sync_threads();
}
""")

a = s.index("__kernel void bonsai_flash_async_tile(")
b = s.index("\n}\n", a) + 3
body = s[a:b]
block = """        const uint chunk = (key_start+VAR)/32, available = chunk < chunks ? 16 : 0;
        const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
        for (uint index = thread*4; index < 32*128; index += 512) {
            bonsai_copy16(staging+OFFSET,BUF+base+index,available);
        }
        bonsai_copy_complete();
        sync_threads();"""
k_old = block.replace("VAR", "kb").replace("OFFSET", "(index/128)*132+index%128").replace("BUF", "keys")
v_old = block.replace("VAR", "vb").replace("OFFSET", "(index/16)*20+index%16").replace("BUF", "values")
k_new = """        flash_copy_q4_tile(raw, keys, kv_head, key_start+kb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            staging[key*132+pair] = key_start+kb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
        }
        sync_threads();"""
v_new = """        flash_copy_q4_tile(raw, values, kv_head, key_start+vb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            const uint packed = key_start+vb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
            __local ushort *values_t = (__local ushort *)staging;
            values_t[pair*80+key] = (ushort)packed;
            values_t[pair*80+40+key] = (ushort)(packed>>16);
        }
        sync_threads();"""
for mode in ("k", "v", "kv"):
    code = body.replace("bonsai_flash_async_tile", f"bonsai_flash_direct_{mode}")
    code = code.replace("    __local float score", "    __local __attribute__((aligned(16))) uint raw[32 * 36];\n    __local float score")
    for letter, old, new, arg in (("k", k_old, k_new, "keys"), ("v", v_old, v_new, "values")):
        if letter in mode:
            assert code.count(old) == 1, letter
            code = code.replace(old, new).replace(f"__global const uint *{arg}", f"__global const uchar *{arg}")
    print(code)

# Preserve each key tile's arithmetic and output slot, with Q prepared once.
for count in (2, 4):
    code = body.replace("bonsai_flash_async_tile", f"bonsai_flash_tile_group{count}")
    first = """    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;"""
    target = "    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;"
    opening = "    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {"
    assert all(code.count(key) == 1 for key in (first,target,opening))
    code = code.replace(first, f"""    const uint tile_base = flash_grid_y() * {count};
    if (tile_base * FLASH_KEYS >= first_visible + last_row/6) return;""")
    code = code.replace(target, "")
    at = code.index(opening)
    inside = code[at:code.rfind("\n}")]
    offset = "        " + target.strip()
    start = f"""    for (uint j = 0; j < {count}; ++j) {{
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + last_row/6) break;
{offset}
"""
    print(code[:at] + start + "\n".join("    " + line if line else "" for line in inside.split("\n")) + "\n    }\n}")
