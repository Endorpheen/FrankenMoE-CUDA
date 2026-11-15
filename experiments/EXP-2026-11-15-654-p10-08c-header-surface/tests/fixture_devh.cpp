// Differential gate for P10-08 S7: the fourteen per-backend device headers.
//
// Native arms, all derived on every build:
//   - tests/extract_native.py lists every macro and every GGML_BACKEND_API
//     declaration with the header it lives in;
//   - this fixture expands each macro for real and classifies each declaration
//     with decltype, so a header that moves without the port moving is a
//     difference, not a silently stale table;
//   - the capability matrix is recomputed here from the native name list with
//     an independent scan and compared with the port's derived matrix.
//
// Port arm: the devh_ exports of source/device-headers.goose.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cuda.h"
#include "ggml-metal.h"
#include "ggml-vulkan.h"
#include "ggml-rpc.h"
#include "ggml-opencl.h"
#include "ggml-hexagon.h"
#include "ggml-sycl.h"
#include "ggml-cann.h"
#include "ggml-blas.h"
#include "ggml-et.h"
#include "ggml-virtgpu.h"
#include "ggml-zdnn.h"
#include "ggml-zendnn.h"
#include "ggml-webgpu.h"

#include "devh_gen.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        std::printf("FAIL %-58s want=%lld got=%lld\n", what, (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        std::printf("FAIL %-58s want=[%s] got=[%s]\n", what, want.c_str(), got.c_str());
    }
}

static int64_t call_str(int64_t (*fn)(int64_t, sl_u8), int64_t i, uint8_t * buf, int64_t cap) {
    const sl_u8 s = { buf, cap };
    return fn(i, s);
}

static std::string port_str(int64_t (*fn)(int64_t, sl_u8), int64_t i) {
    uint8_t buf[512];
    std::memset(buf, 0xAA, sizeof buf);
    const int64_t n = call_str(fn, i, buf, (int64_t) sizeof buf);
    if (n < 0) {
        return "<refused>";
    }
    return std::string(reinterpret_cast<char *>(buf), static_cast<size_t>(n));
}

static int64_t call_shape(int64_t (*fn)(int64_t, sl_i64), int64_t i, int64_t * out, int64_t cap) {
    sl_i64 s = { out, cap };
    return fn(i, s);
}

// ── kind codes, identical to the port's -------------------------------
enum : int { K_VOID = 0, K_BOOL = 1, K_INT = 2, K_SIZE = 3, K_FP = 4, K_PTR = 5, K_OTHER = 6 };

template <typename T> struct kind { static constexpr int v = K_OTHER; };
template <> struct kind<void> { static constexpr int v = K_VOID; };
template <> struct kind<bool> { static constexpr int v = K_BOOL; };
template <> struct kind<char> { static constexpr int v = K_INT; };
template <> struct kind<short> { static constexpr int v = K_INT; };
template <> struct kind<int> { static constexpr int v = K_INT; };
template <> struct kind<long> { static constexpr int v = K_INT; };
template <> struct kind<long long> { static constexpr int v = K_INT; };
template <> struct kind<size_t> { static constexpr int v = K_SIZE; };
template <> struct kind<float> { static constexpr int v = K_FP; };
template <> struct kind<double> { static constexpr int v = K_FP; };
template <typename T> struct kind<T *> { static constexpr int v = K_PTR; };
template <typename T> struct kind<const T *> { static constexpr int v = K_PTR; };

template <typename F> struct shape;
template <typename R, typename... A> struct shape<R (*)(A...)> {
    static constexpr int arity = sizeof...(A);
    static constexpr int ret = kind<R>::v;
    static constexpr int par(int i) {
        if (i >= static_cast<int>(sizeof...(A))) {
            return -1;
        }
        constexpr int v[sizeof...(A) + 1] = { kind<A>::v..., -1 };
        return v[i];
    }
};

// ── native tables ───────────────────────────────────────────────────────────
struct ConstRow {
    const char * name;
    int kind;
    long long num;
    const char * txt;
};

static const ConstRow NCONST[] = {
#define DEVH_CHECK(fn, hid)
#define DEVH_NUM(x) { #x, 0, (long long) (x), "" },
#define DEVH_STR(x) { #x, 1, 0, (x) },
#include "native_devh.inc"
#undef DEVH_NUM
#undef DEVH_STR
#undef DEVH_CHECK
};

struct FnRow {
    const char * name;
    int hdr;
    int arity;
    int ret;
    int par[5];
};

