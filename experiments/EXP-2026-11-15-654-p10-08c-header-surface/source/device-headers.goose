// P10-08 S7: the per-backend device header surface, frozen.
//
// Native sources at the base commit (upstream/llama.cpp 0b5be7e4a,
// ggml/include/): ggml-cuda.h, ggml-metal.h, ggml-vulkan.h, ggml-rpc.h,
// ggml-opencl.h, ggml-hexagon.h, ggml-sycl.h, ggml-cann.h, ggml-blas.h,
// ggml-et.h, ggml-virtgpu.h, ggml-zdnn.h, ggml-zendnn.h, ggml-webgpu.h.
//
// What a loader of these headers depends on, and what is frozen here:
//   - the fourteen macros a caller reads before it links anything (device
//     ceilings, the three RPC protocol numbers and the six backend name
//     texts). The CUDA name macros are conditional: GGML_CUDA_NAME is "ROCm",
//     "MUSA" or "CUDA" depending on the build, and the gate resolves the live
//     expansion from the header instead of trusting the port;
//   - every one of the 84 GGML_BACKEND_API declarations: which header declares
//     it, its arity, its return class and every parameter class. A backend
//     header is a link-time contract; a declaration that appears in a header
//     it does not belong to, or that gains a parameter, breaks every consumer
//     that was built against the old one;
//   - the capability matrix: which of the twelve operations a backend exposes
//     (init, reg, is_*, buffer_type, host_buffer_type, device_count,
//     device_description, device_memory, split_buffer_type, set_n_threads,
//     guid, comm). A backend that silently loses `reg` stops being loadable
//     through the registry even though every remaining symbol still resolves.
//
// Declaration order is the header's, and the port sorts by name so the table
// can be bisected; the header index stays a separate column so a name that
// moves between headers is a difference, not a renumbering.
//
// No native value is copied here: tests/extract_native.py re-reads the headers
// on every build, and the fixture classifies each real declaration with
// decltype, so a header change that the port ignores fails the gate.
//
// The prefix is devh_.

import std;

const DEVH_NONE: i64 = 0 - 1;

// ── the fourteen headers ────────────────────────────────────────────────────
const DEVH_HDR_N: i64 = 14;

const DEVH_HDR_NAME: const u8[:][14] = [
    "ggml-cuda.h",
    "ggml-metal.h",
    "ggml-vulkan.h",
    "ggml-rpc.h",
    "ggml-opencl.h",
    "ggml-hexagon.h",
    "ggml-sycl.h",
    "ggml-cann.h",
    "ggml-blas.h",
    "ggml-et.h",
    "ggml-virtgpu.h",
    "ggml-zdnn.h",
    "ggml-zendnn.h",
    "ggml-webgpu.h"
];

// ── macros, name order ──────────────────────────────────────────────────────
// Kind 0 is an integer macro, kind 1 a string macro. The string texts are
// stored in DEVH_CONST_TXT at the same index.
const DEVH_CONST_N: i64 = 14;

const DEVH_CONST_NAME: const u8[:][14] = [
    "GGML_CANN_MAX_DEVICES",
    "GGML_CUBLAS_NAME",
    "GGML_CUDA_MAX_DEVICES",
    "GGML_CUDA_NAME",
    "GGML_ET_NAME",
    "GGML_RPC_MAX_SERVERS",
    "GGML_SYCL_MAX_DEVICES",
    "GGML_SYCL_NAME",
    "GGML_VK_MAX_DEVICES",
    "GGML_VK_NAME",
    "GGML_WEBGPU_NAME",
    "RPC_PROTO_MAJOR_VERSION",
    "RPC_PROTO_MINOR_VERSION",
    "RPC_PROTO_PATCH_VERSION"
];

const DEVH_CONST_KIND: i64[14] = [0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0];
const DEVH_CONST_NUM: i64[14] = [16, 0, 16, 0, 0, 16, 48, 0, 16, 0, 0, 6, 0, 0];

const DEVH_CONST_TXT: const u8[:][14] = [
    "",
    "cuBLAS",
    "",
    "CUDA",
    "ET",
    "",
    "",
    "SYCL",
    "",
    "Vulkan",
    "WebGPU",
    "",
    "",
    ""
];

// The device ceiling each header publishes, or -1 when it publishes none.
const DEVH_MAXDEV: i64[14] = [16, 0 - 1, 16, 0 - 1, 0 - 1, 0 - 1, 48, 16, 0 - 1, 0 - 1, 0 - 1, 0 - 1, 0 - 1, 0 - 1];

