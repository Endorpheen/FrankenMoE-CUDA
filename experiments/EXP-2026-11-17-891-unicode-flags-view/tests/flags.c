// The normalised classification view, checked against what Unicode says each codepoint is.
//
// Not a tautology: the expected classes below are written from the character's meaning, not read back from the
// same function. It also proves the view is layout-independent (explicit masks, no bitfield dependency) and that
// no bits outside the documented mask survive.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#define UNDEFINED   0x0001
#define NUMBER      0x0002
#define LETTER      0x0004
#define SEPARATOR   0x0008
#define ACCENT_MARK 0x0010
#define PUNCTUATION 0x0020
#define SYMBOL      0x0040
#define CONTROL     0x0080
#define WHITESPACE  0x0100
#define LOWERCASE   0x0200
#define UPPERCASE   0x0400
#define NFD         0x0800
#define ALL         0x0FFF

typedef const uint16_t * (*view_fn)(void);
typedef int64_t (*count_fn)(void);

static int fails;
static void expect(view_fn v, uint32_t cpt, uint16_t want, uint16_t mask, const char * what) {
    const uint16_t got = v()[cpt];
    if ((got & mask) != want) {
        printf("FAIL U+%04X %s: got 0x%03X want 0x%03X (mask 0x%03X)\n", cpt, what, got & mask, want, mask);
        ++fails;
    }
}

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "runtime/goose-stage/lib/libllama.so";
    void * h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!h) { printf("FG_UNICODE_FLAGS FAIL dlopen: %s\n", dlerror()); return 1; }
    view_fn v = (view_fn) dlsym(h, "fg_unicode_flags_view_c");
    count_fn n = (count_fn) dlsym(h, "fg_unicode_flags_count_c");
    if (!v || !n) { printf("FG_UNICODE_FLAGS FAIL symbols missing\n"); return 1; }

    if (n() != 0x110000) { printf("FAIL count %lld\n", (long long) n()); ++fails; }

    // ASCII letters
    expect(v, 'A', LETTER | UPPERCASE, LETTER | UPPERCASE, "latin capital A");
    expect(v, 'z', LETTER | LOWERCASE, LETTER | LOWERCASE, "latin small z");
    // digits and whitespace
    expect(v, '7', NUMBER, NUMBER, "digit seven");
    expect(v, ' ', WHITESPACE | SEPARATOR, WHITESPACE | SEPARATOR, "space");
    expect(v, '\t', WHITESPACE | CONTROL, WHITESPACE | CONTROL, "tab");
    // punctuation and symbol
    expect(v, '.', PUNCTUATION, PUNCTUATION, "full stop");
    expect(v, '$', SYMBOL, SYMBOL, "dollar");
    // non-ASCII
    expect(v, 0x00E9, LETTER | LOWERCASE, LETTER | LOWERCASE, "e acute");
    expect(v, 0x4E2D, LETTER, LETTER, "CJK zhong");
    expect(v, 0x0301, ACCENT_MARK, ACCENT_MARK, "combining acute");

    // Every entry must stay inside the documented mask.
    int64_t stray = 0;
    for (int64_t c = 0; c < n(); ++c) {
        if (v()[c] & ~ALL) { if (!stray) printf("FAIL stray bits at U+%04llX: 0x%04X\n", (long long) c, v()[c]); ++stray; }
    }

    printf("FG_UNICODE_FLAGS codepoints=%lld stray=%lld %s\n",
           (long long) n(), (long long) stray, (fails || stray) ? "FAIL" : "PASS");
    return (fails || stray) ? 1 : 0;
}
