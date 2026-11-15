// P10-08 S4: the backend public header surface, frozen.
//
// Native sources at the base commit (upstream/llama.cpp 0b5be7e4a):
//   ggml/include/ggml-backend.h:49-53     enum ggml_backend_buffer_usage
//   ggml/include/ggml-backend.h:134-145   enum ggml_backend_dev_type
//   ggml/include/ggml-backend.h:148-179   ggml_backend_dev_caps / _props
//   ggml/include/ggml-backend.h:221-225   ggml_backend_feature
//   ggml/include/ggml-backend.h:360-396   META_MAX_DEVICES, split axes, split state
//   ggml/include/ggml-backend.h:411-420   ggml_backend_graph_copy
//   ggml/include/ggml-backend.h:208-219, 316, 399, 422  the callback typedefs
//   ggml/src/ggml-backend-meta.cpp:27-48  ggml_backend_meta_split_axis_name
//
// What a consumer of this header depends on and what is frozen here:
//   - every enum value of the two public enums and of the split-axis enum,
//     including the four bookkeeping values that are not axes 0..3;
//   - the layout of the five public structs that cross the header by pointer
//     or by value (size and every field offset, LP64);
//   - the shape of the eleven public callback typedefs: arity, return class and
//     parameter classes. A callback whose arity or pointer-ness drifts is a
//     source break for every backend that registers one, even when the name and
//     the enum values are unchanged;
//   - the eight texts ggml_backend_meta_split_axis_name returns. The native
//     switch aborts on anything else, so the port refuses with -1 instead.
//
// Struct sizes are frozen as values, not as declarations: a consumer built
// against one layout and a provider built against another disagree silently at
// the call site, and only the numbers can catch that.
//
// The prefix is bkh_.

import std;

const BKH_NONE: i64 = 0 - 1;

// ── constants and layout entries, name order ────────────────────────────────
// The names are the header identifiers; the two prefixed families name a
// sizeof or an offsetof of a public struct.
const BKH_CONST_N: i64 = 39;

const BKH_CONST_NAME: const u8[:][39] = [
    "GGML_BACKEND_BUFFER_USAGE_ANY",
    "GGML_BACKEND_BUFFER_USAGE_WEIGHTS",
    "GGML_BACKEND_BUFFER_USAGE_COMPUTE",
    "GGML_BACKEND_DEVICE_TYPE_CPU",
    "GGML_BACKEND_DEVICE_TYPE_GPU",
    "GGML_BACKEND_DEVICE_TYPE_IGPU",
    "GGML_BACKEND_DEVICE_TYPE_ACCEL",
    "GGML_BACKEND_DEVICE_TYPE_META",
    "GGML_BACKEND_META_MAX_DEVICES",
    "GGML_BACKEND_SPLIT_AXIS_0",
    "GGML_BACKEND_SPLIT_AXIS_1",
    "GGML_BACKEND_SPLIT_AXIS_2",
    "GGML_BACKEND_SPLIT_AXIS_3",
    "GGML_BACKEND_SPLIT_AXIS_MIRRORED",
    "GGML_BACKEND_SPLIT_AXIS_PARTIAL",
    "GGML_BACKEND_SPLIT_AXIS_NONE",
    "GGML_BACKEND_SPLIT_AXIS_UNKNOWN",
    "sizeof:ggml_backend_dev_caps",
    "offsetof:ggml_backend_dev_caps.async",
    "offsetof:ggml_backend_dev_caps.host_buffer",
    "offsetof:ggml_backend_dev_caps.buffer_from_host_ptr",
    "offsetof:ggml_backend_dev_caps.events",
    "offsetof:ggml_backend_dev_caps.mmap_support",
    "sizeof:ggml_backend_dev_props",
    "offsetof:ggml_backend_dev_props.name",
    "offsetof:ggml_backend_dev_props.description",
    "offsetof:ggml_backend_dev_props.memory_free",
    "offsetof:ggml_backend_dev_props.memory_total",
    "offsetof:ggml_backend_dev_props.type",
    "offsetof:ggml_backend_dev_props.device_id",
    "offsetof:ggml_backend_dev_props.caps",
    "sizeof:ggml_backend_feature",
    "offsetof:ggml_backend_feature.name",
    "offsetof:ggml_backend_feature.value",
    "sizeof:ggml_backend_meta_split_state",
    "offsetof:ggml_backend_meta_split_state.axis",
    "offsetof:ggml_backend_meta_split_state.ne",
    "offsetof:ggml_backend_meta_split_state.nr",
    "offsetof:ggml_backend_meta_split_state.n_segments"
];

const BKH_CONST_VAL: i64[39] = [
    0, 1, 2,
    0, 1, 2, 3, 4,
    16,
    0, 1, 2, 3, 10, 11, 98, 99,
    5,
    0, 1, 2, 3, 4,
    56,
    0, 8, 16, 24, 32, 40, 48,
    16,
    0, 8,
    2128,
    0, 8, 2056, 2120
];

