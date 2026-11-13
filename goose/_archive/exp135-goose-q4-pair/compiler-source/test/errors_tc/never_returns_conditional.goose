// error: function value can fall off the end without returning value(s)
fn maybe_stop(ok: bool) { if ok { abort("stopped"); } }
fn value(ok: bool) -> i64 { maybe_stop(ok); }
fn main() { value(false); }
