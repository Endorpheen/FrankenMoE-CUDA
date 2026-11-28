#if defined(__linux__) || \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    (defined(__APPLE__) && !TARGET_OS_TV && !TARGET_OS_WATCH)

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#if defined(__ANDROID__)
#include <unwind.h>
#include <dlfcn.h>
#include <stdio.h>

struct backtrace_state {
    void ** current;
    void ** end;
};

static _Unwind_Reason_Code unwind_callback(struct _Unwind_Context* context, void* arg) {
    struct backtrace_state * state = (struct backtrace_state *)arg;
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = (void*)pc;
        }
    }
    return _URC_NO_REASON;
}

static void ggml_print_backtrace_symbols(void) {
    const int max = 100;
    void* buffer[max];

    struct backtrace_state state = {buffer, buffer + max};
    _Unwind_Backtrace(unwind_callback, &state);

    int count = state.current - buffer;

    for (int idx = 0; idx < count; ++idx) {
        const void * addr = buffer[idx];
        const char * symbol = "";

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname) {
            symbol = info.dli_sname;
        }

        fprintf(stderr, "%d: %p %s\n", idx, addr, symbol);
    }
}
#elif defined(__linux__) && defined(__GLIBC__)
#include <execinfo.h>
static void ggml_print_backtrace_symbols(void) {
    void * trace[100];
    int nptrs = backtrace(trace, sizeof(trace)/sizeof(trace[0]));
    backtrace_symbols_fd(trace, nptrs, STDERR_FILENO);
}
#elif defined(__APPLE__)
#include <execinfo.h>
static void ggml_print_backtrace_symbols(void) {
    void * trace[100];
    int nptrs = backtrace(trace, sizeof(trace)/sizeof(trace[0]));
    backtrace_symbols_fd(trace, nptrs, STDERR_FILENO);
}
#else
static void ggml_print_backtrace_symbols(void) {
    // platform not supported
}
#endif

void ggml_print_backtrace(void) {
    const char * GGML_NO_BACKTRACE = getenv("GGML_NO_BACKTRACE");
    if (GGML_NO_BACKTRACE) {
        return;
    }
#if defined(__APPLE__)
    // On macOS, fork+debugger attachment is problematic due to:
    // 1. libdispatch "poisons" forked child processes
    // 2. lldb has issues attaching to parent from forked child
    // Use simple backtrace() instead to avoid Terminal.app crashes
    const char * GGML_BACKTRACE_LLDB = getenv("GGML_BACKTRACE_LLDB");
    if (!GGML_BACKTRACE_LLDB) {
        fprintf(stderr, "WARNING: Using native backtrace. Set GGML_BACKTRACE_LLDB for more info.\n");
        fprintf(stderr, "WARNING: GGML_BACKTRACE_LLDB may cause native MacOS Terminal.app to crash.\n");
        fprintf(stderr, "See: https://github.com/ggml-org/llama.cpp/pull/17869\n");
        ggml_print_backtrace_symbols();
        return;
    }
#endif
#if defined(__linux__)
    FILE * f = fopen("/proc/self/status", "r");
    size_t size = 0;
    char * line = NULL;
    ssize_t length = 0;
    while ((length = getline(&line, &size, f)) > 0) {
        if (!strncmp(line, "TracerPid:", sizeof("TracerPid:") - 1) &&
            (length != sizeof("TracerPid:\t0\n") - 1 || line[length - 2] != '0')) {
            // Already being debugged, and the breakpoint is the later abort()
            free(line);
            fclose(f);
            return;
        }
    }
    free(line);
    fclose(f);
    int lock[2] = { -1, -1 };
    (void) !pipe(lock); // Don't start gdb until after PR_SET_PTRACER
#endif
    const int parent_pid = getpid();
    const int child_pid = fork();
    if (child_pid < 0) { // error
#if defined(__linux__)
        close(lock[1]);
        close(lock[0]);
#endif
        return;
    } else if (child_pid == 0) { // child
        char attach[32];
        snprintf(attach, sizeof(attach), "attach %d", parent_pid);
#if defined(__linux__)
        close(lock[1]);
        (void) !read(lock[0], lock, 1);
        close(lock[0]);
#endif
        // try gdb
        execlp("gdb", "gdb", "--batch",
            "-ex", "set style enabled on",
            "-ex", attach,
            "-ex", "bt -frame-info source-and-location",
            "-ex", "detach",
            "-ex", "quit",
            (char *) NULL);
        // try lldb
        execlp("lldb", "lldb", "--batch",
            "-o", "bt",
            "-o", "quit",
            "-p", &attach[sizeof("attach ") - 1],
            (char *) NULL);
        // gdb failed, fallback to backtrace_symbols
        ggml_print_backtrace_symbols();
        _Exit(0);
    } else { // parent
#if defined(__linux__)
        prctl(PR_SET_PTRACER, child_pid);
        close(lock[1]);
        close(lock[0]);
#endif
        waitpid(child_pid, NULL, 0);
    }
}
#else
void ggml_print_backtrace(void) {
    // platform not supported
}
#endif
