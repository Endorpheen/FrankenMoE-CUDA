#include "main.h"
#include "goose-host.h"
#include "windows/platform.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Only test executables link this deterministic model oracle.
int64_t __wrap_fg_q35_generation_prepare(void) { return 0; }
void __wrap_hose_reset_state(void) {}
int64_t __wrap_fg_http_eog(sl_i64 ids) { (void)ids; return 0; }

int64_t __wrap_fg_q35_generation_eval(sl_i32 tokens, sl_f32 logits, uint8_t prefill) {
    (void)prefill;
    if (tokens.len < 1 || (logits.len != 0 && logits.len != 4)) return -1;
    for (int64_t i = 0; i < logits.len; ++i) logits.data[i] = i == 2 ? 20.0f : -20.0f;
    return logits.len;
}

void __wrap_fg_q35_generation_profile(sl_i64 profile) {
    profile.data[0] = 4096;
    profile.data[1] = 4096;
    profile.data[2] = 0;
}

int64_t fg_http_tokenize(sl_u8 prompt, sl_i64 out) {
    static const char suffix[] = "<|im_start|>assistant\n<think>";
    if (prompt.len < sizeof(suffix) - 1 || out.len < 1 ||
        memcmp(prompt.data + prompt.len - (sizeof(suffix) - 1), suffix, sizeof(suffix) - 1))
        return -1;
    out.data[0] = 0;
    return 1;
}

int64_t fg_http_detokenize(sl_i64 ids, sl_u8 out) {
    static const char result[] = "fixture reply";
    if (ids.len < 1 || ids.data[0] != 2 || out.len < sizeof(result) - 1) return -1;
    memcpy(out.data, result, sizeof(result) - 1);
    return sizeof(result) - 1;
}

void fg_http_stats(sl_i64 stats) {
    assert(stats.len >= 6);
    memset(stats.data, 0, (size_t)stats.len * sizeof(*stats.data));
    stats.data[0] = 4;
    stats.data[5] = 32768;
}

void fg_http_listening(int64_t port) { fprintf(stderr, "listening on port %lld\n", (long long)port); }
int64_t fg_http_set_timeout(int64_t fd) { return fg_win_socket_timeout(fd); }

int wmain(int argc, wchar_t **argv) {
    if (argc != 2 || !fg_win_network_ready()) return 1;
    char *path = fg_win_to_utf8(argv[1]);
    size_t size = 0;
    void *file = path ? fg_win_map_model(path, &size) : NULL;
    free(path);
    if (!file || size != 8 || memcmp(file, "GGUF", 4) != 0 || !UnmapViewOfFile(file)) return 2;
    fg_goose_host_ensure_init();
    static unsigned char request[262144], output[65536], response[524288];
    static int64_t ids[2048];
    return ih_serve(8815, 7, (sl_u8){request, sizeof request}, (sl_u8){output, sizeof output},
        (sl_u8){response, sizeof response}, (sl_i64){ids, sizeof ids / sizeof *ids}) < 0;
}
