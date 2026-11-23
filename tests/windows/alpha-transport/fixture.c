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
int64_t __wrap_fg_http_eog(sl_i64 ids) {
    if (ids.len != 1) return -1;
    ids.data[0] = 3;
    return 1;
}

int64_t __wrap_fg_q35_generation_eval(sl_i32 tokens, sl_f32 logits, uint8_t prefill) {
    if (tokens.len < 1 || (logits.len != 0 && logits.len != 4)) return -1;
    const int64_t best = prefill ? (tokens.data[0] == 2 ? 0 : tokens.data[0] == 1 ? 1 : 2) : 3;
    for (int64_t i = 0; i < logits.len; ++i) logits.data[i] = i == best ? 20.0f : -20.0f;
    return logits.len;
}

void __wrap_fg_q35_generation_profile(sl_i64 profile) {
    profile.data[0] = 4096;
    profile.data[1] = 4096;
    profile.data[2] = 0;
}

static bool prompt_contains(sl_u8 prompt, const char *text) {
    const int64_t len = strlen(text);
    for (int64_t i = 0; i + len <= prompt.len; ++i) {
        if (memcmp(prompt.data + i, text, (size_t)len) == 0) return true;
    }
    return false;
}

int64_t fg_http_tokenize(sl_u8 prompt, sl_i64 out) {
    static const char suffix[] = "<|im_start|>assistant\n<think>";
    static const char native[] = "native fixture";
    const bool native_prompt = prompt.len == sizeof(native) - 1 &&
        !memcmp(prompt.data, native, sizeof(native) - 1);
    const bool chat_prompt = prompt.len >= sizeof(suffix) - 1 &&
        !memcmp(prompt.data + prompt.len - (sizeof(suffix) - 1), suffix, sizeof(suffix) - 1);
    if ((!native_prompt && !chat_prompt) || out.len < 1) return -1;
    if (chat_prompt && prompt_contains(prompt, "RECOVERY-FOLLOWUP-FIXTURE") &&
        (!prompt_contains(prompt, "<function=search>\n<parameter=query>\nbrowser\n</parameter>") ||
         !prompt_contains(prompt, "<tool_response>\nUnknown tool: search\n</tool_response>"))) return -1;
    if (chat_prompt && prompt_contains(prompt, "RECOVERY-FOLLOWUP-FIXTURE")) {
        const bool active_reasoning = prompt_contains(prompt, "<think>\nReasoning continuity fixture.\n</think>");
        const bool new_turn = prompt_contains(prompt, "NEW-USER-TURN-FIXTURE");
        if (active_reasoning == new_turn) return -1;
    }
    out.data[0] = prompt_contains(prompt, "UNKNOWN-TOOL-FIXTURE") ? 2 :
        prompt_contains(prompt, "MIXED-TOOL-FIXTURE") ? 1 : 0;
    return 1;
}

int64_t fg_http_detokenize(sl_i64 ids, sl_u8 out) {
    static const char result[] = "fixture reply";
    static const char mixed[] = "thinking</think>\nOpening the browser.\n"
        "<tool_call>\n<function=skill>\n<parameter=id>\nbrowser\n</parameter>\n</function>\n</tool_call>\n"
        "<tool_call>\n<function=search>\n<parameter=query>\nbrowser\n</parameter>\n</function>\n</tool_call>";
    static const char unknown[] = "thinking</think>\n"
        "<tool_call>\n<function=search>\n<parameter=query>\nbrowser\n</parameter>\n</function>\n</tool_call>";
    if (ids.len < 1 || ids.data[0] < 0 || ids.data[0] > 2) return -1;
    const char *text = ids.data[0] == 0 ? unknown : ids.data[0] == 1 ? mixed : result;
    const int64_t size = strlen(text);
    if (out.len < size) return -1;
    memcpy(out.data, text, (size_t)size);
    return size;
}

void fg_http_stats(sl_i64 stats) {
    assert(stats.len >= 6);
    memset(stats.data, 0, (size_t)stats.len * sizeof(*stats.data));
    stats.data[0] = 4;
    stats.data[2] = 1;
    stats.data[5] = 32768;
}

void fg_http_listening(int64_t port) { fprintf(stderr, "listening on port %lld\n", (long long)port); }
int64_t fg_http_set_timeout(int64_t fd) { return fg_win_socket_timeout(fd); }

int wmain(int argc, wchar_t **argv) {
    if (argc != 3 || !fg_win_network_ready()) return 1;
    const long limit = wcstol(argv[2], NULL, 10);
    if (limit <= 0) return 1;
    char *path = fg_win_to_utf8(argv[1]);
    size_t size = 0;
    void *file = path ? fg_win_map_model(path, &size) : NULL;
    free(path);
    if (!file || size != 8 || memcmp(file, "GGUF", 4) != 0 || !UnmapViewOfFile(file)) return 2;
    fg_goose_host_ensure_init();
    static unsigned char request[262144], output[65536], response[524288];
    static int64_t ids[2048];
    return ih_serve(8815, limit, (sl_u8){request, sizeof request}, (sl_u8){output, sizeof output},
        (sl_u8){response, sizeof response}, (sl_i64){ids, sizeof ids / sizeof *ids}) < 0;
}
