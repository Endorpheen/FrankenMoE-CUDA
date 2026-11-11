// EXP256 carries host-side arithmetic only; the device unit is a no-op.
extern "fg256d_fail" fn fail(c:i64);
fn main(){ fail(1 as i64); }
