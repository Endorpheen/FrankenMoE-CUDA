// Differential gate for the Edge0 pack reader (EXP511).
// Oracle: an independent C++ parse of the same two tables. The port reads the
// files through the Goose module; every statistic must match.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "pack-gen.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static std::string read_file(const std::string & path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

struct Row { std::string name; long long file, dtype, offset, nbytes; };

static std::vector<std::string> words(const std::string & line) {
    std::vector<std::string> out;
    std::istringstream s(line);
    std::string w;
    while (s >> w) out.push_back(w);
    return out;
}

static void expect(const char * what, long long got, long long want) {
    ++values;
    if (got != want) {
        ++fails;
        if (fails <= 8) std::printf("FAIL %s: got %lld want %lld\n", what, got, want);
    }
}

int main(int argc, char ** argv) {
    goose_init(0, 0);
    const std::string dir = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Edge0-120B-q3k";
    const std::string index_text = read_file(dir + "/index.txt");
    const std::string native_text = read_file(dir + "/native_experts.txt");
    if (index_text.empty() || native_text.empty()) {
        std::printf("cannot read the pack tables\n");
        return 2;
    }

    // Independent oracle.
    long long o_rows = 0, o_in_pool = 0, o_pool = 0, o_align = 0, o_declared = 0;
    long long o_max_end = 0, o_misaligned = 0, o_out = 0, o_bytes = 0, o_bad = 0;
    {
        std::istringstream s(index_text);
        std::string line;
        while (std::getline(s, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            auto w = words(line);
            if (line[0] == '#') {
                for (size_t i = 0; i + 1 < w.size(); ++i) {
                    if (w[i] == "align") o_align = std::stoll(w[i + 1]);
                    if (w[i] == "pool") o_pool = std::stoll(w[i + 1]);
                    if (w[i] == "tensors") o_declared = std::stoll(w[i + 1]);
                }
                continue;
            }
            if (w.size() < 5) { ++o_bad; continue; }
            ++o_rows;
            const long long off = std::stoll(w[3]);
            const long long nb = std::stoll(w[4]);
            if (nb > 0) {
                ++o_in_pool;
                o_bytes += nb;
                const long long end = off + nb;
                if (end > o_max_end) o_max_end = end;
                if (o_align > 0 && off % o_align != 0) ++o_misaligned;
                if (end > o_pool) ++o_out;
            }
        }
    }

    long long o_layers = 0, o_record = 0, o_stride = 0, o_bad_type = 0, o_bad_off = 0;
    {
        std::istringstream s(native_text);
        std::string line;
        while (std::getline(s, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            auto w = words(line);
            if (w.size() != 5) { ++o_bad; continue; }
            const long long layer = std::stoll(w[0]);
            const long long gate = std::stoll(w[1]);
            const long long down = std::stoll(w[2]);
            const long long off = std::stoll(w[3]);
            const long long size = std::stoll(w[4]);
            if (o_layers == 0) { o_record = size; o_stride = 512 * size; }
            ++o_layers;
            if (gate != 11 || down != 20) ++o_bad_type;
            if (off != layer * o_stride) ++o_bad_off;
        }
    }

    sl_u8 text{ (uint8_t *) index_text.data(), (int64_t) index_text.size() };
    int64_t out[10] = {0};
    sl_i64 out_slice{ out, 10 };
    ++cases;
    if (index_stats(text, out_slice) < 0) {
        std::printf("FAIL index_stats returned an error\n");
        ++fails;
    }
    expect("rows", out[0], o_rows);
    expect("in_pool", out[1], o_in_pool);
    expect("pool", out[2], o_pool);
    expect("align", out[3], o_align);
    expect("declared", out[4], o_declared);
    expect("max_end", out[5], o_max_end);
    expect("misaligned", out[6], o_misaligned);
    expect("out_of_pool", out[7], o_out);
    expect("malformed", out[8], o_bad);
    expect("total_bytes", out[9], o_bytes);

    sl_u8 ntext{ (uint8_t *) native_text.data(), (int64_t) native_text.size() };
    int64_t nout[8] = {0};
    sl_i64 nout_slice{ nout, 8 };
    ++cases;
    if (native_stats(ntext, nout_slice) < 0) {
        std::printf("FAIL native_stats returned an error\n");
        ++fails;
    }
    expect("layers", nout[0], o_layers);
    expect("record", nout[1], o_record);
    expect("stride", nout[2], o_stride);
    expect("bad_type", nout[3], o_bad_type);
    expect("bad_offset", nout[4], o_bad_off);
    expect("native_malformed", nout[5], 0);
    expect("first_layer", nout[6], 0);
    expect("last_layer", nout[7], o_layers - 1);

    // Facts the loader relies on for this specific pack.
    expect("rows fixed", out[0], 1367);
    expect("in_pool fixed", out[1], 1065);
    expect("pool fixed", out[2], 1538625024LL);
    expect("align fixed", out[3], 64);
    expect("max_end fixed", out[5], 1538625024LL);
    expect("layers fixed", nout[0], 48);
    expect("record fixed", nout[1], 1164800LL);
    expect("stride fixed", nout[2], 596377600LL);

    // Synthetic corpus: the real pack is correct, so the negative paths need
    // malformed rows of their own.
    {
        const std::string bad =
            "# align 64 pool 128 tensors 2\n"
            "a 0 2 0 64 0 0 0 0 0\n"
            "b 0 2 65 64 65 0 0 0 0\n";
        sl_u8 t{ (uint8_t *) bad.data(), (int64_t) bad.size() };
        int64_t b[10] = {0};
        sl_i64 bs{ b, 10 };
        ++cases;
        if (index_stats(t, bs) < 0) { ++fails; std::printf("FAIL synthetic index_stats\n"); }
        expect("syn rows", b[0], 2);
        expect("syn in_pool", b[1], 2);
        expect("syn align", b[3], 64);
        expect("syn misaligned", b[6], 1);
        expect("syn out_of_pool", b[7], 1);
        expect("syn max_end", b[5], 129);
        expect("syn total", b[9], 128);
    }
    {
        const std::string bad =
            "# native\n"
            "0 11 20 0 8\n"
            "1 11 20 8 8\n"
            "2 12 20 8192 8\n";
        sl_u8 t{ (uint8_t *) bad.data(), (int64_t) bad.size() };
        int64_t b[8] = {0};
        sl_i64 bs{ b, 8 };
        ++cases;
        if (native_stats(t, bs) < 0) { ++fails; std::printf("FAIL synthetic native_stats\n"); }
        expect("syn layers", b[0], 3);
        expect("syn record", b[1], 8);
        expect("syn stride", b[2], 4096);
        expect("syn bad_type", b[3], 1);
        expect("syn bad_offset", b[4], 1);
    }

    std::printf("FGEDGE0_PACK cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
