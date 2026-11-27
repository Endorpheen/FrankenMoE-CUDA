#include "probability.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct { uint64_t *data; int64_t len; } probability_words;

struct probability_pipeline {
    pthread_mutex_t lock;
    pthread_cond_t progress;
    pthread_t worker;
    uint64_t payload[14];
};
static _Thread_local struct probability_pipeline *request_pipeline;

extern void fg_probability_init(int argc, char **argv);

static void *probability_thread(void *arg) {
    fg_probability_init(0, NULL);
    fg_prob_worker_run((uintptr_t)arg);
    return NULL;
}

uint64_t fg_prob_platform_create(void) {
    struct probability_pipeline *p = calloc(1, sizeof(*p));
    if (!p) return 0;
    if (pthread_mutex_init(&p->lock, NULL) != 0) { free(p); return 0; }
    if (pthread_cond_init(&p->progress, NULL) != 0) {
        pthread_mutex_destroy(&p->lock);
        free(p);
        return 0;
    }
    return (uintptr_t)p;
}

probability_words fg_prob_platform_payload(uint64_t address) {
    struct probability_pipeline *p = (void *)(uintptr_t)address;
    return (probability_words){p->payload, 14};
}

uint8_t fg_prob_platform_start(uint64_t address) {
    struct probability_pipeline *p = (void *)(uintptr_t)address;
    if (pthread_create(&p->worker, NULL, probability_thread, p) != 0) return 0;
    request_pipeline = p;
    return 1;
}

uint64_t fg_prob_platform_current(uint8_t detach) {
    struct probability_pipeline *p = request_pipeline;
    if (detach) request_pipeline = NULL;
    return (uintptr_t)p;
}

void fg_prob_platform_event(uint64_t address, int32_t event) {
    struct probability_pipeline *p = (void *)(uintptr_t)address;
    switch (event) {
    case 0: pthread_mutex_lock(&p->lock); break;
    case 1: pthread_mutex_unlock(&p->lock); break;
    case 2: pthread_cond_wait(&p->progress, &p->lock); break;
    case 3: pthread_cond_broadcast(&p->progress); break;
    case 4: pthread_join(p->worker, NULL); break;
    case 5:
        pthread_cond_destroy(&p->progress);
        pthread_mutex_destroy(&p->lock);
        free(p);
        break;
    }
}

uint8_t fg_prob_pipeline_start(int64_t vocab, int64_t top, int64_t limit) {
    fg_probability_init(0, NULL);
    return fg_prob_start(vocab, top, limit);
}

uint8_t fg_prob_pipeline_submit(sl_f32 logits, int64_t selected) {
    return request_pipeline ? fg_prob_submit(logits, selected) : 0;
}

uint8_t fg_prob_pipeline_finish(int64_t count, sl_i64 ids, sl_f32 logp) {
    return request_pipeline ? fg_prob_finish(count, ids, logp) : 0;
}
