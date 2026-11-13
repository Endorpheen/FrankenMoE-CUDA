// error: extern fn cannot be checkfree
// An extern has no body to check. (docs/design/index_types.md)
checkfree extern fn f() -> i64;
fn main() {}
