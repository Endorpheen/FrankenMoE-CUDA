// Reference: the native expressions, and a real UTF-8 encoder for the multi-byte bytes.
#include <cstdio>
#include <cstdint>
#include <string>
extern "C" int64_t g_u8len_call(int64_t);
extern "C" int64_t g_u8byte_call(int64_t,int64_t);
static int64_t ref_len(int64_t c){ return 1 + (c >= 0x80) + (c >= 0x800) + (c >= 0x10000); }
static void encode(std::string & out, int64_t c){
    if (c < 0x80) { out += (char) c; return; }
    if (c < 0x800) { out += (char) (0xC0 | (c >> 6)); out += (char) (0x80 | (c & 0x3F)); return; }
    if (c < 0x10000) { out += (char) (0xE0 | (c >> 12)); out += (char) (0x80 | ((c >> 6) & 0x3F)); out += (char) (0x80 | (c & 0x3F)); return; }
    out += (char) (0xF0 | (c >> 18)); out += (char) (0x80 | ((c >> 12) & 0x3F));
    out += (char) (0x80 | ((c >> 6) & 0x3F)); out += (char) (0x80 | (c & 0x3F));
}
int main(){
    // boundaries and a sweep
    const int64_t pts[] = {0, 1, 0x7F, 0x80, 0x7FF, 0x800, 0xFFFF, 0x10000, 0x10FFFF, 0x2581, 0xE9};
    int fails = 0;
    for (int64_t c : pts) {
        if (g_u8len_call(c) != ref_len(c)) { fails++; printf("LEN mismatch %lld\n", (long long) c); }
        std::string enc; encode(enc, c);
        if (g_u8len_call(c) != (int64_t) enc.size()) { fails++; printf("LEN vs encoder %lld\n", (long long) c); }
        for (size_t i = 0; i < enc.size(); ++i) {
            int64_t got = g_u8byte_call(c, (int64_t) i);
            if (got != (unsigned char) enc[i]) { fails++; printf("BYTE mismatch cp=%lld i=%zu got=%lld want=%d\n",
                (long long) c, i, (long long) got, (unsigned char) enc[i]); }
        }
    }
    // a dense sweep over the whole BMP boundary neighbourhood
    for (int64_t c = 0x7FF0; c <= 0x8010; ++c) {
        if (g_u8len_call(c) != ref_len(c)) { fails++; printf("dense LEN %lld\n", (long long) c); }
    }
    printf("utf8: fails=%d\n", fails);
    return fails ? 1 : 0;
}
