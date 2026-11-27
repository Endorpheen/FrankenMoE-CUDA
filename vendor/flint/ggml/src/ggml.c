#define _CRT_SECURE_NO_DEPRECATE // Disables "unsafe" warnings on Windows
#define _USE_MATH_DEFINES // For M_PI on MSVC

#include "ggml-version.h"
#include "ggml-impl.h"
#include "ggml-threading.h"
#include "ggml.h"

#ifndef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml-quants.h"
#endif

#ifdef GGML_USE_CPU_HBM
#include <hbwmalloc.h>
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h> // using malloc.h with MSC/MINGW
#elif !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <alloca.h>
#endif

#include <assert.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <signal.h>
#if defined(__gnu_linux__)
#include <syscall.h>
#endif

#if defined(__APPLE__)
#include <unistd.h>
#include <mach/mach.h>
#include <TargetConditionals.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#endif

#define UNUSED GGML_UNUSED

// Needed for ggml_fp32_to_bf16_row()
#if defined(__AVX512BF16__)
#if defined(_MSC_VER)
#define m512i(p) p
#else
#include <immintrin.h>
#define m512i(p) (__m512i)(p)
#endif // defined(_MSC_VER)
#endif // defined(__AVX512BF16__)

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

static ggml_abort_callback_t g_abort_callback = NULL;

// Set the abort callback (passing null will restore original abort functionality: printing a message to stdout)
void ggml_abort(const char * file, int line, const char * fmt, ...) {
    fflush(stdout);

    char message[2048];
    int offset = snprintf(message, sizeof(message), "%s:%d: ", file, line);

    va_list args;
    va_start(args, fmt);
    vsnprintf(message + offset, sizeof(message) - offset, fmt, args);
    va_end(args);

    if (g_abort_callback) {
        g_abort_callback(message);
    } else {
        // default: print error and backtrace to stderr
        fprintf(stderr, "%s\n", message);
        ggml_print_backtrace();
    }

    abort();
}

// ggml_print_backtrace is registered with std::set_terminate by ggml.cpp

//
// logging
//

struct ggml_logger_state {
    ggml_log_callback log_callback;
    void * log_callback_user_data;
};
static struct ggml_logger_state g_logger_state = {ggml_log_callback_default, NULL};

static void ggml_log_internal_v(enum ggml_log_level level, const char * format, va_list args) {
    if (format == NULL) {
        return;
    }
    va_list args_copy;
    va_copy(args_copy, args);
    char buffer[128];
    int len = vsnprintf(buffer, 128, format, args);
    if (len < 128) {
        g_logger_state.log_callback(level, buffer, g_logger_state.log_callback_user_data);
    } else {
        char * buffer2 = (char *) calloc(len + 1, sizeof(char));
        vsnprintf(buffer2, len + 1, format, args_copy);
        buffer2[len] = 0;
        g_logger_state.log_callback(level, buffer2, g_logger_state.log_callback_user_data);
        free(buffer2);
    }
    va_end(args_copy);
}

void ggml_log_internal(enum ggml_log_level level, const char * format, ...) {
    va_list args;
    va_start(args, format);
    ggml_log_internal_v(level, format, args);
    va_end(args);
}

void ggml_log_callback_default(enum ggml_log_level level, const char * text, void * user_data) {
    (void) level;
    (void) user_data;
    fputs(text, stderr);
    fflush(stderr);
}

//
// end of logging block
//

#ifdef GGML_USE_ACCELERATE
// uncomment to use vDSP for soft max computation
// note: not sure if it is actually faster
//#define GGML_SOFT_MAX_ACCELERATE
#endif


// the thread sanitizer does not intercept vm_allocate and vm_deallocate, so it produces false positives
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define GGML_TSAN_ENABLED 1
#endif
#else
#if defined(__SANITIZE_THREAD__)
#define GGML_TSAN_ENABLED 1
#endif
#endif

