// Differential gate for the P4-12 slices.
//
// The oracle is the real native code, not a retyped copy:
//   - meta device / buffer-type getters run on synthetic ggml devices with a
//     controlled iface, so no GPU and no driver is needed;
//   - ggml_threadpool_params_default/match come from libggml-base;
//   - bmoe::validate comes from runtime/core/src/config.cpp;
//   - only two static helpers (cpumask_next/valid and the plan clamp) are
//     transcribed, and their transcription is marked below.
//
// Every case prints one line; the program exits non-zero on any mismatch.

#include <ggml.h>
#include <ggml-backend.h>
#include <ggml-backend-impl.h>

#include "runtime-gen.h"

#include <bmoe/config.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// ---------------- port helpers ----------------

static sl_u8 sl_of(const std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}

static sl_u8 sl_of(std::vector<uint8_t> & v) {
    return sl_u8{ v.data(), (int64_t) v.size() };
}

static sl_i64 sl_of(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static std::string port_name(const std::string & blob) {
    std::vector<uint8_t> out(256, 0);
    std::string b = blob;
    int64_t n = mb_name(sl_of(out), sl_of(b));
    if (n < 0 || n > (int64_t) out.size()) {
        return "<overflow>";
    }
    return std::string((const char *) out.data(), (size_t) n);
}

static std::string port_desc(const std::string & blob) {
    std::vector<uint8_t> out(256, 0);
    std::string b = blob;
    int64_t n = mb_description(sl_of(out), sl_of(b));
    if (n < 0 || n > (int64_t) out.size()) {
        return "<overflow>";
    }
    return std::string((const char *) out.data(), (size_t) n);
}

// ---------------- synthetic ggml devices ----------------

struct Fake {
    ggml_backend_device      dev{};
    ggml_backend_buffer_type buft{};
    std::string name;
    std::string desc;
    std::string buft_name;
    size_t free_b = 0;
    size_t total_b = 0;
    bool cap_async = true;
    bool cap_host_buffer = true;
    bool cap_bhptr = true;
    bool cap_events = true;
    bool cap_mmap = true;
    bool sup_op = true;
    size_t max_size = 0;
    size_t align = 16;
    size_t alloc = 0;
    bool is_host = false;
    bool has_host = false;
    ggml_backend_buffer_type_t host_buft = nullptr;
};

static Fake * fake_dev(ggml_backend_dev_t d) { return (Fake *) d->context; }
static Fake * fake_buft(ggml_backend_buffer_type_t b) { return (Fake *) b->context; }

static const char * fd_name(ggml_backend_dev_t d) { return fake_dev(d)->name.c_str(); }
static const char * fd_desc(ggml_backend_dev_t d) { return fake_dev(d)->desc.c_str(); }
static void fd_mem(ggml_backend_dev_t d, size_t * f, size_t * t) {
    *f = fake_dev(d)->free_b;
    *t = fake_dev(d)->total_b;
}
static enum ggml_backend_dev_type fd_type(ggml_backend_dev_t) { return GGML_BACKEND_DEVICE_TYPE_GPU; }
static void fd_props(ggml_backend_dev_t d, ggml_backend_dev_props * p) {
    Fake * f = fake_dev(d);
    p->name = f->name.c_str();
    p->description = f->desc.c_str();
    p->memory_free = f->free_b;
    p->memory_total = f->total_b;
    p->type = GGML_BACKEND_DEVICE_TYPE_GPU;
    p->device_id = nullptr;
    p->caps.async = f->cap_async;
    p->caps.host_buffer = f->cap_host_buffer;
    p->caps.buffer_from_host_ptr = f->cap_bhptr;
    p->caps.events = f->cap_events;
    p->caps.mmap_support = f->cap_mmap;
}
static ggml_backend_t fd_init(ggml_backend_dev_t, const char *) { return nullptr; }
static ggml_backend_buffer_type_t fd_buft(ggml_backend_dev_t d) { return &fake_dev(d)->buft; }
static ggml_backend_buffer_type_t fd_host_buft(ggml_backend_dev_t d) {
    Fake * f = fake_dev(d);
    return f->has_host ? f->host_buft : nullptr;
}
static ggml_backend_buffer_t fd_from_ptr(ggml_backend_dev_t, void *, size_t, size_t) { return nullptr; }
static bool fd_sup_op(ggml_backend_dev_t d, const ggml_tensor *) { return fake_dev(d)->sup_op; }
static bool fd_sup_buft(ggml_backend_dev_t, ggml_backend_buffer_type_t) { return false; }

static const char * fb_name(ggml_backend_buffer_type_t b) { return fake_buft(b)->buft_name.c_str(); }
static ggml_backend_buffer_t fb_alloc(ggml_backend_buffer_type_t, size_t) { return nullptr; }
static size_t fb_align(ggml_backend_buffer_type_t b) { return fake_buft(b)->align; }
static size_t fb_max(ggml_backend_buffer_type_t b) { return fake_buft(b)->max_size; }
static size_t fb_alloc_size(ggml_backend_buffer_type_t b, const ggml_tensor *) { return fake_buft(b)->alloc; }
static bool fb_is_host(ggml_backend_buffer_type_t b) { return fake_buft(b)->is_host; }

static ggml_backend_meta_split_state split_cb(const ggml_tensor *, void *) {
    ggml_backend_meta_split_state s{};
    s.axis = GGML_BACKEND_SPLIT_AXIS_NONE;
    return s;
}

// All fakes live for the whole run: the native meta registry keys on device
// pointers, so a recycled address would return a stale meta device.
static std::vector<std::unique_ptr<Fake>> g_fakes;

static Fake * make_dev(const std::string & name, const std::string & buft_name,
                       size_t free_b, size_t total_b, size_t align, size_t alloc) {
    auto f = std::make_unique<Fake>();
    f->name = name;
    f->desc = name + " description";
    f->buft_name = buft_name;
    f->free_b = free_b;
    f->total_b = total_b;
    f->align = align;
    f->alloc = alloc;

    f->dev.iface.get_name = fd_name;
    f->dev.iface.get_description = fd_desc;
    f->dev.iface.get_memory = fd_mem;
    f->dev.iface.get_type = fd_type;
    f->dev.iface.get_props = fd_props;
    f->dev.iface.init_backend = fd_init;
    f->dev.iface.get_buffer_type = fd_buft;
    f->dev.iface.get_host_buffer_type = fd_host_buft;
    f->dev.iface.buffer_from_host_ptr = fd_from_ptr;
    f->dev.iface.supports_op = fd_sup_op;
    f->dev.iface.supports_buft = fd_sup_buft;
    f->dev.context = f.get();

    f->buft.iface.get_name = fb_name;
    f->buft.iface.alloc_buffer = fb_alloc;
    f->buft.iface.get_alignment = fb_align;
    f->buft.iface.get_max_size = nullptr;
    f->buft.iface.get_alloc_size = fb_alloc_size;
    f->buft.iface.is_host = fb_is_host;
    f->buft.device = &f->dev;
    f->buft.context = f.get();

    g_fakes.push_back(std::move(f));
    return g_fakes.back().get();
}

// ---------------- case accounting ----------------

static int g_pass = 0;
static int g_fail = 0;

static void check(const std::string & name, bool ok, const std::string & nv, const std::string & pv) {
    if (ok) {
        g_pass++;
        std::printf("case %-42s PASS native=%s port=%s\n", name.c_str(), nv.c_str(), pv.c_str());
    } else {
        g_fail++;
        std::printf("case %-42s FAIL native=%s port=%s\n", name.c_str(), nv.c_str(), pv.c_str());
    }
}

static void check_s(const std::string & name, const std::string & nv, const std::string & pv) {
    check(name, nv == pv, nv, pv);
}

// ---------------- slice 1: meta device / buffer-type policy ----------------

static ggml_backend_dev_t meta_from(std::vector<Fake *> devs, void * ud = nullptr) {
    std::vector<ggml_backend_dev_t> d;
    for (Fake * f : devs) d.push_back(&f->dev);
    return ggml_backend_meta_device(d.data(), d.size(), split_cb, ud);
}

static void slice1() {
    // names and descriptions
    {
        Fake * a = make_dev("CPU", "CPU_Buft", 0, 0, 16, 0);
        ggml_backend_dev_t m = meta_from({ a });
        check_s("s1.name.one", ggml_backend_dev_name(m), port_name("CPU"));
        check_s("s1.desc.one", ggml_backend_dev_description(m), port_desc("CPU"));
    }
    {
        Fake * a = make_dev("CUDA0", "CUDA0_Buft", 0, 0, 16, 0);
        Fake * b = make_dev("CUDA1", "CUDA1_Buft", 0, 0, 16, 0);
        Fake * c = make_dev("CUDA2", "CUDA2_Buft", 0, 0, 16, 0);
        ggml_backend_dev_t m = meta_from({ a, b, c });
        check_s("s1.name.three", ggml_backend_dev_name(m), port_name("CUDA0;CUDA1;CUDA2"));
        check_s("s1.desc.two", ggml_backend_dev_description(meta_from({ a, b })),
                port_desc("CUDA0;CUDA1"));
    }
    // memory sums
    {
        Fake * a = make_dev("A", "BA", 7ull << 30, 8ull << 30, 16, 0);
        Fake * b = make_dev("B", "BB", 3ull << 30, 4ull << 30, 16, 0);
        ggml_backend_dev_t m = meta_from({ a, b });
        size_t f = 0, t = 0;
        ggml_backend_dev_memory(m, &f, &t);
        std::vector<int64_t> fa{ (int64_t) a->free_b, (int64_t) b->free_b };
        std::vector<int64_t> ta{ (int64_t) a->total_b, (int64_t) b->total_b };
        check("s1.memory.free", (int64_t) f == mb_free_sum(sl_of(fa)),
              std::to_string(f), std::to_string(mb_free_sum(sl_of(fa))));
        check("s1.memory.total", (int64_t) t == mb_total_sum(sl_of(ta)),
              std::to_string(t), std::to_string(mb_total_sum(sl_of(ta))));
    }
    // caps AND over three devices, two of them short one capability
    {
        Fake * a = make_dev("A", "BA", 0, 0, 16, 0);
        Fake * b = make_dev("B", "BB", 0, 0, 16, 0);
        Fake * c = make_dev("C", "BC", 0, 0, 16, 0);
        a->cap_events = false;
        b->cap_host_buffer = false;
        ggml_backend_dev_t m = meta_from({ a, b, c });
        ggml_backend_dev_props p{};
        ggml_backend_dev_get_props(m, &p);
        int64_t bits = 0;
        bits |= (p.caps.async ? 1 : 0);
        bits |= (p.caps.host_buffer ? 2 : 0);
        bits |= (p.caps.buffer_from_host_ptr ? 4 : 0);
        bits |= (p.caps.events ? 8 : 0);
        bits |= (p.caps.mmap_support ? 16 : 0);
        std::vector<int64_t> in{ 31 - 8, 31 - 2, 31 };
        check("s1.caps.and", bits == mb_caps_and(sl_of(in)),
              std::to_string(bits), std::to_string(mb_caps_and(sl_of(in))));
    }
    // device type and supports_op
    {
        Fake * a = make_dev("A", "BA", 0, 0, 16, 0);
        Fake * b = make_dev("B", "BB", 0, 0, 16, 0);
        Fake * c = make_dev("C", "BC", 0, 0, 16, 0);
        b->sup_op = false;
        ggml_backend_dev_t mixed = meta_from({ a, b, c });
        ggml_backend_dev_t all = meta_from({ a, c });

        ggml_init_params ip{ 4096, nullptr, false };
        ggml_context * ctx = ggml_init(ip);
        ggml_tensor * t = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 16);

        std::vector<int64_t> flags{ 1, 0, 1 };
        std::vector<int64_t> flags_all{ 1, 1 };
        check("s1.devtype.meta", (int64_t) ggml_backend_dev_type(mixed) == mb_device_type(),
              std::to_string((int) ggml_backend_dev_type(mixed)), std::to_string(mb_device_type()));
        check("s1.supports_op.mixed", ggml_backend_dev_supports_op(mixed, t) == (mb_supports_op(sl_of(flags)) != 0),
              std::to_string(ggml_backend_dev_supports_op(mixed, t)), std::to_string(mb_supports_op(sl_of(flags))));
        check("s1.supports_op.all_true", ggml_backend_dev_supports_op(all, t) == (mb_supports_op(sl_of(flags_all)) != 0),
              std::to_string(ggml_backend_dev_supports_op(all, t)), std::to_string(mb_supports_op(sl_of(flags_all))));
        ggml_free(ctx);
    }
    // buffer-type name, alignment, max size, alloc size, is_host
    {
        Fake * a = make_dev("A", "BA", 0, 0, 8, 128);
        Fake * b = make_dev("B", "BB", 0, 0, 16, 4096);
        Fake * c = make_dev("C", "BC", 0, 0, 32, 96);
        ggml_backend_dev_t m = meta_from({ a, b, c });
        ggml_backend_buffer_type_t bt = ggml_backend_dev_buffer_type(m);
        check_s("s1.buft.name", ggml_backend_buft_name(bt), port_name("BA;BB;BC"));

        std::vector<int64_t> al{ 8, 16, 32 };
        check("s1.buft.alignment", (int64_t) ggml_backend_buft_get_alignment(bt) == mb_alignment_max(sl_of(al)),
              std::to_string(ggml_backend_buft_get_alignment(bt)), std::to_string(mb_alignment_max(sl_of(al))));

        Fake * d = make_dev("D", "BD", 0, 0, 64, 0);
        Fake * e = make_dev("E", "BE", 0, 0, 8, 0);
        Fake * f = make_dev("F", "BF", 0, 0, 16, 0);
        ggml_backend_buffer_type_t bt2 = ggml_backend_dev_buffer_type(meta_from({ d, e, f }));
        std::vector<int64_t> al2{ 64, 8, 16 };
        check("s1.buft.alignment.order", (int64_t) ggml_backend_buft_get_alignment(bt2) == mb_alignment_max(sl_of(al2)),
              std::to_string(ggml_backend_buft_get_alignment(bt2)), std::to_string(mb_alignment_max(sl_of(al2))));

        a->max_size = 1ull << 40; a->buft.iface.get_max_size = fb_max;
        b->max_size = 1ull << 39; b->buft.iface.get_max_size = fb_max;
        ggml_backend_buffer_type_t bt3 = ggml_backend_dev_buffer_type(meta_from({ a, b }));
        std::vector<int64_t> ms{ (int64_t) (1ull << 40), (int64_t) (1ull << 39) };
        check("s1.buft.max_size.min", (int64_t) ggml_backend_buft_get_max_size(bt3) == mb_max_size_min(sl_of(ms)),
              std::to_string(ggml_backend_buft_get_max_size(bt3)), std::to_string(mb_max_size_min(sl_of(ms))));

        Fake * g = make_dev("G", "BG", 0, 0, 16, 0);
        Fake * h = make_dev("H", "BH", 0, 0, 16, 0);
        h->max_size = 1ull << 32; h->buft.iface.get_max_size = fb_max;
        ggml_backend_buffer_type_t bt4 = ggml_backend_dev_buffer_type(meta_from({ g, h }));
        std::vector<int64_t> ms2{ -1, (int64_t) (1ull << 32) };
        check("s1.buft.max_size.unlimited", (int64_t) ggml_backend_buft_get_max_size(bt4) == mb_max_size_min(sl_of(ms2)),
              std::to_string(ggml_backend_buft_get_max_size(bt4)), std::to_string(mb_max_size_min(sl_of(ms2))));

        ggml_init_params ip{ 4096, nullptr, false };
        ggml_context * ctx = ggml_init(ip);
        ggml_tensor * t = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 16); // 64 bytes
        std::vector<int64_t> as{ 128, 4096, 96 };
        check("s1.buft.alloc_size.max", (int64_t) ggml_backend_buft_get_alloc_size(bt, t) == mb_alloc_size_max(sl_of(as)),
              std::to_string(ggml_backend_buft_get_alloc_size(bt, t)), std::to_string(mb_alloc_size_max(sl_of(as))));
        ggml_free(ctx);

        Fake * i1 = make_dev("I", "BI", 0, 0, 16, 0);
        Fake * i2 = make_dev("J", "BJ", 0, 0, 16, 0);
        Fake * i3 = make_dev("K", "BK", 0, 0, 16, 0);
        i1->is_host = true; i2->is_host = true; i3->is_host = true;
        ggml_backend_buffer_type_t bt_all_host = ggml_backend_dev_buffer_type(meta_from({ i1, i2, i3 }));
        std::vector<int64_t> hs_all{ 1, 1, 1 };
        check("s1.buft.is_host.all", (int64_t) ggml_backend_buft_is_host(bt_all_host) == mb_is_host(sl_of(hs_all)),
              std::to_string(ggml_backend_buft_is_host(bt_all_host)), std::to_string(mb_is_host(sl_of(hs_all))));

        Fake * i4 = make_dev("L", "BL", 0, 0, 16, 0);
        Fake * i5 = make_dev("M", "BM", 0, 0, 16, 0);
        i4->is_host = false; i5->is_host = true;
        ggml_backend_buffer_type_t bt_mixed = ggml_backend_dev_buffer_type(meta_from({ i4, i5 }));
        std::vector<int64_t> hs_mixed{ 0, 1 };
        check("s1.buft.is_host.mixed", (int64_t) ggml_backend_buft_is_host(bt_mixed) == mb_is_host(sl_of(hs_mixed)),
              std::to_string(ggml_backend_buft_is_host(bt_mixed)), std::to_string(mb_is_host(sl_of(hs_mixed))));
    }
    // host buffer type resolution
    {
        Fake host{};
        host.buft_name = "HostPinned";
        host.buft.context = &host;
        host.buft.iface.get_name = fb_name;
        host.buft.iface.alloc_buffer = fb_alloc;
        host.buft.iface.get_alignment = fb_align;
        host.buft.iface.get_alloc_size = fb_alloc_size;
        host.buft.iface.is_host = fb_is_host;

        Fake other{};
        other.buft_name = "HostOther";
        other.buft.context = &other;
        other.buft.iface.get_name = fb_name;
        other.buft.iface.alloc_buffer = fb_alloc;
        other.buft.iface.get_alignment = fb_align;
        other.buft.iface.get_alloc_size = fb_alloc_size;
        other.buft.iface.is_host = fb_is_host;

        Fake * a = make_dev("A", "BA", 0, 0, 16, 0);
        Fake * b = make_dev("B", "BB", 0, 0, 16, 0);
        a->has_host = true; a->host_buft = &host.buft;
        b->has_host = true; b->host_buft = &host.buft;
        ggml_backend_dev_t m = meta_from({ a, b });
        std::vector<int64_t> ids{ 5, 5 };
        check("s1.host_buft.shared", (ggml_backend_dev_host_buffer_type(m) == &host.buft) == (mb_host_buft(sl_of(ids)) >= 0),
              ggml_backend_dev_host_buffer_type(m) ? "ptr" : "null", std::to_string(mb_host_buft(sl_of(ids))));

        Fake * c = make_dev("C", "BC", 0, 0, 16, 0);
        Fake * d = make_dev("D", "BD", 0, 0, 16, 0);
        c->has_host = true; c->host_buft = &host.buft;
        d->has_host = true; d->host_buft = &other.buft;
        ggml_backend_dev_t m2 = meta_from({ c, d });
        std::vector<int64_t> ids2{ 5, 6 };
        check("s1.host_buft.mismatch", (ggml_backend_dev_host_buffer_type(m2) == nullptr) == (mb_host_buft(sl_of(ids2)) < 0),
              ggml_backend_dev_host_buffer_type(m2) ? "ptr" : "null", std::to_string(mb_host_buft(sl_of(ids2))));

        Fake * e = make_dev("E", "BE", 0, 0, 16, 0);
        Fake * f = make_dev("F", "BF", 0, 0, 16, 0);
        e->has_host = true; e->host_buft = &host.buft;
        ggml_backend_dev_t m3 = meta_from({ e, f });
        std::vector<int64_t> ids3{ 5, -1 };
        check("s1.host_buft.missing", (ggml_backend_dev_host_buffer_type(m3) == nullptr) == (mb_host_buft(sl_of(ids3)) < 0),
              ggml_backend_dev_host_buffer_type(m3) ? "ptr" : "null", std::to_string(mb_host_buft(sl_of(ids3))));
    }
    // supports_buft identity: meta flag, length, then ordered device identity
    {
        Fake * a = make_dev("A", "BA", 0, 0, 16, 0);
        Fake * b = make_dev("B", "BB", 0, 0, 16, 0);
        Fake * c = make_dev("C", "BC", 0, 0, 16, 0);
        ggml_backend_dev_t m = meta_from({ a, b });
        ggml_backend_dev_t m_rev = meta_from({ b, a });
        ggml_backend_dev_t m_three = meta_from({ a, b, c });
        ggml_backend_buffer_type_t bt = ggml_backend_dev_buffer_type(m);
        ggml_backend_buffer_type_t bt_three = ggml_backend_dev_buffer_type(m_three);

        std::vector<int64_t> ab{ 1, 2 }, ab2{ 1, 2 }, ba{ 2, 1 }, abc{ 1, 2, 3 };
        check("s1.supports_buft.same", ggml_backend_dev_supports_buft(m, bt) == (mb_supports_buft(sl_of(ab), sl_of(ab2), 1, 1) != 0),
              std::to_string(ggml_backend_dev_supports_buft(m, bt)), std::to_string(mb_supports_buft(sl_of(ab), sl_of(ab2), 1, 1)));
        check("s1.supports_buft.reversed", ggml_backend_dev_supports_buft(m_rev, bt) == (mb_supports_buft(sl_of(ba), sl_of(ab), 1, 1) != 0),
              std::to_string(ggml_backend_dev_supports_buft(m_rev, bt)), std::to_string(mb_supports_buft(sl_of(ba), sl_of(ab), 1, 1)));
        check("s1.supports_buft.length", ggml_backend_dev_supports_buft(m, bt_three) == (mb_supports_buft(sl_of(ab), sl_of(abc), 1, 1) != 0),
              std::to_string(ggml_backend_dev_supports_buft(m, bt_three)), std::to_string(mb_supports_buft(sl_of(ab), sl_of(abc), 1, 1)));
        check("s1.supports_buft.non_meta", ggml_backend_dev_supports_buft(m, &a->buft) == (mb_supports_buft(sl_of(ab), sl_of(ab), 0, 0) != 0),
              std::to_string(ggml_backend_dev_supports_buft(m, &a->buft)), std::to_string(mb_supports_buft(sl_of(ab), sl_of(ab), 0, 0)));
    }
}

