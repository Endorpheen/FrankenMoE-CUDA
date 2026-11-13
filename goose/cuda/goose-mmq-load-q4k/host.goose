// Placeholder host unit for EXP255: this experiment carries device code only.
extern "fg255qh_fail" fn fail(c: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    ensure(packet == 0 as i64, 1);
    true
}

fn main() {
    run(0);
}