void * ggml_aligned_malloc(size_t size) {
#if defined(__s390x__)
    const int alignment = 256;
#else
    const int alignment = 64;
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
    return _aligned_malloc(size, alignment);
#else
    if (size == 0) {
        GGML_LOG_WARN("Behavior may be unexpected when allocating 0 bytes for ggml_aligned_malloc!\n");
        return NULL;
    }
    void * aligned_memory = NULL;
  #ifdef GGML_USE_CPU_HBM
    int result = hbw_posix_memalign(&aligned_memory, alignment, size);
  #elif TARGET_OS_OSX && !defined(GGML_TSAN_ENABLED)
    GGML_UNUSED(alignment);
    kern_return_t alloc_status = vm_allocate((vm_map_t) mach_task_self(), (vm_address_t *) &aligned_memory, size, VM_FLAGS_ANYWHERE);
    int result = EFAULT;
    switch (alloc_status) {
        case KERN_SUCCESS:
            result = 0;
            break;
        case KERN_INVALID_ADDRESS:
            result = EINVAL;
            break;
        case KERN_NO_SPACE:
            result = ENOMEM;
            break;
        default:
            result = EFAULT;
            break;
    }
  #else
    int result = posix_memalign(&aligned_memory, alignment, size);
  #endif
    if (result != 0) {
        // Handle allocation failure
        const char *error_desc = "unknown allocation error";
        switch (result) {
            case EINVAL:
                error_desc = "invalid alignment value";
                break;
            case ENOMEM:
                error_desc = "insufficient memory";
                break;
        }
        GGML_LOG_ERROR("%s: %s (attempted to allocate %6.2f MB)\n", __func__, error_desc, size/(1024.0*1024.0));
        return NULL;
    }
    return aligned_memory;
#endif
}

void ggml_aligned_free(void * ptr, size_t size) {
    GGML_UNUSED(size);
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(ptr);
#elif GGML_USE_CPU_HBM
    if (ptr != NULL) {
        hbw_free(ptr);
    }
#elif TARGET_OS_OSX && !defined(GGML_TSAN_ENABLED)
    if (ptr != NULL) {
        vm_deallocate((vm_map_t)mach_task_self(), (vm_address_t)ptr, size);
    }
#else
    free(ptr);
#endif
}


inline static void * ggml_malloc(size_t size) {
    if (size == 0) {
        GGML_LOG_WARN("Behavior may be unexpected when allocating 0 bytes for ggml_malloc!\n");
        return NULL;
    }
    void * result = malloc(size);
    if (result == NULL) {
        GGML_LOG_ERROR("%s: failed to allocate %6.2f MB\n", __func__, size/(1024.0*1024.0));
        GGML_ABORT("fatal error");
    }
    return result;
}

// calloc
inline static void * ggml_calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) {
        GGML_LOG_WARN("Behavior may be unexpected when allocating 0 bytes for ggml_calloc!\n");
        return NULL;
    }
    void * result = calloc(num, size);
    if (result == NULL) {
        GGML_LOG_ERROR("%s: failed to allocate %6.2f MB\n", __func__, size/(1024.0*1024.0));
        GGML_ABORT("fatal error");
    }
    return result;
}

#define GGML_MALLOC(size)      ggml_malloc(size)
#define GGML_CALLOC(num, size) ggml_calloc(num, size)

#define GGML_FREE(ptr) free(ptr)

const char * ggml_status_to_string(enum ggml_status status) {
    switch (status) {
        case GGML_STATUS_ALLOC_FAILED: return "GGML status: error (failed to allocate memory)";
        case GGML_STATUS_FAILED:       return "GGML status: error (operation failed)";
        case GGML_STATUS_SUCCESS:      return "GGML status: success";
        case GGML_STATUS_ABORTED:      return "GGML status: warning (operation aborted)";
    }

    return "GGML status: unknown";
}

//
// cross-platform UTF-8 file paths
//

#ifdef _WIN32
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
#include "gooseinfer-type-metadata.h"
#else
#include "ggml-quantization-reference.inc"
#endif


//
// ggml object
//

struct ggml_object {
    size_t offs;
    size_t size;

    struct ggml_object * next;

    enum ggml_object_type type;

    char padding[4];
};

static const size_t GGML_OBJECT_SIZE = sizeof(struct ggml_object);

//
// ggml context
//

struct ggml_context {
    size_t mem_size;
    void * mem_buffer;
    bool   mem_buffer_owned;
    bool   no_alloc;

    int    n_objects;

    struct ggml_object * objects_begin;
    struct ggml_object * objects_end;
};

//
// data types
//

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
#include "gooseinfer-op-names.h"
#else
#include "ggml-op-names-reference.inc"
#endif

static_assert(GGML_OP_COUNT == 101, "GGML_OP_COUNT != 101");

static_assert(GGML_OP_COUNT == 101, "GGML_OP_COUNT != 101");

static_assert(GGML_OP_POOL_COUNT == 2, "GGML_OP_POOL_COUNT != 2");

