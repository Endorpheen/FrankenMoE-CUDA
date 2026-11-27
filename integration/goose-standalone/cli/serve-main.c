// gooseinfer-serve: the standalone runtime's HTTP entry point.
//
// Every decision is Goose (goose/server/http-*.goose): request parsing, routing,
// the API-key policy, CORS, the response format and keep-alive. This file owns
// argv, the socket loop and nothing else - it decides no header and no status.
//
// No libllama. The listener binds an ephemeral port by default, because port 9101
// belongs to the GooseInfer runtime server and a second process there would be a
// conflict, not a test (rule 21).
//
//   gooseinfer-serve [--port N] [--key K]... [--origin O]... [--credentials] [--once]
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

// The signal HANDLER only records that a signal arrived: format strings, buffered
// I/O and any Goose call are not async-signal-safe. The DECISION - one interrupt runs
// the shutdown, a second forces exit - is Goose (goose/server/shutdown-policy.goose,
// on_signal), and it is asked from the loop, not from the handler. A local "any
// signal means stop" was a second implementation of that policy.
static volatile sig_atomic_t signals_seen = 0;

static void mark_signal(int sig) {
    (void) sig;
    signals_seen++;
}

static void usage(void) {
    fprintf(stderr,
            "usage: gooseinfer-serve [--port N] [--key K]... [--origin O]...\n"
            "                        [--credentials] [--once]\n"
            "\n"
            "  --port N        listen on N; 0 (the default) asks the kernel for one\n"
            "  --key K         accept K as an API key; repeatable\n"
            "  --origin O      allow O as a CORS origin; repeatable\n"
            "  --credentials   send Access-Control-Allow-Credentials\n"
            "  --once          serve one connection and exit\n"
            "\n"
            "Port 9101 belongs to the GooseInfer runtime server; the default here is\n"
            "an ephemeral port so this process cannot collide with it.\n");
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    int64_t port = 0;
    int once = 0, credentials = 0;

    // The key and origin lists are packed as one buffer with count + 1 offsets,
    // the layout goose/server/api-key-policy.goose and cors-policy.goose expect.
    static uint8_t keys_blob[4096];
    int64_t keys_off[65];
    int64_t keys_n = 0;
    size_t keys_len = 0;

    // cors_allow_origin takes ONE origins string, exactly as the native cors_origins
    // configuration does - "*", "localhost" or a literal. Not a list: a list would be
    // a different decision than the one the module owns.
    const char * origins = "";
    size_t origins_len = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = strtoll(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--key") == 0 && i + 1 < argc) {
            const char * k = argv[++i];
            const size_t n = strlen(k);
            if (keys_n < 64 && keys_len + n <= sizeof(keys_blob)) {
                keys_off[keys_n++] = (int64_t) keys_len;
                memcpy(keys_blob + keys_len, k, n);
                keys_len += n;
            }
        } else if (strcmp(argv[i], "--origin") == 0 && i + 1 < argc) {
            origins = argv[++i];
            origins_len = strlen(origins);
        } else if (strcmp(argv[i], "--credentials") == 0) {
            credentials = 1;
        } else if (strcmp(argv[i], "--once") == 0) {
            once = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "gooseinfer-serve: unknown argument %s\n", argv[i]);
            usage();
            return 2;
        }
    }
    keys_off[keys_n] = (int64_t) keys_len;

    if (port == 9101) {
        fprintf(stderr, "gooseinfer-serve: 9101 belongs to the GooseInfer runtime server;"
                        " choose another port (rule 21)\n");
        return 2;
    }

    int64_t want[1];
    want[0] = port;
    const int64_t fd = hl_open((sl_i64){ want, 1 });
    if (fd < 0) {
        fprintf(stderr, "gooseinfer-serve: cannot listen: %s\n", strerror(errno));
        return 1;
    }
    printf("gooseinfer-serve: listening on port %lld\n", (long long) want[0]);
    fflush(stdout);

    signal(SIGINT, mark_signal);
    signal(SIGTERM, mark_signal);

    static uint8_t req_buf[16384];
    static uint8_t resp_buf[65536];

    int served = 0;
    int64_t interrupting = 0;
    while (1) {
        const int64_t n = hl_serve_keep(fd, (sl_u8){ req_buf, (int64_t) sizeof req_buf },
                                        (sl_u8){ resp_buf, (int64_t) sizeof resp_buf },
                                        (sl_u8){ keys_blob, (int64_t) keys_len },
                                        (sl_i64){ keys_off, keys_n + 1 },
                                        (sl_u8){ (uint8_t *) (uintptr_t) origins, (int64_t) origins_len },
                                        credentials, 64);
        if (n < 0) {
            if (signals_seen > 0) {
                // out: 0 action (0 nothing, 1 run shutdown, 2 force exit), 1 exit code,
                // 2 message flag.
                int64_t decision[3] = { 0, 0, 0 };
                on_signal(interrupting, 1, (uint64_t) (uintptr_t) decision);
                if (decision[0] == 2) {
                    if (decision[2] != 0) {
                        fprintf(stderr, "gooseinfer-serve: second interrupt, forcing exit\n");
                    }
                    hl_close(fd);
                    return decision[1] != 0 ? (int) decision[1] : 1;
                }
                interrupting = 1;
                break;
            }
            break;
        }
        served += (int) n;
        if (once && served > 0) {
            break;
        }
    }

    hl_close(fd);
    printf("gooseinfer-serve: served %d request(s)\n", served);
    return 0;
}
