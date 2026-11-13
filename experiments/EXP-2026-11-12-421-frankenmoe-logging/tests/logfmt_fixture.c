// Fixture for the Goose FrankenMoE log formatter.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

uint8_t fglog_ld_u8(uint64_t p, uint64_t o) { return ((const uint8_t *) (uintptr_t) p)[o]; }
void fglog_st_u8(uint64_t p, uint64_t o, uint8_t v) { ((uint8_t *) (uintptr_t) p)[o] = v; }
void fglog_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) ((char *) (uintptr_t) p + o) = v; }
int64_t fglog_run(uint64_t out, int64_t cap, uint64_t evout, int64_t level, int64_t mode,
                  int64_t last, uint64_t text, int64_t len, uint64_t ts, int64_t tslen);

static int g_checks, g_fails;
static void check(int ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fails; fprintf(stderr, "FAIL %s\n", what); }
}

static int64_t run_mode(char * dst, int cap, int mode, int level, int last,
                        const char * text, int64_t * ev) {
    int64_t e = -1;
    const char * ts = "0.00.000.001";
    int64_t n = fglog_run((uint64_t) (uintptr_t) dst, cap, (uint64_t) (uintptr_t) &e,
                          level, mode, last, (uint64_t) (uintptr_t) text,
                          (int64_t) strlen(text), (uint64_t) (uintptr_t) ts, (int64_t) strlen(ts));
    if (ev) *ev = e;
    if (n < cap) dst[n] = 0; else dst[cap - 1] = 0;
    return n;
}

int main(void) {
    char buf[8192];
    int64_t ev;

    // classic: byte-for-byte copy
    const char * classic = "llama_model_loader: loaded meta data\n";
    int64_t n = run_mode(buf, sizeof buf, 0, 3, -1, classic, &ev);
    check(n == (int64_t) strlen(classic), "classic length");
    check(memcmp(buf, classic, (size_t) n) == 0, "classic bytes");

    // franken: banner once, then no banner for the same event
    const char * start = "starting server on 127.0.0.1:8081\n";
    run_mode(buf, sizeof buf, 1, 3, -1, start, &ev);
    check(ev == 1, "server_start event");
    check(strstr(buf, "FRANKENMOE IS ALIVE") != NULL, "start banner");
    check(strstr(buf, "[INFO] starting server") != NULL, "info prefix");
    run_mode(buf, sizeof buf, 1, 3, ev, start, &ev);
    check(strstr(buf, "FRANKENMOE IS ALIVE") == NULL, "banner not repeated");

    // error banner and error tag
    run_mode(buf, sizeof buf, 1, 1, -1, "CUDA error: out of memory\n", &ev);
    check(ev == 8, "error event");
    check(strstr(buf, "HIT A WALL") != NULL, "error banner");
    check(strstr(buf, "[ERROR]") != NULL, "error prefix");

    // classification matrix
    struct { const char * text; int level; int want; } cases[] = {
        {"ggml_cuda_init: found 1 CUDA devices\n", 3, 2},
        {"llama_model_loader: loading model\n", 3, 3},
        {"model loaded, load time = 100 ms\n", 3, 4},
        {"KV cache size = 1024 MiB\n", 3, 5},
        {"server is ready\n", 3, 6},
        {"prompt eval time = 10 ms, 100 tokens per second\n", 3, 7},
        {"shutting down\n", 3, 9},
        {"HTTP request: POST /completion\n", 3, 11},
        {"MoE expert routing\n", 3, 12},
        {"plain message\n", 3, 0},
        {"plain message\n", 2, 10},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        run_mode(buf, sizeof buf, 2, cases[i].level, -1, cases[i].text, &ev);
        check(ev == cases[i].want, cases[i].text);
    }

    // JSON shape and escaping
    const char * tricky = "quote \" backslash \\ newline\n";
    n = run_mode(buf, sizeof buf, 2, 3, -1, tricky, &ev);
    check(buf[0] == '{' && buf[n - 1] == '\n', "json braces");
    check(strstr(buf, "\"level\":\"info\"") != NULL, "json level");
    check(strstr(buf, "\"event\":\"log\"") != NULL, "json event");
    check(strstr(buf, "quote \\\" backslash \\\\ newline") != NULL, "json escape");
    check(strstr(buf, "\n\"") == NULL, "json has no raw newline in msg");

    // truncation: never write past cap, always terminate
    char small[32];
    memset(small, 0x5A, sizeof small);
    n = run_mode(small, (int) sizeof small, 2, 3, -1, tricky, &ev);
    check(n >= (int64_t) sizeof small, "truncation reports needed length");
    check(small[sizeof small - 1] == 0, "truncation terminates");

    printf("FLOG_FIXTURE checks=%d failures=%d\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