static const FnRow NFN[] = {
#define DEVH_NUM(x)
#define DEVH_STR(x)
#define DEVH_CHECK(fn, hid) { #fn, hid, shape<decltype(&fn)>::arity, shape<decltype(&fn)>::ret, \
    { shape<decltype(&fn)>::par(0), shape<decltype(&fn)>::par(1), shape<decltype(&fn)>::par(2), \
      shape<decltype(&fn)>::par(3), shape<decltype(&fn)>::par(4) } },
#include "native_devh.inc"
#undef DEVH_CHECK
#undef DEVH_NUM
#undef DEVH_STR
};

static const char * const NHDR_NAME[] = {
#define DEVH_HEADER(name) name,
#include "native_devh_headers.inc"
#undef DEVH_HEADER
};

struct FnHdrRow {
    const char * name;
    int hdr;
};
static const FnHdrRow NFN_HDR[] = {
    {"", 0},
#define DEVH_HDR(name, hid) { name, hid },
#include "native_devh_hdr.inc"
#undef DEVH_HDR
};

struct MacroRow {
    const char * name;
    int hdr;
};
static const MacroRow NMACRO[] = {
    {"", 0},
#define DEVH_MACRO_HDR(name, hid) { name, hid },
#include "native_devh_macro.inc"
#undef DEVH_MACRO_HDR
};

static int native_hdr_of(const std::string & name) {
    for (const auto & r : NFN_HDR) {
        if (name == r.name) {
            return r.hdr;
        }
    }
    return -2;
}

