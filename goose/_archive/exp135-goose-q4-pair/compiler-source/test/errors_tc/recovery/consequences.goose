// error: expected a value of type i64, got bool
// error: unknown identifier: nothere
// error: expected a value of type bool, got i64
// What follows from an error reported is not reported again
// (docs/design/language_server.md §5 D2): a global whose initializer
// failed is unassigned for good, and a use of it ends that root silently
// rather than as a use before assignment; a specialization an error cut
// short is never reused, and a call that would reuse it ends its root
// silently; a specialization that completed before the error is reused
// as any other, so late_user's own error is found.
let g: i64 = true;

fn uses_g() -> i64 { g + 1 }

fn broken_helper(x: i64) -> i64 {
    let y = x + nothere;
    y
}

fn fine_helper(x: i64) -> i64 { x + 1 }

fn calls_broken() -> i64 {
    let y = broken_helper(2);
    let z: bool = y;
    y
}

fn late_user() -> i64 {
    let y = fine_helper(2);
    let z: bool = y;
    y
}

fn main() {
    let a = fine_helper(1);
    let b = broken_helper(a);
    let c: bool = a;
    print(a + b);
}
