// Console line editing, ported from common/console.cpp.
//
// Owned decisions: the whole readline_advanced editing loop with no terminal
// attached. A key script is a byte string; the port decodes it exactly like
// getchar32 does under a UTF-8 locale, edits a line buffer, and appends the
// bytes that the native code would write to its output stream. The port also
// owns the history state machine (add, view, prev, next, backup line).
//
// Rendering is kept as data: move_cursor writes one '\b' per left column and
// one "\033[C" per right column, replace_last writes "\b" plus the character,
// pop_cursor writes "\b". The gate compares those bytes, so the terminal is not
// needed to check the editing decisions.
//
// Not owned: termios, ioctl window size, the Windows console API, the
// completion callback (a tab is a plain character here, as it is natively when
// no callback is set), the spinner, the cursor-position query in put_codepoint,
// and wcwidth. The port pins the width to 0 for control code points and 1
// otherwise; the graded corpus uses ASCII, Cyrillic and two control bytes, all
// of which glibc reports as width <= 1. Wide and combining code points stay
// native.
//
// is_space_codepoint is pinned to the ASCII whitespace set (space and HT..CR),
// the only whitespace the corpus uses; the full iswspace table stays native.
//
// State is explicit: `line` is the buffer, `llen[0]` its byte length,
// `pos[0]` the character index, `pos[1]` the byte index, `out`/`olen[0]` the
// emitted bytes, and history lives in an arena with per-entry offsets.

let CON_WEOF: i64 = 0 - 1;

// ---------------------------------------------------------------------------
// UTF-8 primitives

// decode_utf8: 1..4 bytes, U+FFFD on a bad continuation or a truncated tail.
fn con_decode(line: const u8[:], len: i64, pos: i64, adv: i64[:]) -> i64 {
    let c = line[pos] as i64;

    if c & 128 == 0 {
        adv[0] = 1;
        return c;
    }
    if c & 224 == 192 && pos + 1 < len {
        let c1 = line[pos + 1] as i64;

        if c1 & 192 != 128 {
            adv[0] = 1;
            return 65533;
        }
        adv[0] = 2;
        return ((c & 31) << 6) | (c1 & 63);
    }
    if c & 240 == 224 && pos + 2 < len {
        let c1 = line[pos + 1] as i64;
        let c2 = line[pos + 2] as i64;

        if c1 & 192 != 128 || c2 & 192 != 128 {
            adv[0] = 1;
            return 65533;
        }
        adv[0] = 3;
        return ((c & 15) << 12) | ((c1 & 63) << 6) | (c2 & 63);
    }
    if c & 248 == 240 && pos + 3 < len {
        let c1 = line[pos + 1] as i64;
        let c2 = line[pos + 2] as i64;
        let c3 = line[pos + 3] as i64;

        if c1 & 192 != 128 || c2 & 192 != 128 || c3 & 192 != 128 {
            adv[0] = 1;
            return 65533;
        }
        adv[0] = 4;
        return ((c & 7) << 18) | ((c1 & 63) << 12) | ((c2 & 63) << 6) | (c3 & 63);
    }
    adv[0] = 1;
    return 65533;
}

// append_utf8: 1..4 bytes, nothing for a code point above U+10FFFF.
fn con_encode(cp: i64, out: u8[:], at: i64) -> i64 {
    var p = at;

    if cp <= 127 {
        out[p] = cp as u8;
        return p + 1;
    }
    if cp <= 2047 {
        out[p] = (192 | ((cp >> 6) & 31)) as u8;
        out[p + 1] = (128 | (cp & 63)) as u8;
        return p + 2;
    }
    if cp <= 65535 {
        out[p] = (224 | ((cp >> 12) & 15)) as u8;
        out[p + 1] = (128 | ((cp >> 6) & 63)) as u8;
        out[p + 2] = (128 | (cp & 63)) as u8;
        return p + 3;
    }
    if cp <= 1114111 {
        out[p] = (240 | ((cp >> 18) & 7)) as u8;
        out[p + 1] = (128 | ((cp >> 12) & 63)) as u8;
        out[p + 2] = (128 | ((cp >> 6) & 63)) as u8;
        out[p + 3] = (128 | (cp & 63)) as u8;
        return p + 4;
    }
    return p;
}

