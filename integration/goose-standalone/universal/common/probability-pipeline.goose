
const PROB_WORKSPACE: i64 = 0;
const PROB_ORDER: i64 = 1;
const PROB_IDS: i64 = 2;
const PROB_LOGP: i64 = 3;
const PROB_SELECTED: i64 = 4;
const PROB_VOCAB: i64 = 7;
const PROB_TOP: i64 = 8;
const PROB_LIMIT: i64 = 9;
const PROB_WRITTEN: i64 = 10;
const PROB_CONSUMED: i64 = 11;
const PROB_CLOSED: i64 = 12;
const PROB_FAILED: i64 = 13;
const PROB_SLOTS: u64 = 3;

const PROB_LOCK: i32 = 0;
const PROB_UNLOCK: i32 = 1;
const PROB_WAIT: i32 = 2;
const PROB_NOTIFY: i32 = 3;
const PROB_JOIN: i32 = 4;
const PROB_DESTROY: i32 = 5;

extern fn fg_prob_platform_payload(address: u64) -> u64[:];
extern "fg_cuda_host_malloc" fn prob_malloc(bytes: u64) -> u64;
extern "fg_cuda_host_free" fn prob_free(address: u64);
extern "fg_cuda_host_view_i64" fn prob_i64(address: u64, length: i64) -> i64[:];
extern "fg_cuda_host_view_f32" fn prob_f32(address: u64, length: i64) -> f32[:];
extern fn fg_cuda_host_getenv(name: u64) -> u64;
extern fn fg_cuda_host_env_equal(value: u64, expected: u64) -> bool;
extern "memcpy" fn prob_copy(destination: c_ptr<void>, source: c_ptr<const void>, bytes: u64) -> c_ptr<void>;

fn prob_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    prob_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn prob_enabled() -> bool { return prob_env_is("GOOSE_CPU_PROBABILITY_PIPELINE", 49); }

fn prob_destroy(address: u64, data: const u64[:]) {
    for i in 4 { prob_free(data[i]); }
    fg_prob_platform_event(address, PROB_DESTROY);
}

export fn fg_prob_worker_run(address: u64) {
    var data = fg_prob_platform_payload(address);
    let vocab = data[PROB_VOCAB];
    let width = data[PROB_TOP] + 1;
    fg_prob_platform_event(address, PROB_LOCK);
    loop {
        while data[PROB_CONSUMED] == data[PROB_WRITTEN] && data[PROB_CLOSED] == 0 {
            fg_prob_platform_event(address, PROB_WAIT);
        }
        let row = data[PROB_CONSUMED];
        if row == data[PROB_WRITTEN] || data[PROB_FAILED] != 0 { break; }
        let slot = (row % PROB_SLOTS) as! i64;
        let selected = data[PROB_SELECTED + slot] as! i64;
        let logits = data[PROB_WORKSPACE] + (slot as! u64) * vocab * 4;
        fg_prob_platform_event(address, PROB_UNLOCK);
        let ok = fg_probability_worker(prob_f32(logits, vocab as! i64),
            prob_i64(data[PROB_ORDER], vocab as! i64), selected, data[PROB_TOP] as! i64,
            prob_i64(data[PROB_IDS] + row * width * 8, width as! i64),
            prob_f32(data[PROB_LOGP] + row * width * 4, width as! i64));
        fg_prob_platform_event(address, PROB_LOCK);
        data[PROB_CONSUMED] += 1;
        if !ok { data[PROB_FAILED] = 1; }
        fg_prob_platform_event(address, PROB_NOTIFY);
    }
    fg_prob_platform_event(address, PROB_UNLOCK);
}

