// Differential gate for P10-12 S1 (the carrier's positioned file reader).
//
// Native arm: the real runtime/core/src/io/file_reader.cpp, compiled from the
// base-commit repository and linked into this binary. Every compared number is
// produced by the native class itself: FileReader::read()'s return value,
// direct(), lanes(). Two decisions have no callable observer (the open-time
// verify offset and the bounce test are internal), and those are compared with
// a transcription of the two native lines, cited at each site.
//
// Port arm: the fr_ exports of source/file_reader_windows.goose.
//
// No model, no GPU, no llama library: the reader is plain POSIX I/O over a
// temporary file created by the gate.
#include "fr.h"

#include "file_reader.h"
#include "platform_io.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;
static int skipped = 0;

static void eq_int(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-26s want=%lld got=%lld\n", name, (long long) want, (long long) got);
    }
}

// ── the scratch file ───────────────────────────────────────────────────────
// A file whose byte at offset i is (i * 37 + 11) & 0xff, so a misaligned read
// shows up as a content mismatch as well as a length mismatch.
static std::string g_dir;

static std::string make_file(const char * name, size_t size) {
    const std::string p = g_dir + "/" + name;
    FILE * f = fopen(p.c_str(), "wb");
    if (!f) {
        printf("FATAL cannot create %s\n", p.c_str());
        exit(2);
    }
    std::vector<unsigned char> b(size);
    for (size_t i = 0; i < size; ++i) {
        b[i] = (unsigned char) ((i * 37 + 11) & 0xff);
    }
    if (size && fwrite(b.data(), 1, size, f) != size) {
        printf("FATAL short write %s\n", p.c_str());
        exit(2);
    }
    fclose(f);
    return p;
}

static unsigned char byte_at(uint64_t i) {
    return (unsigned char) ((i * 37 + 11) & 0xff);
}

// ── S1: plain (non-alignment-constrained) reads ────────────────────────────
static void run_plain(const std::string & path, int64_t fsize) {
    bmoe::FileReader r;
    if (!r.open(path, 1, false, 4096, 4096)) {
        printf("FATAL open plain failed\n");
        exit(2);
    }
    eq_int("lanes(1)", 1, (int64_t) r.lanes());

    struct C { int64_t off, nbytes; };
    const C cs[] = {
        {0, 0}, {0, 1}, {0, 100}, {0, 4096}, {1, 1}, {1, 100}, {1, 4095},
        {4095, 1}, {4095, 2}, {4096, 1}, {4096, 4096}, {5000, 10},
        {fsize - 1, 1}, {fsize - 1, 2}, {fsize - 100, 100}, {fsize - 100, 101},
        {fsize, 1}, {fsize, 100}, {fsize + 1, 1},
    };
    std::vector<unsigned char> dst(8192, 0);
    for (const C & c : cs) {
        memset(dst.data(), 0x5a, dst.size());
        const int64_t got = r.read(0, dst.data(), (uint64_t) c.off, (uint64_t) c.nbytes);
        cases++;
        eq_int("plain_len", fr_plain_len(c.off, c.nbytes, fsize), got);
        // The bytes the reader moved must be the file's bytes at that offset.
        if (got > 0) {
            for (int64_t i = 0; i < got; ++i) {
                if (dst[(size_t) i] != byte_at((uint64_t) (c.off + i))) {
                    values++;
                    fails++;
                    printf("FAIL plain_bytes off=%lld +%lld\n", (long long) c.off, (long long) i);
                    break;
                }
            }
        }
    }
    r.close();
}

// ── S1: aligned (O_DIRECT / NO_BUFFERING) reads ────────────────────────────
// The window a direct read returns is the reader's own arithmetic, so it can be
// compared without reimplementing anything. Only the storage decides whether
// this half runs: tmpfs refuses O_DIRECT and the reader silently downgrades.
static bool has_direct(const std::string & path) {
    bmoe::FileReader r;
    if (!r.open(path, 1, true, 4096, 8192)) {
        return false;
    }
    const bool d = r.direct();
    r.close();
    return d;
}

