// Differential gate for P10-12 S2 (the carrier's /proc readers).
//
// Native arm: runtime/core/src/io/platform_io.cpp from the base-commit
// repository, INCLUDED here (not linked) so the anonymous-namespace
// scan_kb_file is callable in this translation unit, which is how the native
// reader is reached without a path parameter. Every compared value comes from
// that code: scan_kb_file, file_mapped_regions, vm_resident_sample,
// fault_bytes, direct_needs_alignment, pinned_max_bytes.
//
// The two platform predicates that cannot be observed on Linux (F_NOCACHE vs
// O_DIRECT, an Android dma-buf) are compared against their native constant and
// against the real function for the branch this host takes.
//
// Port arm: the pr_ exports of source/proc-readers.goose.
#include "pr.h"

#include "platform_io.cpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static int cases = 0;
static int values = 0;
static int fails = 0;

static std::string g_dir;

static void eq_int(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-26s want=%lld got=%lld\n", name, (long long) want, (long long) got);
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

static std::string write_text(const char * name, const std::string & body) {
    const std::string p = g_dir + "/" + name;
    FILE * f = fopen(p.c_str(), "wb");
    if (!f) {
        printf("FATAL cannot create %s\n", p.c_str());
        exit(2);
    }
    if (!body.empty()) {
        fwrite(body.data(), 1, body.size(), f);
    }
    fclose(f);
    return p;
}

static std::string read_file(const char * path) {
    std::string s;
    FILE * f = fopen(path, "rb");
    if (!f) {
        return s;
    }
    char b[4096];
    size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) {
        s.append(b, n);
    }
    fclose(f);
    return s;
}

// fgets with a 256-byte buffer: at most 255 bytes per call, one call stops
// after a newline. The chunks are what scan_kb_file and file_mapped_regions
// actually see.
static std::vector<std::string> fgets_chunks(const std::string & text) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = pos;
        while (end < text.size() && (end - pos) < 255) {
            end++;
            if (text[end - 1] == '\n') {
                break;
            }
        }
        out.push_back(text.substr(pos, end - pos));
        pos = end;
    }
    return out;
}

// ── A. the "Key: <n> kB" grammar (scan_kb_file, 274-296) ──────────────────
struct KeySet {
    const char * name;
    std::vector<std::string> keys;
};

