const PROB_PLATFORM_COND: u64 = 64;
const PROB_PLATFORM_THREAD: u64 = 128;
const PROB_PLATFORM_SIZE: u64 = 248;

extern fn pthread_key_create(key: c_ptr<u32>, destructor: c_ptr<void>) -> i32;
extern fn pthread_getspecific(key: u32) -> c_ptr<void>;
extern fn pthread_setspecific(key: u32, value: c_ptr<const void>) -> i32;
extern fn pthread_mutex_init(mutex: c_ptr<void>, attr: c_ptr<const void>) -> i32;
extern fn pthread_mutex_destroy(mutex: c_ptr<void>) -> i32;
extern fn pthread_mutex_lock(mutex: c_ptr<void>) -> i32;
extern fn pthread_mutex_unlock(mutex: c_ptr<void>) -> i32;
extern fn pthread_cond_init(condition: c_ptr<void>, attr: c_ptr<const void>) -> i32;
extern fn pthread_cond_destroy(condition: c_ptr<void>) -> i32;
extern fn pthread_cond_wait(condition: c_ptr<void>, mutex: c_ptr<void>) -> i32;
extern fn pthread_cond_broadcast(condition: c_ptr<void>) -> i32;
extern fn pthread_join(thread: u64, result: c_ptr<void>) -> i32;
extern "memset" fn prob_platform_fill(dst: c_ptr<void>, value: i32, size: u64) -> c_ptr<void>;
extern fn fg_prob_platform_launch(worker: u64, address: u64) -> i32;
extern fn fg_prob_native_avx2() -> bool;
extern fn fg_cuda_inkling_quants_enabled() -> bool;

var prob_request_key: u32;
var prob_request_ready: bool;

export fn fg_prob_platform_init() {
    prob_request_ready = pthread_key_create(unsafe_c_address<c_ptr<u32>>(prob_request_key), null) == 0;
}

fn prob_platform_pointer(address: u64) -> c_ptr<void> {
    return unsafe_c_ptr<c_ptr<void>>(address);
}

fn fg_prob_platform_create() -> u64 {
    if !prob_request_ready { return 0; }
    let address = prob_malloc(PROB_PLATFORM_SIZE);
    if address == 0 { return 0; }
    let mutex = prob_platform_pointer(address);
    let condition = prob_platform_pointer(address + PROB_PLATFORM_COND);
    prob_platform_fill(mutex, 0, PROB_PLATFORM_SIZE);
    if pthread_mutex_init(mutex, null) != 0 {
        prob_free(address);
        return 0;
    }
    if pthread_cond_init(condition, null) != 0 {
        pthread_mutex_destroy(mutex);
        prob_free(address);
        return 0;
    }
    return address;
}

fn fg_prob_platform_current(detach: bool) -> u64 {
    if !prob_request_ready { return 0; }
    let pointer = pthread_getspecific(prob_request_key);
    var address: u64 = 0;
    prob_copy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    if detach && address != 0 { pthread_setspecific(prob_request_key,null); }
    return address;
}

fn fg_prob_platform_start(address: u64) -> bool {
    if pthread_setspecific(prob_request_key, unsafe_c_ptr<c_ptr<const void>>(address)) != 0 { return false; }
    if fg_prob_platform_launch(address + PROB_PLATFORM_THREAD, address) == 0 { return true; }
    pthread_setspecific(prob_request_key, null);
    return false;
}

fn fg_prob_platform_event(address: u64, event: i32) {
    let mutex = prob_platform_pointer(address);
    let condition = prob_platform_pointer(address + PROB_PLATFORM_COND);
    if event == PROB_LOCK { pthread_mutex_lock(mutex); }
    else if event == PROB_UNLOCK { pthread_mutex_unlock(mutex); }
    else if event == PROB_WAIT { pthread_cond_wait(condition, mutex); }
    else if event == PROB_NOTIFY { pthread_cond_broadcast(condition); }
    else if event == PROB_JOIN {
        var worker: u64 = 0;
        prob_copy(unsafe_c_address<c_ptr<void>>(worker),
            unsafe_c_ptr<c_ptr<const void>>(address + PROB_PLATFORM_THREAD),8);
        pthread_join(worker, null);
    } else if event == PROB_DESTROY {
        pthread_cond_destroy(condition);
        pthread_mutex_destroy(mutex);
        prob_free(address);
    }
}

export fn fg_prob_pipeline_start(vocab: i64, top: i64, limit: i64) -> bool {
    return fg_prob_start(vocab, top, limit);
}

export fn fg_prob_pipeline_submit(logits: const f32[:], selected: i64) -> bool {
    return fg_prob_submit(logits, selected);
}

export fn fg_prob_pipeline_finish(count: i64, ids: i64[:], logp: f32[:]) -> bool {
    return fg_prob_finish(count, ids, logp);
}

fn prob_env_is(name: const u8[:], value: u8) -> bool {
    var key: u8[32];
    for i in name.len { key[i] = name[i]; }
    key[name.len] = 0;
    let expected: u8[2] = [value, 0];
    return fg_cuda_host_env_equal(fg_cuda_host_getenv(prob_address(unsafe_c_address<c_ptr<const void>>(key[0]))),
        prob_address(unsafe_c_address<c_ptr<const void>>(expected[0])));
}

export fn fg_q35_topk_scan_enabled() -> bool {
    return !prob_env_is("GOOSE_CPU_TOPK_SCAN", 48) && fg_prob_native_avx2();
}

export fn fg_i2_sampler_scan_enabled(count: i64) -> bool {
    return count >= 1024 && prob_env_is("I2_SAMPLER_SCAN", 49) && fg_cuda_inkling_quants_enabled();
}