static void run_aligned(const std::string & path, int64_t fsize, int64_t align0) {
    bmoe::FileReader r;
    if (!r.open(path, 1, true, (size_t) align0, 8192)) {
        printf("FATAL open direct failed\n");
        exit(2);
    }
    if (!r.direct()) {
        skipped++;
        r.close();
        return;
    }
    const int64_t align = align0 ? align0 : 4096;
    struct C { int64_t off, nbytes; };
    const C cs[] = {
        {0, 1}, {0, 100}, {0, 4096}, {1, 1}, {1, 100}, {1, 4095},
        {100, 100}, {4095, 1}, {4096, 1}, {4096, 4096}, {4097, 3},
        {8192, 100}, {8191, 2}, {fsize - 1, 1}, {fsize - 100, 100},
        {fsize - 100, 101}, {fsize - 4096, 4096}, {fsize - 4097, 4097},
    };
    std::vector<unsigned char> dst(16384, 0);
    for (const C & c : cs) {
        memset(dst.data(), 0x5a, dst.size());
        const int64_t got = r.read(0, dst.data(), (uint64_t) c.off, (uint64_t) c.nbytes);
        int64_t win[7] = {0, 0, 0, 0, 0, 0, 0};
        cases++;
        const int64_t pw = fr_aligned_win(c.off, c.nbytes, align0, fsize, sl_i64{win, 7});
        eq_int("aligned_window", pw, got);
        eq_int("aligned_read_len", fr_read_len(c.off, c.nbytes, align0, fsize, 1), got);
        eq_int("aligned_bounce", fr_bounce_grow(win[2], 8192),
               win[2] > 8192 ? 1 : 0); // read() 120-132: bounce_sz_ < len
        eq_int("aligned_a0_mod", 0, win[0] % align);
        eq_int("aligned_len_ge_req", 1, win[2] >= c.nbytes ? 1 : 0);
        // The bulk/tail split is only observable through its invariants: the bulk
        // end is aligned (read() 134-135) and the tail is shorter than a block
        // (it is the part O_DIRECT cannot read, so it goes through the buffered fd).
        eq_int("aligned_bulk_mod", 0, win[4] % align);
        eq_int("aligned_tail_lt_align", 1, win[6] < align ? 1 : 0);
        eq_int("aligned_window_split", win[5], (win[3] - win[0]));
        // The interior bytes are the file's, whatever the window was.
        for (int64_t i = 0; i < c.nbytes; ++i) {
            if (dst[(size_t) i] != byte_at((uint64_t) (c.off + i))) {
                values++;
                fails++;
                printf("FAIL aligned_bytes off=%lld +%lld\n", (long long) c.off, (long long) i);
                break;
            }
        }
    }
    r.close();
}

int main() {
    goose_init(0, nullptr);

    const char * d = getenv("FG_SCRATCH");
    g_dir = d && *d ? d : "/tmp/opencode/sub-648-scratch";
    const std::string mk = "mkdir -p '" + g_dir + "'";
    if (system(mk.c_str()) != 0) {
        printf("FATAL cannot create scratch dir\n");
        return 2;
    }

    // ── the alignment default and the lane clamp (open(), 33-35) ──────────
    {
        const std::string p = make_file("size-10000.bin", 10000);
        // align 0 means 4096: the reader opens and reports what it uses, and a
        // zero-lane request still yields one usable lane.
        bmoe::FileReader a;
        if (!a.open(p, 0, false, 0, 4096)) {
            printf("FATAL open lanes=0 align=0 failed\n");
            return 2;
        }
        cases++;
        eq_int("lanes_zero", fr_lanes(0), (int64_t) a.lanes());
        eq_int("align_zero", fr_align(0), 4096);
        a.close();
    }

    // ── the open-time O_DIRECT verify offset (open(), 60-63) ──────────────
    {
        struct V { int64_t fsize, align, aligned; } vs[] = {
            {10000, 4096, 1}, {10000, 4096, 0}, {4096, 4096, 1}, {4095, 4096, 1},
            {8192, 4096, 1}, {1, 4096, 1}, {0, 4096, 1}, {16384, 4096, 1},
            {12289, 4096, 1}, {10000, 512, 1}, {10000, 0, 1}, {10000, 4096, 2},
        };
        for (const V & v : vs) {
            // Native lines 60-63, transcribed: the branch that guards the block.
            const int64_t al = v.align ? v.align : 4096;
            int64_t want = -1;
            if (v.aligned != 0 && v.fsize >= al) {
                want = (v.fsize / 2) & ~(al - 1);
                if (want + al > v.fsize) {
                    want = 0;
                }
            }
            cases++;
            eq_int("verify_off", want, fr_verify_off(v.fsize, v.align, v.aligned));
        }
    }

    // ── plain reads over a real file ─────────────────────────────────────
    {
        const std::string p = make_file("plain-10000.bin", 10000);
        run_plain(p, 10000);
        const std::string q = make_file("plain-12288.bin", 12288);
        run_plain(q, 12288);
    }

    // ── aligned reads, when the scratch storage supports O_DIRECT ────────
    {
        const std::string p = make_file("direct-10000.bin", 10000);
        const std::string q = make_file("direct-12288.bin", 12288);
        if (has_direct(p)) {
            run_aligned(p, 10000, 4096);
            run_aligned(p, 10000, 0);   // the default: align 0 behaves as 4096
            run_aligned(q, 12288, 4096);
        } else {
            printf("NOTE O_DIRECT unsupported at %s: aligned half skipped\n", g_dir.c_str());
            skipped += 3;
        }
    }

    printf("FG648_FR cases=%d values=%d failures=%d skipped=%d\n", cases, values, fails, skipped);
    return fails == 0 ? 0 : 1;
}
