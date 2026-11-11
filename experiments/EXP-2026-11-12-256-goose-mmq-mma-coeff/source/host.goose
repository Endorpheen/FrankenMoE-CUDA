// EXP256: device-only module; host unit is a placeholder.
extern "fg256h_fail" fn fail(c:i64);
fn run(packet:i64)->bool { fail(1 as i64); true }
fn main(){run(0);}