static_assert(GGML_UNARY_OP_COUNT == 22, "GGML_UNARY_OP_COUNT != 22");

static_assert(GGML_GLU_OP_COUNT == 7, "GGML_GLU_OP_COUNT != 7");

static_assert(sizeof(struct ggml_object)%GGML_MEM_ALIGN == 0, "ggml_object size must be a multiple of GGML_MEM_ALIGN");
static_assert(sizeof(struct ggml_tensor)%GGML_MEM_ALIGN == 0, "ggml_tensor size must be a multiple of GGML_MEM_ALIGN");


////////////////////////////////////////////////////////////////////////////////

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
typedef struct { const int64_t *data; int64_t len; } ggml_shape_slice;
typedef struct { uint64_t *data; int64_t len; } ggml_layout_slice;
#endif

#ifndef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-geometry-reference.inc"
#endif

int64_t ggml_blck_size(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].blck_size;
}

size_t ggml_type_size(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].type_size;
}

size_t ggml_row_size(enum ggml_type type, int64_t ne) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    assert(ne % ggml_blck_size(type) == 0);
    return ggml_type_size(type)*ne/ggml_blck_size(type);
}

double ggml_type_sizef(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return ((double)(type_traits[type].type_size))/type_traits[type].blck_size;
}

const char * ggml_type_name(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].type_name;
}

bool ggml_is_quantized(enum ggml_type type) {
    assert(type >= 0);
    assert(type < GGML_TYPE_COUNT);
    return type_traits[type].is_quantized;
}

const char * ggml_op_name(enum ggml_op op) {
    return GGML_OP_NAME[op];
}

const char * ggml_unary_op_name(enum ggml_unary_op op) {
    return GGML_UNARY_OP_NAME[op];
}

const char * ggml_glu_op_name(enum ggml_glu_op op) {
    return GGML_GLU_OP_NAME[op];
}

// assert that pointer is aligned to GGML_MEM_ALIGN
#define GGML_ASSERT_ALIGNED(ptr) \
    GGML_ASSERT(((uintptr_t) (ptr))%GGML_MEM_ALIGN == 0)

////////////////////////////////////////////////////////////////////////////////

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern void fg143_cpu_init(int argc, char **argv);
extern int32_t fg143_object_layout(uint64_t previous_offset, uint64_t previous_size, uint64_t size,
    uint64_t alignment, uint64_t header, uint64_t capacity, ggml_layout_slice layout);
extern uint8_t fg143_tensor_layout(int64_t type, ggml_shape_slice shape, ggml_layout_slice layout);
extern uint8_t fg143_graph_arena_layout(uint64_t size, uint64_t hash_size, uint64_t base_size,
    uint64_t pointer_size, uint64_t bitset_size, uint8_t grads, ggml_layout_slice layout);
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern uint64_t fg143_tensor_overhead(void);

size_t ggml_tensor_overhead(void) {
    fg143_cpu_init(0, NULL);
    return fg143_tensor_overhead();
}
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern uint64_t fg143_arena_init(uint64_t capacity, uint64_t buffer, uint8_t no_alloc);
#endif

struct ggml_context * ggml_init(struct ggml_init_params params) {
    static bool is_first_call = true;

    ggml_critical_section_start();

    if (is_first_call) {
#ifdef GOOSEINFER_TYPE_METADATA_ONLY
        fg143_cpu_init(0, NULL);
#else
        ggml_time_init();
#endif

        is_first_call = false;
    }

    ggml_critical_section_end();

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
    return (void *)(uintptr_t)fg143_arena_init(params.mem_size, (uintptr_t)params.mem_buffer, params.no_alloc);
#else
    struct ggml_context * ctx = GGML_MALLOC(sizeof(struct ggml_context));

    // allow to call ggml_init with 0 size
    if (params.mem_size == 0) {
        params.mem_size = GGML_MEM_ALIGN;
    }

    const size_t mem_size = params.mem_buffer ? params.mem_size : GGML_PAD(params.mem_size, GGML_MEM_ALIGN);

    *ctx = (struct ggml_context) {
        /*.mem_size           =*/ mem_size,
        /*.mem_buffer         =*/ params.mem_buffer ? params.mem_buffer : ggml_aligned_malloc(mem_size),
        /*.mem_buffer_owned   =*/ params.mem_buffer ? false : true,
        /*.no_alloc           =*/ params.no_alloc,
        /*.n_objects          =*/ 0,
        /*.objects_begin      =*/ NULL,
        /*.objects_end        =*/ NULL,
    };

