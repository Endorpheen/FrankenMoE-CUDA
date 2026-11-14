// P4-12 slice source: meta backend device/buffer-type policy, meta registry
// identity, and thread-count/cpumask policy.
//
// Ported from, at base commit df3ce73a:
//   upstream/llama.cpp/ggml/src/ggml-backend-meta.cpp  (device and buffer-type
//       getters, lines 54-392)
//   upstream/llama.cpp/ggml/src/ggml.c                 (ggml_threadpool_params_
//       default/init/match, lines 8045-8065)
//   upstream/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c    (ggml_thread_cpumask_next,
//       ggml_thread_cpumask_is_valid, the n_threads clamp in ggml_graph_compute,
//       lines 2682-2712 and 3384-3410)
//   runtime/core/src/config.cpp                        (n_threads and
//       moe.io_threads admission rules)
//
// Every function is pure: inputs are integers or byte blobs, outputs are
// integers or bytes written into a caller buffer. No ggml handle, no GPU, no
// OS call crosses this boundary. The differential fixture in tests/fixture.cpp
// drives the real native code with synthetic devices and compares.
//
// Name prefixes: mb_ = meta device/buffer-type, rg_ = registry identity,
// th_ = ggml threadpool policy, rt_ = runtime config admission.

import std;

// ---------- byte output helpers ----------

fn put_byte(out: u8[:], w: i64, b: u8) -> i64 {
    if w < out.len {
        out[w] = b;
    }
    return w + 1;
}

// Native builds the name in the constructor, but the loop reads the by-value
// parameter after it was moved into the member vector (ggml-backend-meta.cpp
// lines 62-77). The parameter is therefore empty, and both the device name and
// the description are the constant "Meta()" at this base commit. The buffer
// type constructor has the same shape (lines 256-265). The port reproduces the
// observed behaviour; the intended join is recorded in REPORT.md as a follow-up
// tied to the upstream fix.
fn put_meta_empty(out: u8[:]) -> i64 {
    var w: i64 = 0;
    w = put_byte(out, w, 77); // M
    w = put_byte(out, w, 101); // e
    w = put_byte(out, w, 116); // t
    w = put_byte(out, w, 97); // a
    w = put_byte(out, w, 40); // (
    w = put_byte(out, w, 41); // )
    return w;
}

export fn mb_name(out: u8[:], blob: const u8[:]) -> i64 {
    return put_meta_empty(out);
}

export fn mb_description(out: u8[:], blob: const u8[:]) -> i64 {
    return put_meta_empty(out);
}

// ---------- meta device aggregation ----------

// get_memory sums both fields over the simple devices.
export fn mb_free_sum(vals: const i64[:]) -> i64 {
    var t: i64 = 0;
    var i: i64 = 0;
    while i < vals.len {
        t = t + vals[i];
        i = i + 1;
    }
    return t;
}

export fn mb_total_sum(vals: const i64[:]) -> i64 {
    var t: i64 = 0;
    var i: i64 = 0;
    while i < vals.len {
        t = t + vals[i];
        i = i + 1;
    }
    return t;
}

// get_props starts from async and mmap_support true, the other three false
// (ggml-backend-meta.cpp lines 130-136), then ANDs every device. The three
// false seeds stay false for every meta device at this base: host_buffer,
// buffer_from_host_ptr and events are always reported as unsupported.
// Bit 0 async, 1 host_buffer, 2 buffer_from_host_ptr, 3 events, 4 mmap_support.
export fn mb_caps_and(bits: const i64[:]) -> i64 {
    var m: i64 = 17;
    var i: i64 = 0;
    while i < bits.len {
        m = m & bits[i];
        i = i + 1;
    }
    return m;
}

