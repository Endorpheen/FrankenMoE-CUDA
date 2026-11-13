// runtime-debug: place IDs are local to a body; global initializers get a fresh prescan.
var backing: i64[>..] = [11, 22, 33];
var view: i64[:] = backing[..];

recursive fn cut_global() { view = backing[0..1]; }

let first = {
    let old_length = view.len;
    cut_global();
    if old_length > 0 { view[0] } else { 0 } // bce:keep
};

let second = {
    view = backing[..];
    let old_length = view.len;
    cut_global();
    if old_length > 0 { view[0] } else { 0 } // bce:keep
};

recursive fn captured() -> i64 {
    var a: i64[>..] = [7, 8, 9];
    var s = a[..];
    let r = &s;
    fn cut() { r = a[0..1]; }
    let old_length = s.len;
    cut();
    if old_length > 0 { s[0] } else { 0 } // bce:keep
}

recursive fn separate(a: i64[:]) -> i64 {
    var total = 0;
    for i in a.len { total += a[i]; } // bce:elide
    total
}

fn main() {
    print(first);
    print(second);
    print(captured());
    print(separate(backing[..]));
    print(captured());
}
