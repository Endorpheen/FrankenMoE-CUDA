// Differential gate for P10-07 S2 (vendored-file provenance).
//
// Native arm: tests/oracle_vnd.py — the vendor table and the patch table are
// parsed out of the byte-identical tests/native/sync_vendor.py, the anchors are
// counted with Python's str.count(), and the tree facts are recomputed from the
// real vendor directory.
//
// Port arm: the vnd_ exports of source/vendor-provenance.goose.
//
// Usage: gate-vnd <cases.tsv> <oracle.py> <sync_vendor.py> <fixtures> <vendor-root> <workdir>
#include "vnd.h"

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

static void eq(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        std::string w = want.size() > 100 ? want.substr(0, 100) + "..." : want;
        std::string g = got.size() > 100 ? got.substr(0, 100) + "..." : got;
        printf("FAIL %-14s want=[%s] got=[%s]\n", name, w.c_str(), g.c_str());
    }
}

static std::string hex(const uint8_t * p, int64_t n) {
    static const char * d = "0123456789abcdef";
    std::string out;
    for (int64_t i = 0; i < n; i++) {
        out.push_back(d[p[i] >> 4]);
        out.push_back(d[p[i] & 15]);
    }
    return out;
}

static std::string unhex(const std::string & h) {
    std::string out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char) std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return out;
}

static std::string decode(const std::string & f) {
    std::string out;
    for (size_t i = 0; i < f.size(); i++) {
        if (f.compare(i, 5, "~512~") == 0) {
            out.append(512, 'x');
            i += 4;
        } else if (f[i] == '\\' && i + 1 < f.size()) {
            char c = f[++i];
            if (c == 'x' && i + 2 < f.size()) {
                out.push_back((char) std::stoi(f.substr(i + 1, 2), nullptr, 16));
                i += 2;
            } else {
                out.push_back(c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c);
            }
        } else {
            out.push_back(f[i]);
        }
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
    return i < f.size() ? decode(f[i]) : std::string();
}

static int64_t num(const std::vector<std::string> & f, size_t i) {
    if (i >= f.size() || f[i].empty()) {
        return 0;
    }
    return std::stoll(f[i]);
}

static std::string read_file(const std::string & path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

int main(int argc, char ** argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: gate-vnd <cases> <oracle.py> <sync_vendor.py> <fixtures> <root> <workdir>\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], oracle = argv[2], native = argv[3];
    const std::string fixtures = argv[4], root = argv[5], work = argv[6];
    const std::string out_path = work + "/vnd-oracle.txt";
    std::string cmd = "python3 '" + oracle + "' '" + cases_path + "' '" + native + "' '" +
                      fixtures + "' '" + root + "' > '" + out_path + "'";
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

    static uint8_t out[65536];
    static uint8_t namebuf[256];
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
        int64_t n = 0;
        memset(out, 0x5a, sizeof out);
        memset(namebuf, 0, sizeof namebuf);
        if (kind == "pin") {
            eq("pin", w, std::to_string(vnd_pin_kind(SL(get(f, 1)))));
        } else if (kind == "pin_name") {
            n = vnd_pin_name(vnd_pin_kind(SL(get(f, 1))), sl_u8{out, (int64_t) sizeof out});
            eq("pin_name", w, std::string((char *) out, (size_t) n));
        } else if (kind == "repro") {
            eq("repro", w, std::to_string(vnd_reproducible(num(f, 1))));
        } else if (kind == "anchor") {
            const std::string text = read_file(fixtures + "/" + f[1]);
            eq("anchor", w, std::to_string(vnd_anchor_count(SL(text), SL(get(f, 2)))));
        } else if (kind == "real_anchor") {
            // f[1] is the native path, f[3] the anchor text from the patch table
            std::string rel = f[1];
            size_t slash = rel.find_last_of('/');
            const std::string text = read_file(fixtures + "/" + rel.substr(slash + 1));
            eq("real_anchor", w, std::to_string(vnd_anchor_count(SL(text), SL(get(f, 3)))));
        } else if (kind == "anchor_state") {
            eq("anchor_state", w, std::to_string(vnd_anchor_state(num(f, 1))));
        } else if (kind == "patch") {
            n = vnd_patch(SL(get(f, 1)), SL(get(f, 2)), SL(get(f, 3)),
                          sl_u8{out, (int64_t) sizeof out});
            eq("patch", w, n < 0 ? "-1" : hex(out, n));
        } else if (kind == "tree_state" || kind == "tree_state_synth") {
            const size_t base = kind == "tree_state" ? 2 : 1;
            eq(kind.c_str(), w, std::to_string(vnd_file_state(
                num(f, base), num(f, base + 1), num(f, base + 2), num(f, base + 3))));
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
        (void) unhex;
        (void) namebuf;
    }

    printf("FG649_VND cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
