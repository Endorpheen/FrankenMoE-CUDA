fn probe(a:u8[:], b:const u8[:], n:i64)->i64 {
    var s:i64=0;
    for i in n { s += (a[i] as i64) + (b[i] as i64); }
    return s;
}
fn main() { let a:u8[1]=default<u8[1]>(); let b:u8[1]=default<u8[1]>(); print(probe(a,b,1)); }