    GGML_ASSERT(ctx->mem_buffer != NULL);

    GGML_ASSERT_ALIGNED(ctx->mem_buffer);

    GGML_PRINT_DEBUG("%s: context initialized\n", __func__);

    return ctx;
#endif
}


////////////////////////////////////////////////////////////////////////////////

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
static_assert(GGML_MEM_ALIGN == 16 && sizeof(struct ggml_object) == 32 &&
              offsetof(struct ggml_tensor, op_params) == 84 &&
              offsetof(struct ggml_object, offs) == 0 &&
              offsetof(struct ggml_object, size) == 8 &&
              offsetof(struct ggml_object, next) == 16 &&
              offsetof(struct ggml_object, type) == 24 &&
              offsetof(struct ggml_context, mem_size) == 0 &&
              offsetof(struct ggml_context, mem_buffer) == 8 &&
              offsetof(struct ggml_context, mem_buffer_owned) == 16 &&
              offsetof(struct ggml_context, no_alloc) == 17 &&
              sizeof(struct ggml_context) == 40 &&
              offsetof(struct ggml_context, n_objects) == 20 &&
              offsetof(struct ggml_context, objects_begin) == 24 &&
              offsetof(struct ggml_context, objects_end) == 32 &&
              GGML_OBJECT_TYPE_TENSOR == 0, "Goose arena ABI");

extern struct ggml_object * ggml_new_object(struct ggml_context *, enum ggml_object_type, size_t);

void fg143_arena_failure(int status, uint64_t needed, uint64_t capacity) {
    GGML_ASSERT(status != -1);
    if (status == -4) {
        GGML_LOG_WARN("ggml_new_object: not enough space in the context's memory pool (needed %zu, available %zu)\n",
                (size_t)needed, (size_t)capacity);
#ifndef NDEBUG
        GGML_ABORT("not enough space in the context's memory pool");
#endif
    } else {
        GGML_LOG_WARN("ggml_new_object: overflow detected in context allocation\n");
    }
}
#else
#include "ggml-arena-reference.inc"
#endif


struct ggml_tensor * ggml_format_name(struct ggml_tensor * tensor, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(tensor->name, sizeof(tensor->name), fmt, args);
    va_end(args);
    return tensor;
}


////////////////////////////////////////////////////////////////////////////////

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-goose-constructors.inc"
#else
#include "ggml-operator-reference.inc"
#endif

struct ggml_tensor * ggml_softplus(
        struct ggml_context * ctx,
        struct ggml_tensor  * a) {
    return ggml_unary(ctx, a, GGML_UNARY_OP_SOFTPLUS);
}

// ggml_tanh

struct ggml_tensor * ggml_tanh(
        struct ggml_context * ctx,
        struct ggml_tensor  * a) {
    return ggml_unary(ctx, a, GGML_UNARY_OP_TANH);
}

// ggml_relu

struct ggml_tensor * ggml_relu(
        struct ggml_context * ctx,
        struct ggml_tensor  * a) {
    return ggml_unary(ctx, a, GGML_UNARY_OP_RELU);
}

// ggml_sigmoid

struct ggml_tensor * ggml_sigmoid(
        struct ggml_context * ctx,
        struct ggml_tensor  * a) {
    return ggml_unary(ctx, a, GGML_UNARY_OP_SIGMOID);
}

// ggml_silu

struct ggml_tensor * ggml_silu(
        struct ggml_context * ctx,
        struct ggml_tensor  * a) {
    return ggml_unary(ctx, a, GGML_UNARY_OP_SILU);
}

// ggml hardswish

// ggml hardsigmoid

// ggml exp

// ggml_glu

// ggml_prec

bool ggml_prec_set_acc(
        struct ggml_tensor * a,
        enum ggml_prec       prec) {
    switch (a->op) {
        case GGML_OP_MUL_MAT:
        case GGML_OP_MUL_MAT_ID:
            {
                const int32_t prec_i32 = (int32_t) prec;
                ggml_set_op_params_i32(a, 0, prec_i32);
            }
            break;
        case GGML_OP_FLASH_ATTN_EXT:
            {
                const int32_t prec_i32 = (int32_t) prec;
                ggml_set_op_params_i32(a, 3, prec_i32);
            }
            break;
        default:
            return false;
    };

    return true;
}

// ggml_mul_mat

