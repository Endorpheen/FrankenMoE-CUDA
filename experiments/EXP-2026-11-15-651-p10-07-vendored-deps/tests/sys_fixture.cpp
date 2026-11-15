// Differential gate for P10-07 S3 (vendor selection and linkage).
//
// Native arm: tests/oracle_sys.py — the ggml resolution blocks and the vendor
// directory list are extracted from the byte-identical CMake copies and executed
// by `cmake -P`, and the component-to-vendor linkage is parsed from the real
// target_link_libraries lines.
//
// Port arm: the sys_ exports of source/vendor-selection.goose.
//
// Usage: gate-sys <cases.tsv> <oracle.py> <native-dir> <workdir>
#include "sys.h"

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

static void eq(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-16s want=%lld got=%lld\n", name, (long long) want, (long long) got);
    }
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
    if (argc < 5) {
        fprintf(stderr, "usage: gate-sys <cases.tsv> <oracle.py> <native-dir> <workdir>\n");
        return 2;
    }
    goose_init(0, nullptr);

    const std::string cases_path = argv[1], oracle = argv[2], native = argv[3], work = argv[4];
    const std::string out_path = work + "/sys-oracle.txt";
    std::string cmd = "python3 '" + oracle + "' '" + cases_path + "' '" + native +
                      "' > '" + out_path + "'";
    if (system(cmd.c_str()) != 0) {
        fprintf(stderr, "oracle failed: %s\n", cmd.c_str());
        return 2;
    }
    std::map<int, int64_t> want;
    {
        std::istringstream in(read_file(out_path));
        std::string line;
        while (std::getline(in, line)) {
            size_t tab = line.find('\t');
            if (tab == std::string::npos) {
                continue;
            }
            want[std::stoi(line.substr(0, tab))] = std::stoll(line.substr(tab + 1));
        }
    }

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
        const int64_t w = want[row];
        if (kind == "ggml") {
            eq("ggml", w, sys_ggml_plan(num(f, 1), num(f, 2)));
        } else if (kind == "dirs") {
            eq("dirs", w, sys_vendor_dirs(num(f, 1)));
        } else if (kind == "deps") {
            eq("deps", w, sys_component_deps(num(f, 1)));
        } else if (kind == "closure") {
            eq("closure", w, sys_link_closure(num(f, 1), num(f, 2), num(f, 3)));
        } else if (kind == "added_not_linked") {
            eq("added_not_linked", w, sys_added_not_linked(num(f, 1), num(f, 2)));
        } else if (kind == "linked_not_added") {
            eq("linked_not_added", w, sys_linked_not_added(num(f, 1), num(f, 2)));
        } else {
            fprintf(stderr, "unknown case kind: %s\n", kind.c_str());
            return 2;
        }
    }

    printf("FG649_SYS cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
