#include "ggml-backend.h"
#include "ggml-backend-impl.h"
#include "ggml-cpu.h"
#include "repack.h"
#include "traits.h"
#include "ggml-impl.h"
#include "amx/amx.h"

#include <cctype>
#include <string>
#include <vector>

#ifdef GGML_USE_CPU_HBM
#    include "hbm.h"
#endif

#ifdef GGML_USE_CPU_KLEIDIAI
#    include "kleidiai/kleidiai.h"
#endif

#ifdef GGML_USE_CPU_RISCV64_SPACEMIT
#    include "spacemit/ime.h"
#endif

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#else
#    include <unistd.h>
#endif

#if defined(__APPLE__)
#    include <sys/sysctl.h>
#    include <sys/types.h>
#endif


#include <new>
#include <cstdlib>
#include <cstring>
#include <cstdint>
struct ggml_backend_cpu_context;
struct ggml_backend_plan_cpu;
#include "request.h"
extern "C" int64_t fg145_backend_api(int64_t,int64_t);
static bool fg145_enabled=true;
__attribute__((constructor))static void fg145_config(){const char*v=std::getenv("GGML_GOOSE_CPU_BACKEND_API");fg145_enabled=!v||std::strcmp(v,"0");}
static int64_t fg145_call(int64_t mode,fg145_request&q){return fg145_backend_api(mode,(int64_t)(uintptr_t)&q);}
static fg145_request fg145_request_backend(ggml_backend_t);
static std::vector<ggml_backend_buffer_type_t>&fg145_extras();
static ggml_backend_buffer_type_t*fg145_extra_copy();
static ggml_backend_feature*fg145_features();

// ggml-backend interface

std::vector<ggml_backend_buffer_type_t> & ggml_backend_cpu_get_extra_buffer_types() {
if(fg145_enabled)return fg145_extras();

    static std::vector<ggml_backend_buffer_type_t> bufts = []() {
        std::vector<ggml_backend_buffer_type_t> bufts;

#if defined(__AMX_INT8__) && defined(__AVX512VNNI__)
        if (ggml_backend_amx_buffer_type()) {
            bufts.push_back(ggml_backend_amx_buffer_type());
        }
#endif

#ifdef GGML_USE_CPU_RISCV64_SPACEMIT
        if (ggml_backend_cpu_riscv64_spacemit_buffer_type()) {
            bufts.push_back(ggml_backend_cpu_riscv64_spacemit_buffer_type());
        }
#endif

#ifdef GGML_USE_CPU_KLEIDIAI
        if (ggml_backend_cpu_kleidiai_buffer_type()) {
            bufts.push_back(ggml_backend_cpu_kleidiai_buffer_type());
        }
#endif

#ifdef GGML_USE_CPU_REPACK
        if (ggml_backend_cpu_repack_buffer_type()) {
            bufts.push_back(ggml_backend_cpu_repack_buffer_type());
        }
#endif

        return bufts;
    }();

    return bufts;
}

static ggml_backend_buffer_type_t * ggml_backend_cpu_device_get_extra_buffers_type(ggml_backend_dev_t device) {
if(fg145_enabled)return fg145_extra_copy();

    static std::vector<ggml_backend_buffer_type_t> extra_bufts = [] {
        std::vector<ggml_backend_buffer_type_t> bufts = ggml_backend_cpu_get_extra_buffer_types();
        bufts.push_back(nullptr);
        return bufts;
    }();

    return extra_bufts.data();

    GGML_UNUSED(device);
}

static bool ggml_backend_cpu_is_extra_buffer_type(ggml_backend_buffer_type_t buft) {
if(fg145_enabled){fg145_request q={};q.buft=buft;return fg145_call(12,q)!=0;}

    for (auto * extra : ggml_backend_cpu_get_extra_buffer_types()) {
        if (extra == buft) {
            return true;
        }
    }
    return false;
}

// CPU backend - backend (stream)

struct ggml_backend_cpu_context {
    int                 n_threads;
    ggml_threadpool_t   threadpool;

    uint8_t *           work_data;
    size_t              work_size;

    ggml_abort_callback abort_callback;
    void *              abort_callback_data;

    bool                use_ref;  // use reference implementation
};

static const char * ggml_backend_cpu_get_name(ggml_backend_t backend) {
    return "CPU";

    GGML_UNUSED(backend);
}

static void ggml_backend_cpu_free(ggml_backend_t backend) {
if(fg145_enabled){auto q=fg145_request_backend(backend);fg145_call(1,q);return;}

    struct ggml_backend_cpu_context * cpu_ctx = (struct ggml_backend_cpu_context *)backend->context;
    delete[] cpu_ctx->work_data;
    delete cpu_ctx;
    delete backend;
}

struct ggml_backend_plan_cpu {
    struct ggml_cplan cplan;
    struct ggml_cgraph cgraph;
};

static ggml_backend_graph_plan_t ggml_backend_cpu_graph_plan_create(ggml_backend_t backend, const struct ggml_cgraph * cgraph) {
if(fg145_enabled){auto q=fg145_request_backend(backend);q.graph=const_cast<ggml_cgraph*>(cgraph);return(ggml_backend_graph_plan_t)(uintptr_t)fg145_call(2,q);}

    struct ggml_backend_cpu_context * cpu_ctx = (struct ggml_backend_cpu_context *)backend->context;

    struct ggml_backend_plan_cpu * cpu_plan = new ggml_backend_plan_cpu;

    cpu_plan->cplan = ggml_graph_plan(cgraph, cpu_ctx->n_threads, cpu_ctx->threadpool);
    cpu_plan->cgraph = *cgraph; // FIXME: deep copy

    if (cpu_plan->cplan.work_size > 0) {
        cpu_plan->cplan.work_data = new uint8_t[cpu_plan->cplan.work_size];
        if (cpu_plan->cplan.work_data == NULL) {
            delete cpu_plan;
            return NULL;
        }
    }

    cpu_plan->cplan.abort_callback      = cpu_ctx->abort_callback;
    cpu_plan->cplan.abort_callback_data = cpu_ctx->abort_callback_data;
    cpu_plan->cplan.use_ref             = cpu_ctx->use_ref;

    return cpu_plan;
}

