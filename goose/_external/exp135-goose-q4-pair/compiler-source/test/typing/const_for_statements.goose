struct Pair<T> { first: T, pad, second: T }
struct Empty {}
struct Record { A: i64, flag: bool, text: const u8[:] }
type Numbers = Pair<i32>;
const SMALL: u8 = 7;
const WORD = "word";

fn visit<T, F>(value: T&) {
    const for <N, A> in fields<T> {
        F(N, &value.A);
    }
}

fn show_type<T>() {
    let v: T = default<T>();
    print(v);
}

fn fields() { print("ordinary fields"); }
fn types() { print("ordinary types"); }

fn main() {
    var r = Record { 3, true, "hi" };
    visit(r) { n, f => print(n, "=", f) }
    const for <N, A> in fields<Record> {
        let A = 0;
        print(r.A + A);
    }
    var pair = Numbers { 4, 5 };
    visit(pair) { n, f => f += 10; print(n, "=", f) }
    print(pair);
    const for <N, A> in fields<const Numbers> {
        let local = pair.A;
        fn nested() { print(N, ":", local); }
        nested();
        { let N = "shadow"; print(N); }
    }
    const for <N, A> in fields<Empty> { nonexistent(N, A); }
    const for <U> in types<> { nonexistent<U>(); }
    const for <C> in [] { nonexistent(C); }
    const for <U> in types<i8, i16, bool, Pair<u8>, i8> {
        show_type<U>();
        fn nested(value: U) { print(value); }
        nested(default<U>());
    }
    const for <C> in [1, 2 + 1, 4] {
        var a: u8[C] = [0; C];
        print(a.len);
        const for <D> in [C, C + 10] { print(D); }
    }
    const for <C> in [SMALL, -2, 'a', 1.5, -2.5, true, WORD] { print(C); }
    const for <C> in [1, 2] {
        fn nested() { print(C); }
        fn shadow(C: i64) { print(C); }
        nested(); shadow(99);
        for C in 0..2 { print(C); }
        let C = 42;
        print(C);
    }
    var count = 0;
    const for <C> in [1, 2, 3] {
        if count < 2 { print(C); }
        count++;
        for i in 0..3 { if i == 1 { continue; } if i == 2 { break; } }
        block { break; }
    }
    fields(); types();
}