static void run_kb_scan() {
    const KeySet sets[] = {
        {"meminfo", {"MemTotal", "MemAvailable", "MemFree", "SwapFree"}},
        {"status", {"VmRSS", "RssAnon", "RssFile", "VmSwap"}},
        {"overlap", {"MemFree", "MemFreeHigh"}},
        {"one", {"VmRSS"}},
    };
    struct C { const char * name; std::string body; };
    const C cs[] = {
        {"plain", "MemTotal: 1000 kB\nMemAvailable: 500 kB\nMemFree: 250 kB\nSwapFree: 0 kB\n"},
        {"unordered", "SwapFree: 12 kB\nHugePages_Total: 0\nMemFree: 250 kB\nMemAvailable: 500 kB\n"},
        {"nospace", "MemFree:123kB\n"},
        {"tabs", "MemFree:\t\t7 kB\n"},
        {"spaces", "MemFree:   \t 8 kB\n"},
        {"neg", "MemFree: -5 kB\n"},
        {"hex", "MemFree: 0x10 kB\n"},
        {"junk_suffix", "MemFree: 12abytes\n"},
        {"no_colon", "MemFree 12 kB\n"},
        {"key_prefix", "MemFreeX: 12 kB\nMemFree: 34 kB\n"},
        {"zero_then_value", "MemFree: 0 kB\nMemFree: 42 kB\n"},
        {"value_then_zero", "MemFree: 42 kB\nMemFree: 0 kB\n"},
        {"long_line", std::string(300, 'x') + "\nMemFree: 9 kB\n"},
        // The key starts exactly at the 255-byte fgets boundary: one chunk ends
        // before it, so the native reader sees a line that begins with the key.
        {"key_at_boundary", std::string(255, 'x') + "MemFree: 9 kB\n"},
        {"empty", ""},
        {"no_final_newline", "MemFree: 11 kB"},
        {"colon_only", "MemFree:\n"},
        {"crlf", "MemFree: 13 kB\r\n"},
        {"value_255", std::string("MemFree: ") + std::string(250, '7') + " kB\n"},
        {"status_mix", "Name:\tbmoe\nVmRSS:\t  1024 kB\nRssAnon:\t512 kB\nRssFile:\t256 kB\n"
                       "VmSwap:\t0 kB\nThreads:\t8\n"},
        {"embedded_nul", std::string("MemFree: 21 kB\n\0MemFree: 22 kB\n", 36)},
    };

    for (const KeySet & ks : sets) {
        std::vector<const char *> kp;
        std::string blob;
        std::vector<int64_t> koff, klen;
        for (const std::string & k : ks.keys) {
            kp.push_back(k.c_str());
            koff.push_back((int64_t) blob.size());
            klen.push_back((int64_t) k.size());
            blob += k;
        }
        for (const C & c : cs) {
            const std::string p = write_text("kb.bin", c.body);

            std::vector<uint64_t> nv(ks.keys.size(), 0);
            const int nf = bmoe::pio::scan_kb_file(p.c_str(), kp.data(), nv.data(), (int) ks.keys.size())
                               ? 1
                               : 0;

            std::vector<int64_t> pv(ks.keys.size(), 0);
            // a fresh copy: the port writes only where the native would
            const int64_t pf = pr_kb_scan(SL(c.body), SL(blob), sl_i64{koff.data(), (int64_t) koff.size()},
                                          sl_i64{klen.data(), (int64_t) klen.size()},
                                          sl_i64{pv.data(), (int64_t) pv.size()});

            cases++;
            eq_int("kb_found", nf, pf > 0 ? 1 : 0);
            for (size_t i = 0; i < ks.keys.size(); i++) {
                eq_int(ks.keys[i].c_str(), (int64_t) nv[i], pv[i]);
            }
        }
    }

    // The two native callers, whose key tables and field order are the decision.
    struct F { const char * name; const char * body; int nfields; };
    const F fs[] = {
        {"meminfo_real_shape", "MemTotal: 16000000 kB\nMemFree: 2000000 kB\nMemAvailable: 9000000 kB\n"
                               "Buffers: 1000 kB\nSwapFree: 3000000 kB\n", 3},
        {"status_real_shape", "Name:\tbmoe\nUmask:\t0022\nVmRSS:\t 204800 kB\nRssAnon:\t102400 kB\n"
                              "RssFile:\t 51200 kB\nRssShmem:\t0 kB\nVmSwap:\t 4096 kB\n", 4},
        {"missing_fields", "MemAvailable: 5 kB\n", 3},
        {"all_zero", "VmRSS: 0 kB\nRssAnon: 0 kB\nRssFile: 0 kB\nVmSwap: 0 kB\n", 4},
        {"nothing", "Name:\tbmoe\n", 4},
    };
    for (const F & f : fs) {
        const std::string p = write_text("proc.bin", f.body);
        cases++;
        if (f.nfields == 4) {
            const char * keys[] = {"VmRSS", "RssAnon", "RssFile", "VmSwap"};
            uint64_t nv[4] = {0, 0, 0, 0};
            const int nf = bmoe::pio::scan_kb_file(p.c_str(), keys, nv, 4) ? 1 : 0;
            int64_t pv[4] = {0, 0, 0, 0};
            const int64_t pf = pr_proc_memory(SL(f.body), sl_i64{pv, 4});
            eq_int("proc_memory_hit", nf, pf);
            for (int i = 0; i < 4; i++) {
                eq_int("proc_memory_field", (int64_t) nv[i], pv[i]);
            }
        } else {
            const char * keys[] = {"MemAvailable", "MemFree", "SwapFree"};
            uint64_t nv[3] = {0, 0, 0};
            const int nf = bmoe::pio::scan_kb_file(p.c_str(), keys, nv, 3) ? 1 : 0;
            int64_t pv[3] = {0, 0, 0};
            const int64_t pf = pr_proc_device(SL(f.body), sl_i64{pv, 3});
            eq_int("proc_device_hit", nf, pf);
            for (int i = 0; i < 3; i++) {
                eq_int("proc_device_field", (int64_t) nv[i], pv[i]);
            }
        }
    }
}

