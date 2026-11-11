// Differential gate for the Goose recurrent state payload plan.
//
// Oracle: a transcription of state_write_data / state_read_data that produces the same transfer
// records. The byte format is abstracted: the stream is an array of int64 slots, one per field.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg297dh_run(int64_t packet);

static int64_t g_packet[5][16];

extern "C" int64_t fg297k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg297k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg297k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg297k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg297k_fail(int64_t c) { std::fprintf(stderr, "FG297K validation code %lld\n", (long long)c); std::abort(); }

struct Rec { int64_t kind, id, off, size, val; };

struct Model {
    int n_layer = 0;
    int64_t n_embd_s = 0, size = 0, cell_count = 0, head = 0, s_trans = 0;
    std::vector<int64_t> r_present, r_type, r_row, s_present, s_type, s_row, p_present, p_row;
    std::vector<std::pair<int64_t,int64_t>> ranges;
};

static void put_u32(std::vector<Rec> & out, int64_t v) { out.push_back({0, 0, 0, 0, v}); }
static void put_u64(std::vector<Rec> & out, int64_t v) { out.push_back({1, 0, 0, 0, v}); }
static void put_wr(std::vector<Rec> & out, int64_t id, int64_t off, int64_t size) { out.push_back({2, id, off, size, 0}); }
static void put_rd(std::vector<Rec> & out, int64_t id, int64_t off, int64_t size) { out.push_back({3, id, off, size, 0}); }

static std::vector<Rec> ref_write(const Model & M) {
    std::vector<Rec> out;
    put_u32(out, 0);
    put_u32(out, M.n_layer);
    for (int il = 0; il < M.n_layer; ++il) {
        if (M.r_present[il]) {
            put_u32(out, M.r_type[il]);
            put_u64(out, M.r_row[il]);
            for (auto & r : M.ranges) put_wr(out, il*4, r.first*M.r_row[il], (r.second - r.first)*M.r_row[il]);
        }
        if (M.p_present[il]) {
            put_u64(out, M.p_row[il]);
            for (auto & r : M.ranges) put_wr(out, il*4+2, r.first*M.p_row[il], (r.second - r.first)*M.p_row[il]);
        }
    }
    if (!M.s_trans) {
        for (int il = 0; il < M.n_layer; ++il) {
            if (M.s_present[il]) {
                put_u32(out, M.s_type[il]);
                put_u64(out, M.s_row[il]);
                for (auto & r : M.ranges) put_wr(out, il*4+1, r.first*M.s_row[il], (r.second - r.first)*M.s_row[il]);
            }
        }
    } else {
        const int64_t el = 4;   // element size, abstracted
        for (int il = 0; il < M.n_layer; ++il) {
            if (M.s_present[il]) {
                put_u32(out, M.s_type[il]);
                put_u32(out, el);
                put_u32(out, M.n_embd_s);
                for (int64_t j = 0; j < M.n_embd_s; ++j)
                    for (auto & r : M.ranges)
                        put_wr(out, il*4+1, (r.first + j*M.size)*el, (r.second - r.first)*el);
            }
        }
    }
    return out;
}

