// Differential gate for P10-08 S4 (the backend public header surface).
//
// Native arms:
//   - compile time: the five public structs are sized and every field offset is
//     taken from the real ggml-backend.h; the eleven callback typedefs are
//     classified by template deduction over their real declarations;
//   - build time: tests/extract_native.py parses the split-axis switch out of
//     ggml/src/ggml-backend-meta.cpp and resolves the enumerators from the
//     header;
//   - run time: ggml_backend_meta_split_axis_name is called through dlopen from
//     the reference libggml-base.so.
//
// Port arm: the bkh_ exports of source/backend-headers.goose.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <dlfcn.h>

#include "ggml-backend.h"

#include "bkh.h"
#include "native_abi.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-46s want=%lld got=%lld\n", what, (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-46s want=[%s] got=[%s]\n", what, want.c_str(), got.c_str());
    }
}

// ── the native constant / layout map ────────────────────────────────────────
static std::vector<std::pair<std::string, int64_t>> NAT;

static void nat_add(const char * name, int64_t v) { NAT.emplace_back(name, v); }

static void build_nat() {
#define BKH(id) nat_add(#id, (int64_t) (id));
#define BKH_SZ(t) nat_add("sizeof:" #t, (int64_t) sizeof(t));
#define BKH_OFF(t, f) nat_add("offsetof:" #t "." #f, (int64_t) offsetof(t, f));
#include "native_bkh.inc"
#undef BKH
#undef BKH_SZ
#undef BKH_OFF
}

// Compile-time freeze that does not depend on the map: a consumer built against
// a different layout links, then misreads every field.
static_assert(sizeof(ggml_backend_dev_caps) == 5, "dev_caps layout");
static_assert(sizeof(ggml_backend_dev_props) == 56, "dev_props layout");
static_assert(offsetof(ggml_backend_dev_props, caps) == 48, "dev_props caps");
static_assert(sizeof(ggml_backend_feature) == 16, "feature layout");
static_assert(sizeof(ggml_backend_meta_split_state) == 2128, "split_state layout");
static_assert(offsetof(ggml_backend_meta_split_state, n_segments) == 2120, "split_state n_segments");
static_assert(sizeof(struct ggml_backend_graph_copy) == 32, "graph_copy layout");
static_assert(16 * GGML_BACKEND_META_MAX_DEVICES == 256, "meta device ceiling");

// ── callback shapes ─────────────────────────────────────────────────────────
enum : int { K_VOID = 0, K_BOOL = 1, K_INT = 2, K_SIZE = 3, K_FP = 4, K_PTR = 5, K_STRUCT = 6 };

template <typename T> struct kind { static constexpr int v = K_STRUCT; };
template <> struct kind<void> { static constexpr int v = K_VOID; };
template <> struct kind<bool> { static constexpr int v = K_BOOL; };
template <> struct kind<int> { static constexpr int v = K_INT; };
template <> struct kind<int64_t> { static constexpr int v = K_INT; };
template <> struct kind<uint32_t> { static constexpr int v = K_INT; };
template <> struct kind<size_t> { static constexpr int v = K_SIZE; };
template <> struct kind<float> { static constexpr int v = K_FP; };
template <> struct kind<double> { static constexpr int v = K_FP; };
template <typename T> struct kind<T *> { static constexpr int v = K_PTR; };

struct shape {
    int64_t arity = 0;
    std::vector<int64_t> slots;  // return class, then parameter classes
};

template <typename R, typename... A> static shape shape_of(R (*)(A...)) {
    shape s;
    s.arity = (int64_t) sizeof...(A);
    s.slots.push_back(kind<R>::v);
    (s.slots.push_back(kind<A>::v), ...);
    return s;
}

struct cb_case {
    const char * name;
    shape s;
};

// ── port helpers ────────────────────────────────────────────────────────────
static std::string port_str(int64_t (*fn)(int64_t, sl_u8), int64_t key) {
    char buf[128];
    memset(buf, 0x5a, sizeof buf);
    int64_t n = fn(key, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf - 1});
    if (n < 0) {
        return "!";
    }
    return std::string(buf, (size_t) n);
}

