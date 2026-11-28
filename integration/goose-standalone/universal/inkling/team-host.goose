extern fn i2_team_mutex_init(lock: u64, attributes: u64) -> i32;
extern fn i2_team_mutex_destroy(lock: u64) -> i32;
extern fn i2_team_mutex_lock(lock: u64) -> i32;
extern fn i2_team_mutex_unlock(lock: u64) -> i32;
extern fn i2_team_cond_init(condition: u64, attributes: u64) -> i32;
extern fn i2_team_cond_destroy(condition: u64) -> i32;
extern fn i2_team_cond_wait(condition: u64, lock: u64) -> i32;
extern fn i2_team_cond_signal(condition: u64) -> i32;
extern fn i2_team_cond_broadcast(condition: u64) -> i32;
extern fn i2_prefill_team_start(team: u64, tasks: i64, handles: I2DraftSpan) -> i64;
extern fn i2_prefill_team_join(handles: I2DraftSpan) -> i64;
extern fn fg143_gather_affinity(prefill: bool, task: i64) -> i64;

export fn fg_i2_prefill_team_worker(team: u64, task: i64) {
    let affinity_error = fg143_gather_affinity(i2d_byte(team + I2_TEAM_PREFILL) != 0, task);
    i2_team_mutex_lock(team);
    if affinity_error != 0 {
        i2d_set_byte(team + I2_TEAM_FAILED, 1);
        i2_team_cond_signal(team + I2_TEAM_JOB_DONE);
        i2_team_mutex_unlock(team);
        return;
    }
    var observed: u64 = 0;
    while i2d_byte(team + I2_TEAM_STOP) == 0 {
        let generation = i2d_read(team + I2_TEAM_GENERATION);
        if observed == generation {
            i2_team_cond_wait(team + I2_TEAM_JOB_READY, team);
            continue;
        }
        observed = generation;
        let work = i2d_read(team + I2_TEAM_WORK);
        let count = i2d_int(team + I2_TEAM_COUNT) as! i64;
        let tasks = i2d_int(team + I2_TEAM_TASKS) as! i64;
        i2_team_mutex_unlock(team);
        fg_i2_prefill_partition(work, count * task / tasks, count * (task + 1) / tasks);
        i2_team_mutex_lock(team);
        let pending = i2d_int(team + I2_TEAM_PENDING) - 1;
        i2d_set_int(team + I2_TEAM_PENDING, pending);
        if pending == 0 { i2_team_cond_signal(team + I2_TEAM_JOB_DONE); }
    }
    i2_team_mutex_unlock(team);
}

export fn prefill_team_open(team: u64, tasks: i32, prefill: bool) -> bool {
    if i2_team_mutex_init(team, 0) != 0 { return false; }
    let ready = team + I2_TEAM_JOB_READY;
    let done = team + I2_TEAM_JOB_DONE;
    if i2_team_cond_init(ready, 0) != 0 {
        i2_team_mutex_destroy(team);
        return false;
    }
    if i2_team_cond_init(done, 0) != 0 {
        i2_team_cond_destroy(ready);
        i2_team_mutex_destroy(team);
        return false;
    }
    i2d_set_int(team + I2_TEAM_TASKS, tasks);
    i2d_set_byte(team + I2_TEAM_PREFILL, if prefill { 1 } else { 0 });
    let count = tasks as! i64;
    assert(i2_prefill_team_start(team, count, i2d_span(team + I2_TEAM_HANDLES, count - 1)) == count - 1);
    return true;
}

export fn prefill_team_close(team: u64) {
    i2_team_mutex_lock(team);
    i2d_set_byte(team + I2_TEAM_STOP, 1);
    i2_team_cond_broadcast(team + I2_TEAM_JOB_READY);
    i2_team_mutex_unlock(team);
    assert(i2_prefill_team_join(i2d_span(team + I2_TEAM_HANDLES, (i2d_int(team + I2_TEAM_TASKS) as! i64) - 1)) == 0);
    i2_team_cond_destroy(team + I2_TEAM_JOB_DONE);
    i2_team_cond_destroy(team + I2_TEAM_JOB_READY);
    i2_team_mutex_destroy(team);
}

export fn prefill_team_run(team: u64, work: u64, count: i32) {
    i2_team_mutex_lock(team);
    assert(i2d_byte(team + I2_TEAM_FAILED) == 0 && i2d_int(team + I2_TEAM_PENDING) == 0);
    let tasks = i2d_int(team + I2_TEAM_TASKS);
    i2d_set(team + I2_TEAM_WORK, work);
    i2d_set_int(team + I2_TEAM_COUNT, count);
    i2d_set_int(team + I2_TEAM_PENDING, tasks - 1);
    i2d_set(team + I2_TEAM_GENERATION, i2d_read(team + I2_TEAM_GENERATION) + 1);
    i2_team_cond_broadcast(team + I2_TEAM_JOB_READY);
    i2_team_mutex_unlock(team);

    fg_i2_prefill_partition(work, 0, (count / tasks) as! i64);
    i2_team_mutex_lock(team);
    while i2d_int(team + I2_TEAM_PENDING) != 0 && i2d_byte(team + I2_TEAM_FAILED) == 0 {
        i2_team_cond_wait(team + I2_TEAM_JOB_DONE, team);
    }
    assert(i2d_byte(team + I2_TEAM_FAILED) == 0);
    i2_team_mutex_unlock(team);
}

extern fn i2_parallel_prefill(work: u64, rows: i64, tasks: i64) -> i64;
extern fn fg_standalone_cpu_decode_threads() -> i64;

var i2_parallel_logits: i32 = -1;

fn fg_i2_prefill_team_start(team: u64, count: i32) -> bool {
    let tasks = fg_i2_prefill_task_count(count);
    if tasks <= 1 { return false; }
    fg_i2_host_cached_history();
    return prefill_team_open(team, tasks, true);
}

fn fg_i2_prefill_prepare(team: u64, work: u64, count: i32) {
    if team != 0 {
        prefill_team_run(team, work, count);
        return;
    }
    let rows = count as! i64;
    let tasks = fg_i2_prefill_task_count(count);
    if tasks > 1 {
        fg_i2_host_cached_history();
        assert(i2_parallel_prefill(work, rows, tasks as! i64) == rows);
    } else {
        fg_i2_prefill_partition(work, 0, rows);
    }
}

fn fg_i2_parallel_logits(decoder: u64, logits: u64) -> bool {
    if i2_parallel_logits < 0 { i2_parallel_logits = if i2d_env_is("I2_PARALLEL_LOGITS", 49) { 1 } else { 0 }; }
    if i2_parallel_logits == 0 { return false; }
    let team_slot = decoder + 887272;
    var team = i2d_read(team_slot);
    if team == 0 {
        let configured = fg_standalone_cpu_decode_threads();
        let tasks = if configured > 4 { 4 } else { configured };
        if tasks < 2 { return false; }
        let allocation = i2_prefill_calloc(1, I2_TEAM_BYTES);
        i2d_copy(unsafe_c_address<c_ptr<void>>(team), unsafe_c_address<c_ptr<const void>>(allocation), 8);
        if team == 0 { return false; }
        if !prefill_team_open(team, tasks as! i32, false) {
            i2d_free(allocation);
            return false;
        }
        i2d_set(team_slot, team);
    }
    var work: u64[14] = [0; 14];
    work[12] = logits;
    prefill_team_run(team, i2d_address(unsafe_c_address<c_ptr<const void>>(work[0])), 200058);
    return true;
}