static bool ref_read(const Model & M, const std::vector<int64_t> & stream, size_t & cur, std::vector<Rec> & out) {
    out.clear();
    // The module walks the stream in bytes; the fixture stream is one int64 per field.
    auto take = [&]() { if (cur / 8 >= stream.size()) std::abort(); const int64_t v = stream[cur / 8]; cur += 8; return v; };
    const int64_t hdr_s = take();
    const int64_t hdr_n = take();
    if (hdr_n != M.n_layer) return false;
    if (M.cell_count > M.size) return false;
    if (hdr_s != 0) return false;
    for (int il = 0; il < M.n_layer; ++il) {
        if (M.r_present[il]) {
            if (take() != M.r_type[il]) return false;
            if (take() != M.r_row[il]) return false;
            if (M.cell_count > 0) put_rd(out, il*4, M.head*M.r_row[il], M.cell_count*M.r_row[il]);
        }
        if (M.p_present[il]) {
            if (take() != M.p_row[il]) return false;
            if (M.cell_count > 0) put_rd(out, il*4+2, M.head*M.p_row[il], M.cell_count*M.p_row[il]);
        }
    }
    if (!M.s_trans) {
        for (int il = 0; il < M.n_layer; ++il) {
            if (M.s_present[il]) {
                if (take() != M.s_type[il]) return false;
                if (take() != M.s_row[il]) return false;
                if (M.cell_count > 0) put_rd(out, il*4+1, M.head*M.s_row[il], M.cell_count*M.s_row[il]);
            }
        }
    } else {
        const int64_t el = 4;
        for (int il = 0; il < M.n_layer; ++il) {
            if (M.s_present[il]) {
                if (take() != M.s_type[il]) return false;
                if (take() != el) return false;
                if (take() != M.n_embd_s) return false;
                if (M.cell_count > 0)
                    for (int64_t j = 0; j < M.n_embd_s; ++j)
                        put_rd(out, il*4+1, (M.head + j*M.size)*el, M.cell_count*el);
            }
        }
    }
    return true;
}

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(297011);
    int nbad = 0;
    for (int trial = 0; trial < 600; ++trial) {
        Model M;
        M.n_layer = 1 + (int)(rng() % 4);
        M.n_embd_s = 1 + (int)(rng() % 6);
        M.size = 16 + (int)(rng() % 16);
        M.head = (int)(rng() % (M.size - 4));
        M.cell_count = (int)(rng() % (M.size - M.head + 1));
        M.s_trans = (rng() % 4 == 0) ? 1 : 0;
        M.r_present.assign(M.n_layer, 0); M.r_type.assign(M.n_layer, 0); M.r_row.assign(M.n_layer, 0);
        M.s_present.assign(M.n_layer, 0); M.s_type.assign(M.n_layer, 0); M.s_row.assign(M.n_layer, 0);
        M.p_present.assign(M.n_layer, 0); M.p_row.assign(M.n_layer, 0);
        for (int il = 0; il < M.n_layer; ++il) {
            M.r_present[il] = (rng() % 5) != 0;
            M.r_type[il] = 1 + (int)(rng() % 3);
            M.r_row[il] = 8 * (1 + (int)(rng() % 4));
            M.s_present[il] = (rng() % 3) != 0;
            M.s_type[il] = 1 + (int)(rng() % 3);
            M.s_row[il] = 8 * (1 + (int)(rng() % 4));
            M.p_present[il] = (rng() % 6) == 0;
            M.p_row[il] = 8 * (1 + (int)(rng() % 4));
        }
        const int n_ranges = 1 + (int)(rng() % 3);
        std::vector<int64_t> flat;
        for (int r = 0; r < n_ranges; ++r) {
            int64_t lo = rng() % (M.size - 1);
            int64_t hi = lo + 1 + rng() % (M.size - lo - 1);
            M.ranges.push_back({lo, hi});
            flat.push_back(lo); flat.push_back(hi);
        }

        // --- write ---
        static std::vector<int64_t> recs(5 * 100000);
        static std::vector<int64_t> cnt(1);
        cnt[0] = 0;
        int64_t cur_slot = 0;
        int64_t elem = 4;
        g_packet[0][0] = 0;
        g_packet[0][1] = (int64_t)(uintptr_t)recs.data();
        g_packet[0][2] = (int64_t)(uintptr_t)cnt.data();
        g_packet[0][3] = M.n_layer;
        g_packet[0][4] = 0;
        g_packet[0][5] = M.n_embd_s;
        g_packet[0][6] = 0;
        g_packet[0][7] = M.size;
        g_packet[0][8] = M.cell_count;
        g_packet[0][9] = M.head;
        g_packet[0][10] = M.s_trans;
        g_packet[0][11] = n_ranges;
        g_packet[0][12] = (int64_t)(uintptr_t)flat.data();
        g_packet[0][13] = 0;
        g_packet[1][0] = (int64_t)(uintptr_t)M.r_present.data();
        g_packet[1][1] = (int64_t)(uintptr_t)M.r_type.data();
        g_packet[1][2] = (int64_t)(uintptr_t)M.r_row.data();
        g_packet[2][0] = (int64_t)(uintptr_t)M.s_present.data();
        g_packet[2][1] = (int64_t)(uintptr_t)M.s_type.data();
        g_packet[2][2] = (int64_t)(uintptr_t)M.s_row.data();
        g_packet[3][0] = (int64_t)(uintptr_t)M.p_present.data();
        g_packet[3][1] = (int64_t)(uintptr_t)M.p_row.data();
        g_packet[3][2] = elem;
        g_packet[3][3] = 0;
        g_packet[3][4] = (int64_t)(uintptr_t)&cur_slot;
        g_packet[4][0] = 0;
        if (!fg297dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        const std::vector<Rec> want = ref_write(M);
        check((size_t)cnt[0] == want.size(), "record count", trial);
        const size_t n = std::min((size_t)cnt[0], want.size());
        for (size_t i = 0; i < n; ++i) {
            const int64_t * r = &recs[i*5];
            check(r[0] == want[i].kind && r[1] == want[i].id && r[2] == want[i].off &&
                  r[3] == want[i].size && r[4] == want[i].val, "record", trial);
        }

        // --- read: build the stream from the write transcription, possibly corrupted ---
        std::vector<int64_t> stream;
        for (const Rec & r : want) {
            if (r.kind == 0 || r.kind == 1) stream.push_back(r.val);
        }
        // The stream order of the write transcription is exactly the field order the reader consumes.
        if (rng() % 5 == 0 && !stream.empty()) {
            const int which = (int)(rng() % 3);
            if (which == 0 && stream.size() > 1) stream[1] += 1;          // layer count
            else if (which == 1) stream[0] = 1;                          // s_trans
            else if (stream.size() > 2) stream[2] += 1;                  // first type
        }
        size_t rcur = 0;
        std::vector<Rec> want_rd;
        const bool want_ok = ref_read(M, stream, rcur, want_rd);

        cnt[0] = 0;
        cur_slot = 0;
        std::vector<int64_t> sbuf = stream;
        g_packet[0][0] = 1;
        g_packet[0][13] = (int64_t)(uintptr_t)sbuf.data();
        g_packet[3][3] = 0;
        g_packet[3][4] = (int64_t)(uintptr_t)&cur_slot;
        g_packet[4][0] = 0;
        if (!fg297dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        const bool got_ok = g_packet[4][0] != 0;
        if (!want_ok) ++nbad;
        check(got_ok == want_ok, "read result", trial);
        if (!want_ok) continue;
        check((size_t)cnt[0] == want_rd.size(), "read record count", trial);
        const size_t m = std::min((size_t)cnt[0], want_rd.size());
        for (size_t i = 0; i < m; ++i) {
            const int64_t * r = &recs[i*5];
            check(r[0] == want_rd[i].kind && r[1] == want_rd[i].id && r[2] == want_rd[i].off &&
                  r[3] == want_rd[i].size, "read record", trial);
        }
        check((size_t)cur_slot == rcur, "stream cursor", trial);
    }
    std::printf("FG297K_FIXTURE checks=%d failures=%d rejected=%d\n", checks, failures, nbad);
    return failures ? 1 : 0;
}