// supports_op is std::all_of over the simple devices.
export fn mb_supports_op(flags: const i64[:]) -> i64 {
    var i: i64 = 0;
    while i < flags.len {
        if flags[i] == 0 {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

// supports_buft first requires both sides to be meta, then equal device count,
// then the same simple device at every index. Order matters.
export fn mb_supports_buft(a: const i64[:], b: const i64[:], a_meta: i64, b_meta: i64) -> i64 {
    if a_meta == 0 || b_meta == 0 {
        return 0;
    }
    if a.len != b.len {
        return 0;
    }
    var i: i64 = 0;
    while i < a.len {
        if a[i] != b[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

// A meta device reports type META regardless of the members. The enum value is
// part of ggml-backend.h; the fixture compares against the native constant.
export fn mb_device_type() -> i64 {
    return 4;
}

// ---------- meta buffer-type policy ----------

// get_alignment starts at 1 and takes the maximum; native asserts that every
// alignment divides the running maximum.
export fn mb_alignment_max(al: const i64[:]) -> i64 {
    var m: i64 = 1;
    var i: i64 = 0;
    while i < al.len {
        if al[i] > m {
            m = al[i];
        }
        i = i + 1;
    }
    return m;
}

// get_max_size starts at SIZE_MAX and takes the minimum (unsigned).
export fn mb_max_size_min(sz: const i64[:]) -> i64 {
    var m: u64 = 18446744073709551615;
    var i: i64 = 0;
    while i < sz.len {
        let v = sz[i] as u64;
        if v < m {
            m = v;
        }
        i = i + 1;
    }
    return m as i64;
}

// get_alloc_size starts at 0 and takes the maximum.
export fn mb_alloc_size_max(sz: const i64[:]) -> i64 {
    var m: i64 = 0;
    var i: i64 = 0;
    while i < sz.len {
        if sz[i] > m {
            m = sz[i];
        }
        i = i + 1;
    }
    return m;
}

// is_host is true only when every simple buffer type is host memory.
export fn mb_is_host(flags: const i64[:]) -> i64 {
    var i: i64 = 0;
    while i < flags.len {
        if flags[i] == 0 {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

// The host buffer type exists only when every device reports one and they are
// all the same handle. -1 means "no host buffer type".
export fn mb_host_buft(ids: const i64[:]) -> i64 {
    if ids.len == 0 {
        return -1;
    }
    let first = ids[0];
    if first < 0 {
        return -1;
    }
    var i: i64 = 1;
    while i < ids.len {
        if ids[i] != first {
            return -1;
        }
        i = i + 1;
    }
    return first;
}

// ---------- registry identity ----------

// The meta device and buffer-type maps key on the ordered device sequence plus
// the split-state callback and its userdata. Equality of that key is exactly
// ordered equality, not set equality.
export fn rg_key_eq(a: const i64[:], b: const i64[:], a_cb: i64, b_cb: i64, a_ud: i64, b_ud: i64) -> i64 {
    if a.len != b.len {
        return 0;
    }
    var i: i64 = 0;
    while i < a.len {
        if a[i] != b[i] {
            return 0;
        }
        i = i + 1;
    }
    if a_cb != b_cb || a_ud != b_ud {
        return 0;
    }
    return 1;
}

// Insert-or-find outcome: an existing slot on a hit, otherwise the next free
// slot, which is what the native map does with emplace/find.
export fn rg_memo_slot(used: i64, hit: i64) -> i64 {
    if hit >= 0 {
        return hit;
    }
    return used;
}

// The meta buffer type is memoized per meta device handle, so two devices that
// deduplicated to one handle share one buffer type.
export fn rg_buft_slot(dev_slot: i64) -> i64 {
    return dev_slot;
}

// ---------- threading policy ----------

// ggml_threadpool_params_init/default: n_threads is copied verbatim (the caller
// clamps), prio 0, poll 50, strict_cpu false, paused false, cpumask all zero.
export fn th_params_default(n: i64, out: i64[:]) -> i64 {
    out[0] = n;
    out[1] = 0;
    out[2] = 50;
    out[3] = 0;
    out[4] = 0;
    return 5;
}

export fn th_cpumask_valid(mask: const i64[:]) -> i64 {
    var i: i64 = 0;
    while i < mask.len {
        if mask[i] != 0 {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

// Strict placement hands each thread one distinct CPU, round-robin, wrapping at
// GGML_MAX_N_THREADS. Non-strict placement copies the whole mask.
export fn th_cpumask_next(global: const i64[:], strict: i64, iter: i64, local: i64[:]) -> i64 {
    var i: i64 = 0;
    if strict == 0 {
        while i < global.len {
            local[i] = global[i];
            i = i + 1;
        }
        return iter;
    }
    while i < local.len {
        local[i] = 0;
        i = i + 1;
    }
    var k: i64 = 0;
    while k < 512 {
        var idx: i64 = iter + k;
        if idx >= 512 {
            idx = idx - 512;
        }
        if global[idx] != 0 {
            local[idx] = 1;
            return idx + 1;
        }
        k = k + 1;
    }
    return iter;
}

// ggml_graph_plan: a non-positive request means the pool width, else 4.
// ggml_graph_compute then clamps the request to the pool width.
export fn th_plan_threads(req: i64, pool: i64) -> i64 {
    var n: i64 = req;
    if n <= 0 {
        if pool > 0 {
            n = pool;
        } else {
            n = 4;
        }
    }
    if n > pool {
        n = pool;
    }
    return n;
}

// ggml_threadpool_params_match compares n_threads, prio, poll, strict_cpu and
// the cpumask. cpumask is passed as an integer bit pattern. paused is ignored.
export fn th_params_match(a: const i64[:], b: const i64[:]) -> i64 {
    var i: i64 = 0;
    while i < 5 {
        if a[i] != b[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

// ---------- runtime config admission ----------

// config.cpp: n_threads must be positive.
export fn rt_n_threads_ok(v: i64) -> i64 {
    if v <= 0 {
        return 0;
    }
    return 1;
}

// config.cpp: moe.io_threads must be in [1, MoeStreamConfig::io_threads_max],
// with io_threads_max = 8 (include/bmoe/config.h).
export fn rt_io_threads_ok(v: i64) -> i64 {
    if v < 1 || v > 8 {
        return 0;
    }
    return 1;
}