fn con_prev_pos(line: const u8[:], len: i64, pos: i64) -> i64 {
    if pos == 0 {
        return 0;
    }
    var p = pos - 1;

    while p > 0 && (line[p] as i64) & 192 == 128 {
        p = p - 1;
    }
    return p;
}

fn con_next_pos(line: const u8[:], len: i64, pos: i64) -> i64 {
    if pos >= len {
        return len;
    }
    var p = pos + 1;

    while p < len && (line[p] as i64) & 192 == 128 {
        p = p + 1;
    }
    return p;
}

// estimateWidth with tty == nullptr returns wcwidth, and a negative width is
// clamped to 0 by the caller. Pinned: control code points are 0, the graded
// range is 1.
fn con_width(cp: i64) -> i64 {
    if cp < 32 || cp == 127 {
        return 0;
    }
    return 1;
}

// iswspace over the graded range.
fn con_is_space(cp: i64) -> i64 {
    if cp == 32 || (cp >= 9 && cp <= 13) {
        return 1;
    }
    return 0;
}

// has_ctrl_modifier: any ';' separated parameter equal to 5.
fn con_has_ctrl(params: const u8[:], plen: i64) -> i64 {
    var start: i64 = 0;

    while start < plen {
        var end = plen;
        var i = start;

        while i < plen {
            if params[i] == 59 {
                end = i;
                i = plen;
            } else {
                i = i + 1;
            }
        }
        let n = end - start;

        if n > 0 {
            var value: i64 = 0;
            var bad = false;
            var j: i64 = 0;

            while j < n {
                let ch = params[start + j];

                if ch < 48 || ch > 57 {
                    bad = true;
                    j = n;
                } else {
                    value = value * 10 + (ch as i64 - 48);
                    j = j + 1;
                }
            }
            if !bad && value == 5 {
                return 1;
            }
        }
        if end == plen {
            start = plen;
        } else {
            start = end + 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Output helpers

fn con_cursor(delta: i64, out: u8[:], olen: i64[:]) {
    if delta == 0 {
        return;
    }
    if delta < 0 {
        var i: i64 = 0;

        while i < 0 - delta {
            out[olen[0]] = 8;
            olen[0] = olen[0] + 1;
            i = i + 1;
        }
    } else {
        var i: i64 = 0;

        while i < delta {
            out[olen[0]] = 27;
            out[olen[0] + 1] = 91;
            out[olen[0] + 2] = 67;
            olen[0] = olen[0] + 3;
            i = i + 1;
        }
    }
}

fn con_emit(out: u8[:], olen: i64[:], src: const u8[:], from: i64, to: i64) {
    var i = from;

    while i < to {
        out[olen[0]] = src[i];
        olen[0] = olen[0] + 1;
        i = i + 1;
    }
}

fn con_spaces(n: i64, out: u8[:], olen: i64[:]) {
    var i: i64 = 0;

    while i < n {
        out[olen[0]] = 32;
        olen[0] = olen[0] + 1;
        i = i + 1;
    }
}

fn con_width_at(line: const u8[:], len: i64, pos: i64) -> i64 {
    var adv: i64[1] = [0];
    let cp = con_decode(line, len, pos, adv);

    return con_width(cp);
}

fn con_char_count(line: const u8[:], len: i64) -> i64 {
    var p: i64 = 0;
    var n: i64 = 0;

    while p < len {
        var adv: i64[1] = [0];

        con_decode(line, len, p, adv);
        p = p + adv[0];
        n = n + 1;
    }
    return n;
}

// Byte offset of character index `idx`.
fn con_byte_of(line: const u8[:], len: i64, idx: i64) -> i64 {
    var p: i64 = 0;
    var n: i64 = 0;

    while n < idx && p < len {
        var adv: i64[1] = [0];

        con_decode(line, len, p, adv);
        p = p + adv[0];
        n = n + 1;
    }
    return p;
}

fn con_erase_bytes(line: u8[:], llen: i64[:], at: i64, count: i64) {
    var i = at;
    let n = llen[0];

    while i + count < n {
        line[i] = line[i + count];
        i = i + 1;
    }
    llen[0] = n - count;
}

fn con_insert_bytes(line: u8[:], llen: i64[:], at: i64, src: const u8[:], sn: i64) {
    var i = llen[0];

    while i > at {
        line[i + sn - 1] = line[i - 1];
        i = i - 1;
    }
    var j: i64 = 0;

    while j < sn {
        line[at + j] = src[j];
        j = j + 1;
    }
    llen[0] = llen[0] + sn;
}

// ---------------------------------------------------------------------------
// Editing operations. `pos[0]` is the character index, `pos[1]` the byte index.

fn con_move_start(line: const u8[:], llen: i64[:], pos: i64[:], out: u8[:], olen: i64[:]) {
    var back: i64 = 0;
    var p: i64 = 0;

    while p < pos[1] {
        back = back + con_width_at(line, llen[0], p);
        var adv: i64[1] = [0];

        con_decode(line, llen[0], p, adv);
        p = p + adv[0];
    }
    con_cursor(0 - back, out, olen);
    pos[0] = 0;
    pos[1] = 0;
}

fn con_move_end(line: const u8[:], llen: i64[:], pos: i64[:], out: u8[:], olen: i64[:]) {
    var forward: i64 = 0;
    var p = pos[1];

    while p < llen[0] {
        forward = forward + con_width_at(line, llen[0], p);
        var adv: i64[1] = [0];

        con_decode(line, llen[0], p, adv);
        p = p + adv[0];
    }
    con_cursor(forward, out, olen);
    pos[0] = con_char_count(line, llen[0]);
    pos[1] = llen[0];
}

fn con_word_left(line: const u8[:], llen: i64[:], pos: i64[:], out: u8[:], olen: i64[:]) {
    if pos[0] == 0 {
        return;
    }
    var cpos = pos[0];
    var bpos = pos[1];
    var move: i64 = 0;
    var adv: i64[1] = [0];

    while cpos > 0 {
        let prev = con_prev_pos(line, llen[0], bpos);
        let cp = con_decode(line, llen[0], prev, adv);

        if con_is_space(cp) == 0 {
            break;
        }
        move = move + con_width(cp);
        cpos = cpos - 1;
        bpos = prev;
    }
    while cpos > 0 {
        let prev = con_prev_pos(line, llen[0], bpos);
        let cp = con_decode(line, llen[0], prev, adv);

        if con_is_space(cp) == 1 {
            break;
        }
        move = move + con_width(cp);
        cpos = cpos - 1;
        bpos = prev;
    }
    con_cursor(0 - move, out, olen);
    pos[0] = cpos;
    pos[1] = bpos;
}

fn con_word_right(line: const u8[:], llen: i64[:], pos: i64[:], out: u8[:], olen: i64[:]) {
    let total = con_char_count(line, llen[0]);

    if pos[0] >= total {
        return;
    }
    var cpos = pos[0];
    var bpos = pos[1];
    var move: i64 = 0;
    var adv: i64[1] = [0];

    // leading spaces
    while cpos < total {
        let cp = con_decode(line, llen[0], bpos, adv);

        if con_is_space(cp) == 0 {
            break;
        }
        move = move + con_width(cp);
        cpos = cpos + 1;
        bpos = bpos + adv[0];
    }
    // the word
    while cpos < total {
        let cp = con_decode(line, llen[0], bpos, adv);

        if con_is_space(cp) == 1 {
            break;
        }
        move = move + con_width(cp);
        cpos = cpos + 1;
        bpos = bpos + adv[0];
    }
    // trailing spaces
    while cpos < total {
        let cp = con_decode(line, llen[0], bpos, adv);

        if con_is_space(cp) == 0 {
            break;
        }
        move = move + con_width(cp);
        cpos = cpos + 1;
        bpos = bpos + adv[0];
    }
    con_cursor(move, out, olen);
    pos[0] = cpos;
    pos[1] = bpos;
}

// Redraw every character from `from` and return the summed width.
fn con_redraw_tail(line: const u8[:], llen: i64[:], from: i64, out: u8[:], olen: i64[:]) -> i64 {
    var p = from;
    var width: i64 = 0;

    while p < llen[0] {
        var adv: i64[1] = [0];
        let cp = con_decode(line, llen[0], p, adv);

        con_emit(out, olen, line, p, p + adv[0]);
        width = width + con_width(cp);
        p = p + adv[0];
    }
    return width;
}

fn con_delete(line: u8[:], llen: i64[:], pos: i64[:], out: u8[:], olen: i64[:]) {
    if pos[0] >= con_char_count(line, llen[0]) {
        return;
    }
    let next = con_next_pos(line, llen[0], pos[1]);
    let w = con_width_at(line, llen[0], pos[1]);
    let char_len = next - pos[1];

    con_erase_bytes(line, llen, pos[1], char_len);
    let tail = con_redraw_tail(line, llen, pos[1], out, olen);

    con_spaces(w, out, olen);
    con_cursor(0 - (tail + w), out, olen);
}

fn con_backspace(line: u8[:], llen: i64[:], pos: i64[:], out: u8[:], olen: i64[:]) {
    if pos[0] == 0 {
        return;
    }
    let prev = con_prev_pos(line, llen[0], pos[1]);
    let w = con_width_at(line, llen[0], prev);
    let char_len = pos[1] - prev;

    con_cursor(0 - w, out, olen);
    pos[0] = pos[0] - 1;
    pos[1] = prev;
    con_erase_bytes(line, llen, pos[1], char_len);
    let tail = con_redraw_tail(line, llen, pos[1], out, olen);

    con_spaces(w, out, olen);
    con_cursor(0 - (tail + w), out, olen);
}

fn con_insert(line: u8[:], llen: i64[:], pos: i64[:], cp: i64, out: u8[:], olen: i64[:]) {
    var enc: u8[4] = [0, 0, 0, 0];
    let n = con_encode(cp, enc, 0);
    let w = con_width(cp);
    let total = con_char_count(line, llen[0]);

    if pos[0] == total {
        con_insert_bytes(line, llen, llen[0], enc, n);
        con_emit(out, olen, line, llen[0] - n, llen[0]);
        pos[0] = pos[0] + 1;
        pos[1] = llen[0];
        return;
    }
    let at = pos[1];

    con_insert_bytes(line, llen, at, enc, n);
    con_emit(out, olen, line, at, at + n);
    let tail = con_redraw_tail(line, llen, at + n, out, olen);

    con_cursor(0 - tail, out, olen);
    pos[0] = pos[0] + 1;
    pos[1] = at + n;
}

// set_line_contents: cursor to the start, blank the old line, write the new
// one, leave the cursor at its end.
fn con_set_line(src: const u8[:], sn: i64, line: u8[:], llen: i64[:], pos: i64[:], out: u8[:], olen: i64[:]) {
    con_move_start(line, llen, pos, out, olen);
    var total: i64 = 0;
    var p: i64 = 0;

    while p < llen[0] {
        let w = con_width_at(line, llen[0], p);

        if w > 0 {
            total = total + w;
        } else {
            total = total + 1;
        }
        var adv: i64[1] = [0];

        con_decode(line, llen[0], p, adv);
        p = p + adv[0];
    }
    con_spaces(total, out, olen);
    con_cursor(0 - total, out, olen);

    llen[0] = 0;
    pos[0] = 0;
    pos[1] = 0;
    con_insert_bytes(line, llen, 0, src, sn);
    p = 0;
    while p < llen[0] {
        var adv: i64[1] = [0];

        let cp = con_decode(line, llen[0], p, adv);

        con_emit(out, olen, line, p, p + adv[0]);
        p = p + adv[0];
    }
    pos[0] = con_char_count(line, llen[0]);
    pos[1] = llen[0];
}

// ---------------------------------------------------------------------------
// History

fn con_hist_add(
    arena: u8[:],
    used: i64[:],
    off: i64[:],
    len: i64[:],
    count: i64[:],
    text: const u8[:],
    tlen: i64,
    view: i64[:],
    blen: i64[:]) {
    if tlen == 0 {
        return;
    }
    var same = false;

    if count[0] > 0 {
        let last = count[0] - 1;

        if len[last] == tlen {
            same = true;
            var i: i64 = 0;

            while i < tlen {
                if arena[off[last] + i] != text[i] {
                    same = false;
                    i = tlen;
                } else {
                    i = i + 1;
                }
            }
        }
    }
    if !same {
        off[count[0]] = used[0];
        len[count[0]] = tlen;
        var i: i64 = 0;

        while i < tlen {
            arena[used[0] + i] = text[i];
            i = i + 1;
        }
        used[0] = used[0] + tlen;
        count[0] = count[0] + 1;
    }
    view[0] = 0 - 1;
    blen[0] = 0;
}

fn con_hist_begin(line: const u8[:], llen: i64[:], backup: u8[:], blen: i64[:], count: i64[:], view: i64[:]) {
    var i: i64 = 0;

    while i < llen[0] {
        backup[i] = line[i];
        i = i + 1;
    }
    blen[0] = llen[0];
    view[0] = count[0];
}

fn con_hist_prev(
    arena: const u8[:],
    off: i64[:],
    len: i64[:],
    count: i64[:],
    view: i64[:],
    line: u8[:],
    llen: i64[:],
    pos: i64[:],
    out: u8[:],
    olen: i64[:]) {
    if count[0] == 0 || view[0] < 0 {
        return;
    }
    if view[0] > 0 {
        view[0] = view[0] - 1;
    }
    let e = view[0];

    con_set_line(arena[off[e] .. off[e] + len[e]], len[e], line, llen, pos, out, olen);
}

fn con_hist_next(
    arena: const u8[:],
    off: i64[:],
    len: i64[:],
    count: i64[:],
    view: i64[:],
    backup: const u8[:],
    blen: i64[:],
    line: u8[:],
    llen: i64[:],
    pos: i64[:],
    out: u8[:],
    olen: i64[:]) {
    if count[0] == 0 || view[0] < 0 {
        return;
    }
    view[0] = view[0] + 1;
    if view[0] >= count[0] {
        con_set_line(backup[0..blen[0]], blen[0], line, llen, pos, out, olen);
        view[0] = 0 - 1;
        blen[0] = 0;
    } else {
        let e = view[0];

        con_set_line(arena[off[e] .. off[e] + len[e]], len[e], line, llen, pos, out, olen);
    }
}

// ---------------------------------------------------------------------------
// The editing loop

fn con_next_key(keys: const u8[:], kpos: i64[:]) -> i64 {
    if kpos[0] >= keys.len {
        return CON_WEOF;
    }
    var adv: i64[1] = [0];
    let cp = con_decode(keys, keys.len, kpos[0], adv);

    kpos[0] = kpos[0] + adv[0];
    return cp;
}

fn con_eq_digits(d: const u8[:], dn: i64, lit: const u8[:]) -> bool {
    if dn != lit.len {
        return false;
    }
    var i: i64 = 0;

    while i < dn {
        if d[i] != lit[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn con_replace_last(line: const u8[:], llen: i64[:], ch: u8, out: u8[:], olen: i64[:]) {
    out[olen[0]] = 8;
    olen[0] = olen[0] + 1;
    out[olen[0]] = ch;
    olen[0] = olen[0] + 1;
}

fn con_pop_cursor(out: u8[:], olen: i64[:]) {
    out[olen[0]] = 8;
    olen[0] = olen[0] + 1;
}

fn con_emit_newline(line: u8[:], llen: i64[:]) {
    line[llen[0]] = 10;
    llen[0] = llen[0] + 1;
}

fn con_edit(
    keys: const u8[:],
    multiline: i64,
    line: u8[:],
    llen: i64[:],
    pos: i64[:],
    out: u8[:],
    olen: i64[:],
    arena: u8[:],
    used: i64[:],
    hoff: i64[:],
    hlen: i64[:],
    hcount: i64[:],
    view: i64[:],
    backup: u8[:],
    blen: i64[:]) -> i64 {
    var kpos: i64[1] = [0];
    var is_special = false;
    var eos = false;
    var run = true;

    while run {
        let ch = con_next_key(keys, kpos);

        if ch == 13 || ch == 10 {
            run = false;
        } else if ch == CON_WEOF || ch == 4 {
            eos = true;
            run = false;
        } else {
            if is_special {
                con_replace_last(line, llen, line[llen[0] - 1], out, olen);
                is_special = false;
            }
            if ch == 27 {
                var code = con_next_key(keys, kpos);

                if code == 91 {
                    var params: u8[128] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
                    var plen: i64 = 0;

                    var more = true;

                    while more {
                        code = con_next_key(keys, kpos);
                        if (code >= 65 && code <= 90) || (code >= 97 && code <= 122) || code == 126 || code == CON_WEOF {
                            more = false;
                        } else {
                            params[plen] = code as u8;
                            plen = plen + 1;
                        }
                    }
                    let ctrl = con_has_ctrl(params, plen);

                    if code == 68 {
                        if ctrl == 1 {
                            con_word_left(line, llen, pos, out, olen);
                        } else if pos[0] > 0 {
                            let prev = con_prev_pos(line, llen[0], pos[1]);
                            let w = con_width_at(line, llen[0], prev);

                            con_cursor(0 - w, out, olen);
                            pos[0] = pos[0] - 1;
                            pos[1] = prev;
                        }
                    } else if code == 67 {
                        if ctrl == 1 {
                            con_word_right(line, llen, pos, out, olen);
                        } else if pos[0] < con_char_count(line, llen[0]) {
                            let w = con_width_at(line, llen[0], pos[1]);

                            con_cursor(w, out, olen);
                            pos[0] = pos[0] + 1;
                            pos[1] = con_next_pos(line, llen[0], pos[1]);
                        }
                    } else if code == 72 {
                        con_move_start(line, llen, pos, out, olen);
                    } else if code == 70 {
                        con_move_end(line, llen, pos, out, olen);
                    } else if code == 65 {
                        if view[0] < 0 {
                            con_hist_begin(line, llen, backup, blen, hcount, view);
                        }
                        con_hist_prev(arena, hoff, hlen, hcount, view, line, llen, pos, out, olen);
                        is_special = false;
                    } else if code == 66 {
                        if view[0] >= 0 {
                            con_hist_next(arena, hoff, hlen, hcount, view, backup, blen, line, llen, pos, out, olen);
                        }
                        is_special = false;
                    } else if (code == 126 || (code >= 65 && code <= 90) || (code >= 97 && code <= 122)) && plen > 0 {
                        if code == 126 {
                            var digits: u8[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
                            var dn: i64 = 0;
                            var i: i64 = 0;

                            while i < plen {
                                if params[i] == 59 {
                                    i = plen;
                                } else {
                                    if params[i] >= 48 && params[i] <= 57 {
                                        digits[dn] = params[i];
                                        dn = dn + 1;
                                    }
                                    i = i + 1;
                                }
                            }
                            if con_eq_digits(digits, dn, "1") || con_eq_digits(digits, dn, "7") {
                                con_move_start(line, llen, pos, out, olen);
                            } else if con_eq_digits(digits, dn, "4") || con_eq_digits(digits, dn, "8") {
                                con_move_end(line, llen, pos, out, olen);
                            } else if con_eq_digits(digits, dn, "3") {
                                con_delete(line, llen, pos, out, olen);
                            }
                        }
                    }
                } else if code == 27 {
                    var drain = true;

                    while drain {
                        code = con_next_key(keys, kpos);
                        if code == CON_WEOF {
                            drain = false;
                        } else if (code >= 65 && code <= 90) || (code >= 97 && code <= 122) || code == 126 {
                            drain = false;
                        }
                    }
                }
            } else if ch == 8 || ch == 127 {
                con_backspace(line, llen, pos, out, olen);
            } else {
                con_insert(line, llen, pos, ch, out, olen);
            }
            if llen[0] > 0 && (line[llen[0] - 1] == 92 || line[llen[0] - 1] == 47) {
                con_replace_last(line, llen, line[llen[0] - 1], out, olen);
                is_special = true;
            }
        }
    }

    var has_more = multiline;

    if is_special {
        con_replace_last(line, llen, 32, out, olen);
        con_pop_cursor(out, olen);

        let last = line[llen[0] - 1];

        llen[0] = llen[0] - 1;
        if last == 92 {
            con_emit_newline(line, llen);
            out[olen[0]] = 10;
            olen[0] = olen[0] + 1;
            has_more = 1 - has_more;
        } else {
            if llen[0] == 1 && line[0] == 32 {
                llen[0] = 0;
                con_pop_cursor(out, olen);
            }
            con_emit_newline(line, llen);
            has_more = 0;
        }
    } else {
        if eos {
            has_more = 0;
        } else {
            con_emit_newline(line, llen);
            out[olen[0]] = 10;
            olen[0] = olen[0] + 1;
        }
    }
    if !eos && llen[0] > 0 {
        var hl = llen[0];

        if line[hl - 1] == 10 {
            hl = hl - 1;
        }
        con_hist_add(arena, used, hoff, hlen, hcount, line, hl, view, blen);
    }
    return has_more;
}

// Export wrappers: Goose cannot call an export fn from another function.
export fn ce_decode(line: const u8[:], len: i64, pos: i64, adv: i64[:]) -> i64 {
    return con_decode(line, len, pos, adv);
}

export fn ce_encode(cp: i64, out: u8[:], at: i64) -> i64 {
    return con_encode(cp, out, at);
}

export fn ce_prev_pos(line: const u8[:], len: i64, pos: i64) -> i64 {
    return con_prev_pos(line, len, pos);
}

export fn ce_next_pos(line: const u8[:], len: i64, pos: i64) -> i64 {
    return con_next_pos(line, len, pos);
}

export fn ce_has_ctrl(params: const u8[:], plen: i64) -> i64 {
    return con_has_ctrl(params, plen);
}

export fn ce_width(cp: i64) -> i64 {
    return con_width(cp);
}

export fn ce_is_space(cp: i64) -> i64 {
    return con_is_space(cp);
}

export fn ce_edit(
    keys: const u8[:],
    multiline: i64,
    line: u8[:],
    llen: i64[:],
    pos: i64[:],
    out: u8[:],
    olen: i64[:],
    arena: u8[:],
    used: i64[:],
    hoff: i64[:],
    hlen: i64[:],
    hcount: i64[:],
    view: i64[:],
    backup: u8[:],
    blen: i64[:]) -> i64 {
    return con_edit(keys, multiline, line, llen, pos, out, olen, arena, used, hoff, hlen, hcount, view, backup, blen);
}
