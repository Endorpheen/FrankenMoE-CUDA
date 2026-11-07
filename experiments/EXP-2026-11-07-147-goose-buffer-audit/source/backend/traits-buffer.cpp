
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdint>
struct ggml_backend_cpu_context;
struct ggml_backend_plan_cpu;
#include "request.h"
extern "C" int64_t fg145_backend_api(int64_t,int64_t);
static bool fg145_enabled=true;
__attribute__((constructor))static void fg145_config_tr(){const char*v=std::getenv("GGML_GOOSE_CPU_BACKEND_API");fg145_enabled=!v||std::strcmp(v,"0");}
static int64_t fg145_call(int64_t mode,fg145_request&q){return fg145_backend_api(mode,(int64_t)(uintptr_t)&q);}

#include "traits.h"

#include "ggml-backend-impl.h"
#include "ggml-backend.h"

namespace ggml::cpu {
tensor_traits::~tensor_traits() {}

extra_buffer_type::~extra_buffer_type() {}
}  // namespace ggml::cpu

bool ggml_cpu_extra_compute_forward(struct ggml_compute_params * params, struct ggml_tensor * op) {if(fg145_enabled){fg145_request q={};q.op=op;q.params=params;return fg145_call(30,q)!=0;}
    for (auto extra : ggml_backend_cpu_get_extra_buffer_types()) {
        if (extra && extra->context) {
            auto buf_extra     = (ggml::cpu::extra_buffer_type *) extra->context;
            auto tensor_traits = buf_extra->get_tensor_traits(op);
            if (tensor_traits && tensor_traits->compute_forward(params, op)) {
                return true;
            }
        }
    }
    return false;
}

bool ggml_cpu_extra_work_size(int n_threads, const struct ggml_tensor * op, size_t * size) {if(fg145_enabled){fg145_request q={};q.op=op;q.size_ptr=size;q.value=n_threads;return fg145_call(31,q)!=0;}
    for (auto extra : ggml_backend_cpu_get_extra_buffer_types()) {
        if (extra && extra->context) {
            auto buf_extra     = (ggml::cpu::extra_buffer_type *) extra->context;
            auto tensor_traits = buf_extra->get_tensor_traits(op);
            if (tensor_traits && tensor_traits->work_size(n_threads, op, *size)) {
                return true;
            }
        }
    }
    return false;
}
