// Differential gate for P10-05 S3 (generated-artifact and canonical-tree policy).
//
// Native arm: tests/oracle_art.py, which imports the copied
// tools/check-generated-files.py and tools/check-goose-tree.py, evaluates the
// native size-limit and gate expressions from the copied text, and calls the
// native check_required() on a synthetic tree.
//
// Port arm: the art_ exports of source/generated-artifact-policy.goose.
//
// Usage: gate-art <cases.tsv> <oracle.py>
#include "art.h"

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
    printf("FAIL %-14s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
}

static void eq_str(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fail(name, want, got);
    }
}

static void eq_int(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fail(name, std::to_string(want), std::to_string(got));
    }
}

static int64_t wnum(const std::string & w) {
    try {
        return std::stoll(w);
    } catch (...) {
        return -999999;
    }
}

static std::string unhex(const std::string & h) {
    std::string out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char) std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return out;
}

// The same escapes the oracle decodes; ~512~ is the filler the head-truncation
// case needs.
static std::string decode(const std::string & f) {
    std::string out;
    for (size_t i = 0; i < f.size(); i++) {
        if (f.compare(i, 5, "~512~") == 0) {
            out += std::string(512, 'x');
            i += 4;
        } else if (f[i] == '\\' && i + 1 < f.size()) {
            char c = f[++i];
            out.push_back(c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c);
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
    if (i >= f.size() || f[i].empty() || f[i] == "-") {
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
    if (argc < 3) {
        fprintf(stderr, "usage: gate-art <cases.tsv> <oracle.py>\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], oracle = argv[2];
    const std::string out_path = (argc > 3 ? std::string(argv[3]) : std::string(".")) + "/art-oracle.txt";
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

    uint8_t out[8192], relbuf[512], snipbuf[512];
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
        if (kind == "generated") {
            std::string path = get(f, 1), head = get(f, 2);
            if (head.size() > 512) {
                head.resize(512);  // index_head() hands the checker 512 bytes
            }
            eq_int("generated", std::stoll(w), art_is_generated(SL(path), SL(head)));
        } else if (kind == "limit") {
            eq_int("limit", std::stoll(w), art_limit(num(f, 1)));
        } else if (kind == "gate") {
            eq_int("gate", std::stoll(w), art_gate(num(f, 1), num(f, 2), num(f, 3)));
        } else if (kind == "required") {
            std::string text = get(f, 1);
            int64_t n = art_required_kind(SL(text), num(f, 2), num(f, 3),
                                          sl_u8{relbuf, (int64_t) sizeof relbuf},
                                          sl_u8{snipbuf, (int64_t) sizeof snipbuf});
            eq_int("required", std::stoll(w), n);
        } else if (kind == "required_parts") {
            std::string text = get(f, 1);
            memset(relbuf, 0, sizeof relbuf);
            memset(snipbuf, 0, sizeof snipbuf);
            int64_t split = art_required_split(SL(text), sl_u8{relbuf, (int64_t) sizeof relbuf},
                                               sl_u8{snipbuf, (int64_t) sizeof snipbuf});
            if (split == 0) {
                eq_str("required_parts", "-", "-");
            } else {
                size_t rl = strlen((char *) relbuf), sl2 = strlen((char *) snipbuf);
                std::string joined((char *) relbuf, rl);
                joined.push_back(28);
                joined.append((char *) snipbuf, sl2);
                eq_str("required_parts", unhex(w), joined);
            }
        } else if (kind == "orphan") {
            eq_int("orphan", wnum(w), art_orphan(num(f, 1)));
        } else if (kind == "manifest") {
            eq_int("manifest", std::stoll(w), art_manifest_mask(num(f, 1), num(f, 2), num(f, 3)));
        } else if (kind == "verdict") {
            eq_int("verdict", std::stoll(w), art_verdict(num(f, 1), num(f, 2)));
        } else if (kind == "exports") {
            std::string text = get(f, 1);
            std::vector<int64_t> off(256, 0), len(256, 0);
            int64_t n = art_export_names(SL(text), sl_i64{off.data(), (int64_t) off.size()},
                                         sl_i64{len.data(), (int64_t) len.size()},
                                         sl_u8{out, (int64_t) sizeof out});
            std::string joined;
            for (int64_t i = 0; i < n; i++) {
                if (i > 0) {
                    joined.push_back('\n');
                }
                joined += std::string((char *) out + off[i], (size_t) len[i]);
            }
            eq_str("exports", unhex(w), joined);
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
    }

    printf("FG645_ART cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
