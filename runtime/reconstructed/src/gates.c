#include <stdlib.h>
#include <string.h>
static int goose_on(const char * name) {
    const char * f = getenv(name);
    return !(f && strcmp(f, "0") == 0);
}
int fg286m_enabled(void) { return goose_on("GGML_GOOSE_MMAP_BOOK"); }
int fg291b_enabled(void) { return goose_on("GGML_GOOSE_BATCH_REJECT"); }
int fg262a_enabled(void) { return goose_on("GGML_GOOSE_MOE_CACHE_ADMIT"); }
int fg263c_enabled(void) { return goose_on("GGML_GOOSE_MOE_CACHE_COPY"); }
