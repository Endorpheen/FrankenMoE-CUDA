#!/usr/bin/env python3
"""Emit x86-64 CUDA Driver API imports for a standalone Goose unit."""
import argparse
import json
from pathlib import Path
import re


def native_type(kind):
    pointer = re.fullmatch(r"(c_)?(p+)_(u64|i32|i8|void)", kind)
    if not pointer:
        return kind
    immutable, depth, base = pointer.groups()
    value = ("const " if immutable else "") + base
    for _ in depth:
        value = f"c_ptr<{value}>"
    return value


def generate(manifest, native_target):
    library = "nvcuda.dll" if native_target == "windows" else "libcuda.so.1"
    goose = [
        f'fn fg_cuda_driver_open() -> u64 {{ return unsafe_c_library_open("{library}"); }}',
        "fn fg_cuda_driver_close(library: u64) { unsafe_c_library_close(library); }",
        "extern fn fg_cuda_driver_words_address(words: u64[:]) -> u64;",
        "extern fn fg_cuda_driver_bytes_address(bytes: u8[:]) -> u64;",
    ]
    assembly = [
        ".text", ".globl fg_cuda_driver_words_address", ".globl fg_cuda_driver_bytes_address",
    ]
    for kind in ("u8", "i32", "u32", "i64", "u64", "f32"):
        symbol = f"fg_cuda_driver_slice_{kind}"
        assembly.extend([f".globl {symbol}", f"{symbol}:"])
    assembly.extend([".globl fg_cuda_driver_expert_slice", "fg_cuda_driver_expert_slice:",
                     ".globl fg_cuda_driver_replay_slice", "fg_cuda_driver_replay_slice:",
                     ".globl fg_cuda_driver_buffer_slice", "fg_cuda_driver_buffer_slice:"])
    assembly.extend([
        "fg_cuda_driver_words_address:", "fg_cuda_driver_bytes_address:",
        "#ifdef _WIN32", "    movq (%rcx), %rax",
        "#else", "    movq %rdi, %rax", "#endif", "    ret",
    ])
    # Fixed-size references cross the native boundary as one address.
    scalar_types = ("u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f32", "f64", "bool")
    descriptor_types = (
        "kernel_node_params", "memory_properties", "memory_access",
        "memcpy_batch_attributes", "graph_update_result",
    )
    assembly.extend([".globl fg_cuda_host_pointer_bits", "fg_cuda_host_pointer_bits:"])
    for kind in scalar_types + descriptor_types:
        symbol = f"fg_cuda_driver_arg_{kind}"
        assembly.extend([f".globl {symbol}", f"{symbol}:"])
    assembly.extend([
        "#ifdef _WIN32", "    movq %rcx, %rax",
        "#else", "    movq %rdi, %rax", "#endif", "    ret",
    ])
    load_argument = "fg_cuda_driver_load_argument_address"
    assembly.extend([
        f".globl {load_argument}", f"{load_argument}:",
        "#ifdef _WIN32", "    movq (%rcx), %rax",
        "#else", "    movq (%rdi), %rax", "#endif", "    ret",
    ])
    for name in ('malloc', 'calloc', 'realloc', 'free', 'memcpy', 'memmove', 'memset'):
        symbol = f"fg_cuda_host_{name}"
        assembly.extend([
            f".globl {symbol}", f"{symbol}:",
            "#ifdef _WIN32", f"    jmp {name}",
            "#else", f"    jmp {name}@PLT", "#endif",
        ])
    names = {}
    symbols = set()
    for number, row in enumerate(manifest.read_text().splitlines(), 1):
        fields = row.split()
        if not fields or row.lstrip().startswith("#"):
            continue
        if len(fields) < 2:
            raise ValueError(f"{manifest}:{number}: expected dispatch name and CUDA symbol")
        name, symbol, *types = fields
        if (not re.fullmatch(r"[a-z][a-z0-9_]*", name)
                or not re.fullmatch(r"cu[A-Z][A-Za-z0-9_]*", symbol)
                or any(kind not in {"i32", "u8", "u32", "u64"}
                       and native_type(kind) == kind for kind in types)
                or name in names or symbol in symbols):
            raise ValueError(f"{manifest}:{number}: invalid or duplicate CUDA import")
        names[name] = symbol
        symbols.add(symbol)
        call = f"fg_cuda_driver_{name}"
        bind = f"{call}_bind"
        typed = [native_type(kind) for kind in types]
        raw = ["u64" if kind.startswith("c_ptr<") else kind for kind in typed]
        args = ", ".join(f"a{i}: {kind}" for i, kind in enumerate(raw))
        signature = f"AbiCuda_{name}"
        field = f"abi_cuda_{name}"
        native_args = ", ".join(f"a{i}: {kind}" for i, kind in enumerate(typed))
        native_values = ", ".join(f"a{i}" for i in range(len(typed)))
        parameters = ", ".join(
            f"unsafe_c_ptr<{kind}>(a{i})" if kind.startswith("c_ptr<") else f"a{i}"
            for i, kind in enumerate(typed)
        )
        goose.extend([
            f"type {signature} = c_fnptr({', '.join(typed)}) -> i32;",
            f"var {field}: u64 = 0;",
            f"fn {call}_typed({native_args}) -> i32 {{ return unsafe_c_fnptr<{signature}>({field})({native_values}); }}",
            f"fn {call}({args}) -> i32 {{ return {call}_typed({parameters}); }}",
            f"fn {bind}(library: u64) -> u64 {{",
            f'    let address = unsafe_c_library_symbol(library, "{symbol}");',
            f"    {field} = address;",
            "    return address;",
            "}",
        ])
    if not names:
        raise ValueError(f"{manifest}: no CUDA imports")
    assembly.extend(["#ifndef _WIN32", '.section .note.GNU-stack,"",@progbits', "#endif"])
    return "\n".join(goose) + "\n", "\n".join(assembly) + "\n"



