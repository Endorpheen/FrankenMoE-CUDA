#include "main.h"
#include <nvToolsExt.h>
#include <stdbool.h>
#include <stdio.h>
extern int64_t __real_fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill);

static _Thread_local struct {
    nvtxDomainHandle_t domain;
    nvtxStringHandle_t label;
    unsigned requests;
    bool decoding, marked, finished;
} phase;

int64_t __wrap_fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill) {
    if (prefill) {
        if (!phase.domain) {
            phase.domain = nvtxDomainCreateA("FrankenMoE");
            phase.label = nvtxDomainRegisterStringA(phase.domain, "apex-prefill");
        }
        if (phase.decoding) {
            phase.decoding = false;
            ++phase.requests;
        }
        if (phase.requests == 1 && !phase.marked && !phase.finished) {
            const nvtxEventAttributes_t attributes = {
                .version = NVTX_VERSION, .size = NVTX_EVENT_ATTRIB_STRUCT_SIZE,
                .messageType = NVTX_MESSAGE_TYPE_REGISTERED, .message = {.registered = phase.label},
            };
            nvtxDomainRangePushEx(phase.domain, &attributes);
            phase.marked = true;
        }
    } else {
        phase.decoding = true;
        if (phase.marked) {
            nvtxDomainRangePop(phase.domain);
            phase.marked = false;
            phase.finished = true;
            fputs("Goose profiling: marked measured prefill\n", stderr);
        }
    }
    return __real_fg_q35_generation_eval(tokens, output, prefill);
}