static void ggml_backend_cpu_graph_plan_free(ggml_backend_t backend, ggml_backend_graph_plan_t plan) {
if(fg145_enabled){auto q=fg145_request_backend(backend);q.plan=(ggml_backend_plan_cpu*)plan;fg145_call(3,q);return;}

    struct ggml_backend_plan_cpu * cpu_plan = (struct ggml_backend_plan_cpu *)plan;

    delete[] cpu_plan->cplan.work_data;
    delete cpu_plan;

    GGML_UNUSED(backend);
}

static enum ggml_status ggml_backend_cpu_graph_plan_compute(ggml_backend_t backend, ggml_backend_graph_plan_t plan) {
if(fg145_enabled){auto q=fg145_request_backend(backend);q.plan=(ggml_backend_plan_cpu*)plan;return(enum ggml_status)fg145_call(4,q);}

    struct ggml_backend_plan_cpu * cpu_plan = (struct ggml_backend_plan_cpu *)plan;

    return ggml_graph_compute(&cpu_plan->cgraph, &cpu_plan->cplan);

    GGML_UNUSED(backend);
}

static enum ggml_status ggml_backend_cpu_graph_compute(ggml_backend_t backend, struct ggml_cgraph * cgraph) {
if(fg145_enabled){auto q=fg145_request_backend(backend);q.graph=cgraph;return(enum ggml_status)fg145_call(5,q);}

    struct ggml_backend_cpu_context * cpu_ctx = (struct ggml_backend_cpu_context *)backend->context;

    struct ggml_cplan cplan = ggml_graph_plan(cgraph, cpu_ctx->n_threads, cpu_ctx->threadpool);

    if (cpu_ctx->work_size < cplan.work_size) {
        delete[] cpu_ctx->work_data;
        cpu_ctx->work_data = new uint8_t[cplan.work_size];
        if (cpu_ctx->work_data == NULL) {
            cpu_ctx->work_size = 0;
            return GGML_STATUS_ALLOC_FAILED;
        }
        cpu_ctx->work_size = cplan.work_size;
    }
    cplan.work_data = (uint8_t *)cpu_ctx->work_data;

    cplan.abort_callback      = cpu_ctx->abort_callback;
    cplan.abort_callback_data = cpu_ctx->abort_callback_data;
    cplan.use_ref             = cpu_ctx->use_ref;

    return ggml_graph_compute(cgraph, &cplan);
}

static const struct ggml_backend_i ggml_backend_cpu_i = {
    /* .get_name                = */ ggml_backend_cpu_get_name,
    /* .free                    = */ ggml_backend_cpu_free,
    /* .set_tensor_async        = */ NULL,
    /* .get_tensor_async        = */ NULL,
    /* .set_tensor_2d_async     = */ NULL,
    /* .get_tensor_2d_async     = */ NULL,
    /* .cpy_tensor_async        = */ NULL,
    /* .synchronize             = */ NULL,
    /* .graph_plan_create       = */ ggml_backend_cpu_graph_plan_create,
    /* .graph_plan_free         = */ ggml_backend_cpu_graph_plan_free,
    /* .graph_plan_update       = */ NULL,
    /* .graph_plan_compute      = */ ggml_backend_cpu_graph_plan_compute,
    /* .graph_compute           = */ ggml_backend_cpu_graph_compute,
    /* .event_record            = */ NULL,
    /* .event_wait              = */ NULL,
    /* .graph_optimize          = */ NULL,
};

static ggml_guid_t ggml_backend_cpu_guid(void) {
    static ggml_guid guid = { 0xaa, 0x67, 0xc7, 0x43, 0x96, 0xe6, 0xa3, 0x8a, 0xe3, 0xaf, 0xea, 0x92, 0x36, 0xbc, 0xfc, 0x89 };
    return &guid;
}

ggml_backend_t ggml_backend_cpu_init(void) {
if(fg145_enabled){ggml_cpu_init();fg145_request q={};return(ggml_backend_t)(uintptr_t)fg145_call(0,q);}

    // initialize CPU backend now to avoid slowing the first graph computation
    ggml_cpu_init();

    struct ggml_backend_cpu_context * ctx = new ggml_backend_cpu_context;
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n_threads           = GGML_DEFAULT_N_THREADS;
    ctx->threadpool          = NULL;
    ctx->work_data           = NULL;
    ctx->work_size           = 0;
    ctx->abort_callback      = NULL;
    ctx->abort_callback_data = NULL;
    ctx->use_ref             = false;

    ggml_backend_t cpu_backend = new ggml_backend {
        /* .guid    = */ ggml_backend_cpu_guid(),
        /* .iface   = */ ggml_backend_cpu_i,
        /* .device  = */ ggml_backend_reg_dev_get(ggml_backend_cpu_reg(), 0),
        /* .context = */ ctx,
    };

    if (cpu_backend == NULL) {
        delete ctx;
        return NULL;
    }

    return cpu_backend;
}

bool ggml_backend_is_cpu(ggml_backend_t backend) {
    return backend != NULL && ggml_guid_matches(backend->guid, ggml_backend_cpu_guid());
}

void ggml_backend_cpu_set_n_threads(ggml_backend_t backend_cpu, int n_threads) {
if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.value=n_threads;fg145_call(6,q);return;}

    GGML_ASSERT(ggml_backend_is_cpu(backend_cpu));

    struct ggml_backend_cpu_context * ctx = (struct ggml_backend_cpu_context *)backend_cpu->context;
    ctx->n_threads = n_threads;
}