CUDA_IMAGES = {
    'goose-cuda-image': ('goose_cuda_image', 'goose_cuda_mmq_image'),
    'flash-interior-image': ('flash_interior_image',),
    'rms-onevec-image': ('rms_onevec_image',),
    'fwht-fixed-image': ('fwht_fixed_image',),
    'moe-epilogue-image': ('moe_epilogue_image',),
    'attention-cubin': ('attention_key_bound_image',),
    'attention-first-row-image': ('attention_first_row_image',),
    'attention-unroll-image': ('attention_unroll_image',),
    'fused-image': ('fused_up_gate_image',),
    'shared-image': ('fused_shared_image',),
    'compact-route-image': ('compact_route_image',),
    'route-mailbox-image': ('route_mailbox_image',),
    'softmax-image': ('router_softmax_image',),
    'i2-lazy-prelude-image': ('i2_lazy_prelude_image',),
    'giga-dense-image': ('giga_dense_image',),
    'pred-image': ('pred_image',),
    'giga-vector-image': ('giga_vector_image',),
    'grouped-moe-image': ('grouped_moe_cubin',),
    'matrix-image': ('matrix_cubin',),
    'q8-packed-image': ('q8_packed_cubin',),
    'tanh-image': ('tanh_cubin',),
    'situ-image': ('situ_cubin',),
    'giga-dsa-image': ('giga_dsa_image',),
    'giga-certified-dsa-image': ('giga_certified_dsa_image',),
    'giga-mla-image': ('giga_mla_image',),
    'i2-probe-moe-image': ('moe_epilogue_cubin',),
    'i2-parallel-conv-image': ('i2_parallel_conv_image',),
    'flash-native-image': ('flash_native_cubin',),
    'attention-image': ('attention_cubin',),
    'flash-tiles-image': ('flash_tiles_cubin',),
    'attention-gate-image': ('attention_gate_cubin',),
    'attention-input-image': ('attention_input_cubin',),
    'bridge-image': ('attention_bridge_cubin',),
    'i2-decode-q8-image': ('i2_decode_q8_image',),
}