// ── the 84 declarations, name order ────────────────────────────────────────
// Classes: 0 void, 1 bool, 2 signed integer, 3 size_t, 4 float/double,
// 5 pointer (including function pointers and pointer typedefs), 6 other
// scalar passed by value. Five parameter slots per declaration, padded -1.
const DEVH_FN_N: i64 = 84;

const DEVH_FN_NAME: const u8[:][84] = [
    "ggml_backend_blas_init",
    "ggml_backend_blas_reg",
    "ggml_backend_blas_set_n_threads",
    "ggml_backend_cann_buffer_type",
    "ggml_backend_cann_get_device_count",
    "ggml_backend_cann_get_device_description",
    "ggml_backend_cann_get_device_memory",
    "ggml_backend_cann_host_buffer_type",
    "ggml_backend_cann_init",
    "ggml_backend_cann_reg",
    "ggml_backend_cuda_allreduce_tensor",
    "ggml_backend_cuda_buffer_type",
    "ggml_backend_cuda_get_device_count",
    "ggml_backend_cuda_get_device_description",
    "ggml_backend_cuda_get_device_memory",
    "ggml_backend_cuda_host_buffer_type",
    "ggml_backend_cuda_init",
    "ggml_backend_cuda_reg",
    "ggml_backend_cuda_register_host_buffer",
    "ggml_backend_cuda_unregister_host_buffer",
    "ggml_backend_et_buffer_type",
    "ggml_backend_et_get_device_count",
    "ggml_backend_et_get_device_description",
    "ggml_backend_et_get_device_memory",
    "ggml_backend_et_guid",
    "ggml_backend_et_host_buffer_type",
    "ggml_backend_et_init",
    "ggml_backend_et_reg",
    "ggml_backend_hexagon_init",
    "ggml_backend_hexagon_reg",
    "ggml_backend_is_blas",
    "ggml_backend_is_cann",
    "ggml_backend_is_cuda",
    "ggml_backend_is_et",
    "ggml_backend_is_hexagon",
    "ggml_backend_is_metal",
    "ggml_backend_is_opencl",
    "ggml_backend_is_rpc",
    "ggml_backend_is_sycl",
    "ggml_backend_is_vk",
    "ggml_backend_is_zendnn",
    "ggml_backend_metal_capture_next_compute",
    "ggml_backend_metal_init",
    "ggml_backend_metal_reg",
    "ggml_backend_metal_set_abort_callback",
    "ggml_backend_metal_supports_family",
    "ggml_backend_opencl_buffer_type",
    "ggml_backend_opencl_host_buffer_type",
    "ggml_backend_opencl_init",
    "ggml_backend_opencl_reg",
    "ggml_backend_rpc_add_server",
    "ggml_backend_rpc_buffer_type",
    "ggml_backend_rpc_get_device_memory",
    "ggml_backend_rpc_init",
    "ggml_backend_rpc_reg",
    "ggml_backend_rpc_start_server",
    "ggml_backend_sycl_buffer_type",
    "ggml_backend_sycl_comm_allreduce_tensor",
    "ggml_backend_sycl_comm_free",
    "ggml_backend_sycl_comm_init",
    "ggml_backend_sycl_get_device_count",
    "ggml_backend_sycl_get_device_description",
    "ggml_backend_sycl_get_device_memory",
    "ggml_backend_sycl_get_gpu_list",
    "ggml_backend_sycl_host_buffer_type",
    "ggml_backend_sycl_init",
    "ggml_backend_sycl_print_sycl_devices",
    "ggml_backend_sycl_reg",
    "ggml_backend_sycl_split_buffer_type",
    "ggml_backend_virtgpu_reg",
    "ggml_backend_vk_buffer_type",
    "ggml_backend_vk_get_device_count",
    "ggml_backend_vk_get_device_description",
    "ggml_backend_vk_get_device_memory",
    "ggml_backend_vk_host_buffer_type",
    "ggml_backend_vk_init",
    "ggml_backend_vk_reg",
    "ggml_backend_webgpu_init",
    "ggml_backend_webgpu_reg",
    "ggml_backend_zdnn_buffer_type",
    "ggml_backend_zdnn_reg",
    "ggml_backend_zendnn_init",
    "ggml_backend_zendnn_reg",
    "ggml_backend_zendnn_set_n_threads"
];

const DEVH_FN_HDR: i64[84] = [
    8, 8, 8, 7, 7, 7, 7, 7,
    7, 7, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 9, 9, 9, 9,
    9, 9, 9, 9, 5, 5, 8, 7,
    0, 9, 5, 1, 4, 3, 6, 2,
    12, 1, 1, 1, 1, 1, 4, 4,
    4, 4, 3, 3, 3, 3, 3, 3,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 10, 2, 2,
    2, 2, 2, 2, 2, 13, 13, 11,
    11, 12, 12, 12,
];