void ggml_backend_cpu_set_threadpool(ggml_backend_t backend_cpu, ggml_threadpool_t threadpool) {
if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.pointer=threadpool;fg145_call(7,q);return;}

    GGML_ASSERT(ggml_backend_is_cpu(backend_cpu));

    struct ggml_backend_cpu_context * ctx = (struct ggml_backend_cpu_context *)backend_cpu->context;

    if (ctx->threadpool && ctx->threadpool != threadpool) {
        // already had a different threadpool, pause/suspend it before switching
        ggml_threadpool_pause(ctx->threadpool);
    }
    ctx->threadpool = threadpool;
}

void ggml_backend_cpu_set_abort_callback(ggml_backend_t backend_cpu, ggml_abort_callback abort_callback, void * abort_callback_data) {
if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.pointer=(void*)abort_callback;q.data=abort_callback_data;fg145_call(8,q);return;}

    GGML_ASSERT(ggml_backend_is_cpu(backend_cpu));

    struct ggml_backend_cpu_context * ctx = (struct ggml_backend_cpu_context *)backend_cpu->context;
    ctx->abort_callback = abort_callback;
    ctx->abort_callback_data = abort_callback_data;
}

void ggml_backend_cpu_set_use_ref(ggml_backend_t backend_cpu, bool use_ref) {
if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.value=use_ref;fg145_call(9,q);return;}

    GGML_ASSERT(ggml_backend_is_cpu(backend_cpu));

    struct ggml_backend_cpu_context * ctx = (struct ggml_backend_cpu_context *)backend_cpu->context;
    ctx->use_ref = use_ref;
}

// CPU backend - device

struct ggml_backend_cpu_device_context {
    std::string description = "CPU";

    ggml_backend_cpu_device_context() {
#ifdef __APPLE__
        size_t len = 0;
        if (!sysctlbyname("machdep.cpu.brand_string", NULL, &len, NULL, 0)) {
            description.resize(len);
            sysctlbyname("machdep.cpu.brand_string", &description[0], &len, NULL, 0); // NOLINT
        }
#elif defined(__linux__)
        FILE * f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char buf[1024];
            while (fgets(buf, sizeof(buf), f)) {
                if (strncmp(buf, "model name", 10) == 0) {
                    char * p = strchr(buf, ':');
                    if (p) {
                        p++;
                        while (std::isspace(*p)) {
                            p++;
                        }
                        while (std::isspace(p[strlen(p) - 1])) {
                            p[strlen(p) - 1] = '\0';
                        }
                        description = p;
                        break;
                    }
                }
            }
            fclose(f);
        }
#elif defined(_WIN32)
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
                        0,
                        KEY_READ,
                        &hKey) == ERROR_SUCCESS) {
            DWORD cpu_brand_size = 0;
            if (RegQueryValueExA(hKey,
                                "ProcessorNameString",
                                NULL,
                                NULL,
                                NULL,
                                &cpu_brand_size) == ERROR_SUCCESS) {
                description.resize(cpu_brand_size);
                if (RegQueryValueExA(hKey,
                                    "ProcessorNameString",
                                    NULL,
                                    NULL,
                                    (LPBYTE)&description[0], // NOLINT
                                    &cpu_brand_size) == ERROR_SUCCESS) {
                    if (description.find('\0') != std::string::npos) {
                        description.resize(description.find('\0'));
                    }
                }
            }
            RegCloseKey(hKey);
        }
#endif
    }
};

static const char * ggml_backend_cpu_device_get_name(ggml_backend_dev_t dev) {
    return "CPU";

    GGML_UNUSED(dev);
}

static const char * ggml_backend_cpu_device_get_description(ggml_backend_dev_t dev) {
    struct ggml_backend_cpu_device_context * ctx = (struct ggml_backend_cpu_device_context *)dev->context;

    return ctx->description.c_str();
}

static void ggml_backend_cpu_device_get_memory(ggml_backend_dev_t dev, size_t * free, size_t * total) {
if(fg145_enabled){fg145_request q={};q.dev=dev;q.free_mem=free;q.total_mem=total;fg145_call(16,q);return;}

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    *total = status.ullTotalPhys;
    *free = status.ullAvailPhys;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    *total = pages * page_size;

    // "free" system memory is ill-defined, for practical purposes assume that all of it is free:
    *free = *total;
#endif // _WIN32

    GGML_UNUSED(dev);
}

static enum ggml_backend_dev_type ggml_backend_cpu_device_get_type(ggml_backend_dev_t dev) {
    return GGML_BACKEND_DEVICE_TYPE_CPU;

    GGML_UNUSED(dev);
}

static void ggml_backend_cpu_device_get_props(ggml_backend_dev_t dev, struct ggml_backend_dev_props * props) {
    props->name        = ggml_backend_cpu_device_get_name(dev);
    props->description = ggml_backend_cpu_device_get_description(dev);
    props->type        = ggml_backend_cpu_device_get_type(dev);
    ggml_backend_cpu_device_get_memory(dev, &props->memory_free, &props->memory_total);
    props->caps = {
        /* .async                 = */ false,
        /* .host_buffer           = */ false,
        /* .buffer_from_host_ptr  = */ true,
        /* .events                = */ false,
        /* .mmap_support          = */ true,
    };
}

static ggml_backend_t ggml_backend_cpu_device_init_backend(ggml_backend_dev_t dev, const char * params) {
    return ggml_backend_cpu_init();

    GGML_UNUSED(dev);
    GGML_UNUSED(params);
}

static ggml_backend_buffer_type_t ggml_backend_cpu_device_get_buffer_type(ggml_backend_dev_t dev) {
    return ggml_backend_cpu_buffer_type();

    GGML_UNUSED(dev);
}

static ggml_backend_buffer_t ggml_backend_cpu_device_buffer_from_host_ptr(ggml_backend_dev_t dev, void * ptr, size_t size, size_t max_tensor_size) {
    return ggml_backend_cpu_buffer_from_ptr(ptr, size);

    GGML_UNUSED(dev);
    GGML_UNUSED(max_tensor_size);
}