def generate_images():
    """Expose the existing embedded CUDA image headers through the driver ABI."""
    lines = [
        "#include <stdbool.h>", "#include <inttypes.h>", "#include <stdint.h>", "#include <stdlib.h>", "#include <string.h>",
        "#include <pthread.h>", "#include <time.h>",
        "#ifdef _WIN32",
        '#include "windows/platform.h"',
        "#else",
        "#include <sched.h>", "#include <unistd.h>",
        "#endif",
        "",
        "extern void fg_cuda_host_init_callback(void);",
        "extern void fg_cuda_host_cleanup_callback(void);",
        "",
        "static pthread_once_t fg_cuda_host_once = PTHREAD_ONCE_INIT;",
        "static pthread_mutex_t fg_cuda_host_locks[] = {",
        "    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER",
        "};",
        "",
        "void fg_cuda_host_lock(int32_t which) {",
        "    pthread_mutex_lock(&fg_cuda_host_locks[which]);",
        "}",
        "",
        "void fg_cuda_host_unlock(int32_t which) {",
        "    pthread_mutex_unlock(&fg_cuda_host_locks[which]);",
        "}",
        "",
        "void fg_cuda_host_ensure_initialized(void) {",
        "    pthread_once(&fg_cuda_host_once, fg_cuda_host_init_callback);",
        "}",
        "",
        "void fg_cuda_host_register_cleanup(void) {",
        "    atexit(fg_cuda_host_cleanup_callback);",
        "}",
        "",
        'extern void fg_cuda_host_copy_worker_callback(void);',
        '',
        'static pthread_t fg_cuda_host_worker;',
        'static pthread_mutex_t fg_cuda_host_worker_mutex = PTHREAD_MUTEX_INITIALIZER;',
        'static pthread_cond_t fg_cuda_host_worker_ready = PTHREAD_COND_INITIALIZER;',
        'static pthread_cond_t fg_cuda_host_worker_done = PTHREAD_COND_INITIALIZER;',
        '',
        'static void *fg_cuda_host_worker_entry(void *unused) {',
        '    (void)unused;',
        '    fg_cuda_host_copy_worker_callback();',
        '    return NULL;',
        '}',
        '',
        'uint8_t fg_cuda_host_worker_start(void) {',
        '    cpu_set_t affinity;',
        '    if (sched_getaffinity(0, sizeof(affinity), &affinity) != 0 || CPU_COUNT(&affinity) <= 1)',
        '        return 0;',
        '    return pthread_create(&fg_cuda_host_worker, NULL, fg_cuda_host_worker_entry, NULL) == 0;',
        '}',
        '',
        'void fg_cuda_host_worker_join(void) { pthread_join(fg_cuda_host_worker, NULL); }',
        'void fg_cuda_host_worker_lock(void) { pthread_mutex_lock(&fg_cuda_host_worker_mutex); }',
        'void fg_cuda_host_worker_unlock(void) { pthread_mutex_unlock(&fg_cuda_host_worker_mutex); }',
        'void fg_cuda_host_worker_wait_ready(void) { pthread_cond_wait(&fg_cuda_host_worker_ready, &fg_cuda_host_worker_mutex); }',
        'void fg_cuda_host_worker_wait_done(void) { pthread_cond_wait(&fg_cuda_host_worker_done, &fg_cuda_host_worker_mutex); }',
        'void fg_cuda_host_worker_signal_ready(void) { pthread_cond_signal(&fg_cuda_host_worker_ready); }',
        'void fg_cuda_host_worker_signal_done(void) { pthread_cond_signal(&fg_cuda_host_worker_done); }',
        '',
        'int64_t fg_cuda_host_page_size(void) {',
        '#ifdef _WIN32',
        '    return fg_win_page_size();',
        '#else',
        '    return sysconf(_SC_PAGESIZE);',
        '#endif',
        '}',
        '',
        'int64_t fg_cuda_host_physical_pages(int64_t page) {',
        '#ifdef _WIN32',
        '    return page > 0 ? (int64_t)(fg_win_memory(false) / (uint64_t)page) : 0;',
        '#else',
        '    (void)page;',
        '    return sysconf(_SC_PHYS_PAGES);',
        '#endif',
        '}',
        '',
        'uint8_t fg_cuda_host_verify_mailbox(void) {',
        '#ifdef VERIFY_ROUTE_MAILBOX',
        '    return 1;',
        '#else',
        '    return 0;',
        '#endif',
        '}',
        '',
        'uint32_t fg_cuda_host_load_acquire_u32(uint64_t address) {',
        '    return __atomic_load_n((const uint32_t *)(uintptr_t)address, __ATOMIC_ACQUIRE);',
        '}',
        '',
        'uint64_t fg_host_graph_tls_address(void) {',
        '    static _Thread_local uint64_t words[2];',
        '    return (uintptr_t)words;',
        '}',
        '',
        'uint64_t fg_host_atomic_load_u64(uint64_t address) {',
        '    return __atomic_load_n((const uint64_t *)(uintptr_t)address, __ATOMIC_RELAXED);',
        '}',
        '',
        'void fg_host_atomic_store_u64(uint64_t address, uint64_t value) {',
        '    __atomic_store_n((uint64_t *)(uintptr_t)address, value, __ATOMIC_RELAXED);',
        '}',
        '',
        'void fg_host_atomic_add_u64(uint64_t address, uint64_t value) {',
        '    __atomic_fetch_add((uint64_t *)(uintptr_t)address, value, __ATOMIC_RELAXED);',
        '}',
        '',
        'uint64_t fg_host_atomic_swap_u64(uint64_t address, uint64_t value) {',
        '    return __atomic_exchange_n((uint64_t *)(uintptr_t)address, value, __ATOMIC_RELAXED);',
        '}',
        '',
        'void fg_cuda_host_cpu_pause(void) { __builtin_ia32_pause(); }',
        '',
        'int64_t fg_cuda_host_positive_env(uint64_t address) {',
        '    if (!address) return 0;',
        '    const char *text = (const char *)(uintptr_t)address;',
        '    char *end;',
        '    const intmax_t parsed = strtoimax(text, &end, 10);',
        '    return end != text && !*end && parsed > 0 && parsed <= INT64_MAX ? (int64_t)parsed : 0;',
        '}',
        '',
        'int32_t fg_cuda_host_env_atoi(uint64_t address, int32_t default_value) {',
        '    return address ? atoi((const char *)(uintptr_t)address) : default_value;',
        '}',
        '',
        'uint64_t fg_cuda_host_monotonic_ns(void) {',
        '    struct timespec stamp;',
        '    if (clock_gettime(CLOCK_MONOTONIC, &stamp) != 0) return 0;',
        '    return (uint64_t)stamp.tv_sec * UINT64_C(1000000000) + (uint64_t)stamp.tv_nsec;',
        '}',
        '',
        'int64_t fg_cuda_host_thread_ns(void) {',
        '    struct timespec stamp;',
        '    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stamp) != 0) return 0;',
        '    return (int64_t)stamp.tv_sec * INT64_C(1000000000) + (int64_t)stamp.tv_nsec;',
        '}',
        '',
        'uint32_t fg_cuda_host_env_u32(uint64_t address, uint32_t default_value) {',
        '    return address ? (uint32_t)strtoul((const char *)(uintptr_t)address, NULL, 10) : default_value;',
        '}',
        '',
        'void fg_cuda_host_store_release_u32(uint64_t address, uint32_t value) {',
        '    __atomic_store_n((uint32_t *)(uintptr_t)address, value, __ATOMIC_RELEASE);',
        '}',
        '',
        'extern void i2_lfu_init_goose(void);',
        'static pthread_once_t fg_cuda_once_lfu = PTHREAD_ONCE_INIT;',
        'void fg_cuda_host_once_lfu(void) { pthread_once(&fg_cuda_once_lfu, i2_lfu_init_goose); }',
        '',
        'extern void init_inkling_prior(void);',
        'static pthread_once_t fg_cuda_once_prior = PTHREAD_ONCE_INIT;',
        'void fg_cuda_host_once_prior(void) { pthread_once(&fg_cuda_once_prior, init_inkling_prior); }',
        '',
        'extern void init_inkling_buckets(void);',
        'static pthread_once_t fg_cuda_once_buckets = PTHREAD_ONCE_INIT;',
        'void fg_cuda_host_once_buckets(void) { pthread_once(&fg_cuda_once_buckets, init_inkling_buckets); }',
        '',
        "extern bool fg_standalone_option_enabled(const char *name);",
        "",
        "uint64_t fg_cuda_host_getenv(uint64_t address) {",
        "    return (uint64_t)(uintptr_t)getenv((const char *)(uintptr_t)address);",
        "}",
        "",
        "uint8_t fg_cuda_host_option(uint64_t address) {",
        "    return fg_standalone_option_enabled((const char *)(uintptr_t)address);",
        "}",
        "",
        "uint8_t fg_cuda_host_env_equal(uint64_t address, uint64_t expected) {",
        "    return address && expected && strcmp((const char *)(uintptr_t)address,",
        "        (const char *)(uintptr_t)expected) == 0;",
        "}",
    ]
    view_types = {
        "u8": "uint8_t", "u16": "uint16_t", "i32": "int32_t", "u32": "uint32_t",
                "i64": "int64_t", "u64": "uint64_t", "f32": "float", "expert_jobs": "void", "replay_ops": "void", "vmm_mapping": "void", "device_buffers": "void",
    }
    for name, element in view_types.items():
        view = f"fg_cuda_view_{name}"
        lines.extend([
            "",
            f"typedef struct {{ {element} *data; int64_t len; }} {view};",
            f"{view} fg_cuda_host_view_{name}(uint64_t address, int64_t length) {{",
            f"    return ({view}){{({element} *)(uintptr_t)address, length}};",
            "}",
        ])
    for header in CUDA_IMAGES:
        lines.append(f'#include "{header}.h"')
    for symbols in CUDA_IMAGES.values():
        for symbol in symbols:
            lines.extend([
                "",
                f"uint64_t fg_cuda_image_{symbol}(void) {{",
                f"    return {symbol}[0] ? (uint64_t)(uintptr_t){symbol} : 0;",
                "}",
            ])
    lines.extend([
        "",
        "uint8_t fg_cuda_has_kernel_arg_counts(void) {",
        "#ifdef GOOSE_CUDA_HAS_ARG_COUNTS",
        "    return 1;",
        "#else",
        "    return 0;",
        "#endif",
        "}",
        "",
        "int64_t fg_cuda_kernel_arg_count(uint64_t address) {",
        "#ifdef GOOSE_CUDA_HAS_ARG_COUNTS",
        "    const char *name = (const char *)(uintptr_t)address;",
        "    if (name) {",
        "        for (size_t i = 0; i < sizeof(goose_cuda_arg_counts) / sizeof(*goose_cuda_arg_counts); ++i)",
        "            if (strcmp(goose_cuda_arg_counts[i].name, name) == 0)",
        "                return goose_cuda_arg_counts[i].count;",
        "    }",
        "#else",
        "    (void)address;",
        "#endif",
        "    return -1;",
        "}",
    ])
    return "\n".join(lines) + "\n"