// ---------------- slice 2: registry identity ----------------

static void slice2() {
    Fake * a = make_dev("A", "BA", 0, 0, 16, 0);
    Fake * b = make_dev("B", "BB", 0, 0, 16, 0);
    static int ud1 = 1, ud2 = 2;

    ggml_backend_dev_t d0 = meta_from({ a, b }, &ud1);
    ggml_backend_dev_t d1 = meta_from({ a, b }, &ud1);
    ggml_backend_dev_t d2 = meta_from({ b, a }, &ud1);
    ggml_backend_dev_t d3 = meta_from({ a, b }, &ud2);

    std::vector<int64_t> ab{ 1, 2 }, ab2{ 1, 2 }, ba{ 2, 1 };
    check("s2.device.same_list", (d0 == d1) == (rg_key_eq(sl_of(ab), sl_of(ab2), 1, 1, 1, 1) != 0),
          d0 == d1 ? "same" : "diff", std::to_string(rg_key_eq(sl_of(ab), sl_of(ab2), 1, 1, 1, 1)));
    check("s2.device.reordered", (d0 == d2) == (rg_key_eq(sl_of(ab), sl_of(ba), 1, 1, 1, 1) != 0),
          d0 == d2 ? "same" : "diff", std::to_string(rg_key_eq(sl_of(ab), sl_of(ba), 1, 1, 1, 1)));
    check("s2.device.other_ud", (d0 == d3) == (rg_key_eq(sl_of(ab), sl_of(ab), 1, 1, 1, 2) != 0),
          d0 == d3 ? "same" : "diff", std::to_string(rg_key_eq(sl_of(ab), sl_of(ab), 1, 1, 1, 2)));
    check("s2.device.other_cb", (d0 == d3) == (rg_key_eq(sl_of(ab), sl_of(ab), 1, 2, 1, 1) != 0),
          d0 == d3 ? "same" : "diff", std::to_string(rg_key_eq(sl_of(ab), sl_of(ab), 1, 2, 1, 1)));

    int64_t slot0 = rg_memo_slot(0, -1);
    int64_t slot1 = rg_memo_slot(1, d0 == d1 ? 0 : -1);
    int64_t slot2 = rg_memo_slot(1, d0 == d2 ? 0 : -1);
    check("s2.buft.memo.same_device", (ggml_backend_dev_buffer_type(d0) == ggml_backend_dev_buffer_type(d1)) == (slot0 == slot1),
          ggml_backend_dev_buffer_type(d0) == ggml_backend_dev_buffer_type(d1) ? "same" : "diff",
          std::to_string(slot0) + "/" + std::to_string(slot1));
    check("s2.buft.memo.new_device", (ggml_backend_dev_buffer_type(d0) == ggml_backend_dev_buffer_type(d2)) == (slot0 == slot2),
          ggml_backend_dev_buffer_type(d0) == ggml_backend_dev_buffer_type(d2) ? "same" : "diff",
          std::to_string(slot0) + "/" + std::to_string(slot2));
    check("s2.buft.slot.per_device",
          rg_buft_slot(slot0) == rg_buft_slot(slot1) && rg_buft_slot(slot0) != rg_buft_slot(slot2),
          "distinct-by-device", "distinct-by-device");
    check("s2.buft.is_meta.true", ggml_backend_buft_is_meta(ggml_backend_dev_buffer_type(d0)) == true, "true", "true");
    check("s2.buft.is_meta.false", ggml_backend_buft_is_meta(&a->buft) == false, "false", "false");
}

