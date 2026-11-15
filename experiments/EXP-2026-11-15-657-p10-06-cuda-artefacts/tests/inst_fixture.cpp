// Differential gate for P10-06 S3 (install and dist layout of the CUDA
// backend artefact).
//
// Native arm: tests/oracle_inst.py, which builds and installs a synthetic
// project that contains the native ggml_add_backend_library() text and reads
// the real installed file list, the target properties and the version block.
//
// Port arm: the inst_ exports of source/cuda-install-layout.goose.
//
// Usage: gate-inst <cases.tsv> <oracle.py> [out-dir]
#include "cuda-install-layout.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <algorithm>
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
        printf("FAIL %-10s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
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

// the installed file set: the oracle lists the prefix sorted, so the set is
// compared in sorted order and not by install order
static std::string sorted_set(const std::string & joined) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : joined) {
        if (c == ';') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    std::sort(parts.begin(), parts.end());
    std::string out;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) {
            out.push_back(';');
        }
        out += parts[i];
    }
    return out;
}

static std::string read_file(const std::string & path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: gate-inst <cases.tsv> <oracle.py> [out-dir]\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], oracle = argv[2];
    const std::string out_path = (argc > 3 ? std::string(argv[3]) : std::string(".")) + "/inst-oracle.txt";
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

    uint8_t out[2048];
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
        std::string got;
        if (kind == "kind") {
            got = std::to_string(inst_kind(num(f, 1), num(f, 2)));
        } else if (kind == "dest") {
            got = std::to_string(inst_dest(num(f, 1), num(f, 2)));
        } else if (kind == "files") {
            int64_t n = inst_files(num(f, 1), num(f, 2), SL(get(f, 3)), SL("ggml-cuda"),
                                   SL("0.22.0"), SL("0"), sl_u8{out, (int64_t) sizeof out});
            got.assign((char *) out, (size_t) n);
            got = sorted_set(got);
        } else if (kind == "versioned") {
            got = std::to_string(inst_versioned(num(f, 1), num(f, 2)));
        } else if (kind == "privdefs") {
            int64_t n = inst_defs(num(f, 1), num(f, 2), sl_u8{out, (int64_t) sizeof out});
            got.assign((char *) out, (size_t) n);
        } else if (kind == "pubdefs") {
            int64_t n = inst_public_defs(num(f, 1), sl_u8{out, (int64_t) sizeof out});
            got.assign((char *) out, (size_t) n);
        } else if (kind == "avail") {
            int64_t n = inst_avail(SL(get(f, 1)), SL("ggml-cuda"), sl_u8{out, (int64_t) sizeof out});
            got.assign((char *) out, (size_t) n);
        } else if (kind == "version") {
            int64_t n = 0;
            if (get(f, 1) == "version") {
                n = inst_version(0, 22, 0, sl_u8{out, (int64_t) sizeof out});
            } else {
                n = inst_soversion(0, sl_u8{out, (int64_t) sizeof out});
            }
            got.assign((char *) out, (size_t) n);
        } else if (kind == "guard") {
            got = std::to_string(inst_guard(num(f, 1), num(f, 2)));
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
        eq_str(kind.c_str(), unhex(w), got);
    }

    printf("FG660_INST cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
