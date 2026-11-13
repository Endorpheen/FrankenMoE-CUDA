// Shared GGML tensor geometry. Imports only project POD fields and type metadata.
extern "fg155_ne" fn ne(t: i64, i: i64) -> i64;
extern "fg155_nb" fn nb(t: i64, i: i64) -> u64;
extern "fg155_tensor_block" fn tensor_block(t: i64) -> i64;
extern "fg155_tensor_bytes" fn bytes(t: i64) -> u64;
extern "fg155_type_block" fn type_block(t: i64) -> i64;
extern "fg155_type_bytes" fn type_bytes(t: i64) -> u64;
extern "fg155_alignment" fn alignment() -> u64;
extern "fg155_fail" fn fail();

fn elements(t: i64) -> i64 {
    return ne(t, 0) * ne(t, 1) * ne(t, 2) * ne(t, 3);
}

fn rows(t: i64) -> i64 {
    return ne(t, 1) * ne(t, 2) * ne(t, 3);
}

fn empty(t: i64) -> bool {
    return ne(t, 0) == 0 || ne(t, 1) == 0 || ne(t, 2) == 0 || ne(t, 3) == 0;
}

fn span(t: i64) -> u64 {
    var i: i64 = 0;

    while i < 4 {
        if ne(t, i) <= 0 {
            return 0 as u64;
        }
        i += 1;
    }
    let b = tensor_block(t);
    var result: u64 = 0 as u64;

    if b == 1 {
        result = bytes(t);
        i = 0;
    } else {
        result = (ne(t, 0) as u64) * nb(t, 0) / (b as u64);
        i = 1;
    }

    while i < 4 {
        result += ( (ne(t, i) - 1) as u64) * nb(t, i);
        i += 1;
    }
    return result;
}

fn contiguous(t: i64, m: i64, n: i64) -> bool {
    var next = bytes(t);
    let b = tensor_block(t);

    if ne(t, 0) != b && nb(t, 0) != next {
        return false;
    }
    next *= (ne(t, 0) / b) as u64;
    var i: i64 = 1;

    while i < n {
        if i > m {
            if ne(t, i) != 1 && nb(t, i) != next {
                return false;
            }
            next *= ne(t, i) as u64;
        } else {
            next = (ne(t, i) as u64) * nb(t, i);
        }
        i += 1;
    }
    return true;
}

fn same_shape(a: i64, b: i64) -> bool {
    return ne(a, 0) == ne(b, 0) && ne(a, 1) == ne(b, 1) && ne(a, 2) == ne(b, 2) && ne(a, 3) == ne(b, 3);
}

fn same_stride(a: i64, b: i64) -> bool {
    return nb(a, 0) == nb(b, 0) && nb(a, 1) == nb(b, 1) && nb(a, 2) == nb(b, 2) && nb(a, 3) == nb(b, 3);
}

fn repeat(a: i64, b: i64) -> bool {
    if empty(a) {
        return empty(b);
    }
    return ne(b, 0) % ne(a, 0) == 0 && ne(b, 1) % ne(a, 1) == 0 && ne(b, 2) % ne(a, 2) == 0 && ne(b, 3) % ne(a, 3) == 0;
}

fn truth(x: bool) -> i64 {
    return if x {
        1
    } else {
        0
    }
    ;
}

fn entry(mode: i64, a: i64, b: i64, c: i64) -> i64 {
    if mode == 0 {
        return elements(a);
    }

    if mode == 1 {
        return rows(a);
    }

    if mode == 2 {
        return span(a) as !i64;
    }

    if mode == 3 {
        let align = alignment();
        return ( (span(a) + align - 1) / align * align) as !i64;
    }

    if mode == 4 {
        let blk = type_block(a);

        if blk <= 0 || b % blk != 0 {
            fail();
            return 0;
        }
        return (type_bytes(a) * (b as u64) / (blk as u64)) as !i64;
    }

    if mode == 5 {
        return truth(ne(a, 0) == 1 && ne(a, 1) == 1 && ne(a, 2) == 1 && ne(a, 3) == 1);
    }

    if mode == 6 {
        return truth(ne(a, 1) == 1 && ne(a, 2) == 1 && ne(a, 3) == 1);
    }

    if mode == 7 {
        return truth(ne(a, 2) == 1 && ne(a, 3) == 1);
    }

    if mode == 8 {
        return truth(ne(a, 3) == 1);
    }

    if mode == 9 {
        var i: i64 = 3;

        while i >= 1 {
            if ne(a, i) > 1 {
                return i + 1;
            }
            i -= 1;
        }
        return 1;
    }

    if mode == 10 {
        return truth(contiguous(a, b, c));
    }

    if mode == 11 {
        return truth(span(a) == ( (elements(a) as u64) * bytes(a) / (tensor_block(a) as u64)));
    }

    if mode == 12 {
        return truth(nb(a, 0) > nb(a, 1));
    }

    if mode == 13 {
        return truth(nb(a, 0) > nb(a, 1) || nb(a, 1) > nb(a, 2) || nb(a, 2) > nb(a, 3));
    }

    if mode == 14 {
        return truth(nb(a, 0) > nb(a, 2) && nb(a, 1) > nb(a, 0) && nb(a, 2) == bytes(a));
    }

    if mode == 15 {
        return truth(ne(a, 0) == tensor_block(a) || nb(a, 0) == bytes(a));
    }

    if mode == 16 {
        return truth(nb(a, 0) == bytes(a) && nb(a, 2) == nb(a, 1) * (ne(a, 1) as u64) && nb(a, 3) == nb(a, 2) * (ne(a, 2) as u64));
    }

    if mode == 17 {
        return truth(empty(a));
    }

    if mode == 18 {
        return truth(same_shape(a, b));
    }

    if mode == 19 {
        return truth(same_stride(a, b));
    }

    if mode == 20 {
        return truth(repeat(a, b));
    }

    if mode == 21 {
        return truth(ne(a, 0) == ne(b, 0) && repeat(a, b));
    }

    if mode == 22 {
        return truth(ne(a, 0) == ne(b, 0) && ne(b, 2) % ne(a, 2) == 0 && ne(b, 3) % ne(a, 3) == 0);
    }

    if mode == 23 {
        return truth(ne(a, 1) == ne(b, 1) && ne(b, 2) % ne(a, 2) == 0 && ne(b, 3) % ne(a, 3) == 0);
    }
    return 0;
}

fn main() {
    print(entry(999, 0, 0, 0));
}

