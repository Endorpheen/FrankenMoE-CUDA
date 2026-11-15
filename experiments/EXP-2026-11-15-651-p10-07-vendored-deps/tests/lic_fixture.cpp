// Differential gate for P10-07 S1 (embedded licence payload).
//
// Native arm: tests/oracle_lic.py, which runs the byte-identical
// tests/native/license.cmake in `cmake -P` script mode and hands back the bytes
// license_add_file()/license_generate() actually produce, as hex.
//
// Port arm: the lic_ exports of source/vendor-licence-payload.goose.
//
// Usage: gate-lic <cases.tsv> <oracle.py> <fixtures-dir> <work-dir>
#include "lic.h"

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

static void fail(const char * name, const std::string & want, const std::string & got) {
    fails++;
    std::string w = want.size() > 160 ? want.substr(0, 160) + "..." : want;
    std::string g = got.size() > 160 ? got.substr(0, 160) + "..." : got;
    printf("FAIL %-12s want=[%s] got=[%s]\n", name, w.c_str(), g.c_str());
}

static void eq(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fail(name, want, got);
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

static std::vector<std::string> split(const std::string & line, char sep = '\t') {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t tab = line.find(sep, start);
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

static std::string read_file(const std::string & path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

int main(int argc, char ** argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: gate-lic <cases.tsv> <oracle.py> <license.cmake> <fixtures> <workdir>\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], oracle = argv[2], native = argv[3];
    const std::string fixtures = argv[4], work = argv[5];
    const std::string out_path = work + "/lic-oracle.txt";
    std::string cmd = "python3 '" + oracle + "' '" + cases_path + "' '" + native + "'" +
                      " '" + fixtures + "' > '" + out_path + "'";
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
    static uint8_t text[65536];
    static uint8_t norm[65536];
    uint8_t resolved[4096];
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
        if (kind == "title") {
            n = lic_title(SL(get(f, 1)), sl_u8{out, (int64_t) sizeof out});
            eq("title", w, hex(out, n));
        } else if (kind == "underline") {
            static uint8_t title[4096];
            int64_t tn = lic_title(SL(get(f, 1)), sl_u8{title, (int64_t) sizeof title});
            n = lic_underline(sl_u8{title, tn}, sl_u8{out, (int64_t) sizeof out});
            eq("underline", w, hex(out, n));
        } else if (kind == "file_text") {
            const std::string raw = get(f, 1);
            n = lic_file_text(SL(raw), sl_u8{out, (int64_t) sizeof out});
            eq("file_text", w, hex(out, n));
        } else if (kind == "entry") {
            const std::string raw = read_file(fixtures + "/" + f[2]);
            int64_t tn = lic_file_text(SL(raw), sl_u8{norm, (int64_t) sizeof norm});
            n = lic_entry(SL(get(f, 1)), sl_u8{norm, tn}, sl_u8{out, (int64_t) sizeof out});
            eq("entry", w, hex(out, n));
        } else if (kind == "body") {
            int64_t at = 0;
            for (const std::string & spec : split(f[1], '|')) {
                if (spec.empty()) {
                    continue;
                }
                size_t colon = spec.find(':');
                const std::string name = spec.substr(0, colon);
                const std::string raw = read_file(fixtures + "/" + spec.substr(colon + 1));
                int64_t tn = lic_file_text(SL(raw), sl_u8{norm, (int64_t) sizeof norm});
                at += lic_entry(SL(name), sl_u8{norm, tn},
                                sl_u8{text + at, (int64_t) sizeof text - at});
            }
            n = lic_body(sl_u8{text, at}, sl_u8{out, (int64_t) sizeof out});
            eq("body", w, hex(out, n));
        } else if (kind == "resolve") {
            n = lic_resolve(SL(get(f, 1)), SL(get(f, 2)), 0, sl_u8{out, (int64_t) sizeof out});
            eq("resolve", w, hex(out, n));
        } else if (kind == "no_target") {
            n = lic_missing_target(SL(get(f, 1)), sl_u8{out, (int64_t) sizeof out});
            eq("no_target", w, hex(out, n));
        } else if (kind == "missing") {
            memset(resolved, 0, sizeof resolved);
            int64_t rn = lic_resolve(SL(get(f, 1)), SL(get(f, 2)), 0,
                                     sl_u8{resolved, (int64_t) sizeof resolved});
            n = lic_missing(sl_u8{resolved, rn}, sl_u8{out, (int64_t) sizeof out});
            eq("missing", w, hex(out, n));
        } else if (kind == "absolute") {
            n = lic_is_absolute(SL(get(f, 1)), 0);
            eq("absolute", w, std::to_string(n));
        } else if (kind == "absolute_win") {
            n = lic_is_absolute(SL(get(f, 1)), 1);
            eq("absolute_win", w, std::to_string(n));
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
    }

    printf("FG649_LIC cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
