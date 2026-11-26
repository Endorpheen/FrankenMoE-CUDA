#include "fixture.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int full_steps, device_steps, fallbacks, allow_choice;
static int64_t choices_available;

double gs_exp(double x) { return exp(x); }
float fg158_float(int64_t raw) {
    uint32_t bits = (uint32_t)raw;
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}
int64_t fg_q35_generation_prepare(void) { return 0; }
double fg_q35_generation_now(void) { return 0; }
void hose_reset_state(void) {}
void fg_q35_generation_profile(sl_i64 profile) {
    profile.data[0] = 4;
    profile.data[1] = 4;
    profile.data[2] = 0;
}

static void logits(sl_f32 out, int prefill) {
    assert(out.len == 4);
    out.data[0] = 0;
    out.data[1] = prefill ? 100 : 0;
    out.data[2] = prefill ? 0 : 100;
    out.data[3] = prefill ? 0 : 120;
}

int64_t fg_q35_generation_eval(sl_i32 tokens, sl_f32 out, uint8_t prefill) {
    assert(tokens.len == (prefill ? 3 : 1));
    ++full_steps;
    logits(out, prefill);
    return out.len;
}

uint8_t fg_bonsai_choice_available(sl_i64 eog) {
    ++choices_available;
    assert(eog.len == 1 && eog.data[0] == 3);
    return allow_choice;
}

int64_t fg_bonsai_choose_next(int32_t token, sl_f32 out, uint8_t ignore_eos) {
    assert(token == 1 || token == 2);
    ++device_steps;
    if (device_steps == 2) {
        ++fallbacks;
        logits(out, 0);
        return -2;
    }
    return ignore_eos ? 2 : 3;
}

static void run(int enabled, float temp, int ignore_eos, int top,
                int expected_full, int expected_device, int expected_fallback) {
    full_steps = device_steps = fallbacks = choices_available = 0;
    allow_choice = enabled;
    int64_t ids[4] = {-1, -1, -1, -1};
    const int64_t count = test_generate(temp, ignore_eos, top, (sl_i64){ids, 4});
    assert(count == (ignore_eos ? 4 : 2));
    assert(ids[0] == 1 && ids[1] == (ignore_eos ? 2 : 3));
    if (ignore_eos) assert(ids[2] == 2 && ids[3] == 2);
    assert(full_steps == expected_full && device_steps == expected_device && fallbacks == expected_fallback);
    assert(choices_available == (temp == 0 && top == 0));
}

int main(void) {
    goose_init(0, NULL);
    run(0, 0, 1, 0, 4, 0, 0);
    run(1, 0, 1, 0, 1, 3, 1);
    run(1, 0, 0, 0, 1, 1, 0);
    run(1, 0, 1, 1, 4, 0, 0);
    run(1, .01f, 1, 0, 4, 0, 0);
    puts("generation contract: 5 cases passed");
    return 0;
}
