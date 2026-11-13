// error: unknown function: no_such_function
recursive fn visit(n: i64) {
    if n == 0 { return; }
    visit(n - 1);
    no_such_function();
}
fn main() { visit(3); }