def generate_diagnostics(manifest):
    """Emit fixed-signature wrappers for libc diagnostics."""
    lines = ["#include <stdint.h>", "#include <stdio.h>"]
    types = {"i32": "int", "u32": "unsigned int", "i64": "int64_t",
             "u64": "uint64_t", "f64": "double"}
    for row in json.loads(manifest.read_text()):
        symbol, mode, stream = row["symbol"], row["mode"], row["stream"]
        if not re.fullmatch(r"fg_cuda_host_diagnostic_[0-9]+", symbol):
            raise ValueError(f"invalid diagnostic symbol: {symbol}")
        if mode not in {"printf", "fprintf", "fputs"} or stream not in {"stderr", "stdout"}:
            raise ValueError(f"invalid diagnostic mode: {mode}/{stream}")
        text = row["format"]
        if not re.fullmatch(r'(?:\\[^\n]|[^"\\\n])*', text):
            raise ValueError(f"invalid diagnostic format literal: {symbol}")
        fmt = '"' + text + '"'
        declared = [f"{types[kind]} arg{i}" for i, kind in enumerate(row["types"])]
        values = [f"arg{i}" for i in range(len(declared))]
        if mode != "fputs":
            conversions = re.findall(r"%(?:[-+#0 ]*\d*(?:\.\d+)?)(ll|z|j|t)?([diuxXocfFeEgGaA%])", text)
            conversions = [(width, kind) for width, kind in conversions if kind != "%"]
            if len(conversions) != len(values):
                raise ValueError(f"wrong diagnostic argument count: {symbol}")
            for i, (width, conversion) in enumerate(conversions):
                signed = conversion in "di"
                cast = {
                    "ll": "long long" if signed else "unsigned long long",
                    "z": "intptr_t" if signed else "size_t",
                    "j": "intmax_t" if signed else "uintmax_t",
                    "t": "ptrdiff_t" if signed else "uintptr_t",
                }.get(width)
                if cast:
                    values[i] = f"({cast}){values[i]}"
        params = ", ".join(declared) if declared else "void"
        args = [fmt, stream] if mode == "fputs" else [stream, fmt, *values]
        call = "fputs" if mode == "fputs" else "fprintf"
        lines.extend(["", f"int32_t {symbol}({params}) {{",
                      f"    return {call}({', '.join(args)});", "}"])
    return "\n".join(["#include <stddef.h>", *lines]) + "\n"


