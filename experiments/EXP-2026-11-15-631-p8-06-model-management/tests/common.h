// Shared scaffolding for the three P8-06 differential gates.
//
// One gate binary per slice, because every module's generated header declares
// its own copy of the std-library typedefs and two of them in one translation
// unit would be a conflicting declaration. Each gate links its own module and
// prints one line, so the slices stay independent down to the binary.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

struct Gate {
    const char * tag;
    int cases = 0;
    int values = 0;
    int failures = 0;
    int printed = 0;
    std::string cur;

    explicit Gate(const char * t) : tag(t) {}

    void begin(const std::string & name) {
        cases++;
        cur = name;
    }

    void value(const char * field, const std::string & port, const std::string & native) {
        values++;
        if (port == native) {
            return;
        }
        failures++;
        if (printed < 12) {
            printed++;
            std::printf("FAIL %s/%s/%s\n  port   = %s\n  native = %s\n",
                        tag, cur.c_str(), field, show(port).c_str(), show(native).c_str());
        }
    }

    void value(const char * field, int64_t port, int64_t native) {
        values++;
        if (port == native) {
            return;
        }
        failures++;
        if (printed < 12) {
            printed++;
            std::printf("FAIL %s/%s/%s port=%lld native=%lld\n",
                        tag, cur.c_str(), field, (long long) port, (long long) native);
        }
    }

    // a documented constant the port side must produce, kept apart from the
    // native comparison so a transcription slip shows up as a golden mismatch
    void expect(const char * field, int64_t got, int64_t want) {
        value(field, got, want);
    }

    int finish() const {
        std::printf("FG634_%s cases=%d values=%d failures=%d\n", tag, cases, values, failures);
        return failures;
    }

    static std::string show(const std::string & s) {
        std::string out;
        for (unsigned char c : s) {
            if (c == '\n') {
                out += "\\n";
            } else if (c == '\t') {
                out += "\\t";
            } else if (c < 32 || c > 126) {
                char b[8];
                std::snprintf(b, sizeof(b), "\\x%02X", c);
                out += b;
            } else {
                out += (char) c;
            }
        }
        return "\"" + out + "\"";
    }
};

// an output buffer the port writes into
struct UBuf {
    std::vector<uint8_t> b;

    explicit UBuf(size_t n = 1 << 16) : b(n, 0) {}

    uint8_t * data() {
        return b.data();
    }

    int64_t cap() const {
        return (int64_t) b.size();
    }

    std::string str(int64_t n) const {
        if (n <= 0) {
            return std::string();
        }
        return std::string((const char *) b.data(), (size_t) n);
    }

    void zero() {
        std::fill(b.begin(), b.end(), 0);
    }
};

// a scratch array of words the port writes into
struct IBuf {
    std::vector<int64_t> v;

    explicit IBuf(size_t n = 512) : v(n, 0) {}

    int64_t * data() {
        return v.data();
    }

    int64_t cap() const {
        return (int64_t) v.size();
    }

    void zero() {
        std::fill(v.begin(), v.end(), 0);
    }
};

// model names packed into one buffer, addressed by offset and length, exactly
// the shape the modules take
struct NameTable {
    std::vector<uint8_t> buf;
    std::vector<int64_t> off;
    std::vector<int64_t> len;

    int64_t add(const std::string & s) {
        off.push_back((int64_t) buf.size());
        len.push_back((int64_t) s.size());
        buf.insert(buf.end(), s.begin(), s.end());
        return (int64_t) off.size() - 1;
    }

    int64_t n() const {
        return (int64_t) off.size();
    }
};

inline std::string join(const std::vector<std::string> & v, const char * sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) {
            out += sep;
        }
        out += v[i];
    }
    return out;
}
