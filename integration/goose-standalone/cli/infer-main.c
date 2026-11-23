#include "goose-host-llm.h"
#include "goose-host.h"
#include "graph-execute.h"
#include "main.h"

#include <errno.h>
#include <inttypes.h>
#ifdef _WIN32
#include <wchar.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "windows/platform.h"
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

extern int64_t fg_cuda_dense_bytes(void);
extern int64_t fg_cuda_expert_bytes(void);
extern void fg_cuda_expert_stats(sl_i64 stats);
extern int64_t fg_cuda_model_state_bytes(void);

int64_t fg_http_tokenize(sl_u8 prompt, sl_i64 ids) {
    return hose_tokenize(prompt.data, prompt.len, ids.data, ids.len);
}

int64_t fg_http_detokenize(sl_i64 ids, sl_u8 text) {
    return hose_detokenize(ids.data, ids.len, text.data, text.len);
}

void fg_http_stats(sl_i64 stats) {
    if (stats.len >= 5) hose_stats(stats.data);
    if (stats.len >= 6) stats.data[5] = hose_context_capacity();
    if (stats.len >= 10 && hose_model_residency(stats.data + 6) != 0) stats.data[6] = -1;
    if (stats.len >= 11) stats.data[10] = fg_cuda_dense_bytes();
    if (stats.len >= 12) stats.data[11] = fg_cuda_expert_bytes();
    if (stats.len >= 13) stats.data[12] = fg_cuda_model_state_bytes();
    if (stats.len >= 15) hose_prefill_profile(stats.data + 13);
    if (stats.len >= 21) fg_cuda_expert_stats((sl_i64) { stats.data + 15, 6 });
    if (stats.len >= 22) stats.data[21] = hose_expert_cache_limit();
    if (stats.len >= 23) stats.data[22] = fg_standalone_cpu_batch_threads();
    if (stats.len >= 24) stats.data[23] = fg_standalone_cpu_decode_threads();
}

void fg_http_listening(int64_t port) {
    fprintf(stderr, "listening on 127.0.0.1:%" PRId64 "\n", port);
    fflush(stderr);
}

