// MCP stdio transport shells (P8-07 S5) in Goose, from tools/server/server-mcp.cpp:
// mcp_pump_ndjson (24-129), resolve_command (379-424), parent_env/build_env
// (426-460), write_all (537-602), the reader/writer/errlog pumps (528-632) and
// the manager (653-819).
//
// No process is spawned and no thread runs. Every export is a decision over
// data the caller supplies: the search directories and the existing files, the
// write/poll result sequence, the pipe chunk sequence, the errlog lines, the
// manager clocks and the warmup rows. The environment is a list of "KEY=VALUE"
// entries, not `environ`.

import std;

const MS_MAX_ENV: i64 = 128;
const MS_MAX_ROWS: i64 = 64;
const MS_BUF: i64 = 1024;
const MS_LINES: i64 = 16384;

// ── small helpers ──────────────────────────────────────────────────────────

fn ms_find_range(s: const u8[:], pos_from: i64, to: i64, c: u8) -> i64 {
    var i = pos_from;

    while i < to {
        if s[i] == c {
            return i;
        }
        i = i + 1;
    }
    return to;
}

fn ms_find(s: const u8[:], pos_from: i64, c: u8) -> i64 {
    return ms_find_range(s, pos_from, s.len, c);
}

fn ms_put(dst: u8[:], at: i64, s: const u8[:], pos_from: i64, n: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < n {
        if p < dst.len {
            dst[p] = s[pos_from + i];
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn ms_puts(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return ms_put(dst, at, s, 0, s.len);
}

fn ms_putc(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn ms_put_i(dst: u8[:], at: i64, v: i64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;
    var q = at;

    if x < 0 {
        q = ms_putc(dst, q, 45);
        x = 0 - x;
    }
    if x == 0 {
        return ms_putc(dst, q, 48);
    }
    while x > 0 {
        buf[n] = (48 + x % 10) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;

    while i > 0 {
        i = i - 1;
        q = ms_putc(dst, q, buf[i]);
    }
    return q;
}

// one byte of a pump line: backslash and non-printables are escaped so the
// output stays line oriented
fn ms_put_esc_byte(dst: u8[:], at: i64, c: u8) -> i64 {
    let hex: u8[16] = [48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102];
    var p = at;

    if c == 92 {
        p = ms_putc(dst, p, 92);
        p = ms_putc(dst, p, 92);
    } else if c < 32 || c > 126 {
        p = ms_putc(dst, p, 92);
        p = ms_putc(dst, p, 120);
        p = ms_putc(dst, p, hex[(c / 16) as i64]);
        p = ms_putc(dst, p, hex[(c % 16) as i64]);
    } else {
        p = ms_putc(dst, p, c);
    }
    return p;
}

fn ms_atoi(s: const u8[:], pos_from: i64, n: i64) -> i64 {
    var v: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let c = s[pos_from + i];
        if c >= 48 && c <= 57 {
            v = v * 10 + c - 48;
        }
        i = i + 1;
    }
    return v;
}

// erase the first k bytes of a buffer with an explicit length
fn ms_erase(buf: u8[:], n: i64, k: i64) -> i64 {
    var i = k;

    while i < n {
        buf[i - k] = buf[i];
        i = i + 1;
    }
    return n - k;
}

fn ms_eq_range(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> bool {
    if al != bl {
        return false;
    }
    var i: i64 = 0;

    while i < al {
        if a[ao + i] != b[bo + i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn ms_less_range(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> bool {
    var i: i64 = 0;

    while i < al && i < bl {
        if a[ao + i] != b[bo + i] {
            return a[ao + i] < b[bo + i];
        }
        i = i + 1;
    }
    return al < bl;
}

// ── resolve_command (379-424) ──────────────────────────────────────────────

fn ms_has_sep(s: const u8[:]) -> bool {
    var i: i64 = 0;

    while i < s.len {
        if s[i] == 92 || s[i] == 47 {
            return true;
        }
        i = i + 1;
    }
    return false;
}

// SearchPathW appends an extension only when the name has none; a dot before
// the last separator belongs to a directory, not to the file name.
fn ms_has_ext(s: const u8[:]) -> bool {
    var last_dot: i64 = 0 - 1;
    var last_sep: i64 = 0 - 1;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];
        if c == 92 || c == 47 {
            last_sep = i;
        }
        if c == 46 {
            last_dot = i;
        }
        i = i + 1;
    }
    return last_dot > last_sep;
}

// dir + separator + command + ext, newline terminated; an empty dir means the
// command is already a path.
fn ms_cand(dst: u8[:], at: i64, dir: const u8[:], cmd: const u8[:], ext: const u8[:]) -> i64 {
    var p = at;

    if dir.len > 0 {
        p = ms_put(dst, p, dir, 0, dir.len);
        let last = dir[dir.len - 1];
        if last != 92 && last != 47 {
            p = ms_putc(dst, p, 92);
        }
    }
    p = ms_put(dst, p, cmd, 0, cmd.len);
    p = ms_put(dst, p, ext, 0, ext.len);
    return ms_putc(dst, p, 10);
}

fn ms_dirs_pass(dst: u8[:], at: i64, dirs: const u8[:], cmd: const u8[:], ext: const u8[:]) -> i64 {
    var p = at;
    var t: i64 = 0;

    while t <= dirs.len {
        let e = ms_find(dirs, t, 59);

        if e > t {
            p = ms_cand(dst, p, dirs[t .. e], cmd, ext);
        }
        if e >= dirs.len {
            t = dirs.len + 1;
        } else {
            t = e + 1;
        }
    }
    return p;
}

fn ms_ext_list(dst: u8[:], at: i64, dirs: const u8[:], cmd: const u8[:], list: const u8[:]) -> i64 {
    var p = at;
    var t: i64 = 0;

    while t <= list.len {
        let e = ms_find(list, t, 59);

        if e > t {
            p = ms_dirs_pass(dst, p, dirs, cmd, list[t .. e]);
        }
        if e >= list.len {
            t = list.len + 1;
        } else {
            t = e + 1;
        }
    }
    return p;
}

fn ms_ext_pass(dst: u8[:], at: i64, dirs: const u8[:], cmd: const u8[:], pathext: const u8[:]) -> i64 {
    if pathext.len == 0 {
        return ms_ext_list(dst, at, dirs, cmd, ".COM;.EXE;.BAT;.CMD");
    }
    return ms_ext_list(dst, at, dirs, cmd, pathext);
}

fn ms_cmd_ext_list(dst: u8[:], at: i64, cmd: const u8[:], list: const u8[:]) -> i64 {
    var p = at;
    var t: i64 = 0;

    while t <= list.len {
        let e = ms_find(list, t, 59);

        if e > t {
            p = ms_cand(dst, p, "", cmd, list[t .. e]);
        }
        if e >= list.len {
            t = list.len + 1;
        } else {
            t = e + 1;
        }
    }
    return p;
}

fn ms_cmd_ext_pass(dst: u8[:], at: i64, cmd: const u8[:], pathext: const u8[:]) -> i64 {
    if pathext.len == 0 {
        return ms_cmd_ext_list(dst, at, cmd, ".COM;.EXE;.BAT;.CMD");
    }
    return ms_cmd_ext_list(dst, at, cmd, pathext);
}

// Ordered spawn candidates, one per line, in the order SearchPathW would try
// them: exact name in every search directory, then one PATHEXT extension at a
// time across every directory. A command with a separator is searched only as
// a path. Nothing here stats the files: the caller resolves the first candidate
// that exists.
export fn ms_resolve_candidates(command: const u8[:], dirs: const u8[:],
                                pathext: const u8[:], out: u8[:]) -> i64 {
    if command.len == 0 {
        return 0;
    }
    if ms_has_sep(command) {
        var p = ms_cand(out, 0, "", command, "");
        if ms_has_ext(command) == false {
            p = ms_cmd_ext_pass(out, p, command, pathext);
        }
        return p;
    }
    if ms_has_ext(command) {
        return ms_dirs_pass(out, 0, dirs, command, "");
    }
    var p = ms_dirs_pass(out, 0, dirs, command, "");
    p = ms_ext_pass(out, p, dirs, command, pathext);
    return p;
}

// ── build_env (426-460) ────────────────────────────────────────────────────

// Parent entries whose key is not overridden keep their position and their
// exact bytes; overrides follow, sorted by key like the native std::map.
export fn ms_env_build(parent: const u8[:], overrides: const u8[:], out: u8[:]) -> i64 {
    var ko: i64[MS_MAX_ENV] = [0; MS_MAX_ENV];
    var kl: i64[MS_MAX_ENV] = [0; MS_MAX_ENV];
    var kc: i64[MS_MAX_ENV] = [0; MS_MAX_ENV];
    var cnt: i64 = 0;
    var t: i64 = 0;

    while t < overrides.len && cnt < MS_MAX_ENV {
        let e = ms_find(overrides, t, 10);

        if e > t {
            ko[cnt] = t;
            let eq = ms_find_range(overrides, t, e, 61);
            if eq < e {
                kl[cnt] = eq - t;
            } else {
                kl[cnt] = e - t;
            }
            kc[cnt] = e;
            cnt = cnt + 1;
        }
        t = e + 1;
    }

    // insertion sort by key, byte order
    var i: i64 = 1;
    while i < cnt {
        var j = i;

        while j > 0 && ms_less_range(overrides, ko[j], kl[j], overrides, ko[j - 1], kl[j - 1]) {
            let to_ = ko[j]; ko[j] = ko[j - 1]; ko[j - 1] = to_;
            let tl = kl[j]; kl[j] = kl[j - 1]; kl[j - 1] = tl;
            let tc = kc[j]; kc[j] = kc[j - 1]; kc[j - 1] = tc;
            j = j - 1;
        }
        i = i + 1;
    }

    var p: i64 = 0;
    t = 0;
    while t <= parent.len {
        let e = ms_find(parent, t, 10);

        if e > t {
            let eq = ms_find_range(parent, t, e, 61);
            var pk: i64 = 0;
            if eq < e {
                pk = eq - t;
            } else {
                pk = e - t;
            }
            var hit: i64 = 0;
            var k: i64 = 0;

            while k < cnt {
                if ms_eq_range(parent, t, pk, overrides, ko[k], kl[k]) {
                    hit = 1;
                }
                k = k + 1;
            }
            if hit == 0 {
                p = ms_put(out, p, parent, t, e - t);
                p = ms_putc(out, p, 10);
            }
        }
        t = e + 1;
    }

    i = 0;
    while i < cnt {
        let oe = kc[i];
        let eq = ms_find_range(overrides, ko[i], oe, 61);

        p = ms_put(out, p, overrides, ko[i], kl[i]);
        p = ms_putc(out, p, 61);
        if eq < oe {
            p = ms_put(out, p, overrides, eq + 1, oe - eq - 1);
        }
        p = ms_putc(out, p, 10);
        i = i + 1;
    }
    return p;
}

// ── write_all (537-602) ────────────────────────────────────────────────────

// Script tokens: wN wrote N bytes, 0 wrote zero, i EINTR, g EAGAIN (poll
// follows: t timeout, p POLLOUT, q poll EINTR, h/x/n HUP/ERR/NVAL, e poll
// failure), s running cleared. rc 0 all bytes, 1 failed, 2 script exhausted.
export fn ms_write_plan(size: i64, script: const u8[:], out: u8[:]) -> i64 {
    var total: i64 = 0;
    var steps: i64 = 0;
    var polls: i64 = 0;
    var touts: i64 = 0;
    var rc: i64 = 2;
    var pending: i64 = 0;
    var stop: i64 = 0;
    var t: i64 = 0;

    if size <= 0 {
        rc = 0;
        stop = 1;
    }
    while t < script.len && stop == 0 {
        let e = ms_find(script, t, 32);

        if e > t {
            let c0 = script[t];
            if pending != 0 {
                pending = 0;
                if c0 == 116 {
                    touts = touts + 1;
                } else if c0 == 112 && e - t == 1 {
                    // POLLOUT: retry the write
                } else if c0 == 113 {
                    // poll interrupted: retry the write
                } else if c0 == 104 || c0 == 120 || c0 == 110 || c0 == 101 {
                    rc = 1;
                    stop = 1;
                } else {
                    rc = 2;
                    stop = 1;
                }
            } else if c0 == 48 && e - t == 1 {
                rc = 1;
                steps = steps + 1;
                stop = 1;
            } else if c0 == 105 && e - t == 1 {
                steps = steps + 1;
            } else if c0 == 103 && e - t == 1 {
                steps = steps + 1;
                polls = polls + 1;
                pending = 1;
            } else if c0 == 115 && e - t == 1 {
                if total >= size {
                    rc = 0;
                } else {
                    rc = 1;
                }
                stop = 1;
            } else if c0 == 119 {
                var w = ms_atoi(script, t + 1, e - t - 1);
                if w > size - total {
                    w = size - total;
                }
                if w > 0 {
                    total = total + w;
                }
                steps = steps + 1;
                if total >= size {
                    rc = 0;
                    stop = 1;
                }
            } else {
                rc = 2;
                stop = 1;
            }
        }
        t = e + 1;
    }
    var p = ms_puts(out, 0, "rc=");
    p = ms_put_i(out, p, rc);
    p = ms_puts(out, p, " total=");
    p = ms_put_i(out, p, total);
    p = ms_puts(out, p, " steps=");
    p = ms_put_i(out, p, steps);
    p = ms_puts(out, p, " polls=");
    p = ms_put_i(out, p, polls);
    p = ms_puts(out, p, " timeouts=");
    p = ms_put_i(out, p, touts);
    return p;
}

// ── mcp_pump_ndjson (24-129) ───────────────────────────────────────────────

// Script: one step per line. d<payload> feeds pipe bytes (\n, \r, \\ escaped),
// t poll timeout, i read EINTR, g read EAGAIN, h POLLHUP, q poll EINTR,
// e EOF, r read error, x POLLERR, n POLLNVAL, p poll failure, s running
// cleared. max_line is an argument: the native constant is 8 MiB, the model
// exercises the cap with a small value. State: 0 stopped, 1 eof, 2 error,
// 3 consumer gone, 4 script exhausted.
export fn ms_pump_lines(script: const u8[:], consume: i64, max_line: i64, out: u8[:]) -> i64 {
    var buf: u8[MS_BUF] = [0; MS_BUF];
    var bn: i64 = 0;
    var cb: u8[MS_BUF] = [0; MS_BUF];
    var lb: u8[MS_LINES] = [0; MS_LINES];
    var ln: i64 = 0;
    var st: i64 = 5;
    var warns: i64 = 0;
    var emitted: i64 = 0;
    var skipping: i64 = 0;
    var t: i64 = 0;

    while t < script.len && st == 5 {
        let e = ms_find(script, t, 10);
        let s0 = t;
        t = e + 1;
        if e <= s0 {
            continue;
        }
        let code = script[s0];

        if code == 116 || code == 105 || code == 103 || code == 104 || code == 113 {
            // poll timeout, read EINTR, read EAGAIN, POLLHUP, poll EINTR: loop
        } else if code == 101 {
            st = 1;
        } else if code == 114 || code == 120 || code == 110 || code == 112 {
            st = 2;
        } else if code == 115 {
            st = 0;
        } else if code == 100 {
            // decode the payload
            var cn: i64 = 0;
            var i = s0 + 1;

            while i < e {
                let ch = script[i];

                if ch == 92 && i + 1 < e {
                    let nx = script[i + 1];
                    if nx == 110 {
                        cn = ms_puts(cb, cn, "\n");
                        i = i + 2;
                    } else if nx == 114 {
                        cn = ms_puts(cb, cn, "\r");
                        i = i + 2;
                    } else if nx == 92 {
                        cn = ms_puts(cb, cn, "\\");
                        i = i + 2;
                    } else {
                        cn = ms_put(cb, cn, script, i, 1);
                        i = i + 1;
                    }
                } else {
                    if cn < MS_BUF {
                        cb[cn] = ch;
                        cn = cn + 1;
                    }
                    i = i + 1;
                }
            }

            var k: i64 = 0;
            while k < cn {
                if bn < MS_BUF {
                    buf[bn] = cb[k];
                    bn = bn + 1;
                }
                k = k + 1;
            }

            var phase: i64 = 0;
            if skipping != 0 {
                let nl = ms_find_range(buf, 0, bn, 10);

                if nl >= bn {
                    if bn > max_line {
                        bn = 0;
                    }
                    phase = 1;
                } else {
                    bn = ms_erase(buf, bn, nl + 1);
                    skipping = 0;
                }
            }
            if phase == 0 {
                var nl = ms_find_range(buf, 0, bn, 10);

                while nl < bn {
                    var ll = nl;

                    if ll > 0 && buf[ll - 1] == 13 {
                        ll = ll - 1;
                    }
                    if ll > 0 {
                        ln = ms_putc(lb, ln, 76);
                        var q: i64 = 0;

                        while q < ll {
                            ln = ms_put_esc_byte(lb, ln, buf[q]);
                            q = q + 1;
                        }
                        ln = ms_putc(lb, ln, 10);
                        emitted = emitted + 1;
                        if emitted == consume {
                            st = 3;
                        }
                    }
                    bn = ms_erase(buf, bn, nl + 1);
                    if st == 3 {
                        nl = bn;
                    } else {
                        nl = ms_find_range(buf, 0, bn, 10);
                    }
                }
                if st != 3 && bn > max_line {
                    warns = warns + 1;
                    bn = 0;
                    skipping = 1;
                }
            }
        }
    }
    if st == 5 {
        st = 4;
    }

    var p = ms_puts(out, 0, "state=");
    p = ms_put_i(out, p, st);
    p = ms_puts(out, p, " warns=");
    p = ms_put_i(out, p, warns);
    p = ms_puts(out, p, " lines=");
    p = ms_put_i(out, p, emitted);
    p = ms_putc(out, p, 10);
    p = ms_put(out, p, lb, 0, ln);
    return p;
}

// ── errlog tail (618-632) ──────────────────────────────────────────────────

// Each stderr line is appended with a newline; the tail keeps the last `cap`
// bytes, so a truncation can start mid-line. cap is an argument (native 4096).
export fn ms_err_tail(text: const u8[:], cap: i64, out: u8[:]) -> i64 {
    var buf: u8[4096] = [0; 4096];
    var bn: i64 = 0;
    var t: i64 = 0;

    while t < text.len {
        let e = ms_find(text, t, 10);
        var i = t;

        while i < e {
            if bn < 4096 {
                buf[bn] = text[i];
                bn = bn + 1;
            }
            i = i + 1;
        }
        if bn < 4096 {
            buf[bn] = 10;
            bn = bn + 1;
        }
        if bn > cap {
            bn = ms_erase(buf, bn, bn - cap);
        }
        t = e + 1;
    }
    return ms_put(out, 0, buf, 0, bn);
}

// ── manager get_or_create (771-819) ────────────────────────────────────────

// Nine integers on one line: stopping now dead_present dead_deadline
// alive_present is_alive cfg_present start_ok start_alive. Action: 0 stopped,
// 1 cooldown, 2 reuse the live transport, 3 created, 4 start failed, 5 no
// config. A failed start records a 5 s cooldown.
export fn ms_manager_resolve(args: const u8[:], out: u8[:]) -> i64 {
    var v: i64[9] = [0; 9];
    var n: i64 = 0;
    var t: i64 = 0;

    while t < args.len && n < 9 {
        let e = ms_find(args, t, 32);

        if e > t {
            v[n] = ms_atoi(args, t, e - t);
            n = n + 1;
        }
        t = e + 1;
    }
    let stopping = v[0];
    let now = v[1];
    let dead = v[2];
    let ddl = v[3];
    let alive = v[4];
    let is_alive = v[5];
    let cfg = v[6];
    let sok = v[7];
    let salive = v[8];
    var action: i64 = 5;
    var close_old: i64 = 0;
    var close_new: i64 = 0;
    var cool: i64 = 0;
    var done: i64 = 0;

    if stopping != 0 {
        action = 0;
        done = 1;
    } else if dead != 0 && now < ddl {
        action = 1;
        done = 1;
    }
    if done == 0 && alive != 0 {
        if is_alive != 0 {
            action = 2;
            done = 1;
        } else {
            close_old = 1;
        }
    }
    if done == 0 {
        if cfg == 0 {
            action = 5;
        } else if sok != 0 && salive != 0 {
            action = 3;
        } else {
            action = 4;
            close_new = 1;
            cool = now + 5000;
        }
    }

    var p = ms_puts(out, 0, "action=");
    p = ms_put_i(out, p, action);
    p = ms_puts(out, p, " close_old=");
    p = ms_put_i(out, p, close_old);
    p = ms_puts(out, p, " close_new=");
    p = ms_put_i(out, p, close_new);
    p = ms_puts(out, p, " cooldown=");
    p = ms_put_i(out, p, cool);
    return p;
}

// ── warmup (693-749) ───────────────────────────────────────────────────────

// Rows: name|spawn_ms|start_ok|query_ms|stopping|tools. A duplicate name is
// skipped (configs are unique across both sources); each started server gets
// its own spawn+10 s deadline; the registry sum is reported at the end. A
// partial tool list from an aborted discovery is not modelled.
export fn ms_warmup_plan(rows: const u8[:], out: u8[:]) -> i64 {
    var so: i64[MS_MAX_ROWS] = [0; MS_MAX_ROWS];
    var sl: i64[MS_MAX_ROWS] = [0; MS_MAX_ROWS];
    var sn: i64 = 0;
    var reg: i64 = 0;
    var p: i64 = 0;
    var t: i64 = 0;

    if rows.len == 0 {
        return 0;
    }
    while t < rows.len {
        let e = ms_find(rows, t, 10);

        if e > t {
            let f0 = t;
            let f1 = ms_find_range(rows, f0, e, 124);
            let q1 = ms_find_range(rows, f1 + 1, e, 124);
            let spawn = ms_atoi(rows, f1 + 1, q1 - f1 - 1);
            let q2 = ms_find_range(rows, q1 + 1, e, 124);
            let start_ok = ms_atoi(rows, q1 + 1, q2 - q1 - 1);
            let q3 = ms_find_range(rows, q2 + 1, e, 124);
            let query = ms_atoi(rows, q2 + 1, q3 - q2 - 1);
            let q4 = ms_find_range(rows, q3 + 1, e, 124);
            let stopping = ms_atoi(rows, q3 + 1, q4 - q3 - 1);
            let tools = ms_atoi(rows, q4 + 1, e - q4 - 1);
            var dup: i64 = 0;
            var s: i64 = 0;

            while s < sn {
                if ms_eq_range(rows, f0, f1 - f0, rows, so[s], sl[s]) {
                    dup = 1;
                }
                s = s + 1;
            }

            if dup != 0 {
                p = ms_puts(out, p, "skip|");
                p = ms_put(out, p, rows, f0, f1 - f0);
                p = ms_putc(out, p, 10);
            } else {
                so[sn] = f0;
                sl[sn] = f1 - f0;
                sn = sn + 1;
                p = ms_put(out, p, rows, f0, f1 - f0);

                if start_ok == 0 {
                    p = ms_puts(out, p, "|failed|0|0");
                } else {
                    let dl = spawn + 10000;

                    if stopping != 0 || query >= dl {
                        p = ms_puts(out, p, "|aborted|");
                        p = ms_put_i(out, p, dl);
                        p = ms_puts(out, p, "|0");
                    } else {
                        p = ms_puts(out, p, "|ok|");
                        p = ms_put_i(out, p, dl);
                        p = ms_putc(out, p, 124);
                        p = ms_put_i(out, p, tools);
                        reg = reg + tools;
                    }
                }
                p = ms_putc(out, p, 10);
            }
        }
        t = e + 1;
    }
    p = ms_puts(out, p, "registry=");
    p = ms_put_i(out, p, reg);
    return ms_putc(out, p, 10);
}

// ── constants (657-658, 24-32, 619, 676-678) ───────────────────────────────

export fn ms_limits(out: u8[:]) -> i64 {
    var p = ms_puts(out, 0, "cooldown=5\nwarmup=10\npoll_ms=50\nmax_line=8388608\nerr_tail=4096\ntransport=stdio\n");
    return p;
}