// ---------------- slice 3: threading policy ----------------

// Transcription of ggml_thread_cpumask_next / is_valid and of the graph_plan +
// compute clamp, from ggml-cpu.c at the base commit. These helpers are static,
// so the fixture cannot call them; the bodies below are line equivalents.
static void n_cpumask_next(const bool * global_mask, bool * local_mask, bool strict, int32_t * iter) {
    if (!strict) {
        std::memcpy(local_mask, global_mask, 512);
        return;
    }
    std::memset(local_mask, 0, 512);
    int32_t base_idx = *iter;
    for (int32_t i = 0; i < 512; i++) {
        int32_t idx = base_idx + i;
        if (idx >= 512) {
            idx -= 512;
        }
        if (global_mask[idx]) {
            local_mask[idx] = 1;
            *iter = idx + 1;
            return;
        }
    }
}
static bool n_cpumask_valid(const bool * mask) {
    for (int i = 0; i < 512; i++) if (mask[i]) return true;
    return false;
}
static int n_plan_threads(int n_threads, int pool) {
    if (n_threads <= 0) n_threads = pool > 0 ? pool : 4;
    if (n_threads > pool) n_threads = pool;
    return n_threads;
}

static void slice3() {
    // params_default
    for (int n : { 1, 4, 16, 0, -1 }) {
        ggml_threadpool_params p = ggml_threadpool_params_default(n);
        std::vector<int64_t> out(5, 0);
        int64_t rc = th_params_default(n, sl_of(out));
        bool ok = rc == 5 && out[0] == p.n_threads && out[1] == (int64_t) p.prio &&
                  out[2] == (int64_t) p.poll && out[3] == (int64_t) p.strict_cpu &&
                  out[4] == (int64_t) p.paused;
        check("s3.params_default.n=" + std::to_string(n), ok,
              std::to_string(p.n_threads) + "," + std::to_string((int) p.prio) + "," +
              std::to_string(p.poll) + "," + std::to_string(p.strict_cpu) + "," + std::to_string(p.paused),
              std::to_string(out[0]) + "," + std::to_string(out[1]) + "," +
              std::to_string(out[2]) + "," + std::to_string(out[3]) + "," + std::to_string(out[4]));
    }
    // params_match: n_threads, prio, poll, strict_cpu, cpumask; paused ignored
    {
        auto mk = [](int n, int prio, int poll, bool strict, int bit) {
            ggml_threadpool_params p = ggml_threadpool_params_default(n);
            p.prio = (enum ggml_sched_priority) prio;
            p.poll = (uint32_t) poll;
            p.strict_cpu = strict;
            if (bit >= 0) p.cpumask[bit] = true;
            return p;
        };
        struct Pair { ggml_threadpool_params a, b; };
        std::vector<Pair> pairs;
        { auto a = mk(4, 0, 50, false, -1); auto b = a; pairs.push_back({ a, b }); }
        { auto a = mk(4, 0, 50, false, -1); auto b = mk(8, 0, 50, false, -1); pairs.push_back({ a, b }); }
        { auto a = mk(4, 0, 50, false, -1); auto b = mk(4, 0, 49, false, -1); pairs.push_back({ a, b }); }
        { auto a = mk(4, 0, 50, false, -1); auto b = mk(4, 0, 50, true, -1); pairs.push_back({ a, b }); }
        { auto a = mk(4, 0, 50, false, -1); auto b = mk(4, 0, 50, false, 3); pairs.push_back({ a, b }); }
        { auto a = mk(4, 0, 50, false, -1); auto b = mk(4, 0, 50, false, -1); b.paused = true; pairs.push_back({ a, b }); }

        int idx = 0;
        for (const Pair & pr : pairs) {
            bool native = ggml_threadpool_params_match(&pr.a, &pr.b);
            auto pack = [](const ggml_threadpool_params & p) {
                std::vector<int64_t> v{ p.n_threads, (int64_t) p.prio, (int64_t) p.poll, (int64_t) p.strict_cpu, 0 };
                for (int i = 0; i < 64; i++) if (p.cpumask[i]) v[4] |= (1ll << i);
                return v;
            };
            std::vector<int64_t> va = pack(pr.a), vb = pack(pr.b);
            bool port = th_params_match(sl_of(va), sl_of(vb)) != 0;
            check("s3.params_match.case" + std::to_string(idx), native == port,
                  native ? "match" : "differ", port ? "match" : "differ");
            idx++;
        }
    }
    // cpumask_next: strict round-robin with wrap, non-strict copy, empty mask
    {
        bool g[512] = { false };
        g[0] = true; g[2] = true; g[5] = true;
        std::vector<int64_t> gi(512, 0);
        gi[0] = 1; gi[2] = 1; gi[5] = 1;

        bool ok = true;
        int32_t iter = 0;
        int64_t piter = 0;
        std::string nseq, pseq;
        for (int step = 0; step < 7; step++) {
            bool local[512] = { false };
            std::vector<int64_t> pl(512, 0);
            n_cpumask_next(g, local, true, &iter);
            piter = th_cpumask_next(sl_of(gi), 1, piter, sl_of(pl));
            for (int i = 0; i < 512; i++) {
                if (local[i] != (pl[i] != 0)) ok = false;
                if (local[i]) nseq += std::to_string(i) + " ";
                if (pl[i]) pseq += std::to_string(i) + " ";
            }
            if (!ok) break;
        }
        check("s3.cpumask_next.strict_cycle", ok, nseq, pseq);

        int32_t it2 = 510;
        bool l2[512] = { false };
        std::vector<int64_t> pl2(512, 0);
        n_cpumask_next(g, l2, true, &it2);
        int64_t pit2 = th_cpumask_next(sl_of(gi), 1, 510, sl_of(pl2));
        bool same2 = pit2 == it2;
        for (int i = 0; i < 512; i++) if (l2[i] != (pl2[i] != 0)) same2 = false;
        check("s3.cpumask_next.wrap", same2, std::to_string(it2), std::to_string(pit2));

        // wrap must subtract 512, not reset to 0: the first set bit after the
        // end is at index 1, so a reset-to-0 mutation finds nothing
        bool g2[512] = { false };
        g2[1] = true; g2[3] = true;
        std::vector<int64_t> g2i(512, 0);
        g2i[1] = 1; g2i[3] = 1;
        int32_t it5 = 510;
        bool l5[512] = { false };
        std::vector<int64_t> pl5(512, 0);
        n_cpumask_next(g2, l5, true, &it5);
        int64_t pit5 = th_cpumask_next(sl_of(g2i), 1, 510, sl_of(pl5));
        bool same5 = pit5 == it5;
        for (int i = 0; i < 512; i++) if (l5[i] != (pl5[i] != 0)) same5 = false;
        check("s3.cpumask_next.wrap_skip", same5, std::to_string(it5), std::to_string(pit5));

        bool l3[512] = { false };
        std::vector<int64_t> pl3(512, 0);
        int32_t it3 = 7;
        n_cpumask_next(g, l3, false, &it3);
        int64_t pit3 = th_cpumask_next(sl_of(gi), 0, 7, sl_of(pl3));
        bool same3 = pit3 == it3;
        for (int i = 0; i < 512; i++) if (l3[i] != (pl3[i] != 0)) same3 = false;
        check("s3.cpumask_next.non_strict", same3, std::to_string(it3), std::to_string(pit3));

        bool empty[512] = { false };
        std::vector<int64_t> ei(512, 0);
        std::vector<int64_t> el(512, 0);
        int32_t it4 = 9;
        bool le[512] = { false };
        n_cpumask_next(empty, le, true, &it4);
        int64_t pit4 = th_cpumask_next(sl_of(ei), 1, 9, sl_of(el));
        bool same4 = pit4 == it4;
        for (int i = 0; i < 512; i++) if (le[i] != (el[i] != 0)) same4 = false;
        check("s3.cpumask_next.empty", same4, std::to_string(it4), std::to_string(pit4));

        check("s3.cpumask_valid.empty", n_cpumask_valid(empty) == (th_cpumask_valid(sl_of(ei)) != 0),
              std::to_string(n_cpumask_valid(empty)), std::to_string(th_cpumask_valid(sl_of(ei))));
        std::vector<int64_t> ni(512, 0); ni[5] = 1;
        check("s3.cpumask_valid.one", n_cpumask_valid(g) == (th_cpumask_valid(sl_of(ni)) != 0),
              std::to_string(n_cpumask_valid(g)), std::to_string(th_cpumask_valid(sl_of(ni))));
    }
    // planning clamp
    {
        int cases[][2] = { { 0, 8 }, { 4, 8 }, { 8, 4 }, { -3, 2 }, { 0, 0 }, { 16, 16 } };
        int idx = 0;
        for (auto & c : cases) {
            int native = n_plan_threads(c[0], c[1]);
            int64_t port = th_plan_threads(c[0], c[1]);
            check("s3.plan_threads.case" + std::to_string(idx), native == port,
                  std::to_string(native), std::to_string(port));
            idx++;
        }
    }
    // runtime config admission
    {
        for (int n : { -1, 0, 1, 4 }) {
            bmoe::RunConfig cfg;
            cfg.model_path = "m";
            cfg.n_predict = 1;
            cfg.n_threads = n;
            cfg.n_ctx = 512;
            bmoe::ValidationResult r = bmoe::validate(cfg);
            bool native = r.ok;
            bool port = rt_n_threads_ok(n) != 0;
            check("s3.config.n_threads=" + std::to_string(n), native == port,
                  native ? "ok" : r.error, port ? "ok" : "reject");
        }
        for (int v : { 0, 1, 4, 8, 9 }) {
            bmoe::RunConfig cfg;
            cfg.model_path = "m";
            cfg.n_predict = 1;
            cfg.n_threads = 4;
            cfg.n_ctx = 512;
            cfg.moe.enabled = true;
            cfg.moe.io_threads = v;
            bmoe::ValidationResult r = bmoe::validate(cfg);
            bool native = r.ok;
            bool port = rt_io_threads_ok(v) != 0;
            check("s3.config.io_threads=" + std::to_string(v), native == port,
                  native ? "ok" : r.error, port ? "ok" : "reject");
        }
    }
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    slice1();
    slice2();
    slice3();
    std::printf("summary pass=%d fail=%d total=%d\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail == 0 && g_pass >= 30 ? 0 : 1;
}