void ggml_mul_mat_set_hint(
        struct ggml_tensor * a,
        enum ggml_op_hint    hint) {
    GGML_ASSERT(a->op == GGML_OP_MUL_MAT);

    const int32_t hint_i32 = (int32_t) hint;

    ggml_set_op_params_i32(a, 1, hint_i32);
}

// ggml_get_rows

// Apparently solving `n_rot = 2pi * x * base^((2 * max_pos_emb) / n_dims)` for x, we get
// `corr_dim(n_rot) = n_dims * log(max_pos_emb / (n_rot * 2pi)) / (2 * log(base))`
static float ggml_rope_yarn_corr_dim(int n_dims, int n_ctx_orig, float n_rot, float base) {
    return n_dims * logf(n_ctx_orig / (n_rot * 2 * (float)M_PI)) / (2 * logf(base));
}

void ggml_rope_yarn_corr_dims(
    int n_dims, int n_ctx_orig, float freq_base, float beta_fast, float beta_slow, float dims[2]
) {
    // start and end correction dims
    float start = floorf(ggml_rope_yarn_corr_dim(n_dims, n_ctx_orig, beta_fast, freq_base));
    float end   =  ceilf(ggml_rope_yarn_corr_dim(n_dims, n_ctx_orig, beta_slow, freq_base));
    dims[0] = MAX(0, start);
    dims[1] = MIN(n_dims - 1, end);
}

// ggml_flash_attn_ext

void ggml_flash_attn_ext_set_n_kv_max(
        struct ggml_tensor * a,
        int32_t              n_kv_max) {
    GGML_ASSERT(a->op == GGML_OP_FLASH_ATTN_EXT);
    GGML_ASSERT(n_kv_max >= 0);

    ggml_set_op_params_i32(a, 4, n_kv_max);
}


////////////////////////////////////////////////////////////////////////////////

#ifndef GOOSEINFER_TYPE_METADATA_ONLY
void ggml_hash_set_reset(struct ggml_hash_set * hash_set) {
    memset(hash_set->used, 0, sizeof(ggml_bitset_t) * ggml_bitset_size(hash_set->size));
}
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern uint64_t fg143_graph_hash_size(uint64_t minimum);
size_t ggml_hash_size(size_t minimum) {
    fg143_cpu_init(0, NULL);
    return fg143_graph_hash_size(minimum);
}
#else
#include "ggml-hash-size-reference.inc"
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern void fg143_graph_expand_forward(uint64_t graph, uint64_t tensor);

void ggml_build_forward_expand(struct ggml_cgraph *graph, struct ggml_tensor *tensor) {
    fg143_graph_expand_forward((uintptr_t)graph, (uintptr_t)tensor);
}
#else
#include "ggml-graph-reference.inc"
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
static_assert(sizeof(struct ggml_cgraph) == 96 &&
              offsetof(struct ggml_cgraph, size) == 0 &&
              offsetof(struct ggml_cgraph, n_nodes) == 4 &&
              offsetof(struct ggml_cgraph, n_leafs) == 8 &&
              offsetof(struct ggml_cgraph, nodes) == 16 &&
              offsetof(struct ggml_cgraph, grads) == 24 &&
              offsetof(struct ggml_cgraph, grad_accs) == 32 &&
              offsetof(struct ggml_cgraph, leafs) == 40 &&
              offsetof(struct ggml_cgraph, use_counts) == 48 &&
              offsetof(struct ggml_cgraph, visited_hash_set) == 56 &&
              offsetof(struct ggml_hash_set, used) == 8 &&
              offsetof(struct ggml_hash_set, keys) == 16 &&
              sizeof(ggml_bitset_t) == 4 && GGML_OBJECT_TYPE_GRAPH == 1 &&
              GGML_CGRAPH_EVAL_ORDER_LEFT_TO_RIGHT == 0, "Goose graph ABI");

extern uint64_t fg143_graph_overhead_custom(uint64_t size, uint8_t grads);

size_t ggml_graph_overhead_custom(size_t size, bool grads) {
    fg143_cpu_init(0, NULL);
    return fg143_graph_overhead_custom(size, grads);
}
#else
#include "ggml-graph-storage-reference.inc"
#endif


////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

void ggml_log_set(ggml_log_callback log_callback, void * user_data) {
    g_logger_state.log_callback = log_callback ? log_callback : ggml_log_callback_default;
    g_logger_state.log_callback_user_data = user_data;
}

#ifndef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-runtime-reference.inc"
#endif
