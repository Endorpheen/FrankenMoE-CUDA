extern "i2_load_route" fn load_route(address: u64, route: i64) -> i64;
extern "i2_iq4_dot" fn iq4_dot(weight: u64, input: u64, width: i64, kind: i64) -> f32;
extern "i2_store_result" fn store_result(address: u64, index: i64, value: f32);

struct ExpertJob {
    weights: u64, input: u64, ids: u64, output: u64, kind: i64, width: i64,
    row_bytes: i64, rows: i64, input_slots: i64, routes: i64, tasks: i64
}

fn execute_range(job: ExpertJob, task: i64) -> bool {
    let first = job.routes * task / job.tasks;
    let last = job.routes * (task + 1) / job.tasks;
    for route in first .. last {
        let expert = load_route(job.ids, route);
        if expert < 0 || expert >= 128 {
            return false;
        }
        let row_input = (route / 8) * job.input_slots + route % job.input_slots;
        let input_address = job.input + ((row_input * job.width * 4) as u64);
        let plane = expert * job.rows;
        for row in job.rows {
            let weight_address = job.weights + (((plane + row) * job.row_bytes) as u64);
            let value = iq4_dot(weight_address, input_address, job.width, job.kind);
            store_result(job.output, route * job.rows + row, value);
        }
    }
    return true;
}

thread_fn expert_worker(job: ExpertJob, task: i64) {
    assert(execute_range(job, task));
}

export fn i2_cpu_indexed(weights: u64, input: u64, ids: u64, output: u64,
                         kind: i64, width: i64, row_bytes: i64, rows: i64,
                         input_slots: i64, count: i64, tasks: i64) -> i64 {
    if tasks < 1 || tasks > 8 || tasks - 1 > hardware_threads() {
        return -1;
    }
    let job = ExpertJob {
        weights, input, ids, output, kind, width, row_bytes, rows, input_slots, 8 * count, tasks
    };
    var workers: i64[7] = [0; 7];
    for task in 1 .. tasks {
        workers[task - 1] = thread_spawn_contained(expert_worker, job, task);
    }
    var ok = execute_range(job, 0);
    for task in 1 .. tasks {
        if !thread_wait(workers[task - 1]) {
            ok = false;
        }
    }
    return if ok { job.routes } else { -1 };
}