int main() {
    goose_init(0, nullptr);
    build_nat();

    // 1. constants and layout, both directions.
    cases++;
    {
        std::vector<std::pair<std::string, int64_t>> port;
        const int64_t n = bkh_const_count();
        eq_int("const count", (int64_t) NAT.size(), n);
        for (int64_t i = 0; i < n; i++) {
            port.emplace_back(port_str(bkh_const_name, i), bkh_const_value(i));
        }
        for (auto & p : port) {
            cases++;
            bool found = false;
            for (auto & q : NAT) {
                if (q.first == p.first) {
                    found = true;
                    eq_int(("const " + p.first).c_str(), q.second, p.second);
                }
            }
            if (!found) {
                values++;
                fails++;
                printf("FAIL const %-40s no native identifier\n", p.first.c_str());
            }
        }
        for (auto & q : NAT) {
            bool found = false;
            for (auto & p : port) {
                found = found || p.first == q.first;
            }
            values++;
            if (!found) {
                fails++;
                printf("FAIL const %-40s missing from the port\n", q.first.c_str());
            }
        }
    }

    // 2. boundaries of the constant table.
    cases++;
    eq_int("const_name(-1)", -1, bkh_const_name(-1, sl_u8{(uint8_t *) nullptr, 0}));
    eq_int("const_name(count)", -1, bkh_const_name(bkh_const_count(), sl_u8{(uint8_t *) nullptr, 0}));
    eq_int("const_value(-1)", -1, bkh_const_value(-1));

    // 3. callback shapes.
    cases++;
    {
        std::vector<cb_case> nat = {
            {"ggml_backend_comm_init_t", shape_of((ggml_backend_comm_init_t) nullptr)},
            {"ggml_backend_comm_free_t", shape_of((ggml_backend_comm_free_t) nullptr)},
            {"ggml_backend_comm_allreduce_tensor_t",
             shape_of((ggml_backend_comm_allreduce_tensor_t) nullptr)},
            {"ggml_backend_split_buffer_type_t", shape_of((ggml_backend_split_buffer_type_t) nullptr)},
            {"ggml_backend_set_n_threads_t", shape_of((ggml_backend_set_n_threads_t) nullptr)},
            {"ggml_backend_dev_get_extra_bufts_t",
             shape_of((ggml_backend_dev_get_extra_bufts_t) nullptr)},
            {"ggml_backend_set_abort_callback_t",
             shape_of((ggml_backend_set_abort_callback_t) nullptr)},
            {"ggml_backend_get_features_t", shape_of((ggml_backend_get_features_t) nullptr)},
            {"ggml_backend_sched_eval_callback", shape_of((ggml_backend_sched_eval_callback) nullptr)},
            {"ggml_backend_meta_get_split_state_t",
             shape_of((ggml_backend_meta_get_split_state_t) nullptr)},
            {"ggml_backend_eval_callback", shape_of((ggml_backend_eval_callback) nullptr)},
        };
        eq_int("callback count", (int64_t) nat.size(), bkh_callback_count());
        for (int64_t i = 0; i < bkh_callback_count() && i < (int64_t) nat.size(); i++) {
            cases++;
            eq_str(("cb name " + std::to_string(i)).c_str(), nat[(size_t) i].name,
                   port_str(bkh_callback_name, i));
            int64_t got[8] = {0};
            int64_t n = bkh_callback_shape(i, sl_i64{got, 8});
            eq_int(("cb arity " + std::string(nat[(size_t) i].name)).c_str(),
                   nat[(size_t) i].s.arity, got[0]);
            eq_int(("cb slots " + std::string(nat[(size_t) i].name)).c_str(),
                   (int64_t) nat[(size_t) i].s.slots.size(), n - 1);
            for (size_t k = 0; k < nat[(size_t) i].s.slots.size() && (int64_t) k + 1 < n; k++) {
                eq_int(("cb slot " + std::string(nat[(size_t) i].name) + "." +
                        std::to_string(k)).c_str(),
                       nat[(size_t) i].s.slots[k], got[k + 1]);
            }
        }
        cases++;
        eq_int("callback_shape(-1)", -1, bkh_callback_shape(-1, sl_i64{(int64_t *) nullptr, 0}));
        eq_int("callback_shape(count)", -1,
               bkh_callback_shape(bkh_callback_count(), sl_i64{(int64_t *) nullptr, 0}));
    }

    // 4. the split-axis table, parsed from the native switch.
    cases++;
    {
        eq_int("axis count", NAT_AXIS_N, bkh_split_axis_count());
        for (int i = 0; i < NAT_AXIS_N; i++) {
            cases++;
            eq_int("axis value", NAT_AXIS[i].val, bkh_split_axis_value(i));
            eq_str("axis text", NAT_AXIS[i].text, port_str(bkh_split_axis_name, NAT_AXIS[i].val));
        }
    }

    // 5. the same texts through the reference shared object.
    cases++;
    {
        const std::string path = std::string(NAT_SO_DIR) + "/libggml-base.so.0";
        void * h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (h == nullptr) {
            printf("FAIL dlopen %s: %s\n", path.c_str(), dlerror());
            fails++;
        } else {
            auto fn = (const char * (*)(enum ggml_backend_meta_split_axis)) dlsym(
                h, "ggml_backend_meta_split_axis_name");
            if (fn == nullptr) {
                printf("FAIL dlsym ggml_backend_meta_split_axis_name\n");
                fails++;
            } else {
                for (int i = 0; i < NAT_AXIS_N; i++) {
                    eq_str("axis text (so)", fn((enum ggml_backend_meta_split_axis) NAT_AXIS[i].val),
                           port_str(bkh_split_axis_name, NAT_AXIS[i].val));
                }
            }
            dlclose(h);
        }
    }

    // 6. refusals: the native switch aborts, the port answers -1.
    cases++;
    for (int64_t bad : {-1, 4, 9, 12, 97, 100}) {
        eq_int("axis unknown", -1, bkh_split_axis_name(bad, sl_u8{(uint8_t *) nullptr, 0}));
    }
    eq_int("axis_value(-1)", -1, bkh_split_axis_value(-1));

    printf("FG651_BKH cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