// Independent capability scan: std::string operations, not the port's suffixes.
static bool has(const std::string & s, const std::string & t) {
    return s.find(t) != std::string::npos;
}
static bool ends(const std::string & s, const std::string & t) {
    return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

static int native_cap(const std::string & s, int c) {
    switch (c) {
        case 0:  return !has(s, "_comm_") && ends(s, "_init");
        case 1:  return ends(s, "_reg");
        case 2:  return has(s, "_is_");
        case 3:  return ends(s, "_buffer_type") && !ends(s, "_host_buffer_type") && !ends(s, "_split_buffer_type");
        case 4:  return ends(s, "_host_buffer_type");
        case 5:  return ends(s, "_get_device_count");
        case 6:  return ends(s, "_get_device_description");
        case 7:  return ends(s, "_get_device_memory");
        case 8:  return ends(s, "_split_buffer_type");
        case 9:  return ends(s, "_set_n_threads");
        case 10: return ends(s, "_guid");
        case 11: return has(s, "_comm_");
        default: return -1;
    }
}

int main() {
    goose_init(0, nullptr);
    // ── header names ────────────────────────────────────────────────────────
    eq_int("devh_header_count", 14, devh_header_count());
    cases++;
    for (int h = 0; h < devh_header_count(); h++) {
        cases++;
        char tag[64];
        std::snprintf(tag, sizeof tag, "header[%d]", h);
        eq_str(tag, NHDR_NAME[h], port_str(devh_header_name, h));
    }
    eq_int("header name -1 refused", -1, call_str(devh_header_name, -1, nullptr, 0));
    eq_int("header name 14 refused", -1, call_str(devh_header_name, 14, nullptr, 0));

    // ── macros: names, kinds, values ────────────────────────────────────────
    eq_int("devh_const_count", (int64_t) (sizeof(NCONST) / sizeof(NCONST[0])), devh_const_count());
    cases++;
    for (const auto & c : NCONST) {
        cases++;
        int idx = -1;
        for (int i = 0; i < devh_const_count(); i++) {
            if (port_str(devh_const_name, i) == c.name) {
                idx = i;
                break;
            }
        }
        values++;
        if (idx < 0) {
            fails++;
            std::printf("FAIL macro %-44s missing from port\n", c.name);
            continue;
        }
        eq_int((std::string("macro kind ") + c.name).c_str(), c.kind, devh_const_kind(idx));
        if (c.kind == 0) {
            eq_int((std::string("macro value ") + c.name).c_str(), c.num, devh_const_num(idx));
            eq_int((std::string("macro text refusal ") + c.name).c_str(), -1, call_str(devh_const_text, idx, nullptr, 0));
        } else {
            eq_str((std::string("macro text ") + c.name).c_str(), c.txt, port_str(devh_const_text, idx));
            eq_int((std::string("macro num refusal ") + c.name).c_str(), -1, devh_const_num(idx));
        }
    }
    eq_int("const name -1 refused", -1, call_str(devh_const_name, -1, nullptr, 0));
    eq_int("const kind 14 refused", -1, devh_const_kind(14));

    // ── declarations: name, header, shape ──────────────────────────────────
    eq_int("devh_fn_count", (int64_t) (sizeof(NFN) / sizeof(NFN[0])), devh_fn_count());
    cases++;
    std::vector<std::string> native_names;
    for (const auto & f : NFN) {
        native_names.push_back(f.name);
    }
    std::sort(native_names.begin(), native_names.end());
    std::vector<std::string> port_names;
    for (int i = 0; i < devh_fn_count(); i++) {
        port_names.push_back(port_str(devh_fn_name, i));
    }
    values++;
    if (native_names != port_names) {
        fails++;
        std::printf("FAIL declaration name set differs (native %zu, port %zu)\n",
                    native_names.size(), port_names.size());
        for (size_t i = 0; i < std::min(native_names.size(), port_names.size()); i++) {
            if (native_names[i] != port_names[i]) {
                std::printf("  first gap at %zu: native=[%s] port=[%s]\n", i,
                            native_names[i].c_str(), port_names[i].c_str());
                break;
            }
        }
    }

    for (const auto & f : NFN) {
        cases++;
        int idx = -1;
        for (int i = 0; i < devh_fn_count(); i++) {
            if (port_names[i] == f.name) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            continue;  // already reported by the set difference
        }
        values++;
        const int64_t ph = devh_fn_header(idx);
        if (ph != f.hdr) {
            fails++;
            std::printf("FAIL header of %-40s want=%d got=%lld\n", f.name, f.hdr, (long long) ph);
        }
        int64_t got[8];
        const int64_t n = call_shape(devh_fn_shape, idx, got, 8);
        values++;
        if (n != 2 + f.arity) {
            fails++;
            std::printf("FAIL shape length of %-36s want=%d got=%lld\n", f.name, 2 + f.arity, (long long) n);
            continue;
        }
        if (got[0] != f.arity || got[1] != f.ret) {
            fails++;
            std::printf("FAIL shape of %-44s arity/ret want=%d/%d got=%lld/%lld\n",
                        f.name, f.arity, f.ret, (long long) got[0], (long long) got[1]);
        }
        for (int j = 0; j < f.arity; j++) {
            values++;
            if (got[2 + j] != f.par[j]) {
                fails++;
                std::printf("FAIL param %d of %-42s want=%d got=%lld\n",
                            j, f.name, f.par[j], (long long) got[2 + j]);
            }
        }
        values++;
        if (native_hdr_of(f.name) != f.hdr) {
            fails++;
            std::printf("FAIL native header map inconsistent for %s\n", f.name);
        }
    }
    eq_int("shape 84 refused", -1, call_shape(devh_fn_shape, 84, nullptr, 0));
    eq_int("fn name -1 refused", -1, call_str(devh_fn_name, -1, nullptr, 0));

    // ── the capability matrix ──────────────────────────────────────────────
    eq_int("devh_cap_count", 12, devh_cap_count());
    cases++;
    for (int c = 0; c < devh_cap_count(); c++) {
        cases++;
        const std::string cname = port_str(devh_cap_name, c);
        int any = 0;
        for (int h = 0; h < devh_header_count(); h++) {
            int want = 0;
            for (const auto & f : NFN) {
                if (f.hdr == h && native_cap(f.name, c) == 1) {
                    want = 1;
                }
            }
            any += want;
            char tag[96];
            std::snprintf(tag, sizeof tag, "cap %s on header %d", cname.c_str(), h);
            eq_int(tag, want, devh_cap(h, c));
        }
        values++;
        if (any == 0) {
            fails++;
            std::printf("FAIL capability %s is empty on every header\n", cname.c_str());
        }
    }
    eq_int("cap header -1 refused", -1, devh_cap(-1, 0));
    eq_int("cap 14 refused", -1, devh_cap(14, 0));
    eq_int("cap index 12 refused", -1, devh_cap(0, 12));

    // ── device ceilings ────────────────────────────────────────────────────
    cases++;
    for (int h = 0; h < 14; h++) {
        cases++;
        long long want = -1;
        for (const auto & m : NMACRO) {
            if (m.hdr != h) {
                continue;
            }
            const std::string mn = m.name;
            if (mn.size() > 12 && mn.compare(mn.size() - 12, 12, "_MAX_DEVICES") == 0) {
                for (const auto & c : NCONST) {
                    if (mn == c.name) {
                        want = c.num;
                    }
                }
            }
        }
        char tag[64];
        std::snprintf(tag, sizeof tag, "max devices on header %d", h);
        eq_int(tag, want, devh_max_devices(h));
    }
    eq_int("max devices -1 refused", -1, devh_max_devices(-1));

    std::printf("FG659_DEVH cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
