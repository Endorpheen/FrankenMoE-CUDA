from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
variant = sys.argv[2]
assert variant in ("address", "aligned")

def bounds(number):
    candidates = [line for line in source.splitlines() if line.startswith("static ") and f" flash_vec_gg_function_{number}(" in line and line.endswith("{")]
    assert len(candidates) == 1
    begin = source.index(candidates[0])
    return begin, source.index("\n}", begin) + 2

def replace_once(body, old, new):
    assert body.count(old) == 1, old
    return body.replace(old, new)

begin, end = bounds(30)
body = source[begin:end]
body = replace_once(body, "gg_div(t2, t3)", "((uint64_t)t2 >> 5)")
body = replace_once(body, "gg_mod(t7, t8)", "((uint64_t)t7 & 15)")
body = replace_once(body, "gg_mod(t24, t25)", "((uint64_t)t24 & 31)")
body = replace_once(body, "gg_div(t26, t27)", "((uint64_t)t26 >> 4)")
if variant == "aligned":
    body = replace_once(body, "v4 = ((uint32_t)(((t14) | (t22))));", "v4 = fv_q4_word(v2 + 2 + v3);")
body = body.replace(" flash_vec_gg_function_30(", " fv_q4_value_fixed(")
source = source[:end] + "\n\n" + body + source[end:]
begin, end = bounds(43)
body = source[begin:end]
body = replace_once(body, "gg_div(t3, t4)", "((uint32_t)t3 >> 3)")
body = replace_once(body, "gg_mod(t7, t8)", "((uint32_t)t7 & 7)")
body = replace_once(body, "gg_div(t9, t10)", "((uint32_t)t9 >> 2)")
body = replace_once(body, "gg_mod(t13, t14)", "((uint32_t)t13 & 3)")
if variant == "aligned":
    body = replace_once(body, "flash_vec_gg_function_23(t17, t18, t19, base + 0)", "fv_q4_nibbles(t17 + t18, t19)")
    body = replace_once(body, "flash_vec_gg_function_23(t22, t23, t24, base + 0)", "fv_q4_nibbles(t22 + t23, t24)")
source = source[:begin] + body + source[end:]
begin, end = bounds(42)
body = source[begin:end]
for old, new in [
    ("flash_vec_gg_function_29(v58, v34, 1, base + 0)", "fv_q4_value_fixed(v58, v34, base + 0)"),
    ("flash_vec_gg_function_29(v58, v34 + 128, 1, base + 0)", "fv_q4_value_fixed(v58, v34 + 128, base + 0)"),
]:
    assert body.count(old) == 4, (old, body.count(old))
    body = body.replace(old, new)
source = source[:begin] + body + source[end:]
if variant == "aligned":
    helper = """static uint32_t fv_q4_word(uint64_t address) {
    if ((address & 3) == 0)
        return *(__global const uint32_t *)address;
    return fg175v_ld_u16(address, 0) | (fg175v_ld_u16(address, 2) << 16);
}

static int32_t fv_q4_nibbles(uint64_t address, uint32_t nibble_half) {
    return (int32_t)((fv_q4_word(address) >> (4 * nibble_half)) & 0x0f0f0f0f);
}

"""
    source = helper + source
Path("device/flash-vec-device.h").write_text(source)
