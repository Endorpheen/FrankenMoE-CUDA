// error: const for source C names a local variable, not a constant
const C = 7;
fn main() {
    block {
        let C = 8;
        match C {
            const for <N> in [C] { N => if true { print(N); }, },
            _ => {},
        }
    }
}
