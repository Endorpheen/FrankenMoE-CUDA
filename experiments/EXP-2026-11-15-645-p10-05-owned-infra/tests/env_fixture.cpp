// Differential gate for P10-05 S2 (build-environment preflight).
//
// Native arm: tests/oracle_env.py, which runs the byte-identical scripts and
// runtime/CMakeLists.txt text through bash and `cmake -P`.
//
// Port arm: the env_ exports of source/build-env.goose.
//
// Usage: gate-env <cases.tsv> <native-dir> <oracle.py>
#include "env.h"

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
    printf("FAIL %-18s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
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

static std::string decode(const std::string & f) {
    std::string out;
    for (size_t i = 0; i < f.size(); i++) {
        if (f[i] == '\\' && i + 1 < f.size()) {
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

// The ten integration markers, in the order prepare_upstreams.sh greps them.
static std::vector<std::string> marker_snippets(const std::string & prep) {
    std::vector<std::string> out;
    std::istringstream in(prep);
    std::string line;
    while (std::getline(in, line)) {
        size_t at = line.find("grep -Fq \"");
        if (at == std::string::npos) {
            continue;
        }
        size_t s = at + 10;
        size_t e = line.find('"', s);
        if (e != std::string::npos) {
            out.push_back(line.substr(s, e - s));
        }
    }
    return out;
}

int main(int argc, char ** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: gate-env <cases.tsv> <native-dir> <oracle.py>\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], native = argv[2], oracle = argv[3];
    const std::string prep = read_file(native + "/prepare_upstreams.sh");
    const std::string cmakelists = read_file(native + "/runtime-CMakeLists.txt");
    if (prep.empty() || cmakelists.empty()) {
        fprintf(stderr, "cannot read the native sources in %s\n", native.c_str());
        return 2;
    }
    std::vector<std::string> snips = marker_snippets(prep);
    if (snips.size() != 10) {
        fprintf(stderr, "expected 10 integration markers, found %zu\n", snips.size());
        return 2;
    }

    const std::string out_path = (argc > 4 ? std::string(argv[4]) : std::string(".")) + "/env-oracle.txt";
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
        if (getenv("FG_DEBUG")) fprintf(stderr, "row %d kind %s w=[%s]\n", row, kind.c_str(), w.c_str());
        memset(out, 0x5a, sizeof out);
        if (kind == "dir") {
            std::string root = get(f, 3);
            int64_t n = env_build_dir(SL(root), num(f, 1), SL(get(f, 2)), sl_u8{out, (int64_t) sizeof out});
            eq_str("dir", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "jobs") {
            std::string val = get(f, 2);
            int64_t n = env_jobs(num(f, 1), SL(val), sl_u8{out, (int64_t) sizeof out});
            eq_str("jobs", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "args") {
            std::string root = get(f, 11);
            std::string bd_val = get(f, 10);
            int64_t bn = env_build_dir(SL(root), num(f, 9), SL(bd_val), sl_u8{out, (int64_t) sizeof out});
            std::string build_dir((char *) out, (size_t) bn);
            std::string cc = get(f, 2), cxx = get(f, 4), cuda = get(f, 6), arch = get(f, 8);
            memset(out, 0x5a, sizeof out);
            int64_t n = env_cmake_args(SL(root), SL(build_dir), num(f, 1), SL(cc), num(f, 3), SL(cxx),
                                       num(f, 5), SL(cuda), num(f, 7), SL(arch),
                                       sl_u8{out, (int64_t) sizeof out});
            eq_str("args", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "build_type") {
            int64_t n = env_build_type(num(f, 1), SL(get(f, 2)), num(f, 3), sl_u8{out, (int64_t) sizeof out});
            eq_str("build_type", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "subdirs") {
            int64_t mask = env_subdirs(num(f, 1), num(f, 2), num(f, 3), num(f, 4));
            int64_t want_mask = 0;
            std::string list = unhex(w);
            std::istringstream ls(list);
            std::string name;
            while (std::getline(ls, name, ',')) {
                if (name == "core") want_mask |= 1;
                if (name == "cli") want_mask |= 2;
                if (name == "tests") want_mask |= 4;
                if (name == "tools") want_mask |= 8;
            }
            eq_int("subdirs", want_mask, mask);
        } else if (kind == "pin") {
            std::string a = get(f, 1), b = get(f, 2);
            eq_int("pin", wnum(w), env_pin_ok(SL(a), SL(b)));
        } else if (kind == "integration") {
            eq_int("integration", wnum(w), env_integration_state(num(f, 1), num(f, 2)));
        } else if (kind == "link") {
            eq_int("link", wnum(w), env_link_guard(num(f, 1), num(f, 2)));
        } else if (kind == "markers") {
            std::string flags = f[1];
            std::string text, blob;
            std::vector<int64_t> off, len;
            for (size_t i = 0; i < snips.size(); i++) {
                off.push_back((int64_t) blob.size());
                len.push_back((int64_t) snips[i].size());
                blob += snips[i];
                if (i < flags.size() && flags[i] == '1') {
                    text += "K" + std::to_string(i) + "#" + snips[i] + "\n";
                }
            }
            eq_int("markers", wnum(w), env_markers_all(SL(text), SL(blob),
                                                             sl_i64{off.data(), (int64_t) off.size()},
                                                             sl_i64{len.data(), (int64_t) len.size()},
                                                             (int64_t) snips.size()));
        } else if (kind == "options") {
            int64_t n = env_option_table(SL(cmakelists), sl_u8{out, (int64_t) sizeof out});
            eq_str("options", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "upstream") {
            int64_t n = env_upstream_pins(SL(prep), sl_u8{out, (int64_t) sizeof out});
            eq_str("upstream", unhex(w), std::string((char *) out, (size_t) n));
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
    }

    printf("FG645_ENV cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