static bool ggml_backend_cpu_device_supports_op(ggml_backend_dev_t dev, const struct ggml_tensor * op) {
if(fg145_enabled){fg145_request q={};q.dev=dev;q.op=op;return fg145_call(10,q)!=0;}

    const struct ggml_tensor * src0 = op->src[0];
    const struct ggml_tensor * src1 = op->src[1];

    if (op->op == GGML_OP_NONE || op->op == GGML_OP_RESHAPE || op->op == GGML_OP_VIEW || op->op == GGML_OP_PERMUTE || op->op == GGML_OP_TRANSPOSE) {
        return true;
    }

    // check extra buffer types
    // note: only the first sources are checked for extra buffer types to reduce overhead, increase if necessary
    for (int i = 0; i < 4; i++) {
        if (op->src[i] && op->src[i]->buffer &&
            ggml_backend_cpu_is_extra_buffer_type(op->src[i]->buffer->buft)) {
            auto * buf_extra = (ggml::cpu::extra_buffer_type *) op->src[i]->buffer->buft->context;
            return buf_extra->supports_op(dev, op);
        }
    }

    switch (op->op) {
        case GGML_OP_CPY:
        case GGML_OP_SET_ROWS:
            return
                op->type != GGML_TYPE_IQ3_XXS &&
                op->type != GGML_TYPE_IQ3_S   &&
                op->type != GGML_TYPE_IQ2_XXS &&
                op->type != GGML_TYPE_IQ2_XS  &&
                op->type != GGML_TYPE_IQ2_S   &&
                op->type != GGML_TYPE_IQ1_S   &&
                op->type != GGML_TYPE_IQ1_M; // missing type_traits.from_float
        case GGML_OP_MUL_MAT:
            if (ggml_get_op_params_i32(op, 1) == GGML_HINT_SRC0_IS_HADAMARD &&
                src0->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32) {
                return src1->type == GGML_TYPE_F32 || src1->type == GGML_TYPE_F16;
            }
            // BF16 in src1 is widened into the F32 work buffer
            return src1->type == GGML_TYPE_F32 ||
                   src1->type == ggml_get_type_traits_cpu(src0->type)->vec_dot_type ||
                   (src1->type == GGML_TYPE_BF16 && ggml_get_type_traits_cpu(src0->type)->vec_dot_type == GGML_TYPE_F32);
        case GGML_OP_SOFT_MAX_BACK: {
            if (op->src[0]->type != GGML_TYPE_F32 || op->src[1]->type != GGML_TYPE_F32) {
                return false;
            }
            float max_bias = 0.0f;

            memcpy(&max_bias, (const float *) op->op_params + 1, sizeof(float));

            return max_bias == 0.0f;
        }
        case GGML_OP_IM2COL_BACK:
            return src0->type == GGML_TYPE_F32 && (src1->type == GGML_TYPE_F32 || src1->type == GGML_TYPE_F16);
        case GGML_OP_GET_ROWS_BACK:
            return src0->type == GGML_TYPE_F32 || src0->type == GGML_TYPE_F16;
        case GGML_OP_OUT_PROD:
            return (src0->type == GGML_TYPE_F32 ||
                    ((src0->type == GGML_TYPE_F16 || ggml_is_quantized(src0->type)) && src0->ne[2] == src1->ne[2] && src0->ne[3] == src1->ne[3])) &&
                src1->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32;
        case GGML_OP_CONV_2D:
            return ggml_is_contiguous(op->src[0]);
        case GGML_OP_SSM_SCAN:
            return ggml_get_op_params_i32(op, 0) == 1 || op->src[3]->ne[0] == 1;
        default:
            return true;
    }
}

static bool ggml_backend_cpu_device_supports_buft(ggml_backend_dev_t dev, ggml_backend_buffer_type_t buft) {
if(fg145_enabled){fg145_request q={};q.dev=dev;q.buft=buft;return fg145_call(11,q)!=0;}

    return ggml_backend_buft_is_host(buft) || ggml_backend_cpu_is_extra_buffer_type(buft);
    GGML_UNUSED(dev);
}

static const struct ggml_backend_device_i ggml_backend_cpu_device_i = {
    /* .get_name             = */ ggml_backend_cpu_device_get_name,
    /* .get_description      = */ ggml_backend_cpu_device_get_description,
    /* .get_memory           = */ ggml_backend_cpu_device_get_memory,
    /* .get_type             = */ ggml_backend_cpu_device_get_type,
    /* .get_props            = */ ggml_backend_cpu_device_get_props,
    /* .init_backend         = */ ggml_backend_cpu_device_init_backend,
    /* .get_buffer_type      = */ ggml_backend_cpu_device_get_buffer_type,
    /* .get_host_buffer_type = */ NULL,
    /* .buffer_from_host_ptr = */ ggml_backend_cpu_device_buffer_from_host_ptr,
    /* .supports_op          = */ ggml_backend_cpu_device_supports_op,
    /* .supports_buft        = */ ggml_backend_cpu_device_supports_buft,
    /* .offload_op           = */ NULL,
    /* .event_new            = */ NULL,
    /* .event_free           = */ NULL,
    /* .event_synchronize    = */ NULL,
};

// CPU backend - backend (reg)

static const char * ggml_backend_cpu_reg_get_name(ggml_backend_reg_t reg) {
    return "CPU";

    GGML_UNUSED(reg);
}

static size_t ggml_backend_cpu_reg_get_device_count(ggml_backend_reg_t reg) {
if(fg145_enabled){fg145_request q={};return(size_t)fg145_call(17,q);}

    return 1;

    GGML_UNUSED(reg);
}