int64_t fg_http_set_timeout(int64_t fd) {
#ifdef _WIN32
    return fg_win_socket_timeout(fd);
#else
    const struct timeval timeout = { .tv_sec = 30 };
    if (setsockopt((int) fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) != 0) return -1;
    return setsockopt((int) fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
#endif
}

static int64_t positive_integer(const char * text) {
    char * end;
    errno = 0;
    const intmax_t value = strtoimax(text, &end, 10);
    return errno || end == text || *end || value <= 0 || value > INT64_MAX ? -1 : (int64_t) value;
}

static int serve(const char * model, int64_t port, int64_t limit, int64_t capacity) {
    if (port <= 0 || port > 65535 || limit < 0) {
        fputs("invalid port or connection limit\n", stderr);
        return 2;
    }
#ifdef FG_CHAT_ALPHA
    static unsigned char request[2 * 1024 * 1024];
#else
    static unsigned char request[262144];
#endif
    static unsigned char output[65536], response[524288];
    static int64_t ids[2048];
#ifdef _WIN32
    if (!fg_win_network_ready()) {
        fputs("Windows networking initialization failed\n", stderr);
        return 1;
    }
#endif
#ifdef FG_CHAT_ALPHA
    ih_log_loading(capacity);
#endif
    fg_goose_host_ensure_init();
    hose_host_load_context(model, capacity);
    const int64_t served = ih_serve(port, limit, (sl_u8) { request, sizeof request },
        (sl_u8) { output, sizeof output }, (sl_u8) { response, sizeof response },
        (sl_i64) { ids, sizeof ids / sizeof *ids });
    if (served >= 0) return 0;
    fprintf(stderr, "HTTP listener failed (%" PRId64 ")\n", served);
    return 1;
}

#ifdef FG_CHAT_ALPHA
// Accepted runtime profile (EXP-2026-11-23-1805, cumulative from EXP-1724):
// these switches select the graph replay, compact routing, split expert planes,
// pinned batch DMA and the other accepted CUDA paths. A value set by the user in
// the environment always wins; GOOSE_RUNTIME_DEFAULTS=0 skips the whole table.
static void apply_runtime_defaults(void) {
    static const char * const defaults[][2] = {
    {"GGML_CUDA_REGISTER_HOST", "1"},
    {"GOOSE_EXPERT_PIN_MIB", "18601"},
    {"GOOSE_CUDA_VECTOR_FLAT", "1"},
    {"GOOSE_CUDA_VECTOR_FLAT4", "1"},
    {"GOOSE_MMQ", "1"},
    {"GOOSE_GDN_MMA", "1"},
    {"GOOSE_GDN_MMA_V32", "1"},
    {"GOOSE_FLASH_MMA", "1"},
    {"GOOSE_DENSE_MMA", "1"},
    {"GOOSE_MMVQ", "1"},
    {"GOOSE_MMQ_Q4_REFERENCE", "1"},
    {"GOOSE_MMQ_Q6_REFERENCE", "1"},
    {"GOOSE_MMQ_Q4_M64", "1"},
    {"GOOSE_MMQ_Q4_M128", "1"},
    {"GOOSE_MMQ_Q4_LARGE_TILE", "128x256"},
    {"GOOSE_MMQ_Q4_PREPARE", "1"},
    {"GOOSE_MMQ_Q4_PIPELINE", "1"},
    {"GOOSE_MMQ_Q4_ROUTED_M128", "1"},
    {"GOOSE_MMQ_Q4_ROUTED_DOWN_M128", "1"},
    {"GOOSE_MMQ_Q6_HALF_K", "1"},
    {"GOOSE_MMQ_Q6_PIPELINE", "1"},
    {"GOOSE_MMQ_Q6_ROUTED_PIPELINE", "1"},
    {"GOOSE_MMQ_SWIGLU_PACK", "1"},
    {"GOOSE_DENSE_MMA_REFERENCE", "1"},
    {"GOOSE_SSM_REFERENCE", "1"},
    {"GOOSE_SSM_DECODE_REFERENCE", "0"},
    {"GOOSE_RMS_REFERENCE", "1"},
    {"GOOSE_CUDA_RMS_POSTSCALE", "1"},
    {"GOOSE_KV_LAZY_VMM", "1"},
    {"GOOSE_FLASH_VEC", "1"},
    {"GOOSE_FLASH_TAIL_MMA", "0"},
    {"GOOSE_FLASH_STREAM_K", "0"},
    {"GOOSE_CUDA_FWHT_Q4", "1"},
    {"GOOSE_CUDA_GRAPH", "1"},
    {"GOOSE_CUDA_GRAPH_PACK", "1"},
    {"GOOSE_CUDA_MMVQ_GRAPH", "1"},
    {"GOOSE_CUDA_ROUTED_GRAPH", "1"},
    {"GOOSE_CUDA_FULL_FFN_GRAPH", "1"},
    {"GOOSE_CUDA_DECODE_GRAPH", "1"},
    {"GOOSE_CUDA_DECODE_EXPERT_OVERLAP", "1"},
    {"GOOSE_CUDA_DECODE_REUSE", "1"},
    {"GOOSE_CUDA_ACTIVATION_MIB", "384"},
    {"GOOSE_CUDA_ACTIVATION_REFERENCE", "1"},
    {"GOOSE_CUDA_STAGE_MATRIX_INPUT", "1"},
    {"GOOSE_CUDA_TILED_BLIT", "1"},
    {"GOOSE_CUDA_DIRECT_SSM", "1"},
    {"GOOSE_CUDA_ASYNC_PACKETS", "1"},
    {"GOOSE_CUDA_EXPERT_OVERLAP", "1"},
    {"GOOSE_CUDA_EXPERT_STAGE_CURRENT", "1"},
    {"GOOSE_CUDA_EXPERT_PARALLEL_COPY", "1"},
    {"GOOSE_PREFILL_CHECKPOINT_TAIL", "1"},
    {"GOOSE_MMVQ_DENSE_TAIL", "1"},
    {"GOOSE_MMQ_SHARED_REUSE", "1"},
    {"GOOSE_MMQ_DENSE_REUSE", "1"},
    {"GOOSE_CUDA_EXPERT_SPLIT_PLANES", "1"},
    };
    const char * off = getenv("GOOSE_RUNTIME_DEFAULTS");
    if (off && strcmp(off, "0") == 0) return;
    for (size_t i = 0; i < sizeof defaults / sizeof defaults[0]; ++i) {
        if (getenv(defaults[i][0])) continue;
#ifdef _WIN32
        _putenv_s(defaults[i][0], defaults[i][1]);
#else
        setenv(defaults[i][0], defaults[i][1], 0);
#endif
    }
}
#endif

#ifdef _WIN32
static int run_cli(int argc, char **argv);

static void default_web_root(void) {
    if (GetEnvironmentVariableW(L"FRANKENMOE_WEB_ROOT", NULL, 0)) return;
    wchar_t path[4096];
    const DWORD size = GetModuleFileNameW(NULL, path, 4096);
    if (!size || size >= 4096) return;
    wchar_t *name = wcsrchr(path, L'\\');
    if (!name || (size_t)(name - path) + 5 > 4096) return;
    wcscpy(name + 1, L"web");
    SetEnvironmentVariableW(L"FRANKENMOE_WEB_ROOT", path);
}

int wmain(int argc, wchar_t **wide) {
    default_web_root();
    char **argv = calloc((size_t)argc + 1, sizeof(*argv));
    if (!argv) return 1;
    int count = 0;
    while (count < argc && (argv[count] = fg_win_to_utf8(wide[count])) != NULL) ++count;
    const int status = count == argc ? run_cli(argc, argv) : 1;
    while (count) free(argv[--count]);
    free(argv);
    return status;
}

static int run_cli(int argc, char **argv) {
#else
int main(int argc, char ** argv) {
#endif
#ifdef FG_CHAT_ALPHA
    apply_runtime_defaults();
#endif
    const char * program = argv[0];
    int64_t context = HOSE_DEFAULT_CONTEXT;
    int64_t batch = HOSE_DEFAULT_PREFILL_BATCH;
    int64_t ubatch = 0;
    int64_t batch_threads = 1;
    int64_t threads = 1;
    const char * cpu_mask = NULL, * cpu_mask_batch = NULL;
    bool cpu_strict = false, cpu_strict_batch = false;
    while (argc >= 2) {
        if (strcmp(argv[1], "--cache-type-k") == 0 || strcmp(argv[1], "--cache-type-v") == 0) {
            if (argc < 3 || strcmp(argv[2], "q4_0") != 0) {
                fprintf(stderr, "%s requires q4_0 in the standalone Apex runtime\n", argv[1]);
                return 2;
            }
            argc -= 2;
            argv += 2;
            continue;
        }
        if (strcmp(argv[1], "--expert-cache-mib") == 0) {
            if (argc < 3 || hose_configure_expert_cache(positive_integer(argv[2])) != 0) {
                fputs("--expert-cache-mib requires a positive MiB value that fits in signed 64-bit bytes\n", stderr);
                return 2;
            }
            argc -= 2;
            argv += 2;
            continue;
        }
        const char ** mask = NULL;
        if (strcmp(argv[1], "--cpu-mask") == 0) mask = &cpu_mask;
        if (strcmp(argv[1], "--cpu-mask-batch") == 0) mask = &cpu_mask_batch;
        if (mask) {
            if (argc < 3) {
                fprintf(stderr, "%s requires a hexadecimal CPU mask\n", argv[1]);
                return 2;
            }
            *mask = argv[2];
            argc -= 2;
            argv += 2;
            continue;
        }
        bool * strict = NULL;
        if (strcmp(argv[1], "--cpu-strict") == 0) strict = &cpu_strict;
        if (strcmp(argv[1], "--cpu-strict-batch") == 0) strict = &cpu_strict_batch;
        if (strict) {
            if (argc < 3 || (strcmp(argv[2], "0") != 0 && strcmp(argv[2], "1") != 0)) {
                fprintf(stderr, "%s requires 0 or 1\n", argv[1]);
                return 2;
            }
            *strict = argv[2][0] == '1';
            argc -= 2;
            argv += 2;
            continue;
        }
        int64_t * value = NULL;
        if (strcmp(argv[1], "--ctx-size") == 0) value = &context;
        if (strcmp(argv[1], "--batch-size") == 0) value = &batch;
        if (strcmp(argv[1], "--ubatch-size") == 0) value = &ubatch;
        if (strcmp(argv[1], "--threads-batch") == 0) value = &batch_threads;
        if (strcmp(argv[1], "--threads") == 0) value = &threads;
        if (!value) break;
        if (argc < 3 || (*value = positive_integer(argv[2])) <= 0) {
            fprintf(stderr, "%s requires a positive integer\n", argv[1]);
            return 2;
        }
        argc -= 2;
        argv += 2;
    }
    if (context > INT32_MAX || context % 32 != 0) {
        fputs("--ctx-size must be a positive multiple of 32 within the model context length\n", stderr);
        return 2;
    }
    if (ubatch == 0) ubatch = batch < HOSE_DEFAULT_PREFILL_BATCH ? batch : HOSE_DEFAULT_PREFILL_BATCH;
    if (hose_configure_prefill(batch, ubatch) != 0) {
        fputs("require 1 <= --ubatch-size <= --batch-size <= 4096\n", stderr);
        return 2;
    }
    if (batch_threads > 1 && fg_standalone_configure_cpu_batch(batch_threads) != 0) {
        fputs("--threads-batch exceeds the available Goose worker capacity\n", stderr);
        return 2;
    }
    if (threads > 1 && fg_standalone_configure_cpu_decode(threads) != 0) {
        fputs("--threads exceeds the available Goose worker capacity\n", stderr);
        return 2;
    }
    if (cpu_mask && fg_standalone_configure_cpu_affinity(cpu_mask, cpu_strict, false) != 0) {
        fputs("--cpu-mask requires a nonzero hexadecimal mask of allowed CPUs\n", stderr);
        return 2;
    }
    if (cpu_mask_batch && fg_standalone_configure_cpu_affinity(cpu_mask_batch, cpu_strict_batch, true) != 0) {
        fputs("--cpu-mask-batch requires a nonzero hexadecimal mask of allowed CPUs\n", stderr);
        return 2;
    }
    const int server_mode = argc >= 3 && strcmp(argv[2], "--serve") == 0;
    if (server_mode && (argc == 4 || argc == 5)) {
        return serve(argv[1], positive_integer(argv[3]), argc == 5 ? positive_integer(argv[4]) : 0, context);
    }
    if (server_mode || (argc != 3 && argc != 4)) {
        fputs("options: --ctx-size N --batch-size N --ubatch-size N --cache-type-k q4_0 "
              "--cache-type-v q4_0 --expert-cache-mib N\n"
              "         --threads N --threads-batch N --cpu-mask HEX --cpu-mask-batch HEX "
              "--cpu-strict 0|1 --cpu-strict-batch 0|1\n", stderr);
        fprintf(stderr, "usage: %s [OPTIONS] MODEL.gguf PROMPT [MAX_TOKENS]\n", program);
        fprintf(stderr, "       %s [OPTIONS] MODEL.gguf --serve PORT [MAX_CONNECTIONS]\n", program);
        return 2;
    }
    int64_t max_tokens = 1;
    if (argc == 4) {
        const int64_t value = positive_integer(argv[3]);
        if (value <= 0 ||
            (uintmax_t) value > SIZE_MAX / sizeof(int64_t)) {
            fputs("invalid maximum token count\n", stderr);
            return 2;
        }
        max_tokens = (int64_t) value;
    }

    int64_t * ids = malloc((size_t) max_tokens * sizeof(*ids));
    size_t capacity = 256;
    unsigned char * text = malloc(capacity);
    int status = 1;
    if (!ids || !text) goto done;

    fg_goose_host_ensure_init();
    hose_host_load_context(argv[1], context);
    const int64_t count = hose_generate((const unsigned char *) argv[2], (int64_t) strlen(argv[2]),
                                        max_tokens, ids, NULL, 0, NULL);
    if (count <= 0) {
        fputs("inference failed\n", stderr);
        goto done;
    }
    for (int64_t i = 0; i < count; ++i) {
        int64_t length = hose_detokenize(ids + i, 1, text, (int64_t) capacity);
        while (length < 0) {
            if (capacity > SIZE_MAX / 2 || capacity > INT64_MAX / 2) goto done;
            unsigned char * grown = realloc(text, capacity * 2);
            if (!grown) goto done;
            text = grown;
            capacity *= 2;
            length = hose_detokenize(ids + i, 1, text, (int64_t) capacity);
        }
        if (fwrite(text, 1, (size_t) length, stdout) != (size_t) length) goto done;
    }
    if (putchar('\n') == EOF || fflush(stdout) == EOF) goto done;
    status = 0;

done:
    free(text);
    free(ids);
    return status;
}
