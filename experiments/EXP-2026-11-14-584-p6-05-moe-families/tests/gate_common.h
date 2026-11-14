// Shared structure-gate harness for the three MoE families (P6-05 4/7/13).
//
// Included after a generated runtime-gen-*.h, so sl_i64 is already declared.
// Each fixture is a separate binary: three families never share a symbol table.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_cases = 0;
static int g_values = 0;
static int g_fails = 0;

static sl_i64 as_sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static void gate_fail(const char * what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 100) {
        printf("FAIL %s: %s\n", what, detail.c_str());
    }
}

// One expected scalar: contributes a value but not a case.
static void gate_value(const char * name, int64_t got, int64_t expected) {
    ++g_values;
    if (got != expected) {
        gate_fail(name, "got " + std::to_string((long long) got) + " expected " +
                        std::to_string((long long) expected));
    }
}

// One shape case: three dimensions.
static void gate_shape(const char * name, int64_t ok, const std::vector<int64_t> & out,
                       int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        gate_fail(name, "ok=" + std::to_string((long long) ok) + " got {" +
                        std::to_string((long long) out[0]) + "," +
                        std::to_string((long long) out[1]) + "," +
                        std::to_string((long long) out[2]) + "} expected {" +
                        std::to_string((long long) a) + "," +
                        std::to_string((long long) b) + "," +
                        std::to_string((long long) c) + "}");
    }
}

// Plan case: the kind array is checked element by element; each layer counts as
// one value.
static void gate_plan(const char * name, int64_t got_n, int64_t expected_n,
                      const std::vector<int64_t> & kinds,
                      const std::vector<int64_t> & expected) {
    ++g_cases;
    g_values += (int) expected.size();
    if (got_n != expected_n || kinds.size() < expected.size()) {
        gate_fail(name, "n_layer got " + std::to_string((long long) got_n) +
                        " expected " + std::to_string((long long) expected_n));
        return;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (kinds[i] != expected[i]) {
            gate_fail(name, "kind[" + std::to_string(i) + "] got " +
                            std::to_string((long long) kinds[i]) + " expected " +
                            std::to_string((long long) expected[i]));
            return;
        }
    }
}

static void gate_report(const char * tag) {
    printf("%s cases=%d values=%d failures=%d\n", tag, g_cases, g_values, g_fails);
}
