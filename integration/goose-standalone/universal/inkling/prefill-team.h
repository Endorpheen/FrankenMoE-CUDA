struct i2_prefill_team {
    pthread_mutex_t lock;
    pthread_cond_t job_ready, job_done;
    const struct i2_prefill_work *work;
    int64_t handles[15];
    int tasks, count, pending;
    uint64_t generation;
    bool stop, failed, prefill;
};

#ifdef _WIN32
_Static_assert(sizeof(struct i2_prefill_team) == 184 &&
    offsetof(struct i2_prefill_team, job_ready) == 8 &&
    offsetof(struct i2_prefill_team, job_done) == 16 &&
    offsetof(struct i2_prefill_team, work) == 24 &&
    offsetof(struct i2_prefill_team, handles) == 32 &&
    offsetof(struct i2_prefill_team, tasks) == 152 &&
    offsetof(struct i2_prefill_team, count) == 156 &&
    offsetof(struct i2_prefill_team, pending) == 160 &&
    offsetof(struct i2_prefill_team, generation) == 168 &&
    offsetof(struct i2_prefill_team, stop) == 176 &&
    offsetof(struct i2_prefill_team, failed) == 177 &&
    offsetof(struct i2_prefill_team, prefill) == 178, "Goose team ABI");
#else
_Static_assert(sizeof(struct i2_prefill_team) == 296 &&
    offsetof(struct i2_prefill_team, job_ready) == 40 &&
    offsetof(struct i2_prefill_team, job_done) == 88 &&
    offsetof(struct i2_prefill_team, work) == 136 &&
    offsetof(struct i2_prefill_team, handles) == 144 &&
    offsetof(struct i2_prefill_team, tasks) == 264 &&
    offsetof(struct i2_prefill_team, count) == 268 &&
    offsetof(struct i2_prefill_team, pending) == 272 &&
    offsetof(struct i2_prefill_team, generation) == 280 &&
    offsetof(struct i2_prefill_team, stop) == 288 &&
    offsetof(struct i2_prefill_team, failed) == 289 &&
    offsetof(struct i2_prefill_team, prefill) == 290, "Goose team ABI");
#endif

_Static_assert(offsetof(struct i2_prefill_team, lock) == 0, "Goose team mutex ABI");