static ggml_backend_dev_t ggml_backend_cpu_reg_get_device(ggml_backend_reg_t reg, size_t index) {
    GGML_ASSERT(index == 0);

    static ggml_backend_cpu_device_context ctx;
    static ggml_backend_device ggml_backend_cpu_device = {
        /* .iface   = */ ggml_backend_cpu_device_i,
        /* .reg     = */ reg,
        /* .context = */ &ctx,
    };

    return &ggml_backend_cpu_device;
}

// This is intended to replace the the ggml_cpu_has_* functions when loading the CPU backend dynamically,
// and additionally to allow other backends to expose their own list of features that applications can query using the same API
static ggml_backend_feature * ggml_backend_cpu_get_features(ggml_backend_reg_t reg) {
if(fg145_enabled)return fg145_features();

    static std::vector<ggml_backend_feature> features = []() {
        ggml_cpu_init();

        std::vector<ggml_backend_feature> features;
        if (ggml_cpu_has_sse3()) {
            features.push_back({ "SSE3", "1" });
        }
        if (ggml_cpu_has_ssse3()) {
            features.push_back({ "SSSE3", "1" });
        }
        if (ggml_cpu_has_avx()) {
            features.push_back({ "AVX", "1" });
        }
        if (ggml_cpu_has_avx_vnni()) {
            features.push_back({ "AVX_VNNI", "1" });
        }
        if (ggml_cpu_has_avx2()) {
            features.push_back({ "AVX2", "1" });
        }
        if (ggml_cpu_has_f16c()) {
            features.push_back({ "F16C", "1" });
        }
        if (ggml_cpu_has_fma()) {
            features.push_back({ "FMA", "1" });
        }
        if (ggml_cpu_has_bmi2()) {
            features.push_back({ "BMI2", "1" });
        }
        if (ggml_cpu_has_avx512()) {
            features.push_back({ "AVX512", "1" });
        }
        if (ggml_cpu_has_avx512_vbmi()) {
            features.push_back({ "AVX512_VBMI", "1" });
        }
        if (ggml_cpu_has_avx512_vnni()) {
            features.push_back({ "AVX512_VNNI", "1" });
        }
        if (ggml_cpu_has_avx512_bf16()) {
            features.push_back({ "AVX512_BF16", "1" });
        }
        if (ggml_cpu_has_amx_int8()) {
            features.push_back({ "AMX_INT8", "1" });
        }
        if (ggml_cpu_has_neon()) {
            features.push_back({ "NEON", "1" });
        }
        if (ggml_cpu_has_arm_fma()) {
            features.push_back({ "ARM_FMA", "1" });
        }
        if (ggml_cpu_has_fp16_va()) {
            features.push_back({ "FP16_VA", "1" });
        }
        if (ggml_cpu_has_matmul_int8()) {
            features.push_back({ "MATMUL_INT8", "1" });
        }
        if (ggml_cpu_has_sve()) {
            features.push_back({ "SVE", "1" });
        }
        if (ggml_cpu_has_dotprod()) {
            features.push_back({ "DOTPROD", "1" });
        }
        if (ggml_cpu_get_sve_cnt() > 0) {
            static std::string sve_cnt = std::to_string(ggml_cpu_get_sve_cnt());
            features.push_back({ "SVE_CNT", sve_cnt.c_str() });
        }
        if (ggml_cpu_has_sme()) {
            features.push_back({ "SME", "1" });
        }
        if (ggml_cpu_has_sme2()) {
            features.push_back({ "SME2", "1" });
        }
        if (ggml_cpu_has_riscv_v()) {
            features.push_back({ "RISCV_V", "1" });
        }
        if (ggml_cpu_get_rvv_vlen() > 0) {
            static std::string rvv_vlen = std::to_string(ggml_cpu_get_rvv_vlen());
            features.push_back({ "RVV_VLEN", rvv_vlen.c_str() });
        }
        if (ggml_cpu_has_vsx()) {
            features.push_back({ "VSX", "1" });
        }
        if (ggml_cpu_has_vxe()) {
            features.push_back({ "VXE", "1" });
        }
        if (ggml_cpu_has_wasm_simd()) {
            features.push_back({ "WASM_SIMD", "1" });
        }
        if (ggml_cpu_has_llamafile()) {
            features.push_back({ "LLAMAFILE", "1" });
        }
    #ifdef GGML_USE_ACCELERATE
        features.push_back({ "ACCELERATE", "1" });
    #endif
    #ifdef GGML_USE_CPU_HBM
        features.push_back({ "CPU_HBM", "1" });
    #endif
    #ifdef GGML_USE_OPENMP
        features.push_back({ "OPENMP", "1" });
    #endif
    #ifdef GGML_USE_CPU_KLEIDIAI
        features.push_back({ "KLEIDIAI", "1" });
    #endif
    #ifdef GGML_USE_CPU_REPACK
        features.push_back({ "REPACK", "1" });
    #endif

        features.push_back({ nullptr, nullptr });

        return features;
    }();

    return features.data();

    GGML_UNUSED(reg);
}

static void * ggml_backend_cpu_get_proc_address(ggml_backend_reg_t reg, const char * name) {
if(fg145_enabled){fg145_request q={};q.name=name;q.name_len=name?(int64_t)std::strlen(name):0;return(void*)(uintptr_t)fg145_call(15,q);}

    if (strcmp(name, "ggml_backend_set_n_threads") == 0) {
        ggml_backend_set_n_threads_t fct = ggml_backend_cpu_set_n_threads;
        return (void *)fct;
    }
    if (strcmp(name, "ggml_backend_dev_get_extra_bufts") == 0) {
        ggml_backend_dev_get_extra_bufts_t fct = ggml_backend_cpu_device_get_extra_buffers_type;
        return (void *)fct;
    }
    if (strcmp(name, "ggml_backend_get_features") == 0) {
        return (void *)ggml_backend_cpu_get_features;
    }
    if (strcmp(name, "ggml_backend_set_abort_callback") == 0) {
        return (void *)ggml_backend_cpu_set_abort_callback;
    }
    if (strcmp(name, "ggml_backend_cpu_numa_init") == 0) {
        return (void *)ggml_numa_init;
    }
    if (strcmp(name, "ggml_backend_cpu_is_numa") == 0) {
        return (void *)ggml_is_numa;
    }
    if (strcmp(name, "ggml_backend_cpu_set_use_ref") == 0) {
        return (void *)ggml_backend_cpu_set_use_ref;
    }

    // threadpool - TODO:  move to ggml-base
    if (strcmp(name, "ggml_threadpool_new") == 0) {
        return (void *)ggml_threadpool_new;
    }
    if (strcmp(name, "ggml_threadpool_free") == 0) {
        return (void *)ggml_threadpool_free;
    }
    if (strcmp(name, "ggml_backend_cpu_set_threadpool") == 0) {
        return (void *)ggml_backend_cpu_set_threadpool;
    }

    return NULL;

    GGML_UNUSED(reg);
}

