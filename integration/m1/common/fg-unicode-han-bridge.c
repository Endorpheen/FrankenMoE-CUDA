// Bridge for goose/misc/unicode-han.goose: the Han range classifier. The module has no FFI of its own, so this
// file supplies only the initialisation the generated code needs.
#include <stdint.h>

int64_t uc_han(int64_t cpt);
void fg_unicode_han_goose_init(int argc, char ** argv);

static int g_started = 0;

int64_t fg_han_is_han(int64_t cpt) {
    if (!g_started) {
        fg_unicode_han_goose_init(0, (char **) 0);
        g_started = 1;
    }
    return uc_han(cpt);
}