// ── the eleven callback typedefs ────────────────────────────────────────────
// Shape slots: [arity, return class, parameter classes...]. Classes:
//   0 void, 1 bool, 2 int (int/int64_t), 3 size_t, 4 float/double,
//   5 pointer, 6 struct passed by value.
const BKH_CB_N: i64 = 11;

const BKH_CB_NAME: const u8[:][11] = [
    "ggml_backend_comm_init_t",
    "ggml_backend_comm_free_t",
    "ggml_backend_comm_allreduce_tensor_t",
    "ggml_backend_split_buffer_type_t",
    "ggml_backend_set_n_threads_t",
    "ggml_backend_dev_get_extra_bufts_t",
    "ggml_backend_set_abort_callback_t",
    "ggml_backend_get_features_t",
    "ggml_backend_sched_eval_callback",
    "ggml_backend_meta_get_split_state_t",
    "ggml_backend_eval_callback"
];

// arity, then return/parameter classes in source order.
const BKH_CB_ARITY: i64[11] = [2, 1, 2, 2, 2, 1, 3, 1, 3, 2, 4];
const BKH_CB_RET: i64[11] = [5, 0, 1, 5, 0, 5, 0, 5, 1, 6, 1];

// Four parameter slots per callback, padded with -1.
const BKH_CB_PAR: i64[44] = [
    5, 3, 0 - 1, 0 - 1,
    5, 0 - 1, 0 - 1, 0 - 1,
    5, 5, 0 - 1, 0 - 1,
    2, 5, 0 - 1, 0 - 1,
    5, 2, 0 - 1, 0 - 1,
    5, 0 - 1, 0 - 1, 0 - 1,
    5, 5, 5, 0 - 1,
    5, 0 - 1, 0 - 1, 0 - 1,
    5, 1, 5, 0 - 1,
    5, 5, 0 - 1, 0 - 1,
    2, 5, 5, 5
];

// ── the split-axis name table (ggml-backend-meta.cpp:27-48) ─────────────────
const BKH_AXIS_N: i64 = 8;
const BKH_AXIS_VAL: i64[8] = [0, 1, 2, 3, 10, 11, 98, 99];
const BKH_AXIS_TXT: const u8[:][8] = [
    "0",
    "1",
    "2",
    "3",
    "MIRRORED",
    "PARTIAL",
    "NONE",
    "UNKNOWN"
];

fn bkh_copy(out: u8[:], s: const u8[:]) -> i64 {
    var n: i64 = 0;
    while n < s.len && n < out.len {
        out[n] = s[n];
        n = n + 1;
    }
    return n;
}

// ---- exports --------------------------------------------------------------

export fn bkh_const_count() -> i64 {
    return BKH_CONST_N;
}

export fn bkh_const_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= BKH_CONST_N {
        return BKH_NONE;
    }
    return bkh_copy(out, BKH_CONST_NAME[i]);
}

export fn bkh_const_value(i: i64) -> i64 {
    if i < 0 || i >= BKH_CONST_N {
        return BKH_NONE;
    }
    return BKH_CONST_VAL[i];
}

export fn bkh_callback_count() -> i64 {
    return BKH_CB_N;
}

export fn bkh_callback_name(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= BKH_CB_N {
        return BKH_NONE;
    }
    return bkh_copy(out, BKH_CB_NAME[id]);
}

// Writes arity, return class and every parameter class; returns the number of
// slots written, or -1 for an unknown id.
export fn bkh_callback_shape(id: i64, out: i64[:]) -> i64 {
    if id < 0 || id >= BKH_CB_N {
        return BKH_NONE;
    }
    var n: i64 = 0;
    if n < out.len {
        out[n] = BKH_CB_ARITY[id];
        n = n + 1;
    }
    if n < out.len {
        out[n] = BKH_CB_RET[id];
        n = n + 1;
    }
    var j: i64 = 0;
    while j < BKH_CB_ARITY[id] && n < out.len {
        out[n] = BKH_CB_PAR[id * 4 + j];
        n = n + 1;
        j = j + 1;
    }
    return n;
}

export fn bkh_split_axis_count() -> i64 {
    return BKH_AXIS_N;
}

export fn bkh_split_axis_value(i: i64) -> i64 {
    if i < 0 || i >= BKH_AXIS_N {
        return BKH_NONE;
    }
    return BKH_AXIS_VAL[i];
}

export fn bkh_split_axis_name(axis: i64, out: u8[:]) -> i64 {
    var i: i64 = 0;
    while i < BKH_AXIS_N {
        if BKH_AXIS_VAL[i] == axis {
            return bkh_copy(out, BKH_AXIS_TXT[i]);
        }
        i = i + 1;
    }
    return BKH_NONE;
}
