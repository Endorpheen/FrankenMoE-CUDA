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

#ifndef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-backtrace-reference.inc"
#elif defined(__linux__)
#include <sys/prctl.h>
void fg_ggml_allow_debugger(int child) { prctl(PR_SET_PTRACER, child); }
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


#ifndef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-aligned-storage-reference.inc"
#endif

#if defined(GOOSEINFER_GOOSE_METADATA)
static_assert(GGML_TYPE_COUNT == 43, "Goose quantization type ABI");
#elif defined(GOOSEINFER_TYPE_METADATA_ONLY)
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

#if defined(GOOSEINFER_GOOSE_METADATA)
static_assert(GGML_STATUS_ALLOC_FAILED == -2 && GGML_STATUS_FAILED == -1 &&
    GGML_STATUS_SUCCESS == 0 && GGML_STATUS_ABORTED == 1, "Goose status ABI");
#elif defined(GOOSEINFER_TYPE_METADATA_ONLY)
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

#ifndef GOOSEINFER_GOOSE_METADATA
#include "ggml-metadata-reference.inc"
#endif

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

#ifndef GOOSEINFER_GOOSE_METADATA
#include "ggml-context-api-reference.inc"
#endif

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

void fg143_arena_warn(const char *message) {
    g_logger_state.log_callback(GGML_LOG_LEVEL_WARN, message, g_logger_state.log_callback_user_data);
}

void fg143_arena_abort(int status) {
    GGML_ASSERT(status != -1);
#ifndef NDEBUG
    if (status == -4) GGML_ABORT("not enough space in the context's memory pool");
#endif
}
#else
#include "ggml-arena-reference.inc"
#endif


#ifndef GOOSEINFER_GOOSE_METADATA
struct ggml_tensor * ggml_format_name(struct ggml_tensor * tensor, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(tensor->name, sizeof(tensor->name), fmt, args);
    va_end(args);
    return tensor;
}
#endif

////////////////////////////////////////////////////////////////////////////////

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-goose-constructors.inc"
#else
#include "ggml-operator-reference.inc"
#endif

#ifndef GOOSEINFER_GOOSE_METADATA
#include "ggml-helper-api-reference.inc"
#endif

////////////////////////////////////////////////////////////////////////////////

#ifndef GOOSEINFER_TYPE_METADATA_ONLY
void ggml_hash_set_reset(struct ggml_hash_set * hash_set) {
    memset(hash_set->used, 0, sizeof(ggml_bitset_t) * ggml_bitset_size(hash_set->size));
}
#endif

#ifndef GOOSEINFER_GOOSE_METADATA
#include "ggml-graph-api-reference.inc"
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

#ifndef GOOSEINFER_GOOSE_METADATA
#include "ggml-graph-overhead-reference.inc"
#endif
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
