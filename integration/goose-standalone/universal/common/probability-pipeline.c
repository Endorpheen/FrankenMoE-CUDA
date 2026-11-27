#include "probability.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PROBABILITY_SLOTS = 3 };
struct probability_job {
    int64_t selected;
    float * logits;
};

struct probability_pipeline {
    pthread_mutex_t lock;
    pthread_cond_t progress;
    pthread_t worker;
    struct probability_job jobs[PROBABILITY_SLOTS];
    int64_t vocab, top, limit, written, consumed;
    uint8_t closed, failed;
    float * workspace;
    int64_t * order;
    int64_t * ids;
    float * logp;
};

static _Thread_local struct probability_pipeline * request_pipeline;

extern void fg_probability_init(int argc, char ** argv);

static void * probability_worker(void * arg) {
    struct probability_pipeline * p = arg;
    fg_probability_init(0, NULL);
    pthread_mutex_lock(&p->lock);
    for (;;) {
        while (p->consumed == p->written && !p->closed) pthread_cond_wait(&p->progress, &p->lock);
        if (p->consumed == p->written || p->failed) break;
        const int64_t row = p->consumed;
        const int64_t width = p->top + 1;
        const struct probability_job job = p->jobs[row % PROBABILITY_SLOTS];
        pthread_mutex_unlock(&p->lock);
        const uint8_t ok = fg_probability_worker((sl_f32) {job.logits, p->vocab},
            (sl_i64) {p->order, p->vocab}, job.selected, p->top,
            (sl_i64) {p->ids + row * width, width}, (sl_f32) {p->logp + row * width, width});
        pthread_mutex_lock(&p->lock);
        ++p->consumed;
        if (!ok) p->failed = 1;
        pthread_cond_broadcast(&p->progress);
    }
    pthread_mutex_unlock(&p->lock);
    return NULL;
}

static void free_probability_pipeline(struct probability_pipeline * p) {
    free(p->workspace);
    free(p->order);
    free(p->ids);
    free(p->logp);
    free(p);
}

uint8_t fg_prob_pipeline_start(int64_t vocab, int64_t top, int64_t limit) {
    const char * setting = getenv("GOOSE_CPU_PROBABILITY_PIPELINE");
    if (!setting || strcmp(setting, "1") != 0 || request_pipeline ||
        vocab <= 0 || top <= 0 || top > vocab || limit <= 0) return 0;
    const uint64_t width = (uint64_t) top + 1;
    if ((uint64_t) vocab > SIZE_MAX / (PROBABILITY_SLOTS * sizeof(float)) ||
        (uint64_t) vocab > SIZE_MAX / sizeof(int64_t) ||
        width > INT64_MAX || (uint64_t) limit > INT64_MAX / width ||
        (uint64_t) limit > SIZE_MAX / sizeof(int64_t) / width) return 0;
    struct probability_pipeline * p = calloc(1, sizeof(*p));
    if (!p) return 0;
    p->vocab = vocab;
    p->top = top;
    p->limit = limit;
    const size_t fields = (size_t) limit * (size_t) width;
    p->workspace = malloc(PROBABILITY_SLOTS * (size_t) vocab * sizeof(float));
    p->order = malloc((size_t) vocab * sizeof(int64_t));
    p->ids = malloc(fields * sizeof(int64_t));
    p->logp = malloc(fields * sizeof(float));
    if (!p->workspace || !p->order || !p->ids || !p->logp) goto free_workspace;
    for (size_t i = 0; i < PROBABILITY_SLOTS; ++i) p->jobs[i].logits = p->workspace + i * (size_t) vocab;
    if (pthread_mutex_init(&p->lock, NULL) != 0) goto free_workspace;
    if (pthread_cond_init(&p->progress, NULL) != 0) goto destroy_mutex;
    if (pthread_create(&p->worker, NULL, probability_worker, p) != 0) goto destroy_cond;
    request_pipeline = p;
    return 1;

destroy_cond:
    pthread_cond_destroy(&p->progress);
destroy_mutex:
    pthread_mutex_destroy(&p->lock);
free_workspace:
    free_probability_pipeline(p);
    return 0;
}

uint8_t fg_prob_pipeline_submit(sl_f32 logits, int64_t selected) {
    struct probability_pipeline * p = request_pipeline;
    if (!p || !logits.data || logits.len != p->vocab || selected < 0 || selected >= p->vocab) return 0;
    pthread_mutex_lock(&p->lock);
    while (p->written - p->consumed == PROBABILITY_SLOTS && !p->failed) {
        pthread_cond_wait(&p->progress, &p->lock);
    }
    if (p->failed || p->written >= p->limit) {
        pthread_mutex_unlock(&p->lock);
        return 0;
    }
    struct probability_job * job = &p->jobs[p->written % PROBABILITY_SLOTS];
    memcpy(job->logits, logits.data, (size_t) logits.len * sizeof(float));
    job->selected = selected;
    ++p->written;
    pthread_cond_broadcast(&p->progress);
    pthread_mutex_unlock(&p->lock);
    return 1;
}

uint8_t fg_prob_pipeline_finish(int64_t count, sl_i64 ids, sl_f32 logp) {
    struct probability_pipeline * p = request_pipeline;
    if (!p) return 0;
    request_pipeline = NULL;
    pthread_mutex_lock(&p->lock);
    p->closed = 1;
    pthread_cond_broadcast(&p->progress);
    pthread_mutex_unlock(&p->lock);
    pthread_join(p->worker, NULL);
    const int64_t fields = count > 0 && count <= p->limit ? count * (p->top + 1) : 0;
    const uint8_t ok = !p->failed && count == p->consumed && fields > 0 &&
        ids.data && logp.data && ids.len >= fields && logp.len >= fields;
    if (ok) {
        memcpy(ids.data, p->ids, (size_t) fields * sizeof(int64_t));
        memcpy(logp.data, p->logp, (size_t) fields * sizeof(float));
    }
    pthread_cond_destroy(&p->progress);
    pthread_mutex_destroy(&p->lock);
    free_probability_pipeline(p);
    return ok;
}
