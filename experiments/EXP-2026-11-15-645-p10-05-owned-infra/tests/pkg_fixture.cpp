// Differential gate for P10-05 S1 (release and package planning).
//
// Native arm: tests/oracle_pkg.py, which extracts every rule from the
// byte-identical scripts in tests/native/ and executes it — bash for the shell
// rules, GNU basename/awk/grep, and the packaging python block itself for
// SHA256SUMS and manifest.json.
//
// Port arm: the pkg_ exports of source/release-plan.goose.
//
// Usage: gate-pkg <cases.tsv> <dist-dir> <oracle.py>
#include "pkg.h"

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
    printf("FAIL %-22s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
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

static std::string unhex(const std::string & h) {
    std::string out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char) std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return out;
}

static std::string hex(const std::string & s) {
    static const char * d = "0123456789abcdef";
    std::string out;
    for (unsigned char c : s) {
        out.push_back(d[c >> 4]);
        out.push_back(d[c & 15]);
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

static bool write_file(const std::string & path, const std::string & body) {
    std::ofstream f(path, std::ios::binary);
    f.write(body.data(), (std::streamsize) body.size());
    return (bool) f;
}

static std::string read_file(const std::string & path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

int main(int argc, char ** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: gate-pkg <cases.tsv> <dist-dir> <oracle.py>\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], dist = argv[2], oracle = argv[3];

    // The synthetic dist directory the packaging block reads. Sizes are fixed so
    // the manifest fields are known to both arms.
    std::string c_body(123, 'c'), o_body(456, 'o');
    if (!write_file(dist + "/main.goose", "unit\n") || !write_file(dist + "/main.c", c_body) ||
        !write_file(dist + "/main.h", "/* h */\n") || !write_file(dist + "/main.o", o_body)) {
        fprintf(stderr, "cannot write %s\n", dist.c_str());
        return 2;
    }

    // Native arm.
    const std::string out_path = dist + "/oracle.txt";
    std::string cmd = "python3 '" + oracle + "' '" + cases_path + "' '" + dist +
                      "' 7 9 11 'goose runtime 1e714e6146bc' > '" + out_path + "'";
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

    // Hashes as the native block wrote them, fed to the port arm.
    std::string hblob, nblob;
    std::vector<int64_t> hoff, hlen, noff, nlen;
    {
        std::istringstream in(read_file(dist + "/SHA256SUMS"));
        std::string line;
        while (std::getline(in, line)) {
            if (line.size() < 66) {
                continue;
            }
            std::string hash = line.substr(0, 64);
            std::string name = line.substr(66);
            hoff.push_back((int64_t) hblob.size());
            hlen.push_back((int64_t) hash.size());
            hblob += hash;
            noff.push_back((int64_t) nblob.size());
            nlen.push_back((int64_t) name.size());
            nblob += name;
        }
    }

    uint8_t out[4096];
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
        if (kind == "version") {
            std::string s = get(f, 1);
            eq_int("version", std::stoll(w), pkg_version_ok(SL(s)));
        } else if (kind == "prerelease") {
            std::string s = get(f, 1);
            eq_int("prerelease", std::stoll(w), pkg_prerelease(SL(s)));
        } else if (kind == "basename") {
            std::string s = get(f, 1);
            int64_t n = pkg_basename(SL(s), sl_u8{out, (int64_t) sizeof out});
            eq_str("basename", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "runtime_asset") {
            std::string s = get(f, 1);
            eq_int("runtime_asset", std::stoll(w), pkg_runtime_asset(SL(s)));
        } else if (kind == "asset_selected") {
            std::string s = get(f, 1);
            eq_int("asset_selected", std::stoll(w), pkg_asset_selected(SL(s)));
        } else if (kind == "asset_limit") {
            eq_int("asset_limit", std::stoll(w), pkg_asset_over_limit(num(f, 1)));
        } else if (kind == "tree_limit") {
            eq_int("tree_limit", std::stoll(w), pkg_tree_over_limit(num(f, 1)));
        } else if (kind == "keep") {
            std::string tag = get(f, 3), new_version = get(f, 4), name = get(f, 5);
            int64_t same = (tag == new_version) ? 1 : 0;
            int64_t present = tag.empty() ? 0 : 1;
            eq_int("keep", std::stoll(w), pkg_keep_asset(num(f, 1), num(f, 2), present, same, SL(name)));
        } else if (kind == "module_count") {
            std::string s = get(f, 1);
            eq_int("module_count", std::stoll(w), pkg_module_count(SL(s)));
        } else if (kind == "version_line") {
            std::string s = get(f, 1);
            int64_t n = pkg_version_line(SL(s), sl_u8{out, (int64_t) sizeof out});
            eq_str("version_line", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "dist_files") {
            int64_t n = pkg_dist_files(sl_u8{out, (int64_t) sizeof out});
            eq_str("dist_files", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "sha256sums") {
            int64_t n = pkg_sha256sums(SL(hblob), sl_i64{hoff.data(), (int64_t) hoff.size()},
                                       sl_i64{hlen.data(), (int64_t) hlen.size()}, SL(nblob),
                                       sl_i64{noff.data(), (int64_t) noff.size()},
                                       sl_i64{nlen.data(), (int64_t) nlen.size()},
                                       (int64_t) hoff.size(), sl_u8{out, (int64_t) sizeof out});
            eq_str("sha256sums", unhex(w), std::string((char *) out, (size_t) n));
        } else if (kind == "manifest") {
            std::string unit = "goose-main";
            std::string version = get(f, 6);
            int64_t n = pkg_manifest_json(SL(unit), SL(version), num(f, 1), num(f, 2),
                                          num(f, 3), num(f, 4), num(f, 5),
                                          SL(hblob), sl_i64{hoff.data(), (int64_t) hoff.size()},
                                          sl_i64{hlen.data(), (int64_t) hlen.size()},
                                          sl_u8{out, (int64_t) sizeof out});
            eq_str("manifest", unhex(w), std::string((char *) out, (size_t) n));
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
    }

    printf("FG645_PKG cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