const DEVH_FN_ARITY: i64[84] = [
    0, 0, 2, 1, 0, 3, 3, 0,
    1, 0, 3, 1, 0, 3, 3, 0,
    1, 0, 2, 1, 1, 0, 3, 3,
    0, 0, 1, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 3, 2, 0, 0,
    0, 0, 1, 2, 4, 2, 0, 5,
    1, 2, 1, 2, 0, 3, 3, 2,
    0, 1, 0, 0, 1, 0, 1, 0,
    3, 3, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 2,
];

const DEVH_FN_RET: i64[84] = [
    5, 5, 0, 5, 2, 0, 0, 5,
    5, 5, 1, 5, 2, 0, 0, 5,
    5, 5, 1, 0, 5, 2, 0, 0,
    5, 5, 5, 5, 5, 5, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 5, 5, 0, 1, 5, 5,
    5, 5, 5, 5, 0, 5, 5, 0,
    5, 1, 0, 5, 2, 0, 0, 0,
    5, 5, 0, 5, 5, 5, 5, 2,
    0, 0, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 0,
];

const DEVH_FN_PAR: i64[420] = [
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 5, 2, -1, -1, -1, 2,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 2, 5, 3, -1, -1, 2, 5,
    5, -1, -1, -1, -1, -1, -1, -1,
    2, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 5, 5, 3, -1, -1, 2,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 2, 5, 3, -1, -1, 2, 5,
    5, -1, -1, -1, -1, -1, -1, -1,
    2, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 5, 3, -1, -1, -1, 5,
    -1, -1, -1, -1, 3, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 2, 5,
    3, -1, -1, 2, 5, 5, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 3, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 5, -1,
    -1, -1, -1, 5, -1, -1, -1, -1,
    5, -1, -1, -1, -1, 5, -1, -1,
    -1, -1, 5, -1, -1, -1, -1, 5,
    -1, -1, -1, -1, 5, -1, -1, -1,
    -1, 5, -1, -1, -1, -1, 5, -1,
    -1, -1, -1, 5, -1, -1, -1, -1,
    5, -1, -1, -1, -1, 5, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 5, 5, 5, -1,
    -1, 5, 2, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 5, -1, -1, -1, -1, 5,
    6, -1, -1, -1, 5, 6, 5, 5,
    -1, 5, 6, -1, -1, -1, -1, -1,
    -1, -1, -1, 5, 5, 3, 3, 5,
    2, -1, -1, -1, -1, 5, 5, -1,
    -1, -1, 5, -1, -1, -1, -1, 5,
    3, -1, -1, -1, -1, -1, -1, -1,
    -1, 2, 5, 3, -1, -1, 2, 5,
    5, -1, -1, 5, 2, -1, -1, -1,
    -1, -1, -1, -1, -1, 2, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 5, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 3, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    2, 5, 3, -1, -1, 2, 5, 5,
    -1, -1, -1, -1, -1, -1, -1, 3,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 5,
    2, -1, -1, -1,
];

// ── capability matrix ──────────────────────────────────────────────────────
// Twelve capabilities, tested against the name list above. The port derives
// them, it does not tabulate them: a header that gains a `_reg` declaration
// gains the capability without a table edit, and a scan bug shows up as a
// matrix difference in both directions.
const DEVH_CAP_N: i64 = 12;

const DEVH_CAP_NAME: const u8[:][12] = [
    "init",
    "reg",
    "is",
    "buffer_type",
    "host_buffer_type",
    "device_count",
    "device_description",
    "device_memory",
    "split_buffer_type",
    "set_n_threads",
    "guid",
    "comm"
];

fn devh_copy(out: u8[:], s: const u8[:]) -> i64 {
    var n: i64 = 0;
    while n < s.len && n < out.len {
        out[n] = s[n];
        n = n + 1;
    }
    return n;
}

