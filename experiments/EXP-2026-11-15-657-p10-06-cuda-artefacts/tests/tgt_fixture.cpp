// Differential gate for P10-06 S2 (CUDA compile flags, defines and link set).
//
// Native arm: tests/oracle_tgt.py, which includes the copied native blocks into
// a synthetic CMake project, configures it and reads COMPILE_OPTIONS,
// COMPILE_DEFINITIONS and LINK_LIBRARIES back.
//
// Port arm: the tgt_ exports of source/cuda-target-flags.goose.
//
// Usage: gate-tgt <cases.tsv> <oracle.py> [out-dir]
#include "cuda-target-flags.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) const_cast<char *>(s.data()), (int64_t) s.size()};
}

static void eq_str(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-6s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
    }
}

static std::string unhex(const std::string & h) {
    std::string out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char) std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return out;
}

static std::vector<std::string> split(const std::string & line) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            out.push_back(line.substr(start));
            return out;
        }
        out.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
}

static std::string get(const std::vector<std::string> & f, size_t i) {
    return i < f.size() ? f[i] : std::string();
}

static int64_t num(const std::vector<std::string> & f, size_t i) {
    const std::string s = get(f, i);
    if (s.empty() || s == "-") {
        return 0;
    }
    return std::stoll(s);
}

static std::string read_file(const std::string & path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: gate-tgt <cases.tsv> <oracle.py> [out-dir]\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], oracle = argv[2];
    const std::string out_path = (argc > 3 ? std::string(argv[3]) : std::string(".")) + "/tgt-oracle.txt";
    std::string cmd = "python3 '" + oracle + "' '" + cases_path + "' > '" + out_path + "'";
    if (system(cmd.c_str()) != 0) {
        fprintf(stderr, "oracle failed: %s\n", cmd.c_str());
        return 2;
    }
    std::map<int, std::string> want;
    {
        std::istringstream in(read_file(out_path));
        std::string line;
        while (std::getline(in, line)) {
            size_t tab = line.find('\t');
            if (tab == std::string::npos) {
                continue;
            }
            want[std::stoi(line.substr(0, tab))] = line.substr(tab + 1);
        }
    }

    uint8_t out[8192];
    std::ifstream in(cases_path);
    std::string line;
    int row = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        row++;
        cases++;
        std::vector<std::string> f = split(line);
        const std::string kind = f[0];
        const std::string & w = want[row];
        memset(out, 0x5a, sizeof out);
        int64_t n = 0;
        if (kind == "flags") {
            n = tgt_flags(num(f, 1), num(f, 2), num(f, 3), num(f, 4), SL(get(f, 5)),
                          sl_u8{out, (int64_t) sizeof out});
        } else if (kind == "final") {
            n = tgt_final(num(f, 1), num(f, 2), num(f, 3), num(f, 4), SL(get(f, 5)),
                          num(f, 6), num(f, 7), SL(get(f, 8)),
                          sl_u8{out, (int64_t) sizeof out});
        } else if (kind == "defs") {
            n = tgt_defs(SL(get(f, 1)), num(f, 2), num(f, 3), num(f, 4), num(f, 5),
                         num(f, 6), num(f, 7), num(f, 8), num(f, 9), num(f, 10),
                         sl_u8{out, (int64_t) sizeof out});
        } else if (kind == "link") {
            n = tgt_link(num(f, 1), num(f, 2), num(f, 3), num(f, 4), num(f, 5), num(f, 6),
                         num(f, 7), sl_u8{out, (int64_t) sizeof out});
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
        eq_str(kind.c_str(), unhex(w), std::string((char *) out, (size_t) n));
    }

    printf("FG660_TGT cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