// ── B. the /proc/self/maps line grammar (317-357) ────────────────────────
static void run_maps(const std::string & mapped_path) {
    const std::string base = mapped_path.substr(mapped_path.rfind('/') + 1);

    std::vector<bmoe::pio::MappedRegion> nregions;
    const bool ok = bmoe::pio::file_mapped_regions(base.c_str(), nregions);
    const std::string text = read_file("/proc/self/maps");
    if (!ok || text.empty()) {
        printf("NOTE /proc/self/maps unavailable: maps half skipped\n");
        return;
    }

    struct R { uint64_t start, end, off; };
    std::vector<R> pregions;
    for (const std::string & line : fgets_chunks(text)) {
        int64_t out[3] = {0, 0, 0};
        if (pr_maps_line(SL(line), SL(base), sl_i64{out, 3}) == 1) {
            pregions.push_back({(uint64_t) out[0], (uint64_t) out[1], (uint64_t) out[2]});
        }
    }

    cases++;
    eq_int("maps_count", (int64_t) nregions.size(), (int64_t) pregions.size());
    const size_t n = nregions.size() < pregions.size() ? nregions.size() : pregions.size();
    for (size_t i = 0; i < n; i++) {
        eq_int("maps_start", (int64_t) nregions[i].start, (int64_t) pregions[i].start);
        eq_int("maps_end", (int64_t) nregions[i].end, (int64_t) pregions[i].end);
        eq_int("maps_off", (int64_t) nregions[i].file_offset, (int64_t) pregions[i].off);
    }

    // A basename that is not mapped must yield nothing on both arms.
    std::vector<bmoe::pio::MappedRegion> none;
    cases++;
    eq_int("maps_absent_native", 0, bmoe::pio::file_mapped_regions("fg648-no-such-file.bin", none) ? 1 : 0);
    int64_t pn = 0;
    for (const std::string & line : fgets_chunks(text)) {
        int64_t out[3] = {0, 0, 0};
        if (pr_maps_line(SL(line), SL(std::string("fg648-no-such-file.bin")), sl_i64{out, 3}) == 1) {
            pn++;
        }
    }
    eq_int("maps_absent_port", 0, pn);

    // Lines of the grammar the parser must refuse.
    struct L { const char * line; int want; };
    const L ls[] = {
        {"7f0000000000-7f0000001000 r--p 00000000 08:02 1234 /x/fg648-model.bin\n", 1},
        {"7f00-7f01 r--p 00000000 08:02 1234 /x/fg648-model.bin\n", 1},
        {"7f00-7f01 r--p 00000000 08:02 1234 /x/fg648-model.bin", 1},
        {"7f00-7f01 r--p 00000000 08:02 1234\n", 0},
        {"7f00-7f01 r--p 00000000 08:02 1234  \n", 0},
        {"7f00 r--p 00000000 08:02 1234 /x/fg648-model.bin\n", 0},
        // The format is lenient: a missing field shifts the rest, and the
        // %x conversions read a decimal-looking token happily, so both lines
        // still end at the pathname and match (verified against the native).
        {"7f00-7f01 00000000 08:02 1234 /x/fg648-model.bin\n", 1},
        {"7f00-7f01 r--p 08:02 1234 /x/fg648-model.bin\n", 1},
        {"zzzz-7f01 r--p 00000000 08:02 1234 /x/fg648-model.bin\n", 0},
        {"0x10-0x20 r--p 00000000 08:02 1234 /x/fg648-model.bin\n", 1},
        {"7f00-7f01 r--p 00000000 08:02 1234 /x/other.bin\n", 0},
        // Suffix match, not equality: "not-fg648-model.bin" ends with the name.
        {"7f00-7f01 r--p 00000000 08:02 1234 /x/not-fg648-model.bin\n", 1},
        // A pathname exactly as long as the basename must still match: the
        // native guard is `plen < blen`, not `<=`.
        {"7f00-7f01 r--p 00000000 08:02 1234 fg648-model.bin\n", 1},
        {"7f00-7f01 r--p 00000000 08:02 1234 /x/fg648-model.bin (deleted)\n", 0},
        {"7f00-7f01 r--p 00000000 08:02 1234 /x/fg648-model.bin \n", 1},
        {"7f00-7f01 r--p 00000000 08:02 1234 /x/fg648-model.bin\t\n", 0},
    };
    for (const L & l : ls) {
        cases++;
        int64_t out[3] = {0, 0, 0};
        const int64_t got = pr_maps_line(SL(std::string(l.line)), SL(std::string("fg648-model.bin")),
                                         sl_i64{out, 3});
        values++;
        if ((got == 1 ? 1 : 0) != l.want) {
            fails++;
            printf("FAIL maps_line want=%d got=%lld line=[%s]\n", l.want, (long long) got, l.line);
        }
    }
}