fn devh_suffix(s: const u8[:], t: const u8[:]) -> i64 {
    if t.len > s.len {
        return 0;
    }
    var i: i64 = 0;
    while i < t.len {
        if s[s.len - t.len + i] != t[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn devh_has(s: const u8[:], t: const u8[:]) -> i64 {
    if t.len > s.len {
        return 0;
    }
    var i: i64 = 0;
    while i + t.len <= s.len {
        var j: i64 = 0;
        var ok: i64 = 1;
        while j < t.len {
            if s[i + j] != t[j] {
                ok = 0;
            }
            j = j + 1;
        }
        if ok == 1 {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

// True when declaration id belongs to header h and matches capability c.
fn devh_match(id: i64, c: i64) -> i64 {
    const s: const u8[:] = DEVH_FN_NAME[id];
    if c == 0 {
        if devh_has(s, "_comm_") == 1 {
            return 0;
        }
        return devh_suffix(s, "_init");
    }
    if c == 1 {
        return devh_suffix(s, "_reg");
    }
    if c == 2 {
        return devh_has(s, "_is_");
    }
    if c == 3 {
        if devh_suffix(s, "_host_buffer_type") == 1 {
            return 0;
        }
        if devh_suffix(s, "_split_buffer_type") == 1 {
            return 0;
        }
        return devh_suffix(s, "_buffer_type");
    }
    if c == 4 {
        return devh_suffix(s, "_host_buffer_type");
    }
    if c == 5 {
        return devh_suffix(s, "_get_device_count");
    }
    if c == 6 {
        return devh_suffix(s, "_get_device_description");
    }
    if c == 7 {
        return devh_suffix(s, "_get_device_memory");
    }
    if c == 8 {
        return devh_suffix(s, "_split_buffer_type");
    }
    if c == 9 {
        return devh_suffix(s, "_set_n_threads");
    }
    if c == 10 {
        return devh_suffix(s, "_guid");
    }
    if c == 11 {
        return devh_has(s, "_comm_");
    }
    return 0;
}

// ---- exports --------------------------------------------------------------

export fn devh_header_count() -> i64 {
    return DEVH_HDR_N;
}

export fn devh_header_name(h: i64, out: u8[:]) -> i64 {
    if h < 0 || h >= DEVH_HDR_N {
        return DEVH_NONE;
    }
    return devh_copy(out, DEVH_HDR_NAME[h]);
}

export fn devh_max_devices(h: i64) -> i64 {
    if h < 0 || h >= DEVH_HDR_N {
        return DEVH_NONE;
    }
    return DEVH_MAXDEV[h];
}

export fn devh_const_count() -> i64 {
    return DEVH_CONST_N;
}

export fn devh_const_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= DEVH_CONST_N {
        return DEVH_NONE;
    }
    return devh_copy(out, DEVH_CONST_NAME[i]);
}

// Kind 0 integer, kind 1 string; the value is in devh_const_num/_text.
export fn devh_const_kind(i: i64) -> i64 {
    if i < 0 || i >= DEVH_CONST_N {
        return DEVH_NONE;
    }
    return DEVH_CONST_KIND[i];
}

export fn devh_const_num(i: i64) -> i64 {
    if i < 0 || i >= DEVH_CONST_N || DEVH_CONST_KIND[i] != 0 {
        return DEVH_NONE;
    }
    return DEVH_CONST_NUM[i];
}

export fn devh_const_text(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= DEVH_CONST_N || DEVH_CONST_KIND[i] != 1 {
        return DEVH_NONE;
    }
    return devh_copy(out, DEVH_CONST_TXT[i]);
}

export fn devh_fn_count() -> i64 {
    return DEVH_FN_N;
}

export fn devh_fn_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= DEVH_FN_N {
        return DEVH_NONE;
    }
    return devh_copy(out, DEVH_FN_NAME[i]);
}

export fn devh_fn_header(i: i64) -> i64 {
    if i < 0 || i >= DEVH_FN_N {
        return DEVH_NONE;
    }
    return DEVH_FN_HDR[i];
}

// Writes arity, return class and every parameter class; returns the number of
// slots written, or -1 for an unknown id.
export fn devh_fn_shape(i: i64, out: i64[:]) -> i64 {
    if i < 0 || i >= DEVH_FN_N {
        return DEVH_NONE;
    }
    var n: i64 = 0;
    if n < out.len {
        out[n] = DEVH_FN_ARITY[i];
        n = n + 1;
    }
    if n < out.len {
        out[n] = DEVH_FN_RET[i];
        n = n + 1;
    }
    var j: i64 = 0;
    while j < DEVH_FN_ARITY[i] && n < out.len {
        out[n] = DEVH_FN_PAR[i * 5 + j];
        n = n + 1;
        j = j + 1;
    }
    return n;
}

// 1 when header h declares at least one function for capability c.
export fn devh_cap(h: i64, c: i64) -> i64 {
    if h < 0 || h >= DEVH_HDR_N || c < 0 || c >= DEVH_CAP_N {
        return DEVH_NONE;
    }
    var i: i64 = 0;
    while i < DEVH_FN_N {
        if DEVH_FN_HDR[i] == h && devh_match(i, c) == 1 {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

export fn devh_cap_count() -> i64 {
    return DEVH_CAP_N;
}

export fn devh_cap_name(c: i64, out: u8[:]) -> i64 {
    if c < 0 || c >= DEVH_CAP_N {
        return DEVH_NONE;
    }
    return devh_copy(out, DEVH_CAP_NAME[c]);
}
