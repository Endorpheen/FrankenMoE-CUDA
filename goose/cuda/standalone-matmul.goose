// One worker group computes one output element. The existing Q4_K/Q6_K
// decoders populate the shared row block and all lanes contribute to the dot.
extern "fg_device_decode_q4" fn decode_q4(weights: u64, scratch: u64, lane: i32);
extern "fg_device_decode_q6" fn decode_q6(weights: u64, scratch: u64, lane: i64);
extern "fg_device_global_load" fn global_load(address: u64) -> f32;
extern "fg_device_global_store" fn global_store(address: u64, value: f32);
extern "fg_device_shared_load" fn shared_load(address: u64) -> f32;
extern "fg_device_shared_store" fn shared_store(address: u64, value: f32);
extern "fg_device_barrier" fn barrier();
extern "fg_device_fma" fn fma(a: f32, b: f32, sum: f32) -> f32;

export fn fg_device_matmul(weights: u64, input: u64, output: u64, scratch: u64,
                            block_count: i64, kind: i64, lane: i64, lanes: i64) {
    let block_bytes: i64 = if kind == 12 { 144 } else { 210 };
    var sum: f32 = 0.0;
    for block_index in block_count {
        let at = weights + ((block_index * block_bytes) as u64);
        if kind == 12 { decode_q4(at, scratch, lane as i32); }
        else { decode_q6(at, scratch, lane); }
        barrier();
        for i in 256 / lanes {
            let column = i * lanes + lane;
            let weight = shared_load(scratch + ((column * 4) as u64));
            let activation = global_load(input + (((block_index * 256 + column) * 4) as u64));
            sum = fma(weight, activation, sum);
        }
        barrier();
    }
    let slot = scratch + ((lane * 4) as u64);
    shared_store(slot, sum);
    barrier();
    var step = lanes / 2;
    while step > 0 {
        if lane < step {
            shared_store(slot, shared_load(slot) + shared_load(slot + ((step * 4) as u64)));
        }
        barrier();
        step /= 2;
    }
    if lane == 0 { global_store(output, shared_load(scratch)); }
}
