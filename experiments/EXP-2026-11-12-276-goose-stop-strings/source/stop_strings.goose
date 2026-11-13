// P8-04: stop-sequence matching, in Goose.
//
// Native reference: tools/server/server-context.cpp, server_slot::find_stopping_strings(), and
// common/common.h, string_find_partial_stop().
//
//   full stop: for each anti-prompt, search inside the tail window [len - (|word| + last_token_size), len);
//              the earliest position across words wins, and the word it belongs to is recorded.
//   partial stop: the text ends with a proper prefix of the word; the check walks the prefix length downwards and
//              compares the last byte first, exactly like the native helper.
//
// Byte buffers keep the native result: no Unicode awareness, one byte per comparison.
extern "fg276t_fail" fn fail(c: i64);
extern "fg276t_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg276t_ld_u8" fn ld_u8(base: u64, byte: i64) -> i64;
extern "fg276t_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(a: u64, i: i64) -> i64 {
    return ld_i64(a, i * 8 as i64);
}

fn byte_at(a: u64, i: i64) -> i64 {
    return ld_u8(a, i);
}
// position of the earliest full-stop occurrence, or -1; out_word receives the word index when a stop is found.

fn find_full(
    text: u64,
    len: i64,
    words: u64,
    offs: u64,
    lens: u64,
    n_words: i64,
    last_token_size: i64,
    out_word: u64) -> i64 {
    ensure(len >= 0 as i64 && n_words >= 0 as i64 && last_token_size >= 0 as i64, 10);
    var best: i64 = -1 as i64;
    var best_word: i64 = -1 as i64;
    var w: i64 = 0 as i64;

    while w < n_words {
        let wl: i64 = at(lens, w);
        let tmp: i64 = wl + last_token_size;
        let from: i64 = if len > tmp {
            len - tmp
        } else {
            0 as i64
        }
        ;
        var p: i64 = from;
        var found: i64 = -1 as i64;

        while p + wl <= len && found < 0 as i64 {
            var j: i64 = 0 as i64;
            var same: i64 = 1 as i64;

            while j < wl {
                if byte_at(text, p + j) != byte_at(words, at(offs, w) + j) {
                    same = 0 as i64;
                }
                j = j + 1 as i64;
            }

            if same != 0 as i64 {
                found = p;
            }
            p = p + 1 as i64;
        }

        if found >= 0 as i64 && (best < 0 as i64 || found < best) {
            best = found;
            best_word = w;
        }
        w = w + 1 as i64;
    }

    if best_word >= 0 as i64 {
        st_i64(out_word, 0 as i64, best_word);
    }
    best
}
// partial stop: position where the text ends with a prefix of the word, or -1.

fn find_partial(text: u64, len: i64, word: u64, wlen: i64) -> i64 {
    ensure(len >= 0 as i64 && wlen >= 0 as i64, 11);

    if len == 0 as i64 || wlen == 0 as i64 {
        return -1 as i64;
    }
    let max_len: i64 = if len < wlen {
        len
    } else {
        wlen
    }
    ;
    let last_char: i64 = byte_at(text, len - 1 as i64);
    var k: i64 = max_len;

    while k > 0 as i64 {
        if byte_at(word, k - 1 as i64) == last_char {
            var j: i64 = 0 as i64;
            var same: i64 = 1 as i64;

            while j < k {
                if byte_at(text, len - k + j) != byte_at(word, j) {
                    same = 0 as i64;
                }
                j = j + 1 as i64;
            }

            if same != 0 as i64 {
                return len - k;
            }
        }
        k = k - 1 as i64;
    }
    - 1 as i64
}

fn main() {
    find_full(0 as u64, 0 as i64, 0 as u64, 0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as u64);
    find_partial(0 as u64, 0 as i64, 0 as u64, 0 as i64);
}