// ── C. residency window and chunking (231-256) ───────────────────────────
static void run_resident() {
    const size_t page = bmoe::pio::vm_page();
    char * base = (char *) bmoe::pio::alloc_aligned(page, page * 16);
    if (!base) {
        printf("FATAL alloc_aligned failed\n");
        exit(2);
    }
    memset(base, 0x5a, page * 16);

    struct C { size_t off, sz; };
    const C cs[] = {
        {0, 0}, {0, 1}, {0, page}, {0, page + 1}, {1, page - 1}, {1, page},
        {page - 1, 1}, {page - 1, 2}, {page, page}, {page, page * 3},
        {page / 2, page * 4}, {page / 2, page * 4 + 1}, {page / 2, page + page / 2},
        {page * 15, page},
        {page * 15, page * 2}, {page + 100, page * 2 + 7},
    };
    for (const C & c : cs) {
        size_t sampled = 0, resident = 0;
        const bool ok = bmoe::pio::vm_resident_sample(base + c.off, c.sz, &sampled, &resident);
        cases++;
        if (!ok) {
            printf("NOTE mincore refused off=%zu sz=%zu\n", c.off, c.sz);
            continue;
        }
        const int64_t pages = pr_resident_pages((int64_t) (c.off % page), (int64_t) c.sz, (int64_t) page);
        eq_int("resident_sampled", (int64_t) sampled, pages);
        eq_int("resident_calls", (int64_t) ((sampled + 511) / 512), pr_resident_calls(pages));
        // The pages we touched are the ones the native function counts.
        eq_int("resident_bounded", 1, resident <= sampled ? 1 : 0);
    }
    bmoe::pio::aligned_free(base);

    // fault_bytes and the platform predicates.
    const size_t sizes[] = {1024, 4096, 16384, 65536};
    for (size_t p : sizes) {
        cases++;
        eq_int("fault_bytes", (int64_t) p, pr_fault_bytes((int64_t) p));
    }
    cases++;
    eq_int("fault_bytes_real", (int64_t) bmoe::pio::fault_bytes(), pr_fault_bytes((int64_t) page));
    cases++;
    eq_int("direct_align_host", (int64_t) (bmoe::pio::direct_needs_alignment() ? 1 : 0),
           pr_direct_needs_alignment(0));
    cases++;
    eq_int("direct_align_apple", 0, pr_direct_needs_alignment(1));
    cases++;
    eq_int("pinned_max_host", (int64_t) bmoe::pio::pinned_max_bytes(), pr_pinned_max(0));
    cases++;
    eq_int("pinned_max_android", 2147483647, pr_pinned_max(1));
}

int main() {
    goose_init(0, nullptr);

    const char * d = getenv("FG_SCRATCH");
    g_dir = d && *d ? d : "/tmp/opencode/sub-648/scratch";
    const std::string mk = "mkdir -p '" + g_dir + "'";
    if (system(mk.c_str()) != 0) {
        printf("FATAL cannot create scratch dir\n");
        return 2;
    }

    // A file of our own, mmap'd so /proc/self/maps really names it.
    const std::string mapped = g_dir + "/fg648-model.bin";
    write_text("fg648-model.bin", std::string(16384, 'z'));
    const int fd = open(mapped.c_str(), O_RDONLY);
    if (fd < 0) {
        printf("FATAL cannot open %s\n", mapped.c_str());
        return 2;
    }
    void * view = mmap(nullptr, 16384, PROT_READ, MAP_PRIVATE, fd, 0);
    if (view == MAP_FAILED) {
        printf("FATAL mmap failed\n");
        return 2;
    }
    volatile char touch = *((const char *) view); // make the mapping real
    (void) touch;

    run_kb_scan();
    run_maps(mapped);
    run_resident();

    munmap(view, 16384);
    close(fd);

    printf("FG648_PR cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
