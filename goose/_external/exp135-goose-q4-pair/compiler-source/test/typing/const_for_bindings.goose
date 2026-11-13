// dump-runtime: definition-site binding identities survive dump/reparse and specialization.
struct U { x: i64 }
struct Box<T> { value: T }
struct Arrays { a: i64[], b: i64[] }
struct Optional { p: i64? }
const ONE = 1;
const FLOAT: f32 = 1;
const FLAG = true;
const BYTE: u8 = 255;
const WORD = "word";
var calls = 0;

fn narrow(n: u8) { assert(n == 1); }
fn single(n: f32) { assert(n == 1.0); }
fn arrays<T, F>() -> T {
    T { const for <N, A> in fields<T> { A: F(N), } }
}
fn make_point() -> U { calls++; U { 12 } }
fn main() {
    const for <C> in [ONE] { narrow(C); }
    const for <C> in [FLOAT] { single(C); }
    const for <C> in [FLAG] { assert(C); }
    const for <C> in [BYTE + 1] { assert(C == 0); }
    const for <C> in [WORD] { assert(C == "word"); }
    const for <C> in [2] {
        // A source sees the outer C; the new C binds only the fragment.
        const for <C> in [C + 1] {
            let a: i64[C] = [1; C];
            assert(a.len == 3);
            fn length(a: i64[C]) { assert(a.len == 3); }
            length(a);
        }
        const for <T> in types<i64[C]> {
            let a: T = default<T>();
            assert(a.len == 2);
        }
    }
    const for <U> in types<i32, bool> {
        const for <Unused> in [] { print(U); let x: Unused = U; }
        fn unused() { print(U); }
        let value: U = default<U>();
        fn inner<U>(x: U) -> U { x }
        assert(inner(5) == 5);
        const for <U> in types<U> { let other: U = default<U>(); assert(value == other); }
        let U = 10;
        let still_a_type: U = default<U>();
        assert(U == 10 && still_a_type == value);
    }
    const for <C> in [1] {
        recursive fn C(n: i64) -> i64 { if n == 0 { return 1; } C(n - 1) + 1 }
        assert(C(2) == 3);
    }
    let box = Box { const for <C> in [17] { value: C, } };
    assert(box.value == 17);
    let arrs = arrays<Arrays>() { n => calls++; [calls; 2] };
    assert(arrs.a[0] == 1 && arrs.b[0] == 2);
    let optional = Optional { const for <C> in [] { p: C, } };
    assert(optional.p == null);
    // Expressions in the fragment execute separately, not in the source.
    const for <C> in [1, 2] { assert(make_point().x == 12); }
    assert(calls == 4);
    print("bindings ok");
}
