// Apex sampler selection in Goose (llama_sampler_dist_apply, the single-pass branch this line uses).
//
// The draw stays native -- it is the RNG primitive. Everything else moves:
//   1. the maximum (from the first element when sorted, otherwise a scan)
//   2. p[i] = exp(logit[i] - max), accumulated into sum_cum as double
//   3. target = sum_cum * rnd, then the first index whose running sum of float p reaches the target
//   4. p[i] /= sum_cum for every element, in the same single pass
//
// p[] is float storage and sum_cum/sum_run are double, exactly as the native code keeps them.
//
// Packet:
//   k=0  0 n, 1 sorted, 2 logits (f32), 3 out p (f32), 4 rnd (f64 in memory)
//   k=1  0 out (kind, selected, sum_cum bits)
//   k=2  0 done, 1 ret
//
// Records: 0 pick (a = n, b = selected)
extern "fg386k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg386k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg386k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg386k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg386k_fail" fn fail(code: i64);
extern "fg386k_ldf" fn ldf(base: u64, byte: i64) -> f32;
extern "fg386k_stf" fn stf(base: u64, byte: i64, v: f32);
extern "fg386k_ldf64" fn ldf64(base: u64, byte: i64) -> f64;
extern "gs_expf" fn expf32(x: f32) -> f32;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    stw(arr, i * (8 as i64), v);
}

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (3 as i64), kind);
    setat(out, n * (3 as i64) + 1 as i64, a);
    setat(out, n * (3 as i64) + 2 as i64, b);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn run(packet: i64) -> bool {
    let n: i64 = get(packet, 0, 0) as i64;
    let sorted: i64 = get(packet, 0, 1) as i64;
    let logits: u64 = get(packet, 0, 2);
    let outp: u64 = get(packet, 0, 3);
    let rndp: u64 = get(packet, 0, 4);
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    ensure(n > (0 as i64), 10);
    let rnd: f64 = ldf64(rndp, 0 as i64);
    // maximum
    var max_l: f32 = ldf(logits, 0 as i64);

    if sorted == (0 as i64) {
        var i: i64 = 1 as i64;

        while i < n {
            let v: f32 = ldf(logits, i * (4 as i64));

            if v > max_l {
                max_l = v;
            }
            i += 1 as i64;
        }
    }
    // probabilities and their double total
    var sum_cum: f64 = 0.0 as f64;
    var i: i64 = 0 as i64;

    while i < n {
        let p: f32 = expf32(ldf(logits, i * (4 as i64)) - max_l);
        stf(outp, i * (4 as i64), p);
        sum_cum = sum_cum + ( (ldf(outp, i * (4 as i64)) as f64));
        i += 1 as i64;
    }
    // the target the running sum must reach, then selection and normalisation in one pass
    let tgt: f64 = sum_cum * rnd;
    var run_sum: f64 = 0.0 as f64;
    var sel: i64 = (0 as i64) - (1 as i64);
    i = 0 as i64;

    while i < n {
        if sel < (0 as i64) {
            run_sum = run_sum + ( (ldf(outp, i * (4 as i64)) as f64));

            if run_sum >= tgt {
                sel = i;
            }
        }
        stf(outp, i * (4 as i64), ( (ldf(outp, i * (4 as i64)) as f64) / sum_cum) as f32);
        i += 1 as i64;
    }
    var pick: i64 = sel;

    if pick < (0 as i64) {
        pick = n - (1 as i64);
    }
    emit(out, cntp, 0 as i64, n, pick);
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, pick as u64);
    return true;
}

fn main() {
    run(0);
}