static const struct ggml_backend_reg_i ggml_backend_cpu_reg_i = {
    /* .get_name         = */ ggml_backend_cpu_reg_get_name,
    /* .get_device_count = */ ggml_backend_cpu_reg_get_device_count,
    /* .get_device       = */ ggml_backend_cpu_reg_get_device,
    /* .get_proc_address = */ ggml_backend_cpu_get_proc_address,
};

ggml_backend_reg_t ggml_backend_cpu_reg(void) {
    // init CPU feature detection
    ggml_cpu_init();

    static struct ggml_backend_reg ggml_backend_cpu_reg = {
        /* .api_version = */ GGML_BACKEND_API_VERSION,
        /* .iface       = */ ggml_backend_cpu_reg_i,
        /* .context     = */ NULL,
    };

    return &ggml_backend_cpu_reg;
}

GGML_BACKEND_DL_IMPL(ggml_backend_cpu_reg)


static fg145_request fg145_request_backend(ggml_backend_t backend){fg145_request q={};q.backend=backend;q.cpu=(ggml_backend_cpu_context*)backend->context;return q;}
static ggml_cplan&fg145_plan(fg145_request&r){return r.plan?r.plan->cplan:r.temporary;}
static std::vector<ggml_backend_buffer_type_t>&fg145_extras(){static std::vector<ggml_backend_buffer_type_t> value=[](){std::vector<ggml_backend_buffer_type_t> out;fg145_request q={};q.extras=&out;fg145_call(13,q);return out;}();return value;}
static ggml_backend_buffer_type_t*fg145_extra_copy(){static std::vector<ggml_backend_buffer_type_t> value=[](){std::vector<ggml_backend_buffer_type_t> out;fg145_request q={};q.extras=&out;fg145_call(14,q);return out;}();return value.data();}
static ggml_backend_feature*fg145_features(){static std::vector<ggml_backend_feature> value=[](){ggml_cpu_init();std::vector<ggml_backend_feature> out;fg145_request q={};q.features=&out;fg145_call(18,q);return out;}();return value.data();}
extern "C" int64_t fg145_meta(int64_t context,int64_t field){auto&r=*(fg145_request*)(uintptr_t)context;const ggml_tensor*t=r.op;const ggml_tensor*x=t?t->src[0]:nullptr;const ggml_tensor*y=t?t->src[1]:nullptr;
 if(field>=100&&field<104){auto*source=t->src[field-100];return source&&source->buffer?(int64_t)(uintptr_t)source->buffer->buft:0;}
 if(field>=200&&field<300){auto&v=fg145_extras();size_t i=(size_t)(field-200);GGML_ASSERT(i<v.size());return(int64_t)(uintptr_t)v[i];}
 if(field>=300&&field<400){GGML_ASSERT(field-300<r.name_len);return(uint8_t)r.name[field-300];}
 if(field>=400){switch(field-400){case 0:return ggml_cpu_has_sse3();
case 1:return ggml_cpu_has_ssse3();
case 2:return ggml_cpu_has_avx();
case 3:return ggml_cpu_has_avx_vnni();
case 4:return ggml_cpu_has_avx2();
case 5:return ggml_cpu_has_f16c();
case 6:return ggml_cpu_has_fma();
case 7:return ggml_cpu_has_bmi2();
case 8:return ggml_cpu_has_avx512();
case 9:return ggml_cpu_has_avx512_vbmi();
case 10:return ggml_cpu_has_avx512_vnni();
case 11:return ggml_cpu_has_avx512_bf16();
case 12:return ggml_cpu_has_amx_int8();
case 13:return ggml_cpu_has_neon();
case 14:return ggml_cpu_has_arm_fma();
case 15:return ggml_cpu_has_fp16_va();
case 16:return ggml_cpu_has_matmul_int8();
case 17:return ggml_cpu_has_sve();
case 18:return ggml_cpu_has_dotprod();
case 19:return ggml_cpu_get_sve_cnt();
case 20:return ggml_cpu_has_sme();
case 21:return ggml_cpu_has_sme2();
case 22:return ggml_cpu_has_riscv_v();
case 23:return ggml_cpu_get_rvv_vlen();
case 24:return ggml_cpu_has_vsx();
case 25:return ggml_cpu_has_vxe();
case 26:return ggml_cpu_has_wasm_simd();
case 27:return ggml_cpu_has_llamafile();
case 28:
#ifdef GGML_USE_ACCELERATE
return 1;
#else
return 0;
#endif

case 29:
#ifdef GGML_USE_CPU_HBM
return 1;
#else
return 0;
#endif

case 30:
#ifdef GGML_USE_OPENMP
return 1;
#else
return 0;
#endif

case 31:
#ifdef GGML_USE_CPU_KLEIDIAI
return 1;
#else
return 0;
#endif

case 32:
#ifdef GGML_USE_CPU_REPACK
return 1;
#else
return 0;
#endif
 default:GGML_ABORT("invalid CPU feature");}}
 switch(field){case 0:return ggml_backend_is_cpu(r.backend);case 1:return GGML_DEFAULT_N_THREADS;case 2:return r.cpu->n_threads;case 3:return(int64_t)(uintptr_t)r.cpu->threadpool;case 4:return(int64_t)(uintptr_t)r.cpu->work_data;case 5:GGML_ASSERT(r.cpu->work_size<=INT64_MAX);return(int64_t)r.cpu->work_size;case 6:return(int64_t)(uintptr_t)r.cpu->abort_callback;case 7:return(int64_t)(uintptr_t)r.cpu->abort_callback_data;case 8:return r.cpu->use_ref;case 9:return r.value;case 10:return(int64_t)(uintptr_t)r.pointer;case 11:return(int64_t)(uintptr_t)r.data;case 12:return(int64_t)(uintptr_t)r.plan;case 13:GGML_ASSERT(fg145_plan(r).work_size<=INT64_MAX);return(int64_t)fg145_plan(r).work_size;case 14:return(int64_t)(uintptr_t)fg145_plan(r).work_data;
 case 20:return t->op;case 21:return t->type;case 22:return x?x->type:-1;case 23:return y?y->type:-1;case 24:return ggml_get_type_traits_cpu(x->type)->vec_dot_type;case 25:return x?ggml_is_quantized(x->type):0;case 26:return x?ggml_is_contiguous(x):0;case 27:return ggml_get_op_params_i32(t,0);case 28:return ggml_get_op_params_i32(t,1);case 29:return GGML_HINT_SRC0_IS_HADAMARD;case 30:return t->src[3]?t->src[3]->ne[0]:0;case 31:return x?x->ne[2]:0;case 32:return y?y->ne[2]:0;case 33:return x?x->ne[3]:0;case 34:return y?y->ne[3]:0;case 40:return(int64_t)fg145_extras().size();case 41:return ggml_backend_buft_is_host(r.buft);case 42:return(int64_t)(uintptr_t)r.buft;case 43:return r.name_len;
 case 44:
#ifdef _WIN32
 return 1;
#else
 return 0;
#endif
 case 45:
#ifdef _WIN32
 {MEMORYSTATUSEX status;status.dwLength=sizeof(status);GlobalMemoryStatusEx(&status);return(int64_t)status.ullTotalPhys;}
#else
 return sysconf(_SC_PHYS_PAGES);
#endif
 case 46:
#ifdef _WIN32
 {MEMORYSTATUSEX status;status.dwLength=sizeof(status);GlobalMemoryStatusEx(&status);return(int64_t)status.ullAvailPhys;}
#else
 return sysconf(_SC_PAGE_SIZE);
#endif
 case 47:return GGML_STATUS_ALLOC_FAILED;
 case 60:return(int64_t)(uintptr_t)(r.buffer?(r.buffer->context):r.pointer);case 61:return(int64_t)(r.buffer?r.buffer->size:r.buf_size);case 62:return r.buf_kind;case 63:return r.op?(int64_t)(uintptr_t)r.op->data:0;case 64:return(int64_t)r.buf_offset;case 65:return(int64_t)r.buf_size;case 66:return r.buf_value;case 67:return(int64_t)(uintptr_t)r.data;case 68:return(int64_t)(uintptr_t)r.data2;case 69:return r.op?(int64_t)ggml_nbytes(r.op):0;case 70:return r.buf_kind;case 71:return TENSOR_ALIGNMENT;case 72:return(int64_t)(uintptr_t)r.buft;case 73:return(int64_t)(uintptr_t)r.params;case 74:return(int64_t)(uintptr_t)r.size_ptr;case 75:return r.value;case 76:return r.op?(int64_t)(uintptr_t)r.op:0;case 77:return(int64_t)(uintptr_t)r.buffer;case 51:return 33;default:GGML_ABORT("invalid CPU backend metadata");}
}
extern "C" float fg145_float(int64_t context,int64_t field){auto&r=*(fg145_request*)(uintptr_t)context;GGML_ASSERT(field==0);float result;std::memcpy(&result,(const char*)r.op->op_params+sizeof(float),sizeof(result));return result;}
extern "C" int64_t fg145_action(int64_t context,int64_t op,int64_t a,int64_t b){auto&r=*(fg145_request*)(uintptr_t)context;switch(op){
 case 0:r.cpu=new(std::nothrow)ggml_backend_cpu_context;return(int64_t)(uintptr_t)r.cpu;
 case 1:switch(a){case 0:GGML_ASSERT(b>=INT32_MIN&&b<=INT32_MAX);r.cpu->n_threads=(int)b;break;case 1:r.cpu->threadpool=(ggml_threadpool_t)(uintptr_t)b;break;case 2:r.cpu->work_data=(uint8_t*)(uintptr_t)b;break;case 3:GGML_ASSERT(b>=0);r.cpu->work_size=(size_t)b;break;case 4:r.cpu->abort_callback=(ggml_abort_callback)(uintptr_t)b;break;case 5:r.cpu->abort_callback_data=(void*)(uintptr_t)b;break;case 6:r.cpu->use_ref=b!=0;break;default:GGML_ABORT("invalid CPU context write");}return 0;
 case 2:{auto*backend=new(std::nothrow)ggml_backend{ggml_backend_cpu_guid(),ggml_backend_cpu_i,ggml_backend_reg_dev_get(ggml_backend_cpu_reg(),0),r.cpu};return(int64_t)(uintptr_t)backend;}
 case 3:delete r.cpu;r.cpu=nullptr;return 0;case 4:delete r.backend;r.backend=nullptr;return 0;
 case 5:GGML_ASSERT(a>=0);return(int64_t)(uintptr_t)new(std::nothrow)uint8_t[(size_t)a];
 case 6:delete[](uint8_t*)(uintptr_t)a;return 0;
 case 7:ggml_threadpool_pause((ggml_threadpool_t)(uintptr_t)a);return 0;
 case 8:r.plan=new(std::nothrow)ggml_backend_plan_cpu;return(int64_t)(uintptr_t)r.plan;
 case 9:delete r.plan;r.plan=nullptr;return 0;
 case 10:fg145_plan(r)=ggml_graph_plan(r.graph,r.cpu->n_threads,r.cpu->threadpool);return 0;
 case 11:r.plan->cgraph=*r.graph;return 0;
 case 12:switch(a){case 0:fg145_plan(r).work_data=(uint8_t*)(uintptr_t)b;break;case 1:fg145_plan(r).abort_callback=(ggml_abort_callback)(uintptr_t)b;break;case 2:fg145_plan(r).abort_callback_data=(void*)(uintptr_t)b;break;case 3:fg145_plan(r).use_ref=b!=0;break;default:GGML_ABORT("invalid CPU plan write");}return 0;
 case 13:GGML_ASSERT(a==0||a==1);return a?ggml_graph_compute(&r.plan->cgraph,&r.plan->cplan):ggml_graph_compute(r.graph,&r.temporary);
 case 14:{GGML_ASSERT(a>=0&&a<4);auto*extra=(ggml::cpu::extra_buffer_type*)r.op->src[a]->buffer->buft->context;return extra->supports_op(r.dev,r.op);}
 case 15:r.extras->push_back((ggml_backend_buffer_type_t)(uintptr_t)a);return 0;
 case 16:switch(a){case 0:
#if defined(__AMX_INT8__) && defined(__AVX512VNNI__)
 return(int64_t)(uintptr_t)ggml_backend_amx_buffer_type();
#else
 return 0;
#endif
 case 1:
#ifdef GGML_USE_CPU_RISCV64_SPACEMIT
 return(int64_t)(uintptr_t)ggml_backend_cpu_riscv64_spacemit_buffer_type();
#else
 return 0;
#endif
 case 2:
#ifdef GGML_USE_CPU_KLEIDIAI
 return(int64_t)(uintptr_t)ggml_backend_cpu_kleidiai_buffer_type();
#else
 return 0;
#endif
 case 3:
#ifdef GGML_USE_CPU_REPACK
 return(int64_t)(uintptr_t)ggml_backend_cpu_repack_buffer_type();
#else
 return 0;
#endif
 default:GGML_ABORT("invalid extra buffer provider");}
 case 17:{static void*const addresses[]={(void*)ggml_backend_cpu_set_n_threads,(void*)ggml_backend_cpu_device_get_extra_buffers_type,(void*)ggml_backend_cpu_get_features,(void*)ggml_backend_cpu_set_abort_callback,(void*)ggml_numa_init,(void*)ggml_is_numa,(void*)ggml_backend_cpu_set_use_ref,(void*)ggml_threadpool_new,(void*)ggml_threadpool_free,(void*)ggml_backend_cpu_set_threadpool};GGML_ASSERT(a>=0&&(size_t)a<sizeof(addresses)/sizeof(*addresses));return(int64_t)(uintptr_t)addresses[a];}
 case 18:GGML_ASSERT(b>=0);if(a==0)*r.free_mem=(size_t)b;else{GGML_ASSERT(a==1);*r.total_mem=(size_t)b;}return 0;
 case 19:{if(a==-1){r.features->push_back({nullptr,nullptr});return 0;}static const char*const names[]={"SSE3","SSSE3","AVX","AVX_VNNI","AVX2","F16C","FMA","BMI2","AVX512","AVX512_VBMI","AVX512_VNNI","AVX512_BF16","AMX_INT8","NEON","ARM_FMA","FP16_VA","MATMUL_INT8","SVE","DOTPROD","SVE_CNT","SME","SME2","RISCV_V","RVV_VLEN","VSX","VXE","WASM_SIMD","LLAMAFILE","ACCELERATE","CPU_HBM","OPENMP","KLEIDIAI","REPACK"};GGML_ASSERT(a>=0&&(size_t)a<sizeof(names)/sizeof(*names));const char*value="1";if(a==19){static std::string sve_count=std::to_string(ggml_cpu_get_sve_cnt());value=sve_count.c_str();}if(a==23){static std::string rvv_count=std::to_string(ggml_cpu_get_rvv_vlen());value=rvv_count.c_str();} r.features->push_back({names[a],value});return 0;}
 case 20:GGML_ASSERT(a>=0);return(int64_t)(uintptr_t)ggml_aligned_malloc((size_t)a);
 case 22:GGML_ASSERT(b>=0);ggml_aligned_free((void*)(uintptr_t)a,(size_t)b);return 0;
 case 23:{ GGML_UNUSED(a); GGML_ABORT("HBM path inactive: GGML_USE_CPU_HBM off in this build");return 0;}
 case 24:GGML_ASSERT(a>0&&b>=0);memset((void*)(uintptr_t)a,(int)b,r.buf_size);return 0;
 case 25:GGML_ASSERT(a>0&&b>0);memcpy((void*)(uintptr_t)a,(const void*)(uintptr_t)b,r.buf_size);return 0;
 case 27:{GGML_ASSERT(a>=0&&a<(int64_t)fg145_extras().size());auto*extra=(ggml::cpu::extra_buffer_type*)fg145_extras()[(size_t)a]->context;if(!extra)return 0;auto*tr=extra->get_tensor_traits((ggml_tensor*)r.op);return tr&&tr->compute_forward((ggml_compute_params*)(uintptr_t)r.params,(ggml_tensor*)r.op)?1:0;}
 case 28:{GGML_ASSERT(a>=0&&a<(int64_t)fg145_extras().size());auto*extra=(ggml::cpu::extra_buffer_type*)fg145_extras()[(size_t)a]->context;if(!extra)return 0;auto*tr=extra->get_tensor_traits(r.op);return tr&&tr->work_size((int)r.value,r.op,*r.size_ptr)?1:0;}
 default:GGML_ABORT("invalid CPU backend primitive");}}
