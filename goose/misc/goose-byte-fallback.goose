// Apex byte-fallback decisions in Goose (llm_tokenizer_bpe_session, final pass).
//
// When a merged word has no token, each byte is tried on its own:
//   path     a byte-encoded vocabulary looks the byte up as a one-character string, otherwise the token
//            text uses the <0xXX> form
//   nibble   the two hex digits of a byte, upper case
//   push     a byte that resolves to a token is emitted; one that does not is dropped
//
// Packet:
//   k=0  0 op, 1 a, 2 b, 3 c
//   k=1  0 out
//   k=2  0 done, 1 ret
//
// Records: 0 decide (a = op, b = ret)
extern "fg402k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg402k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg402k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg402k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg402k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    stw(arr, i * (8 as i64), v);
}

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (3 as i64), kind);
    setat(out, n * (3 as i64) + 1 as i64, a);
    setat(out, n * (3 as i64) + 2 as i64, b);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let a: i64 = get(packet, 0, 1) as i64;
    let b: i64 = get(packet, 0, 2) as i64;
    let c: i64 = get(packet, 0, 3) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    var ret: i64 = 0 as i64;

    if op == (0 as i64) {
        // a = byte-encode flag; 0 means the <0xXX> form
        ret = if a != (0 as i64) {
            1 as i64
        } else {
            2 as i64
        }
        ;
    } else {
        if op == (1 as i64) {
            // a = byte value, b = 0 for the high nibble, 1 for the low one
            let nib: i64 = if b == (0 as i64) {
                a / (16 as i64)
            } else {
                a % 16 as i64
            }
            ;
            ret = if nib < (10 as i64) {
                (48 as i64) + nib
            } else {
                (65 as i64) + (nib - (10 as i64))
            }
            ;
        } else {
            if op == (2 as i64) {
                // a = token resolved (0 when LLAMA_TOKEN_NULL)
                ret = if a != (0 as i64) {
                    1 as i64
                } else {
                    0 as i64
                }
                ;
            } else {
                if op == (3 as i64) {
                    // a = the word resolved as a whole, which decides push versus fallback
                    ret = if a != (0 as i64) {
                        0 as i64
                    } else {
                        1 as i64
                    }
                    ;
                } else {
                    ensure(false, 60);
                }
            }
        }
    }
    emit(out, cntp, 0 as i64, op, ret);
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

