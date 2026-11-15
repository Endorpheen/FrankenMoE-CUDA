// Goose bridge for the GGUF writer and the split plan (staged S1.2 trial).
//
// Built only when FG_GOOSE_GGUF=ON. See fg-goose-gguf-bridge.h for the
// contract and gguf_fg_goose_adapter.inl for the side that reads the private
// gguf_context.
//
// Order of operations per call:
//   1. s1_init                       -> a fresh flat KV list
//   2. s1_set_val/str/arr/arr_str    -> the native keys in native order
//   3. s3_init_tensors + s3_add_tensor -> descriptors, dims and offsets
//   4. s3_write                      -> header, KV, descriptors, pad, data
// Every buffer below is per-call scratch: no allocation is cached between
// calls yet (see the report, "remaining seam").
#include "fg-goose-gguf-bridge.h"

#include "gguf-stack.h"
#include "goose-host.h" // fg_goose_host_ensure_init: the one shared start-up

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

namespace fg_goose {
namespace {

std::uint64_t  g_calls[ENTRY_COUNT] = {0, 0, 0, 0, 0};

static const char * entry_name(entry e) {
    switch (e) {
        case ENTRY_WRITE:     return "write";
        case ENTRY_META_SIZE: return "meta_size";
        case ENTRY_META_DATA: return "meta_data";
        case ENTRY_SPLIT:     return "split";
        case ENTRY_LAYOUT:    return "layout";
        default:              return "?";
    }
}

// FG_GOOSE_GGUF_STATS=1 makes the process print the witness counts at exit.
// The split tool has no other way to show that its plan went into Goose: the
// plan output must stay byte-identical to the native one.
void stats_at_exit() {
    std::fprintf(stderr, "FG630_STATS");
    for (int i = 0; i < ENTRY_COUNT; i++) {
        std::fprintf(stderr, " %s=%llu", entry_name(static_cast<entry>(i)),
                     (unsigned long long) g_calls[i]);
    }
    std::fprintf(stderr, "\n");
}

// Slice helper: a Go-style byte/int64 slice over host memory.
sl_u8  s_u8(const void * p, std::int64_t n) {
    return sl_u8{static_cast<std::uint8_t *>(const_cast<void *>(p)), n};
}
sl_i64 s_i64(const void * p, std::int64_t n) {
    return sl_i64{static_cast<std::int64_t *>(const_cast<void *>(p)), n};
}

std::int64_t type_size(std::int32_t t) {
    switch (t) {
        case 0: case 1: case 7: return 1;  // u8 i8 bool
        case 2: case 3:         return 2;  // u16 i16
        case 4: case 5: case 6: return 4;  // u32 i32 f32
        case 10: case 11: case 12: return 8;  // u64 i64 f64
        default:                return 0;  // string / array are not scalars
    }
}

// Little-endian unsigned value of the first `n` bytes, zero-extended to 64 bits.
// The native gguf_kv keeps host values by memcpy, so on a little-endian host
// these bytes are exactly the bits the native writer emits.
std::uint64_t raw_bits(const void * p, std::size_t n) {
    const auto * b = static_cast<const std::uint8_t *>(p);
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n && i < 8; i++) {
        v |= static_cast<std::uint64_t>(b[i]) << (8 * i);
    }
    return v;
}

std::size_t pad(std::size_t v, std::size_t a) {
    return ((v + a - 1) / a) * a;
}

}  // namespace

std::uint64_t calls(entry site) {
    return site < ENTRY_COUNT ? g_calls[site] : 0;
}

static void arm_stats() {
    fg_goose_host_ensure_init();
    static std::once_flag armed;
    std::call_once(armed, []() {
        const char * v = std::getenv("FG_GOOSE_GGUF_STATS");
        if (v != nullptr && v[0] != '\0' && v[0] != '0') {
            std::atexit(stats_at_exit);
        }
    });
}

