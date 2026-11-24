#include "main.h"
#include <nvToolsExt.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

extern int64_t __real_fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill);

static _Thread_local struct {
    nvtxDomainHandle_t domain;
    nvtxStringHandle_t request, prefill, decode;
    unsigned requests, steps;
    bool decoding, marked, finished;
} phase;

static void mark(nvtxStringHandle_t label, const char * name) {
    const nvtxEventAttributes_t attr = {
        .version = NVTX_VERSION, .size = NVTX_EVENT_ATTRIB_STRUCT_SIZE,
        .messageType = NVTX_MESSAGE_TYPE_REGISTERED, .message = {.registered = label},
    };
    struct timespec when;
    clock_gettime(CLOCK_REALTIME, &when);
    fprintf(stderr, "Goose phase %s %lld.%09ld\n", name, (long long) when.tv_sec, when.tv_nsec);
    nvtxDomainRangePushEx(phase.domain, &attr);
}

int64_t __wrap_fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill) {
    if (prefill) {
        if (!phase.domain) {
            phase.domain = nvtxDomainCreateA("FrankenMoE");
            phase.request = nvtxDomainRegisterStringA(phase.domain, "apex-request");
            phase.prefill = nvtxDomainRegisterStringA(phase.domain, "apex-prefill");
            phase.decode = nvtxDomainRegisterStringA(phase.domain, "apex-decode");
        }
        if (phase.decoding) {
            phase.decoding = false;
            ++phase.requests;
        }
        if (phase.requests == 1 && !phase.marked && !phase.finished) {
            mark(phase.request, "request");
            mark(phase.prefill, "prefill");
            phase.marked = true;
        }
    } else {
        if (phase.marked && !phase.decoding) {
            nvtxDomainRangePop(phase.domain);
            mark(phase.decode, "decode");
        }
        phase.decoding = true;
    }
    int64_t result = __real_fg_q35_generation_eval(tokens, output, prefill);
    if (phase.marked && !prefill && (++phase.steps == 511 || result < 0)) {
        struct timespec when;
        clock_gettime(CLOCK_REALTIME, &when);
        fprintf(stderr, "Goose phase end %lld.%09ld steps=%u\n",
                (long long) when.tv_sec, when.tv_nsec, phase.steps);
        nvtxDomainRangePop(phase.domain);
        nvtxDomainRangePop(phase.domain);
        phase.marked = false;
        phase.finished = true;
    }
    return result;
}
