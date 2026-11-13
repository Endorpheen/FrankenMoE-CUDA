// runtime-debug: lexical returns and loop rechecks keep each expansion's bindings.
struct Point { x: i64, y: i64 }
struct Dynamic { count: varint, data: u8[], tail: i64[] }
struct Growing { tag: i64, tail: i64[>..] }
struct Link { me: Link&<u32> }

fn visit<T, F>(v: T&) {
    const for <N, A> in fields<T> { F(N, &v.A); }
}
fn first(p: Point&) -> i64 {
    visit(p) { n, v => if n == "x" { return v; } }
    return -1;
}
fn direct() -> i64 {
    const for <C> in [5, 6] { return C; }
    return 0;
}
fn nonlocal() -> i64 {
    const for <C> in [7, 8] {
        fn leave() { return C from nonlocal; }
        leave();
    }
    return 0;
}
fn invoke<F>() { F(); }
fn own_callback() -> i64 {
    invoke() {
        const for <C> in [9, 10] { return C; }
    }
    return 0;
}
fn forwarded<T, F>(v: T&) { visit(v) { n, f => F(n, f) } }
fn main() {
    var p = Point { 2, 3 };
    print(first(p), " ", direct(), " ", nonlocal(), " ", own_callback());
    var i = 0;
    while i < 2 {
        const for <N, A> in fields<Point> {
            p.A += 1;
            fn read() { print(N, ":", p.A); }
            read();
        }
        i++;
    }
    var d = Dynamic { 300, "abc", [4, 5] };
    forwarded(d) { n, v => print(n, "=", v) }
    var growing = Growing { 1, [2, 3] };
    forwarded(growing) { n, v => print(n, "=", v) }
    // Type lists use the full type grammar and retain qualifiers.
    const for <U> in types<::Point, i64[2], const u8[:]> {
        let value: U = default<U>();
        print(value);
    }
    const for <U> in types<i64&, const i64&> {
        let r: U = p.x;
        print(r);
    }
    // The same spelling is a literal field again after a local shadows A.
    const for <N, A> in fields<Point> {
        let A = 1;
        struct_is_unneeded(A);
    }
    let link = Link { const for <N, A> in fields<Link> { A: self, } };
    print(link.me .== link);
}
fn struct_is_unneeded(x: i64) { print(x); }
