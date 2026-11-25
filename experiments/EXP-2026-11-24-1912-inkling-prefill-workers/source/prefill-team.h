#include <pthread.h>

struct i2_prefill_team {
    pthread_mutex_t lock;
    pthread_cond_t job_ready, job_done;
    const struct i2_prefill_work *work;
    int64_t handles[15];
    int tasks, count, pending;
    uint64_t generation;
    bool stop, failed;
};

extern int64_t i2_prefill_team_start(uint64_t team, int64_t tasks, sl_i64 handles);
extern int64_t i2_prefill_team_join(sl_i64 handles);
extern int64_t fg143_gather_affinity(uint8_t prefill, int64_t task);

void fg_i2_prefill_team_worker(uint64_t handle, int64_t task) {
    struct i2_prefill_team *team = (void *)(uintptr_t)handle;
    const int affinity_error = fg143_gather_affinity(1, task);
    pthread_mutex_lock(&team->lock);
    if (affinity_error) {
        team->failed = true;
        pthread_cond_signal(&team->job_done);
        pthread_mutex_unlock(&team->lock);
        return;
    }
    uint64_t observed = 0;
    while (!team->stop) {
        if (observed == team->generation) {
            pthread_cond_wait(&team->job_ready, &team->lock);
            continue;
        }
        observed = team->generation;
        const struct i2_prefill_work *work = team->work;
        const int count = team->count, tasks = team->tasks;
        pthread_mutex_unlock(&team->lock);
        fg_i2_prefill_partition((uint64_t)(uintptr_t)work, count * task / tasks, count * (task + 1) / tasks);
        pthread_mutex_lock(&team->lock);
        if (--team->pending == 0) pthread_cond_signal(&team->job_done);
    }
    pthread_mutex_unlock(&team->lock);
}

static bool prefill_team_open(struct i2_prefill_team *team, int tasks) {
    if (pthread_mutex_init(&team->lock, NULL) != 0) return false;
    if (pthread_cond_init(&team->job_ready, NULL) != 0) {
        pthread_mutex_destroy(&team->lock);
        return false;
    }
    if (pthread_cond_init(&team->job_done, NULL) != 0) {
        pthread_cond_destroy(&team->job_ready);
        pthread_mutex_destroy(&team->lock);
        return false;
    }
    team->tasks = tasks;
    assert(i2_prefill_team_start((uint64_t)(uintptr_t)team, tasks, ints(team->handles, tasks - 1)) == tasks - 1);
    return true;
}

static void prefill_team_close(struct i2_prefill_team *team) {
    pthread_mutex_lock(&team->lock);
    team->stop = true;
    pthread_cond_broadcast(&team->job_ready);
    pthread_mutex_unlock(&team->lock);
    assert(i2_prefill_team_join(ints(team->handles, team->tasks - 1)) == 0);
    pthread_cond_destroy(&team->job_done);
    pthread_cond_destroy(&team->job_ready);
    pthread_mutex_destroy(&team->lock);
}

static void prefill_team_run(struct i2_prefill_team *team, const struct i2_prefill_work *work, int count) {
    pthread_mutex_lock(&team->lock);
    assert(!team->failed && team->pending == 0);
    team->work = work;
    team->count = count;
    team->pending = team->tasks - 1;
    ++team->generation;
    pthread_cond_broadcast(&team->job_ready);
    pthread_mutex_unlock(&team->lock);

    fg_i2_prefill_partition((uint64_t)(uintptr_t)work, 0, count / team->tasks);
    pthread_mutex_lock(&team->lock);
    while (team->pending && !team->failed) pthread_cond_wait(&team->job_done, &team->lock);
    assert(!team->failed);
    pthread_mutex_unlock(&team->lock);
}