def generate_tls_unit(source):
    """Compile the compiler-emitted unit with an ABI-matched native TLS slot."""
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", source.name):
        raise ValueError(f"{source}: unsupported generated unit filename")
    declaration = re.compile(
        r"\bextern\s+gg_slice\s*\(?fg_cuda_host_thread_storage\)?\s*"
        r"\(\s*(?P<type>(?:struct\s+)?gg_type_[0-9]+)\s*\)\s*;"
    )
    emitted = source.read_text()
    matches = {match["type"] for match in declaration.finditer(emitted)}
    if len(matches) != 1:
        raise ValueError(f"{source}: expected one fixed-size Goose TLS layout ABI")
    reference_type = matches.pop()
    reference = re.search(
        rf"^typedef const (?P<type>gg_type_[0-9]+) \*{re.escape(reference_type)};$",
        emitted, re.MULTILINE,
    )
    if reference is None:
        raise ValueError(f"{source}: TLS layout parameter must be a const reference")
    storage_type = reference["type"]
    return (
        f'#include "{source.name}"\n\n'
        f'static _Thread_local {storage_type} fg_cuda_thread_storage;\n\n'
        f'gg_slice fg_cuda_host_thread_storage({reference_type} layout) {{\n'
        '    (void)layout;\n'
        '    return (gg_slice){&fg_cuda_thread_storage, 1};\n'
        '}\n'

    )



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("goose", type=Path)
    parser.add_argument("assembly", type=Path)
    parser.add_argument("--native-target", choices=["linux", "windows"], required=True)
    parser.add_argument("--tls-source", type=Path, help="compiler-emitted driver unit")
    parser.add_argument("--tls-output", type=Path, help="generated translation unit, beside --tls-source")
    parser.add_argument("--image-output", type=Path, help="generated embedded image ABI unit")
    parser.add_argument("--diagnostics", type=Path)
    parser.add_argument("--diagnostics-output", type=Path)
    args = parser.parse_args()
    if (args.diagnostics is None) != (args.diagnostics_output is None):
        parser.error("--diagnostics and --diagnostics-output must be passed together")
    if (args.tls_source is None) != (args.tls_output is None):
        parser.error("--tls-source and --tls-output must be passed together")
    if args.tls_source and args.tls_source.resolve().parent != args.tls_output.resolve().parent:
        parser.error("--tls-source and --tls-output must be in the same directory")
    if args.tls_source and args.tls_source.resolve() == args.tls_output.resolve():
        parser.error("--tls-output must be separate from compiler output")
    goose, assembly = generate(args.manifest, args.native_target)
    args.goose.write_text(goose)
    args.assembly.write_text(assembly)
    if args.diagnostics:
        args.diagnostics_output.write_text(generate_diagnostics(args.diagnostics))
    if args.image_output:
        args.image_output.write_text(generate_images())
    if args.tls_source:
        args.tls_output.write_text(generate_tls_unit(args.tls_source))


if __name__ == "__main__":
    main()
