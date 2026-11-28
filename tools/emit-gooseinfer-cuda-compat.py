"""Generate the OpenCL compatibility ABI needed by existing Goose emitters."""

from pathlib import Path
import sys


def checked_call(name, result, parameters, predicate, value, trap):
    signature = ", ".join(f"{kind} {argument}" for kind, argument in parameters)
    return (f"static {result} {name}({signature}) {{\n"
            f"    if (!({predicate})) {trap}();\n"
            f"    return {value};\n}}\n")


def trap(name):
    return f'static void {name}(void) {{ __asm__ volatile("trap;"); __builtin_unreachable(); }}'


def types(include_size=False):
    names = {8: "char", 16: "short", 32: "int", 64: "long"}
    for width, native in names.items():
        yield f"typedef {native} int{width}_t;"
        yield f"typedef unsigned {native} uint{width}_t;"
    if include_size:
        yield "typedef unsigned long size_t;"


def shared_division(name, trap_name):
    for op, suffix in (("/", "udiv"), ("%", "umod")):
        yield checked_call(f"{name}_{suffix}", "uint64_t", [("uint64_t", "a"), ("uint64_t", "b")],
                           "fg_device_unsigned_divisor_valid(b)", f"a {op} b", trap_name)


def checked_device():
    yield from types()
    yield from alignment_and_constants()
    yield "#ifndef GOOSE_CUDA_FROZEN_ABI\n#define gg_double_bits(bits) as_double((ulong)(bits))\n#endif"
    yield trap("device_trap")
    yield "static void fg_device_noop(void) {}"
    yield checked_call("device_range", "int64_t", [( "int64_t", v) for v in ("value", "lo", "hi")],
                       "fg_device_range_valid(value, lo, hi)", "value", "device_trap")
    yield checked_call("device_range_u", "uint64_t", [("uint64_t", v) for v in ("value", "hi")],
                       "fg_device_unsigned_range_valid(value, hi)", "value", "device_trap")
    yield from shared_division("device", "device_trap")
    for name, operation in (("add", "add"), ("sub", "sub"), ("mul", "mul"), ("uadd", "add")):
        kind = "uint64_t" if name == "uadd" else "int64_t"
        yield (f"static {kind} device_{name}({kind} a, {kind} b) {{\n"
               f"    {kind} result;\n"
               f"    if (__builtin_{operation}_overflow(a, b, &result)) device_trap();\n"
               "    return result;\n}")
    for op, suffix in (("/", "div"), ("%", "mod")):
        yield checked_call(f"device_{suffix}", "int64_t", [("int64_t", "a"), ("int64_t", "b")],
                           "fg_device_nonnegative_division_valid(a, b)", f"a {op} b", "device_trap")
    yield "#define GS_RANGE(value, lo, hi, type, file, line) device_range(value, lo, hi)"
    yield "#define GS_RANGE_U(value, hi, type, file, line) device_range_u(value, hi)"
    yield "#define GS_IDX(value, size, file, line) device_range(value, 0, (size) - 1)"
    yield "#define GS_MEMZERO(at, bytes) __builtin_memset(at, 0, bytes)"
    yield "#define GS_NOINLINE __attribute__((noinline))"
    yield "#define GS_UNREACHABLE(file, line) device_trap()"
    for op, helper, kind, width in (
        ("add", "add", "i", 64), ("sub", "sub", "i", 64), ("mul", "mul", "i", 64),
        ("add", "uadd", "u", 64), ("div", "div", "i", 64), ("mod", "mod", "i", 64),
        ("div", "udiv", "u", 64), ("mod", "umod", "u", 64),
        ("div", "div", "i", 32), ("mod", "mod", "i", 32),
    ):
        params = "a, b" if helper == "uadd" else (
            "q, a, b, file, line" if op == "mod" and kind == "i" else "a, b, file, line")
        result = f"device_{helper}(a, b)"
        if width == 32:
            result = f"((int32_t){result})"
        yield f"#define gs_{op}_{kind}{width}({params}) {result}"
    for op, token in (("sub", "-"), ("mul", "*")):
        for width in (32, 64):
            yield f"#define gs_{op}_u{width}(a, b) ((uint{width}_t)(a) {token} (uint{width}_t)(b))"
    for op, token in (("add", "+"), ("sub", "-"), ("mul", "*")):
        yield f"#define gs_{op}_i32(a, b, file, line) ((int32_t)((int64_t)(a) {token} (int64_t)(b)))"
    yield "#define gs_shl_i64(a,b) ((int64_t)((uint64_t)(a) << ((b)&63)))"
    for width in (32, 64):
        for kind in ("i", "u"):
            yield f"#define gs_shr_{kind}{width}(a,b) ((a) >> ((b)&{width - 1}))"
        yield f"#define gs_shl_u{width}(a,b) ((a) << ((b)&{width - 1}))"


def alignment_and_constants():
    yield "#ifndef GG_ALIGN1\n#define GG_ALIGN1\n#define GG_UNALIGNED\n#define _Alignas(x)\n#endif"
    for macro, suffix in (("UINT64", "UL"), ("INT64", "L"), ("UINT32", "U"), ("INT32", "")):
        yield f"#define {macro}_C(x) x{f'##{suffix}' if suffix else ''}"


def frozen_prelude():
    yield from types(include_size=True)
    yield from alignment_and_constants()
    yield "#define memset(dst, value, count) __builtin_memset(dst, value, count)"
    yield '#include "checked-device-arithmetic.h"'
    yield trap("gg_device_trap")
    yield "static void gg_fail(const char *text) { gg_device_trap(); }"
    integer_args = [("int64_t", "a"), ("int64_t", "b")]
    yield checked_call("gg_div", "int64_t", integer_args,
                       "fg_device_signed_division_valid(a, b)", "a / b", "gg_device_trap")
    yield checked_call("gg_mod", "int64_t", integer_args, "b != 0",
                       "fg_device_euclidean_mod(a, b)", "gg_device_trap")
    yield from shared_division("gg", "gg_device_trap")
    yield checked_call("gg_index", "int64_t", [("int64_t", "index"), ("int64_t", "len")],
                       "fg_device_index_valid(index, len)", "index", "gg_device_trap")
    yield "static double gg_double_bits(uint64_t bits) { return as_double(bits); }"
    yield "typedef struct { void *data; int64_t len; } gg_slice;"
    yield "static gg_slice gg_view(void *data, int64_t len) { return (gg_slice){ data, len }; }"


def main():
    output = Path(sys.argv[1])
    for name, source in (("cuda-checked-abi.h", checked_device), ("cuda-device-prelude.h", frozen_prelude)):
        (output / name).write_text("\n".join(source()) + "\n")


if __name__ == "__main__":
    main()
