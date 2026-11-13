// error: function value can fall off the end without returning value(s)
enum Choice { A, B }
fn visit(c: Choice.A) { abort("stopped"); }
fn visit(c: Choice.B) {}
fn value(c: Choice) -> i64 { visit(c); }
fn main() { value(Choice.B); }
