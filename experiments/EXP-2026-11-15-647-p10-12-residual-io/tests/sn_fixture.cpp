// Differential gate for P10-12 S3 (gguf shard resolution).
//
// Native arm: runtime/core/src/moe/gguf_offsets.cpp from the base-commit
// repository, INCLUDED here so its anonymous-namespace split_count_from_path
// and make_shard_path are callable. The gguf API half of the same file is
// unused by the gate and dropped by --gc-sections, so no ggml library is
// linked and no model is opened.
//
// Port arm: the sn_ exports of source/shard-naming.goose.
#include "sn.h"

#include "gguf_offsets.cpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-22s want=%lld got=%lld\n", name, (long long) want, (long long) got);
    }
}

static void eq_str(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-22s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

int main() {
    goose_init(0, nullptr);

    const char * paths[] = {
        "model.gguf",
        "model-00001-of-00003.gguf",
        "model-00002-of-00003.gguf",
        "model-00001-of-00001.gguf",
        "model-00001-of-00000.gguf",
        "model-00000-of-00003.gguf",
        "a-00001-of-00003.gguf",
        "/x/y/z/model-00007-of-00008.gguf",
        "/x/y/z/model-00001-of-00008.gguf",
        "model-0001-of-0003.gguf",
        "model-00001-of-0003.gguf",
        "model-001-of-00003.gguf",
        "model-00001-of-00003.GGUF",
        "model-00001-Of-00003.gguf",
        "model+00001-of+00003.gguf",
        "model 00001-of-00003.gguf",
        "-00001-of-00003.gguf",
        "x-00001-of-00003.gguf",
        "model-99999-of-99999.gguf",
        "model-00001-of-00003.gguf.bak",
        "MODEL-00001-OF-00003.gguf",
        "model-00001-of-00003.ggu",
        "model-00001-of-00003.gguf\n",
        "model-00001-of-00003-00001-of-00002.gguf",
        "model-00001-of-00012.gguf",
        "model-00012-of-00012.gguf",
        "model-00099-of-00099.gguf",
        "dir with space/m-00001-of-00002.gguf",
        "",
        "short.gguf",
        "0123456789012345678",
        "model-/0001-of-00003.gguf",
        "model-00001-of-0000/.gguf",
        "model-00001-of-00003.ggu/",
    };
    for (const char * p : paths) {
        std::string np;
        const int want = bmoe::split_count_from_path(std::string(p), np);
        std::vector<char> buf(512, 0);
        const int64_t got = sn_split_count(SL(std::string(p)), sl_u8{(uint8_t *) buf.data(), 512});
        cases++;
        eq_int(p, want, got);
        // The native assigns prefix only for a real set (count > 1) and leaves it
        // empty otherwise, so its prefix length is only observable for want > 1.
        // The remaining names are checked against a transcription of the tail
        // grammar (the same five assertions split_count_from_path makes), and
        // the native's untouched prefix is checked separately.
        const std::string sp(p);
        auto syntactic = [](const std::string & x) -> int64_t {
            const size_t tail_len = 19;
            if (x.size() < tail_len + 1) return -1;
            const size_t tail = x.size() - tail_len;
            auto five = [&](size_t at) {
                for (size_t i = at; i < at + 5; i++)
                    if (x[i] < '0' || x[i] > '9') return false;
                return true;
            };
            if (x[tail - 1] != '-' || !five(tail) || x.compare(tail + 5, 4, "-of-") != 0 ||
                !five(tail + 9) || x.compare(tail + 14, 5, ".gguf") != 0)
                return -1;
            return (int64_t) tail - 1;
        };
        const int64_t plen_want = want > 1 ? (int64_t) np.size() : syntactic(sp);
        eq_int("prefix_len", plen_want, sn_prefix_len(SL(sp)));
        if (want == -1) {
            values++;
            if (!np.empty()) {
                fails++;
                printf("FAIL refused_prefix_not_written want=[] got=[%s]\n", np.c_str());
            }
        }
        if (got > 1) {
            const std::string gp(buf.data(), (size_t) np.size());
            eq_str("prefix", np, gp);
        }
    }

    // make_shard_path over the prefixes the accepted cases produce.
    const char * prefixes[] = {"model", "a", "/x/y/z/model", "dir with space/m", "",
                               "model-00001-of-00003"};
    for (const char * pre : prefixes) {
        for (int no = 1; no <= 3; ++no) {
            for (int count = 1; count <= 12; count += 5) {
                const std::string want = bmoe::make_shard_path(std::string(pre), no, count);
                std::vector<char> buf(512, 0);
                const int64_t n = sn_shard_path(SL(std::string(pre)), (int64_t) no, (int64_t) count,
                                                sl_u8{(uint8_t *) buf.data(), 512});
                cases++;
                eq_str("shard_path", want, std::string(buf.data(), (size_t) n));
            }
        }
    }

    // The five-digit field itself, including the zero and sign shapes.
    for (int v : {0, 1, 9, 10, 99, 100, 9999, 10000, 99999}) {
        std::string pre = "m";
        const std::string want = bmoe::make_shard_path(pre, v, v);
        std::vector<char> buf(512, 0);
        const int64_t n = sn_shard_path(SL(pre), (int64_t) v, (int64_t) v, sl_u8{(uint8_t *) buf.data(), 512});
        cases++;
        eq_str("shard_pad", want, std::string(buf.data(), (size_t) n));
    }

    printf("FG648_SN cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
