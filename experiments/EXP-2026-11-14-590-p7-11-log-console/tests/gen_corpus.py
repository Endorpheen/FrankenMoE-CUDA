#!/usr/bin/env python3
"""Generator for the P7-11 S4/S5 gate.

One table set, five outputs:

  tests/probe_log.cpp       native oracle for common/log.cpp
  tests/probe_debug.cpp     native oracle for common/debug.cpp
  tests/probe_console.cpp   native oracle for common/console.cpp
  build/gate_log.goose      source/log-policy.goose plus a driver over the same
                            log and debug cases
  build/gate_console.goose  source/console-editing.goose plus a driver over the
                            same key scripts

Both sides derive every case from the same index and the same tables, so the
two outputs are diffable line for line. The graded path starts no terminal, no
socket and no logging thread.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Log corpus

LEVELS = [0, 1, 2, 3, 4, 5]          # NONE DEBUG INFO WARN ERROR CONT
TSS = [0, 1, 999, 59999999, 754000123, 3600000000]
MSGS = [
    "hi",
    "abc\n",
    "abc\n\n",
    "",
    "x\ty",
    "\u043f\u0440\u0438\u0432\u0435\u0442",
    'a"b\\c',
    "\x01ctl",
    "line\nline2\n\n",
    '{"a":1}',
]
N_RENDER = 720
THOLDS = [0, 2, 3, 4, 5]

# Debug corpus
FILTER_NAMES = [
    "blk.0.attn_q.weight",
    "attn",
    "x",
    "",
    "model.layers.0.attn",
    "a.b",
    "blk.1",
]
FILTER_SETS = [
    [],
    ["blk."],
    ["attn"],
    ["x", "blk.0"],
    ["model.layers.0"],
    ["nomatch"],
    ["a.b"],
    ["blk.0.attn_q.weight", "a"],
]
NE_CASES = [
    [1, 1, 1, 1],
    [4096, 32, 1, 1],
    [2, 3, 5, 7],
    [0, 1, 1, 1],
]
DUMP_CASES = [
    ([2, 1, 1, 1], [1.25, 0.5], 0),
    ([3, 2, 1, 1], [1.25, 2.0, 0.5, -1.5, 0.0, 3.75], 0),
    ([2, 1, 1, 1], [1.0, float("nan")], 0),
    ([2, 1, 1, 1], [1.0, float("nan")], 1),
    ([4, 2, 1, 1], [float("inf"), 1.0, 0.5, -1.5, 2.0, 0.25, 0.0, 0.0], 0),
    ([4, 1, 1, 1], [float("inf"), 1.0, 0.0, 0.0], 0),
    ([2, 1, 1, 1], [1.25, 0.5], 1),
    ([8, 1, 1, 1], [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0], 0),
    ([1, 8, 1, 1], [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0], 0),
    ([1, 1, 8, 1], [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0], 0),
    ([3, 2, 1, 1], [-1.25, -0.5, 0.0, 0.0, 0.25, 0.5], 0),
]

# ---------------------------------------------------------------------------
# Console corpus

SCRIPTS = [
    "abc\r",
    "abc\x1b[D\x1b[DX\r",
    "abc\x1b[H\x1b[F\r",
    "abc\x1b[H\x1b[3~\r",
    "abc\x1b[H\x1b[4~\r",
    "abc\x1b[1~\x1b[7~X\r",
    "abc\x7f\r",
    "abc\x08\r",
    "\u043f\u0440\u0438\u0432\u0435\u0442\r",
    "\u043f\u0440\x1b[D\x1b[DX\r",
    "a b  c\x1b[1;5D\r",
    "a b  c\x1b[H\x1b[1;5C\r",
    "a b  c\x1b[H\x1b[1;5C\x1b[1;5C\r",
    "abc\x1b[A\r",
    "line1\r",
    "line2\r",
    "\x1b[A\x1b[A\r",
    "abc\\\r",
    "def\r",
    "abc/\r",
    "/\r",
    "\\\r",
    " /\r",
    "abc\x04",
    "abc\x1b[Z\r",
    "abc\x1b\x1bX\r",
    "abc\x1b[99~\r",
    "abc\x1b[3;5~\r",
    "abc\x1b[4;5~\r",
    "abc\x1b[2~\r",
    "a\tb\r",
    "abc\x1b[1K\r",
    "\x1b[A\r",
    "\u043f\u0440\u0438 \u0432\u0435\u0442\x1b[1;5D\r",
    "abc\x1b[D\x1b[D\x1b[3~\r",
    "abc\x1b[D\x1b[D\x1b[3~\x1b[3~\r",
    "abcdef\x1b[H\x1b[C\x1b[CX\x7f\r",
    "x\x1b[H\x1b[3~\r",
    "abc\x1b[DZ\x7f\r",
    "\x1b[1;5C\r",
    "  \x1b[1;5D\r",
    "a\x01b\r",
    "abc\x1b[5~\r",
    "abc\x1b[6~\r",
    "ab \x1b[H\x1b[C\x1b[3~\r",
    "\x1b[D\r",
    "abc\x1b[1;2D\r",
    "abc\n",
    "",
]
SCRIPT_MULTI = [1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 1, 1]


def _random_scripts(n, seed=20261114):
    """Deterministic mixed key scripts: every editing branch is reachable.

    The same list is emitted into both probes, so the port and the native code
    see byte-identical input. The generator is a plain LCG, not random(), so
    the corpus is stable across Python versions.
    """
    tokens = ["a", "b", "c", " ", "\u0434", "\u0436", "\x7f", "\x08",
              "\x1b[D", "\x1b[C", "\x1b[H", "\x1b[F", "\x1b[3~",
              "\x1b[1;5D", "\x1b[1;5C", "\x1b[A", "\x1b[B",
              "\x1b[1~", "\x1b[4~", "\\", "/", "\t", "\x01",
              "\x1b[Z", "\x1b[99~", "\x1b[2~"]
    multi = [0] * n
    scripts = []
    state = seed
    for i in range(n):
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        count = 3 + (state >> 8) % 10
        parts = []
        for _ in range(count):
            state = (state * 1103515245 + 12345) & 0x7FFFFFFF
            parts.append(tokens[(state >> 4) % len(tokens)])
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        tail = (state >> 3) % 4
        if tail == 0:
            parts.append("\x04")
        elif tail == 1:
            parts.append("\n")
            multi[i] = 1
        else:
            parts.append("\r")
        scripts.append("".join(parts))
    return scripts, multi


RANDOM_SCRIPTS, RANDOM_MULTI = _random_scripts(120)
SCRIPTS = SCRIPTS + RANDOM_SCRIPTS
SCRIPT_MULTI = SCRIPT_MULTI + RANDOM_MULTI

DECODE_TEXTS = [
    "abc",
    "\u043f\u0440\u0438",
    "\u4e2d",
    "\U0001f600",
    "\xc3",
    "\xe0\x80",
    "\xc0\x80",
    "\xed\xa0\x80",
    "\xf5\x80\x80\x80",
    "a\x80b",
]
ENCODE_CPS = [0, 65, 127, 128, 0x7FF, 0x800, 0xFFFF, 0x10000, 0x10FFFF, 0x110000, 0x1F600]
CTRL_PARAMS = ["", "5", "1;5", "5;5", "15", "05", "5a", ";5", "2;", "3;5", "10;5", "5;10"]
WIDTH_CPS = [32, 65, 1, 127, 0x410, 0xE9]
SPACE_CPS = [32, 9, 10, 13, 0x410, 65]


# ---------------------------------------------------------------------------
# Emission helpers

def cpp_str(s):
    out = []
    for b in s.encode():
        if 32 <= b < 127 and chr(b) not in '"\\':
            out.append(chr(b))
        else:
            # octal escapes are at most three digits, so a following hex digit
            # in the literal cannot extend them
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def cpp_array(name, items):
    return "static const char *%s[] = {%s};\n" % (name, ", ".join(cpp_str(x) for x in items))


def cpp_int_array(ctype, name, items):
    return "static const %s %s[] = {%s};\n" % (ctype, name, ", ".join(str(x) for x in items))


def goose_str(s):
    out = []
    for b in s.encode():
        if b == 34:
            out.append('\\"')
        elif b == 92:
            out.append('\\\\')
        elif 32 <= b < 127:
            out.append(chr(b))
        else:
            out.append("\\x%02x" % b)
    return '"' + "".join(out) + '"'


def goose_accessor(name, items):
    lines = ["fn %s(k: i64) -> const u8[:] {" % name]
    for idx, val in enumerate(items):
        lines.append("    if k == %d {" % idx)
        lines.append("        return %s;" % goose_str(val))
        lines.append("    }")
    lines.append('    return "";')
    lines.append("}")
    return "\n".join(lines) + "\n\n"


def goose_i64_table(name, items):
    return "let %s: i64[%d] = [%s];\n" % (name, len(items), ", ".join(str(v) for v in items))


def goose_f64(v):
    if v != v:
        return "(0.0 / 0.0)"
    if v == float("inf"):
        return "(1.0 / 0.0)"
    if v == float("-inf"):
        return "(0.0 - 1.0 / 0.0)"
    text = repr(float(v))
    if "." not in text:
        text += ".0"
    return text


def goose_f64_accessor(name, rows):
    lines = ["fn %s(i: i64, j: i64) -> f64 {" % name]
    for idx, row in enumerate(rows):
        lines.append("    if i == %d {" % idx)
        for j, v in enumerate(row):
            lines.append("        if j == %d { return %s; }" % (j, goose_f64(v)))
        lines.append("    }")
    lines.append("    return 0.0;")
    lines.append("}")
    return "\n".join(lines) + "\n\n"


def goose_i64_accessor1(name, items):
    lines = ["fn %s(i: i64) -> i64 {" % name]
    for idx, v in enumerate(items):
        lines.append("    if i == %d { return %d; }" % (idx, v))
    lines.append("    return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n\n"


def goose_i64_accessor2(name, rows):
    lines = ["fn %s(i: i64, j: i64) -> i64 {" % name]
    for idx, row in enumerate(rows):
        lines.append("    if i == %d {" % idx)
        for j, v in enumerate(row):
            lines.append("        if j == %d { return %d; }" % (j, v))
        lines.append("    }")
    lines.append("    return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n\n"


def cpp_f64(v):
    if v != v:
        return "NAN"
    if v == float("inf"):
        return "INFINITY"
    if v == float("-inf"):
        return "-INFINITY"
    text = repr(float(v))
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


# ---------------------------------------------------------------------------
# Case index helpers, shared by the generator and the case-table extractor.

def render_case(i):
    return {
        "level": LEVELS[i % 6],
        "prefix": (i // 7) % 2,
        "ts": TSS[(i // 11) % 6],
        "jsonl": (i // 13) % 2,
        "colors": (i // 17) % 2,
        "is_json": 1 if (i // 19) % 5 == 0 else 0,
        "msg": (i // 23) % len(MSGS),
    }


# ---------------------------------------------------------------------------
# Native log probe

PROBE_LOG = r'''// Native oracle for common/log.cpp, generated by tests/gen_corpus.py.
// The translation unit includes the real log.cpp; only the tty probes and
// ggml_abort are stubbed. Every entry is printed into a memory stream or into
// redirected process streams, so no terminal is needed.
#define _GNU_SOURCE
#include "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/common/log.cpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static int g_tty_can = 0;
static int g_tty_ansi = 1;
bool tty_enable_ansi() { return g_tty_ansi != 0; }
bool tty_can_use_colors() { return g_tty_can != 0; }
extern "C" void ggml_abort(const char * f, int l, const char * fmt, ...) {
    (void) f; (void) l; (void) fmt; abort();
}

__ARRAYS__

static void put(const char * s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char) s[i];
        if (c < 32 || c == 127 || c == '|') printf("?%02x", c);
        else putchar((char) c);
    }
}

static void colors_off() {
    for (size_t k = 0; k < std::size(g_col); k++) g_col[k] = "";
}

static void colors_on() {
    g_col[COMMON_LOG_COL_DEFAULT] = LOG_COL_DEFAULT;
    g_col[COMMON_LOG_COL_BOLD]    = LOG_COL_BOLD;
    g_col[COMMON_LOG_COL_RED]     = LOG_COL_RED;
    g_col[COMMON_LOG_COL_GREEN]   = LOG_COL_GREEN;
    g_col[COMMON_LOG_COL_YELLOW]  = LOG_COL_YELLOW;
    g_col[COMMON_LOG_COL_BLUE]    = LOG_COL_BLUE;
    g_col[COMMON_LOG_COL_MAGENTA] = LOG_COL_MAGENTA;
    g_col[COMMON_LOG_COL_CYAN]    = LOG_COL_CYAN;
    g_col[COMMON_LOG_COL_WHITE]   = LOG_COL_WHITE;
}

static void read_file(FILE * f, std::string & out) {
    fflush(f);
    rewind(f);
    char buf[4096];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
}

int main() {
    int case_id = 0;

    for (int i = 0; i < __N_RENDER__; i++) {
        const int level   = LEVELS[i % 6];
        const int prefix  = (i / 7) % 2;
        const long long ts = TSS[(i / 11) % 6];
        const int jsonl   = (i / 13) % 2;
        const int colors  = (i / 17) % 2;
        const int is_json = ((i / 19) % 5 == 0) ? 1 : 0;
        const int msg_i   = (i / 23) % __NMSG__;

        if (colors) colors_on(); else colors_off();

        common_log_entry e(512);
        e.level = (enum ggml_log_level) level;
        e.prefix = prefix != 0;
        e.timestamp = ts;
        e.jsonl = jsonl != 0;
        e.is_json = is_json != 0;
        strcpy(e.msg.data(), MSGS[msg_i]);

        char * buf = nullptr;
        size_t sz = 0;
        FILE * mem = open_memstream(&buf, &sz);
        e.print(mem);
        fflush(mem);
        fclose(mem);

        printf("R|%d|", case_id);
        put(buf, sz);
        printf("\n");
        free(buf);
        case_id++;
    }

    // Stream choice and the DEBUG stderr gate. print() with no file writes to
    // the process streams, so both are redirected to temporary files.
    colors_off();
    for (int t = 0; t < __NTH__; t++) {
        for (int li = 0; li < 6; li++) {
            for (int j = 0; j < 2; j++) {
                common_log_verbosity_thold = THOLDS[t];
                common_log_jsonl = j != 0;

                FILE * so = tmpfile();
                FILE * se = tmpfile();
                int fd_o = dup(fileno(stdout));
                int fd_e = dup(fileno(stderr));
                fflush(stdout);
                fflush(stderr);
                dup2(fileno(so), fileno(stdout));
                dup2(fileno(se), fileno(stderr));

                common_log_entry e(256);
                e.level = (enum ggml_log_level) LEVELS[li];
                e.prefix = true;
                e.timestamp = 0;
                e.jsonl = j != 0;
                e.is_json = false;
                strcpy(e.msg.data(), "hi");
                e.print(nullptr);

                fflush(stdout);
                fflush(stderr);
                dup2(fd_o, fileno(stdout));
                dup2(fd_e, fileno(stderr));
                close(fd_o);
                close(fd_e);

                std::string o, er;
                read_file(so, o);
                read_file(se, er);
                fclose(so);
                fclose(se);

                int stream = -1;
                const std::string * got = nullptr;
                if (!o.empty()) { stream = 0; got = &o; }
                else if (!er.empty()) { stream = 1; got = &er; }

                printf("T|%d|%d|", case_id, stream);
                if (got) put(got->data(), got->size());
                printf("\n");
                case_id++;
            }
        }
    }

    // Default-callback gate: verbosity(level) <= threshold.
    for (int li = 0; li < 6; li++) {
        for (int t = 0; t < 6; t++) {
            printf("V|%d|%d\n", case_id,
                   common_log_get_verbosity((enum ggml_log_level) LEVELS[li]) <= t ? 1 : 0);
            case_id++;
        }
    }

    // level_str, seen through the JSONL wrapper.
    for (int li = 0; li < 6; li++) {
        common_log_entry e(256);
        e.level = (enum ggml_log_level) LEVELS[li];
        e.jsonl = true;
        e.is_json = false;
        e.timestamp = 0;
        strcpy(e.msg.data(), "m");
        char * buf = nullptr;
        size_t sz = 0;
        FILE * mem = open_memstream(&buf, &sz);
        e.print(mem);
        fflush(mem);
        fclose(mem);
        printf("S|%d|", case_id);
        put(buf, sz);
        printf("\n");
        free(buf);
        case_id++;
    }

    // Color enablement: the AUTO mode asks tty_can_use_colors, then every mode
    // is ANDed with tty_enable_ansi. common_log_init starts the writer thread,
    // but the graded print path never uses it; the logger is freed per case.
    const int modes[3] = {-1, 0, 1};
    for (int mi = 0; mi < 3; mi++) {
        for (int can = 0; can < 2; can++) {
            for (int ansi = 0; ansi < 2; ansi++) {
                g_tty_can = can;
                g_tty_ansi = ansi;
                struct common_log * lg = common_log_init();
                common_log_set_colors(lg, (log_colors) modes[mi]);
                int got = common_log_get_colors(lg) ? 1 : 0;
                common_log_free(lg);
                printf("C|%d|%d\n", case_id, got);
                case_id++;
            }
        }
    }
    return 0;
}
'''


def gen_probe_log():
    arrays = (cpp_int_array("int", "LEVELS", LEVELS) +
              cpp_int_array("long long", "TSS", TSS) +
              cpp_int_array("int", "THOLDS", THOLDS) +
              cpp_array("MSGS", MSGS))
    s = PROBE_LOG.replace("__ARRAYS__", arrays)
    s = s.replace("__N_RENDER__", str(N_RENDER))
    s = s.replace("__NMSG__", str(len(MSGS)))
    s = s.replace("__NTH__", str(len(THOLDS)))
    return s


# ---------------------------------------------------------------------------
# Native debug probe

PROBE_DEBUG = r'''// Native oracle for common/debug.cpp, generated by tests/gen_corpus.py.
// The translation unit includes the real debug.cpp with the ggml accessors
// stubbed, so no tensor is allocated and no backend is touched. The log sink
// is a string, so the printed tensor text is compared byte for byte. A NaN
// abort case runs in a child process because the native code calls exit(0).
#include "ggml.h"
#include "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/common/debug.cpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

static std::string g_out;
static int g_pipe = -1;

struct common_log;
common_log * common_log_main() { return (common_log *) 1; }
int common_log_get_verbosity_thold() { return 5; }
void common_log_flush(struct common_log *) {}
void common_log_add(struct common_log *, enum ggml_log_level, const char * fmt, ...) {
    char b[8192];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    if (g_pipe >= 0) {
        size_t n = strlen(b);
        ssize_t ignored = write(g_pipe, b, n);
        (void) ignored;
    } else {
        g_out += b;
    }
}

extern "C" {
const char * ggml_type_name(enum ggml_type) { return "f32"; }
const char * ggml_op_desc(const struct ggml_tensor *) { return "op"; }
bool ggml_backend_buffer_is_host(ggml_backend_buffer_t) { return true; }
size_t ggml_nbytes(const struct ggml_tensor * t) {
    return (size_t) t->ne[0] * t->ne[1] * t->ne[2] * t->ne[3] * 4;
}
void ggml_backend_tensor_get(const struct ggml_tensor *, void *, size_t, size_t) {}
bool ggml_is_quantized(enum ggml_type) { return true; }
float ggml_fp16_to_fp32(ggml_fp16_t) { return 0.0f; }
float ggml_bf16_to_fp32(ggml_bf16_t) { return 0.0f; }
void ggml_abort(const char * f, int l, const char * fmt, ...) { (void) f; (void) l; (void) fmt; abort(); }
}

__ARRAYS__

static void put(const char * s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char) s[i];
        if (c < 32 || c == 127 || c == '|') printf("?%02x", c);
        else putchar((char) c);
    }
}

static ggml_tensor make_tensor(const char * name, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, float * data) {
    ggml_tensor t;
    memset(&t, 0, sizeof t);
    snprintf(t.name, sizeof t.name, "%s", name);
    t.type = GGML_TYPE_F32;
    t.ne[0] = ne0; t.ne[1] = ne1; t.ne[2] = ne2; t.ne[3] = ne3;
    t.nb[0] = 4;
    t.nb[1] = t.nb[0] * ne0;
    t.nb[2] = t.nb[1] * ne1;
    t.nb[3] = t.nb[2] * ne2;
    t.data = data;
    return t;
}

// Runs one dump in a child, because the native code exits on a NaN abort.
static int run_dump(const int64_t * ne, const float * vals, int abort_on_nan, std::string & text) {
    fflush(stdout); // a child that exits inside the dump must not flush our buffer
    int fds[2];
    if (pipe(fds) != 0) return -1;
    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        g_pipe = fds[1];
        int n = (int) (ne[0] * ne[1] * ne[2] * ne[3]);
        float * copy = (float *) malloc(sizeof(float) * (size_t) n);
        memcpy(copy, vals, sizeof(float) * (size_t) n);
        ggml_tensor t = make_tensor("t", ne[0], ne[1], ne[2], ne[3], copy);
        common_debug_print_tensor((uint8_t *) copy, GGML_TYPE_F32, t.ne, t.nb, 3, abort_on_nan != 0);
        ssize_t ignored = write(fds[1], "RET\n", 4);
        (void) ignored;
        close(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    char buf[4096];
    ssize_t nr;
    text.clear();
    while ((nr = read(fds[0], buf, sizeof buf)) > 0) text.append(buf, nr);
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (text.size() >= 4 && text.compare(text.size() - 4, 4, "RET\n") == 0) {
        text.resize(text.size() - 4);
        return 0;
    }
    return 1;
}

int main() {
    int case_id = __DEBUG_BASE__;
    // Filter matching through the real callback. The tensor type is reported
    // as quantized, so the value dump is skipped and only the header line
    // reveals whether the name matched.
    for (int si = 0; si < __NSETS__; si++) {
        for (int ni = 0; ni < __NNAMES__; ni++) {
            common_debug_cb_user_data cb;
            for (int k = 0; k < SET_COUNT[si]; k++) {
                const char * pat = SET_ITEMS[SET_OFF[si] + k];
                cb.pimpl->tensor_filters.emplace_back(std::string("^") + pat, std::regex::optimize);
            }
            cb.pimpl->abort_on_nan = false;

            float data[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            ggml_tensor src0 = make_tensor("src0", 1, 1, 1, 1, data);
            ggml_tensor t = make_tensor(NAMES[ni], 1, 1, 1, 1, data);
            t.src[0] = &src0;
            t.src[1] = nullptr;

            g_out.clear();
            common_debug_cb_eval(&t, false, &cb);
            printf("F|%d|%d\n", case_id, g_out.empty() ? 0 : 1);
            case_id++;
        }
    }

    for (int i = 0; i < __NNE__; i++) {
        printf("E|%d|%lld, %lld, %lld, %lld\n", case_id,
               (long long) NE[i][0], (long long) NE[i][1],
               (long long) NE[i][2], (long long) NE[i][3]);
        case_id++;
    }

    for (int i = 0; i < __NDUMP__; i++) {
        int64_t ne[4];
        for (int k = 0; k < 4; k++) ne[k] = DUMP_NE[i][k];
        float vals[64];
        int n = (int) (ne[0] * ne[1] * ne[2] * ne[3]);
        for (int k = 0; k < n; k++) vals[k] = DUMP_VALS[i][k];
        std::string text;
        int aborted = run_dump(ne, vals, DUMP_ABORT[i], text);
        printf("D|%d|%d|", case_id, aborted);
        put(text.data(), text.size());
        printf("\n");
        case_id++;
    }
    return 0;
}
'''


LOG_CASE_COUNT = N_RENDER + len(THOLDS) * 6 * 2 + 6 * 6 + 6 + 12


def gen_probe_debug():
    offsets, counts, flat = [], [], []
    for s in FILTER_SETS:
        offsets.append(len(flat))
        counts.append(len(s))
        flat.extend(s)
    arrays = (cpp_array("NAMES", FILTER_NAMES) +
              cpp_array("SET_ITEMS", flat) +
              "static const int SET_OFF[] = {%s};\n" % ", ".join(str(x) for x in offsets) +
              "static const int SET_COUNT[] = {%s};\n" % ", ".join(str(x) for x in counts) +
              "static const int64_t NE[][4] = {%s};\n" % ", ".join(
                  "{%s}" % ", ".join(str(v) for v in row) for row in NE_CASES) +
              "static const int64_t DUMP_NE[][4] = {%s};\n" % ", ".join(
                  "{%s}" % ", ".join(str(v) for v in c[0]) for c in DUMP_CASES) +
              "static const int DUMP_ABORT[] = {%s};\n" % ", ".join(str(c[2]) for c in DUMP_CASES) +
              "static const float DUMP_VALS[][64] = {%s};\n" % ", ".join(
                  "{%s}" % ", ".join(cpp_f64(v) for v in c[1]) for c in DUMP_CASES))
    s = PROBE_DEBUG.replace("__ARRAYS__", arrays)
    s = s.replace("__NSETS__", str(len(FILTER_SETS)))
    s = s.replace("__NNAMES__", str(len(FILTER_NAMES)))
    s = s.replace("__NNE__", str(len(NE_CASES)))
    s = s.replace("__NDUMP__", str(len(DUMP_CASES)))
    s = s.replace("__DEBUG_BASE__", str(LOG_CASE_COUNT))
    return s


# ---------------------------------------------------------------------------
# Native console probe

PROBE_CONSOLE = r'''// Native oracle for common/console.cpp, generated by tests/gen_corpus.py.
// readline_advanced is driven by a key script from a regular file on stdin,
// with simple_io = false and no tty, so the whole editing loop runs and its
// bytes are captured from a memory stream. The UTF-8 primitives are called
// directly through the translation unit.
#define _GNU_SOURCE
#include "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/common/console.cpp"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

struct common_log;
common_log * common_log_main() { return nullptr; }
void common_log_flush(common_log *) {}

__ARRAYS__

static void put(const char * s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char) s[i];
        if (c < 32 || c == 127 || c == '|') printf("?%02x", c);
        else putchar((char) c);
    }
}

int main(int argc, char ** argv) {
    const char * dir = argc > 1 ? argv[1] : ".";

    setlocale(LC_ALL, "C.UTF-8");
    console::simple_io = false;
    console::advanced_display = false;
    console::tty = nullptr;

    int case_id = 0;

    // primitives
    for (int ti = 0; ti < __NDEC__; ti++) {
        const std::string s = DECODE_TEXTS[ti];
        size_t pos = 0;
        while (pos < s.size()) {
            size_t adv = 0;
            char32_t cp = console::decode_utf8(s, pos, adv);
            printf("P|%d|%u|%zu\n", case_id, (unsigned) cp, adv);
            case_id++;
            pos += adv;
        }
        printf("P|%d|-|%zu\n", case_id, s.size());
        case_id++;
    }

    for (int ti = 0; ti < __NENC__; ti++) {
        std::string out;
        console::append_utf8((char32_t) ENCODE_CPS[ti], out);
        printf("E|%d|", case_id);
        put(out.data(), out.size());
        printf("\n");
        case_id++;
    }

    for (int ti = 0; ti < __NDEC__; ti++) {
        const std::string s = DECODE_TEXTS[ti];
        for (size_t pos = 0; pos <= s.size(); pos++) {
            printf("X|%d|%zu|%zu|%zu\n", case_id, pos,
                   console::prev_utf8_char_pos(s, pos), console::next_utf8_char_pos(s, pos));
            case_id++;
        }
    }

    for (int ti = 0; ti < __NCTRL__; ti++) {
        printf("K|%d|%d\n", case_id, console::has_ctrl_modifier(CTRL_PARAMS[ti]) ? 1 : 0);
        case_id++;
    }

    for (int ti = 0; ti < __NW__; ti++) {
        const char32_t cp = (char32_t) WIDTH_CPS[ti];
        int w = (int) wcwidth(cp);
        if (w < 0) w = 0; // the caller's clamp, see put_codepoint width handling
        printf("W|%d|%d|%d\n", case_id, w, console::is_space_codepoint(cp) ? 1 : 0);
        case_id++;
    }
    for (int ti = 0; ti < __NSP__; ti++) {
        const char32_t cp = (char32_t) SPACE_CPS[ti];
        printf("O|%d|%d\n", case_id, console::is_space_codepoint(cp) ? 1 : 0);
        case_id++;
    }

    // key scripts: one readline call each, history persists across calls
    for (int i = 0; i < __NSCR__; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/keys_%03d.bin", dir, i);
        FILE * f = fopen(path, "wb");
        fwrite(SCRIPTS[i], 1, SCRIPT_LEN[i], f);
        fclose(f);

        FILE * sin = freopen(path, "rb", stdin);
        (void) sin;

        char * obuf = nullptr;
        size_t osz = 0;
        FILE * mem = open_memstream(&obuf, &osz);
        console::out = mem;

        std::string line;
        bool more = console::readline(line, SCRIPT_MULTI[i] != 0);

        fflush(mem);
        fclose(mem);
        console::out = stdout;

        printf("C|%d|%d|", case_id, more ? 1 : 0);
        put(line.data(), line.size());
        printf("|");
        put(obuf, osz);
        printf("|%zu|", console::history.entries.size());
        for (size_t k = 0; k < console::history.entries.size(); k++) {
            if (k) printf("|");
            put(console::history.entries[k].data(), console::history.entries[k].size());
        }
        printf("\n");
        free(obuf);
        case_id++;
    }
    return 0;
}
'''


def gen_probe_console():
    arrays = (cpp_array("DECODE_TEXTS", DECODE_TEXTS) +
              cpp_array("SCRIPTS", SCRIPTS) +
              "static const int SCRIPT_LEN[] = {%s};\n" % ", ".join(str(len(s.encode())) for s in SCRIPTS) +
              "static const int SCRIPT_MULTI[] = {%s};\n" % ", ".join(str(v) for v in SCRIPT_MULTI) +
              "static const int ENCODE_CPS[] = {%s};\n" % ", ".join(str(v) for v in ENCODE_CPS) +
              cpp_array("CTRL_PARAMS", CTRL_PARAMS) +
              "static const int WIDTH_CPS[] = {%s};\n" % ", ".join(str(v) for v in WIDTH_CPS) +
              "static const int SPACE_CPS[] = {%s};\n" % ", ".join(str(v) for v in SPACE_CPS))
    s = PROBE_CONSOLE.replace("__ARRAYS__", arrays)
    s = s.replace("__NDEC__", str(len(DECODE_TEXTS)))
    s = s.replace("__NENC__", str(len(ENCODE_CPS)))
    s = s.replace("__NCTRL__", str(len(CTRL_PARAMS)))
    s = s.replace("__NW__", str(len(WIDTH_CPS)))
    s = s.replace("__NSP__", str(len(SPACE_CPS)))
    s = s.replace("__NSCR__", str(len(SCRIPTS)))
    return s


# ---------------------------------------------------------------------------
# Goose drivers

GOOSE_COMMON = r'''fn app_s(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;
    while i < s.len {
        dst[p] = s[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn app_c(dst: u8[:], at: i64, c: u8) -> i64 {
    dst[at] = c;
    return at + 1;
}

fn put_int(dst: u8[:], at: i64, v: i64) -> i64 {
    var p = at;
    var x = v;
    if x < 0 {
        dst[p] = 45;
        p = p + 1;
        x = 0 - x;
    }
    if x == 0 {
        dst[p] = 48;
        return p + 1;
    }
    var buf: u8[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    while x > 0 {
        buf[n] = (48 + (x % 10)) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;
    while i > 0 {
        i = i - 1;
        dst[p] = buf[i];
        p = p + 1;
    }
    return p;
}

fn put_fld(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;
    while i < s.len {
        let c = s[i];
        if c < 32 || c == 127 || c == 124 {
            p = app_c(dst, p, 63);
            let d: const u8[:] = "0123456789abcdef";
            p = app_c(dst, p, d[((c as i64) / 16) % 16]);
            p = app_c(dst, p, d[(c as i64) % 16]);
        } else {
            p = app_c(dst, p, c);
        }
        i = i + 1;
    }
    return p;
}
'''

GOOSE_LOG_DRIVER = r'''
__TABLES__

fn main() {
    var case_id: i64 = 0;

    // render cases
    var i: i64 = 0;
    while i < __N_RENDER__ {
        let level = LP_LEVELS[i % 6];
        let prefix = (i / 7) % 2;
        let ts = LP_TSS[(i / 11) % 6];
        let jsonl = (i / 13) % 2;
        let colors = (i / 17) % 2;
        let is_json = if (i / 19) % 5 == 0 { 1 } else { 0 };
        let msg = t_msg((i / 23) % __NMSG__);

        var buf: u8[4096] = [__Z4096__];
        let n = lp_render(level, prefix, ts, jsonl, is_json, colors, msg, buf);

        var line: u8[8192] = [__Z8192__];
        var p: i64 = 0;
        p = app_c(line, p, 82);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_fld(line, p, buf[0..n]);
        print(line[0..p]);
        case_id = case_id + 1;
        i = i + 1;
    }

    // stream choice and the DEBUG stderr gate
    var t: i64 = 0;
    while t < __NTH__ {
        var li: i64 = 0;
        while li < 6 {
            var j: i64 = 0;
            while j < 2 {
                let stream = lp_stream(LP_LEVELS[li], j, LP_THOLDS[t]);
                var line: u8[512] = [__Z512__];
                var p: i64 = 0;
                p = app_c(line, p, 84);
                p = app_c(line, p, 124);
                p = put_int(line, p, case_id);
                p = app_c(line, p, 124);
                p = put_int(line, p, stream);
                p = app_c(line, p, 124);
                if stream >= 0 {
                    var buf: u8[256] = [__Z256__];
                    let n = lp_render(LP_LEVELS[li], 1, 0, j, 0, 0, "hi", buf);
                    p = put_fld(line, p, buf[0..n]);
                }
                print(line[0..p]);
                case_id = case_id + 1;
                j = j + 1;
            }
            li = li + 1;
        }
        t = t + 1;
    }

    // default-callback gate
    var vi: i64 = 0;
    while vi < 6 {
        t = 0;
        while t < 6 {
            var line: u8[128] = [__Z128__];
            var p: i64 = 0;
            p = app_c(line, p, 86);
            p = app_c(line, p, 124);
            p = put_int(line, p, case_id);
            p = app_c(line, p, 124);
            p = put_int(line, p, lp_passes(LP_LEVELS[vi], t));
            print(line[0..p]);
            case_id = case_id + 1;
            t = t + 1;
        }
        vi = vi + 1;
    }

    // level names
    var li: i64 = 0;
    while li < 6 {
        var buf: u8[256] = [__Z256__];
        let n = lp_render(LP_LEVELS[li], 1, 0, 1, 0, 0, "m", buf);
        var line: u8[512] = [__Z512__];
        var p: i64 = 0;
        p = app_c(line, p, 83);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_fld(line, p, buf[0..n]);
        print(line[0..p]);
        case_id = case_id + 1;
        li = li + 1;
    }

    // color enablement
    var mi: i64 = 0;
    while mi < 3 {
        var can: i64 = 0;
        while can < 2 {
            var ansi: i64 = 0;
            while ansi < 2 {
                var mode: i64 = 0;
                if mi == 0 { mode = 0 - 1; } else if mi == 2 { mode = 1; }
                var line: u8[128] = [__Z128__];
                var p: i64 = 0;
                p = app_c(line, p, 67);
                p = app_c(line, p, 124);
                p = put_int(line, p, case_id);
                p = app_c(line, p, 124);
                p = put_int(line, p, lp_colors(mode, can, ansi));
                print(line[0..p]);
                case_id = case_id + 1;
                ansi = ansi + 1;
            }
            can = can + 1;
        }
        mi = mi + 1;
    }

    // debug filter
    var si: i64 = 0;
    while si < __NSETS__ {
        var ni: i64 = 0;
        while ni < __NNAMES__ {
            var off: i64[32] = [__Z32I__];
            var len: i64[32] = [__Z32I__];
            var k: i64 = 0;
            while k < LP_SET_COUNT[si] {
                let it = LP_SET_OFF[si] + k;
                off[k] = LP_ITEM_OFF[it];
                len[k] = LP_ITEM_LEN[it];
                k = k + 1;
            }
            let matched = lp_filter_match(LP_PAT_BLOB, off, len, LP_SET_COUNT[si], t_name(ni));
            var line: u8[128] = [__Z128__];
            var p: i64 = 0;
            p = app_c(line, p, 70);
            p = app_c(line, p, 124);
            p = put_int(line, p, case_id);
            p = app_c(line, p, 124);
            p = put_int(line, p, matched);
            print(line[0..p]);
            case_id = case_id + 1;
            ni = ni + 1;
        }
        si = si + 1;
    }

    // ne_string
    var ei: i64 = 0;
    while ei < __NNE__ {
        var ne: i64[4] = [0, 0, 0, 0];
        var k2: i64 = 0;
        while k2 < 4 {
            ne[k2] = t_ne(ei, k2);
            k2 = k2 + 1;
        }
        var buf: u8[256] = [__Z256__];
        let n = lp_ne_string(ne, buf);
        var line: u8[512] = [__Z512__];
        var p: i64 = 0;
        p = app_c(line, p, 69);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_fld(line, p, buf[0..n]);
        print(line[0..p]);
        case_id = case_id + 1;
        ei = ei + 1;
    }

    // F32 dump and the NaN abort
    var di: i64 = 0;
    while di < __NDUMP__ {
        var ne: i64[4] = [0, 0, 0, 0];
        var k3: i64 = 0;
        while k3 < 4 {
            ne[k3] = t_ne2(di, k3);
            k3 = k3 + 1;
        }
        let total = ne[0] * ne[1] * ne[2] * ne[3];
        var vals: f64[64] = [__Z64F__];
        var v: i64 = 0;
        while v < total {
            vals[v] = t_val(di, v);
            v = v + 1;
        }
        var buf: u8[8192] = [__Z8192__];
        var outn: i64[1] = [0];
        let aborted = lp_dump_f32(ne, vals, 3, t_abort(di), buf, outn);
        var line: u8[8192] = [__Z8192__];
        var p: i64 = 0;
        p = app_c(line, p, 68);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_int(line, p, aborted);
        p = app_c(line, p, 124);
        p = put_fld(line, p, buf[0..__OUTLEN__]);
        print(line[0..p]);
        case_id = case_id + 1;
        di = di + 1;
    }
}
'''


def gen_goose_log(port):
    tables = []
    tables.append(goose_i64_table("LP_LEVELS", LEVELS))
    tables.append(goose_i64_table("LP_TSS", TSS))
    tables.append(goose_i64_table("LP_THOLDS", THOLDS))
    tables.append(goose_accessor("t_msg", MSGS))
    tables.append(goose_accessor("t_name", FILTER_NAMES))

    offsets, counts, flat = [], [], []
    for s in FILTER_SETS:
        offsets.append(len(flat))
        counts.append(len(s))
        flat.extend(s)
    item_off, item_len, blob = [], [], []
    for p in flat:
        item_off.append(len(blob))
        item_len.append(len(p.encode()))
        blob.extend(p.encode())
    tables.append(goose_i64_table("LP_SET_OFF", offsets))
    tables.append(goose_i64_table("LP_SET_COUNT", counts))
    tables.append(goose_i64_table("LP_ITEM_OFF", item_off))
    tables.append(goose_i64_table("LP_ITEM_LEN", item_len))
    tables.append("let LP_PAT_BLOB: u8[%d] = [%s];\n" % (len(blob), ", ".join(str(b) for b in blob)))
    tables.append(goose_i64_accessor2("t_ne", NE_CASES))
    tables.append(goose_i64_accessor2("t_ne2", [c[0] for c in DUMP_CASES]))
    tables.append(goose_i64_accessor1("t_abort", [c[2] for c in DUMP_CASES]))
    tables.append(goose_f64_accessor("t_val", [c[1] for c in DUMP_CASES]))

    d = GOOSE_LOG_DRIVER
    d = d.replace("p = put_fld(line, p, buf[0..__OUTLEN__]);", "p = put_fld(line, p, buf[0..outn[0]]);")

    d = d.replace("__TABLES__", "\n".join(tables))
    d = d.replace("__N_RENDER__", str(N_RENDER))
    d = d.replace("__NMSG__", str(len(MSGS)))
    d = d.replace("__NTH__", str(len(THOLDS)))
    d = d.replace("__NSETS__", str(len(FILTER_SETS)))
    d = d.replace("__NNAMES__", str(len(FILTER_NAMES)))
    d = d.replace("__NNE__", str(len(NE_CASES)))
    d = d.replace("__NDUMP__", str(len(DUMP_CASES)))
    d = d.replace("__Z4096__", ", ".join(["0"] * 4096))
    d = d.replace("__Z8192__", ", ".join(["0"] * 8192))
    d = d.replace("__Z512__", ", ".join(["0"] * 512))
    d = d.replace("__Z256__", ", ".join(["0"] * 256))
    d = d.replace("__Z128__", ", ".join(["0"] * 128))
    d = d.replace("__Z64F__", ", ".join(["0.0"] * 64))
    d = d.replace("__Z32I__", ", ".join(["0"] * 32))
    return port + GOOSE_COMMON + d


GOOSE_CONSOLE_DRIVER = r'''
__TABLES__

fn main() {
    var case_id: i64 = 0;

    // decode_utf8 over the raw texts
    var ti: i64 = 0;
    while ti < __NDEC__ {
        let s = t_dec(ti);
        var pos: i64 = 0;
        while pos < s.len {
            var adv: i64[1] = [0];
            let cp = ce_decode(s, s.len, pos, adv);
            var line: u8[128] = [__Z128__];
            var p: i64 = 0;
            p = app_c(line, p, 80);
            p = app_c(line, p, 124);
            p = put_int(line, p, case_id);
            p = app_c(line, p, 124);
            p = put_int(line, p, cp);
            p = app_c(line, p, 124);
            p = put_int(line, p, adv[0]);
            print(line[0..p]);
            case_id = case_id + 1;
            pos = pos + adv[0];
        }
        var line: u8[128] = [__Z128__];
        var p: i64 = 0;
        p = app_c(line, p, 80);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = app_c(line, p, 45);
        p = app_c(line, p, 124);
        p = put_int(line, p, s.len);
        print(line[0..p]);
        case_id = case_id + 1;
        ti = ti + 1;
    }

    // append_utf8
    ti = 0;
    while ti < __NENC__ {
        var enc: u8[8] = [__Z8__];
        let n = ce_encode(CE_ENCODE_CPS[ti], enc, 0);
        var line: u8[128] = [__Z128__];
        var p: i64 = 0;
        p = app_c(line, p, 69);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_fld(line, p, enc[0..n]);
        print(line[0..p]);
        case_id = case_id + 1;
        ti = ti + 1;
    }

    // prev/next character position
    ti = 0;
    while ti < __NDEC__ {
        let s = t_dec(ti);
        var pos: i64 = 0;
        while pos <= s.len {
            var line: u8[128] = [__Z128__];
            var p: i64 = 0;
            p = app_c(line, p, 88);
            p = app_c(line, p, 124);
            p = put_int(line, p, case_id);
            p = app_c(line, p, 124);
            p = put_int(line, p, pos);
            p = app_c(line, p, 124);
            p = put_int(line, p, ce_prev_pos(s, s.len, pos));
            p = app_c(line, p, 124);
            p = put_int(line, p, ce_next_pos(s, s.len, pos));
            print(line[0..p]);
            case_id = case_id + 1;
            pos = pos + 1;
        }
        ti = ti + 1;
    }

    // has_ctrl_modifier
    ti = 0;
    while ti < __NCTRL__ {
        let s = t_ctrl(ti);
        var line: u8[128] = [__Z128__];
        var p: i64 = 0;
        p = app_c(line, p, 75);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_int(line, p, ce_has_ctrl(s, s.len));
        print(line[0..p]);
        case_id = case_id + 1;
        ti = ti + 1;
    }

    // width and the space test
    ti = 0;
    while ti < __NW__ {
        let cp = CE_WIDTH_CPS[ti];
        var line: u8[128] = [__Z128__];
        var p: i64 = 0;
        p = app_c(line, p, 87);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_int(line, p, ce_width(cp));
        p = app_c(line, p, 124);
        p = put_int(line, p, ce_is_space(cp));
        print(line[0..p]);
        case_id = case_id + 1;
        ti = ti + 1;
    }
    ti = 0;
    while ti < __NSP__ {
        let cp = CE_SPACE_CPS[ti];
        var line: u8[128] = [__Z128__];
        var p: i64 = 0;
        p = app_c(line, p, 79);
        p = app_c(line, p, 124);
        p = put_int(line, p, case_id);
        p = app_c(line, p, 124);
        p = put_int(line, p, ce_is_space(cp));
        print(line[0..p]);
        case_id = case_id + 1;
        ti = ti + 1;
    }

    // key scripts: one readline call each, history persists across calls
    var line: u8[2048] = [__Z2048__];
    var out: u8[8192] = [__Z8192__];
    var arena: u8[65536] = [__Z65536__];
    var backup: u8[2048] = [__Z2048__];
    var llen: i64[1] = [0];
    var pos: i64[2] = [0, 0];
    var olen: i64[1] = [0];
    var used: i64[1] = [0];
    var hoff: i64[256] = [__Z256I__];
    var hlen: i64[256] = [__Z256I__];
    var hcount: i64[1] = [0];
    var view: i64[1] = [0 - 1];
    var blen: i64[1] = [0];

    var si: i64 = 0;
    while si < __NSCR__ {
        llen[0] = 0;
        pos[0] = 0;
        pos[1] = 0;
        olen[0] = 0;

        let more = ce_edit(t_keys(si), CE_MULTI[si], line, llen, pos, out, olen,
            arena, used, hoff, hlen, hcount, view, backup, blen);

        var lb: u8[16384] = [__Z16384__];
        var p: i64 = 0;
        p = app_c(lb, p, 67);
        p = app_c(lb, p, 124);
        p = put_int(lb, p, case_id);
        p = app_c(lb, p, 124);
        p = put_int(lb, p, more);
        p = app_c(lb, p, 124);
        p = put_fld(lb, p, line[0..llen[0]]);
        p = app_c(lb, p, 124);
        p = put_fld(lb, p, out[0..olen[0]]);
        p = app_c(lb, p, 124);
        p = put_int(lb, p, hcount[0]);
        p = app_c(lb, p, 124);
        var k: i64 = 0;
        while k < hcount[0] {
            if k > 0 {
                p = app_c(lb, p, 124);
            }
            p = put_fld(lb, p, arena[hoff[k] .. hoff[k] + hlen[k]]);
            k = k + 1;
        }
        print(lb[0..p]);
        case_id = case_id + 1;
        si = si + 1;
    }
}
'''


def gen_goose_console(port):
    tables = []
    tables.append(goose_accessor("t_dec", DECODE_TEXTS))
    tables.append(goose_accessor("t_keys", SCRIPTS))
    tables.append(goose_accessor("t_ctrl", CTRL_PARAMS))
    tables.append(goose_i64_table("CE_MULTI", SCRIPT_MULTI))
    tables.append(goose_i64_table("CE_ENCODE_CPS", ENCODE_CPS))
    tables.append(goose_i64_table("CE_WIDTH_CPS", WIDTH_CPS))
    tables.append(goose_i64_table("CE_SPACE_CPS", SPACE_CPS))

    d = GOOSE_CONSOLE_DRIVER.replace("__TABLES__", "\n".join(tables))
    d = d.replace("__NDEC__", str(len(DECODE_TEXTS)))
    d = d.replace("__NENC__", str(len(ENCODE_CPS)))
    d = d.replace("__NCTRL__", str(len(CTRL_PARAMS)))
    d = d.replace("__NW__", str(len(WIDTH_CPS)))
    d = d.replace("__NSP__", str(len(SPACE_CPS)))
    d = d.replace("__NSCR__", str(len(SCRIPTS)))
    d = d.replace("__Z8__", ", ".join(["0"] * 8))
    d = d.replace("__Z128__", ", ".join(["0"] * 128))
    d = d.replace("__Z2048__", ", ".join(["0"] * 2048))
    d = d.replace("__Z8192__", ", ".join(["0"] * 8192))
    d = d.replace("__Z16384__", ", ".join(["0"] * 16384))
    d = d.replace("__Z65536__", ", ".join(["0"] * 65536))
    d = d.replace("__Z256I__", ", ".join(["0"] * 256))
    return port + GOOSE_COMMON + d


def main():
    # Generated C++ and generated Goose land in build/, which is ignored by
    # Git: only the generator, the scripts and the recorded results are source.
    (ROOT / "build").mkdir(parents=True, exist_ok=True)
    (ROOT / "build" / "probe_log.cpp").write_text(gen_probe_log())
    (ROOT / "build" / "probe_debug.cpp").write_text(gen_probe_debug())
    (ROOT / "build" / "probe_console.cpp").write_text(gen_probe_console())
    (ROOT / "build" / "gate_log.goose").write_text(
        gen_goose_log((ROOT / "source" / "log-policy.goose").read_text()))
    (ROOT / "build" / "gate_console.goose").write_text(
        gen_goose_console((ROOT / "source" / "console-editing.goose").read_text()))
    print("wrote build/probe_{log,debug,console}.cpp and build/gate_{log,console}.goose")


if __name__ == "__main__":
    main()