bool gguf_write(entry site, std::uint32_t version, std::size_t alignment,
                const kv_view * kv, std::size_t n_kv,
                const tensor_view * ts, std::size_t n_tensors,
                std::vector<std::int8_t> & out, bool only_meta) {
    arm_stats();

    out.clear();
    if (alignment == 0 || n_kv >= (1u << 24) || n_tensors >= (1u << 24)) {
        return false;
    }

    // ---- scratch sized from the views -------------------------------------

    std::size_t kv_pool_bytes = 0;
    std::size_t t_pool_bytes  = 0;
    std::size_t cap           = 24;  // magic + version + two u64 counts
    for (std::size_t i = 0; i < n_kv; i++) {
        const kv_view & k = kv[i];
        const std::size_t klen = std::strlen(k.key);
        kv_pool_bytes += klen;
        cap += 8 + klen + 4 + 12;
        if (k.is_array) {
            std::size_t bytes = k.n_data;
            if (k.strs != nullptr) {
                bytes = 0;
                for (std::size_t j = 0; j < k.n_strs; j++) {
                    bytes += std::strlen(k.strs[j]) + 1;
                }
            }
            kv_pool_bytes += bytes;
            cap += bytes;
        } else if (k.type == 8) {
            // Scalar string: n_data is the byte length of the text.
            kv_pool_bytes += k.n_data;
            cap += k.n_data;
        } else {
            cap += type_size(k.type);
        }
    }
    for (std::size_t i = 0; i < n_tensors; i++) {
        const tensor_view & t = ts[i];
        const std::size_t nlen = std::strlen(t.name);
        t_pool_bytes += nlen + t.n_data;
        cap += 8 + nlen + 4 + 8 * static_cast<std::size_t>(t.ndims) + 4 + 8;
        cap += pad(t.n_data, alignment);
    }
    cap += alignment * (n_tensors + 2);

    const std::int64_t kv_n = static_cast<std::int64_t>(n_kv);
    const std::int64_t t_n  = static_cast<std::int64_t>(n_tensors);
    // s1_init writes record 0 unconditionally, so the arrays are never empty.
    const std::int64_t kv_cap = n_kv == 0 ? 1 : kv_n;

    std::vector<std::int64_t> st(2, 0);
    std::vector<std::int64_t> ko(std::max<std::size_t>(n_kv, 1), 0);
    std::vector<std::int64_t> kl(std::max<std::size_t>(n_kv, 1), 0);
    std::vector<std::int64_t> ty(std::max<std::size_t>(n_kv, 1), 0);
    std::vector<std::int64_t> ar(std::max<std::size_t>(n_kv, 1), 0);
    std::vector<std::int64_t> v0(std::max<std::size_t>(n_kv, 1), 0);
    std::vector<std::int64_t> v1(std::max<std::size_t>(n_kv, 1), 0);
    std::vector<std::int64_t> v2(std::max<std::size_t>(n_kv, 1), 0);
    std::vector<std::uint8_t> pool(kv_pool_bytes + 1, 0);
    std::vector<std::int64_t> out_rc(1, 0);

    if (s1_init(s_i64(st.data(), 2), s_i64(ko.data(), kv_cap), s_i64(kl.data(), kv_cap),
                s_i64(ty.data(), kv_cap), s_i64(ar.data(), kv_cap), s_i64(v0.data(), kv_cap),
                s_i64(v1.data(), kv_cap), s_i64(v2.data(), kv_cap)) != 0) {
        return false;
    }

    // ---- KV list in native order -----------------------------------------

    std::vector<std::uint8_t> arr_str_blob;
    for (std::size_t i = 0; i < n_kv; i++) {
        const kv_view & k = kv[i];
        const sl_u8 key = s_u8(k.key, static_cast<std::int64_t>(std::strlen(k.key)));
        std::int64_t rc = 0;
        if (k.is_array) {
            if (k.strs != nullptr) {
                arr_str_blob.clear();
                for (std::size_t j = 0; j < k.n_strs; j++) {
                    const char * s = k.strs[j];
                    const std::size_t len = std::strlen(s);
                    arr_str_blob.insert(arr_str_blob.end(), s, s + len);
                    arr_str_blob.push_back(0);
                }
                rc = s1_set_arr_str(s_i64(st.data(), 2), s_u8(pool.data(), (std::int64_t) pool.size()),
                                    s_i64(ko.data(), kv_cap), s_i64(kl.data(), kv_cap),
                                    s_i64(ty.data(), kv_cap), s_i64(ar.data(), kv_cap),
                                    s_i64(v0.data(), kv_cap), s_i64(v1.data(), kv_cap),
                                    s_i64(v2.data(), kv_cap), key,
                                    s_u8(arr_str_blob.data(), (std::int64_t) arr_str_blob.size()),
                                    static_cast<std::int64_t>(k.n_strs), s_i64(out_rc.data(), 1));
            } else {
                rc = s1_set_arr(s_i64(st.data(), 2), s_u8(pool.data(), (std::int64_t) pool.size()),
                                s_i64(ko.data(), kv_cap), s_i64(kl.data(), kv_cap),
                                s_i64(ty.data(), kv_cap), s_i64(ar.data(), kv_cap),
                                s_i64(v0.data(), kv_cap), s_i64(v1.data(), kv_cap),
                                s_i64(v2.data(), kv_cap), key, k.type,
                                s_u8(k.data, (std::int64_t) k.n_data),
                                static_cast<std::int64_t>(k.n_elems), s_i64(out_rc.data(), 1));
            }
        } else if (k.type == 8) {
            rc = s1_set_str(s_i64(st.data(), 2), s_u8(pool.data(), (std::int64_t) pool.size()),
                            s_i64(ko.data(), kv_cap), s_i64(kl.data(), kv_cap),
                            s_i64(ty.data(), kv_cap), s_i64(ar.data(), kv_cap),
                            s_i64(v0.data(), kv_cap), s_i64(v1.data(), kv_cap),
                            s_i64(v2.data(), kv_cap), key,
                            s_u8(k.data, static_cast<std::int64_t>(k.n_data)), s_i64(out_rc.data(), 1));
        } else {
            const std::size_t n = static_cast<std::size_t>(type_size(k.type));
            rc = s1_set_val(s_i64(st.data(), 2), s_u8(pool.data(), (std::int64_t) pool.size()),
                            s_i64(ko.data(), kv_cap), s_i64(kl.data(), kv_cap),
                            s_i64(ty.data(), kv_cap), s_i64(ar.data(), kv_cap),
                            s_i64(v0.data(), kv_cap), s_i64(v1.data(), kv_cap),
                            s_i64(v2.data(), kv_cap), key, k.type,
                            static_cast<std::int64_t>(raw_bits(k.data, n)),
                            s_i64(out_rc.data(), 1));
        }
        if (rc != 0) {
            return false;
        }
    }

    // ---- tensor descriptors ----------------------------------------------

    std::vector<std::int64_t> tst(4, 0);
    std::vector<std::int64_t> tne(std::max<std::size_t>(n_tensors, 1) * 4, 0);
    std::vector<std::int64_t> trec(std::max<std::size_t>(n_tensors, 1) * 8, 0);
    std::vector<std::uint8_t> tpool(t_pool_bytes + 1, 0);

    if (s3_init_tensors(s_i64(tst.data(), 4), static_cast<std::int64_t>(alignment)) != 0) {
        return false;
    }
    for (std::size_t i = 0; i < n_tensors; i++) {
        const tensor_view & t = ts[i];
        const std::int64_t idx = s3_add_tensor(
            s_i64(tst.data(), 4), s_u8(tpool.data(), (std::int64_t) tpool.size()),
            s_i64(tne.data(), t_n * 4), s_i64(trec.data(), t_n * 8),
            s_u8(t.name, static_cast<std::int64_t>(std::strlen(t.name))),
            t.ndims, s_i64(t.ne, 4), t.type, s_u8(t.data, (std::int64_t) t.n_data));
        if (idx < 0) {
            return false;
        }
    }

    // ---- bytes -----------------------------------------------------------

    std::vector<std::uint8_t> buf(cap, 0);
    const std::int64_t n = s3_write(
        s_i64(st.data(), 2), s_u8(pool.data(), (std::int64_t) pool.size()),
        s_i64(ko.data(), kv_cap), s_i64(kl.data(), kv_cap), s_i64(ty.data(), kv_cap),
        s_i64(ar.data(), kv_cap), s_i64(v0.data(), kv_cap), s_i64(v1.data(), kv_cap),
        s_i64(v2.data(), kv_cap), s_i64(tst.data(), 4),
        s_u8(tpool.data(), (std::int64_t) tpool.size()), s_i64(tne.data(), t_n * 4),
        s_i64(trec.data(), t_n * 8), s_u8(buf.data(), (std::int64_t) buf.size()),
        static_cast<std::int64_t>(version), only_meta ? 1 : 0);
    if (n < 0 || static_cast<std::size_t>(n) > buf.size()) {
        return false;
    }
    out.assign(buf.begin(), buf.begin() + n);

    g_calls[site]++;
    return true;
}

std::int64_t should_split(std::int32_t mode, std::int64_t i_tensor,
                          std::int64_t n_tensors, std::int64_t next_size,
                          std::int64_t n_split_tensors,
                          std::int64_t n_bytes_split) {
    arm_stats();
    const std::int64_t rc = gsp_should_split(mode, i_tensor, n_tensors, next_size,
                                             n_split_tensors, n_bytes_split);
    if (rc >= 0) {
        g_calls[ENTRY_SPLIT]++;
    }
    return rc;
}

std::int64_t split_layout(std::int64_t n_tensors, std::int32_t mode,
                          std::int64_t n_split_tensors, std::int64_t n_bytes_split,
                          bool no_tensor_first, const std::int64_t * nb,
                          std::int64_t * t0, std::int64_t * t1) {
    arm_stats();
    const std::int64_t cap = n_tensors + 2;
    const std::int64_t rc = gsp_split_layout(n_tensors, mode, n_split_tensors, n_bytes_split,
                                             no_tensor_first ? 1 : 0,
                                             s_i64(nb, n_tensors), s_i64(t0, cap),
                                             s_i64(t1, cap));
    if (rc >= 0) {
        g_calls[ENTRY_LAYOUT]++;
    }
    return rc;
}

}  // namespace fg_goose