export fn fg_prob_start(vocab: i64, top: i64, limit: i64) -> bool {
    if !prob_enabled() || fg_prob_platform_current(false) != 0 ||
        vocab <= 0 || top <= 0 || top > vocab || limit <= 0 { return false; }
    let v = vocab as! u64;
    let width = (top as! u64) + 1;
    let rows = limit as! u64;
    let maximum: u64 = 0xffffffffffffffff;
    let signed_max: u64 = 0x7fffffffffffffff;
    if v > maximum / 12 || width > signed_max || rows > signed_max / width ||
        rows > maximum / 8 / width { return false; }
    let address = fg_prob_platform_create();
    if address == 0 { return false; }
    var data = fg_prob_platform_payload(address);
    data[PROB_VOCAB] = v;
    data[PROB_TOP] = top as! u64;
    data[PROB_LIMIT] = rows;
    data[PROB_WORKSPACE] = prob_malloc(PROB_SLOTS * v * 4);
    data[PROB_ORDER] = prob_malloc(v * 8);
    data[PROB_IDS] = prob_malloc(rows * width * 8);
    data[PROB_LOGP] = prob_malloc(rows * width * 4);
    if data[PROB_WORKSPACE] == 0 || data[PROB_ORDER] == 0 || data[PROB_IDS] == 0 ||
        data[PROB_LOGP] == 0 || !fg_prob_platform_start(address) {
        prob_destroy(address, data);
        return false;
    }
    return true;
}

export fn fg_prob_submit(logits: const f32[:], selected: i64) -> bool {
    let address = fg_prob_platform_current(false);
    if address == 0 { return false; }
    var data = fg_prob_platform_payload(address);
    let vocab = data[PROB_VOCAB] as! i64;
    let source = unsafe_c_slice_data<c_ptr<const void>>(logits);
    if source == null || logits.len != vocab || selected < 0 || selected >= vocab { return false; }
    fg_prob_platform_event(address, PROB_LOCK);
    while data[PROB_WRITTEN] - data[PROB_CONSUMED] == PROB_SLOTS && data[PROB_FAILED] == 0 {
        fg_prob_platform_event(address, PROB_WAIT);
    }
    if data[PROB_FAILED] != 0 || data[PROB_WRITTEN] >= data[PROB_LIMIT] {
        fg_prob_platform_event(address, PROB_UNLOCK);
        return false;
    }
    let slot = data[PROB_WRITTEN] % PROB_SLOTS;
    let bytes = (vocab as! u64) * 4;
    let destination = data[PROB_WORKSPACE] + slot * bytes;
    prob_copy(unsafe_c_ptr<c_ptr<void>>(destination), source, bytes);
    data[PROB_SELECTED + (slot as! i64)] = selected as! u64;
    data[PROB_WRITTEN] += 1;
    fg_prob_platform_event(address, PROB_NOTIFY);
    fg_prob_platform_event(address, PROB_UNLOCK);
    return true;
}

export fn fg_prob_finish(count: i64, ids: i64[:], logp: f32[:]) -> bool {
    let address = fg_prob_platform_current(true);
    if address == 0 { return false; }
    var data = fg_prob_platform_payload(address);
    fg_prob_platform_event(address, PROB_LOCK);
    data[PROB_CLOSED] = 1;
    fg_prob_platform_event(address, PROB_NOTIFY);
    fg_prob_platform_event(address, PROB_UNLOCK);
    fg_prob_platform_event(address, PROB_JOIN);
    let rows = count as! u64;
    let fields = if count > 0 && rows <= data[PROB_LIMIT] { rows * (data[PROB_TOP] + 1) } else { 0 };
    let out_ids = unsafe_c_slice_data<c_ptr<void>>(ids);
    let out_logp = unsafe_c_slice_data<c_ptr<void>>(logp);
    let ok = data[PROB_FAILED] == 0 && rows == data[PROB_CONSUMED] && fields > 0 &&
        out_ids != null && out_logp != null && ids.len >= (fields as! i64) && logp.len >= (fields as! i64);
    if ok {
        prob_copy(out_ids, unsafe_c_ptr<c_ptr<const void>>(data[PROB_IDS]), fields * 8);
        prob_copy(out_logp, unsafe_c_ptr<c_ptr<const void>>(data[PROB_LOGP]), fields * 4);
    }
    prob_destroy(address, data);
    return ok;
}
