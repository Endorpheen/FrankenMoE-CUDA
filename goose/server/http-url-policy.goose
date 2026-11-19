// HTTP URL and authority policy, ported from common/http.h.
//
// Owned decisions:
//   - common_http_parse_url: scheme must be http/https, userinfo splits on the
//     first ':', the path starts at the first '/', a bracketed IPv6 authority
//     keeps its inner colons, a missing port takes 80/443 from the scheme, and
//     the port text is parsed like std::stoi (leading blanks, an optional sign,
//     at least one digit, int range);
//   - common_http_format_host: an IPv6 literal is bracketed, everything else is
//     copied;
//   - common_http_is_loopback: localhost, ::1, 0.0.0.0, any *.localhost (but not
//     ".localhost" itself) and the whole 127.0.0.0/8 as exactly four numeric
//     octets of at most three digits each;
//   - common_http_proxy_bypass: comma separated NO_PROXY, "*" for everything,
//     an optional leading dot, exact host or subdomain, blank trimming, case
//     folded;
//   - common_http_show_masked_url.
//
// Not owned: httplib clients, TLS, sockets, proxy selection, the free-port
// probe. Exceptions are not ported: parse returns a status class instead, and
// the native message text stays with the caller.
//
// Statuses: 0 ok, 1 no scheme, 2 unsupported scheme, 3 bad IPv6 authority,
// 4 bad port. Offsets and lengths index the input URL. A path offset of -1 means
// the literal "/" that native invents when the URL has no slash.

fn http_eq(s: const u8[:], lit: const u8[:]) -> bool {
    if s.len != lit.len {
        return false;
    }
    var i: i64 = 0;

    while i < s.len {
        if s[i] != lit[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn http_lower(c: u8) -> u8 {
    if c >= 65 && c <= 90 {
        return c + 32;
    }
    return c;
}

// Case-folded comparison, the native form of the lower-case lambda.
fn http_eq_fold(s: const u8[:], lit: const u8[:]) -> bool {
    if s.len != lit.len {
        return false;
    }
    var i: i64 = 0;

    while i < s.len {
        if http_lower(s[i]) != http_lower(lit[i]) {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn http_find_byte(s: const u8[:], pos_from: i64, ch: u8) -> i64 {
    var i: i64 = pos_from;

    while i < s.len {
        if s[i] == ch {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn http_find_seq(s: const u8[:], pos_from: i64, seq: const u8[:]) -> i64 {
    if seq.len == 0 || seq.len > s.len {
        return 0 - 1;
    }
    var i: i64 = pos_from;

    while i + seq.len <= s.len {
        var k: i64 = 0;
        var ok: bool = true;

        while k < seq.len {
            if s[i + k] != seq[k] {
                ok = false;
                k = seq.len;
            } else {
                k = k + 1;
            }
        }
        if ok {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn http_has_colon(s: const u8[:]) -> bool {
    return http_find_byte(s, 0, ':') >= 0;
}

// std::stoi on a slice: blanks, an optional sign, at least one digit, int range.
// Returns 0 on success and writes the value, -1 on any failure.
fn http_stoi(s: const u8[:], pos_from: i64, to: i64, out: i64[:]) -> i64 {
    var i: i64 = pos_from;

    while i < to && (s[i] == 32 || s[i] == 9 || s[i] == 10 || s[i] == 13 || s[i] == 11 || s[i] == 12) {
        i = i + 1;
    }
    var neg: bool = false;

    if i < to && (s[i] == 43 || s[i] == 45) {
        neg = s[i] == 45;
        i = i + 1;
    }
    if i >= to {
        return 0 - 1;
    }
    var v: i64 = 0;
    var digits: i64 = 0;

    // std::stoi stops at the first non-digit and only fails when no digit was
    // consumed; trailing garbage is not an error
    while i < to && s[i] >= 48 && s[i] <= 57 {
        let c = s[i];

        // std::stoi works on int: range check 2147483647 / -2147483648
        v = v * 10 + (c as i64 - 48);
        if neg {
            if v > 2147483648 {
                return 0 - 1;
            }
        } else if v > 2147483647 {
            return 0 - 1;
        }
        digits = digits + 1;
        i = i + 1;
    }
    if digits == 0 {
        return 0 - 1;
    }
    if neg {
        v = 0 - v;
    }
    out[0] = v;
    return 0;
}

fn url_format_host(host: const u8[:], out: u8[:]) -> i64 {
    var p: i64 = 0;

    if http_has_colon(host) {
        out[p] = 91;
        p = p + 1;
    }
    var i: i64 = 0;

    while i < host.len {
        out[p] = host[i];
        p = p + 1;
        i = i + 1;
    }
    if http_has_colon(host) {
        out[p] = 93;
        p = p + 1;
    }
    return p;
}

fn url_is_loopback(host: const u8[:]) -> i64 {
    if http_eq_fold(host, "localhost") || http_eq_fold(host, "::1") || http_eq_fold(host, "0.0.0.0") {
        return 1;
    }
    // more than ".localhost", so the bare suffix is not loopback
    if host.len > 10 {
        let tail = host[host.len - 10 .. host.len];

        if http_eq_fold(tail, ".localhost") {
            return 1;
        }
    }
    var octets: i64[4] = [0, 0, 0, 0];
    var pos: i64 = 0;
    var n: i64 = 0;
    var complete: bool = false;
    var stop: bool = false;

    while n < 4 && pos <= host.len && !stop {
        var dot = http_find_byte(host, pos, 46);

        if dot < 0 {
            dot = host.len;
        }
        if dot == pos || dot - pos > 3 {
            return 0;
        }
        var val: i64 = 0;
        var i: i64 = pos;

        while i < dot {
            if host[i] < 48 || host[i] > 57 {
                return 0;
            }
            val = val * 10 + (host[i] as i64 - 48);
            i = i + 1;
        }
        if val > 255 {
            return 0;
        }
        octets[n] = val;
        n = n + 1;
        if dot == host.len {
            complete = n == 4;
            stop = true;
        } else {
            pos = dot + 1;
        }
    }
    if !complete {
        return 0;
    }
    if octets[0] != 127 {
        return 0;
    }
    return 1;
}

fn url_proxy_bypass(no_proxy: const u8[:], host: const u8[:]) -> i64 {
    var pos: i64 = 0;

    while pos < no_proxy.len {
        var end = http_find_byte(no_proxy, pos, 44);

        if end < 0 {
            end = no_proxy.len;
        }
        var pos_from: i64 = pos;
        var to: i64 = end;

        pos = end + 1;
        while pos_from < to && (no_proxy[pos_from] == 32 || no_proxy[pos_from] == 9) {
            pos_from = pos_from + 1;
        }
        while to > pos_from && (no_proxy[to - 1] == 32 || no_proxy[to - 1] == 9) {
            to = to - 1;
        }
        if pos_from == to {
            continue;
        }
        let entry = no_proxy[pos_from .. to];

        if http_eq_fold(entry, "*") {
            return 1;
        }
        var st: i64 = pos_from;

        if no_proxy[st] == 46 {
            st = st + 1;
        }
        if st == to {
            continue;
        }
        let body = no_proxy[st .. to];

        if http_eq_fold(body, host) {
            return 1;
        }
        if host.len > body.len && host[host.len - body.len - 1] == 46 {
            let tail = host[host.len - body.len .. host.len];

            if http_eq_fold(tail, body) {
                return 1;
            }
        }
    }
    return 0;
}

fn url_parse(
    url: const u8[:],
    off: i64[:],
    len: i64[:],
    port: i64[:]) -> i64 {
    let se = http_find_seq(url, 0, "://");

    if se < 0 {
        return 1;
    }
    let scheme = url[0 .. se];

    if !http_eq(scheme, "http") && !http_eq(scheme, "https") {
        return 2;
    }
    off[0] = 0;
    len[0] = se;
    var rest: i64 = se + 3;
    off[1] = 0;
    len[1] = 0;
    off[2] = 0;
    len[2] = 0;
    let at = http_find_byte(url, rest, 64);

    if at >= 0 {
        let auth_from = rest;
        let auth_to = at;
        let colon = http_find_byte(url, auth_from, 58);

        if colon >= 0 && colon < auth_to {
            off[1] = auth_from;
            len[1] = colon - auth_from;
            off[2] = colon + 1;
            len[2] = auth_to - colon - 1;
        } else {
            off[1] = auth_from;
            len[1] = auth_to - auth_from;
        }
        rest = at + 1;
    }
    let slash = http_find_byte(url, rest, 47);
    var host_from: i64 = rest;
    var host_to: i64 = url.len;
    var path_off: i64 = 0 - 1;
    var path_len: i64 = 1;

    if slash >= 0 {
        host_to = slash;
        path_off = slash;
        path_len = url.len - slash;
    }
    off[4] = path_off;
    len[4] = path_len;
    // authority: host[:port], a bracketed literal keeps its inner colons
    var port_from: i64 = 0 - 1;
    var port_to: i64 = 0 - 1;
    var host_out_from: i64 = host_from;
    var host_out_to: i64 = host_to;

    if host_to > host_from && url[host_from] == 91 {
        let close = http_find_byte(url, host_from + 1, 93);

        if close < 0 || close >= host_to {
            return 3;
        }
        let after = close + 1;

        if after < host_to && url[after] == 58 {
            port_from = after + 1;
            port_to = host_to;
        }
        host_out_from = host_from + 1;
        host_out_to = close;
    } else {
        let colon = http_find_byte(url, host_from, 58);

        if colon >= 0 && colon < host_to {
            port_from = colon + 1;
            port_to = host_to;
            host_out_to = colon;
        }
    }
    off[3] = host_out_from;
    len[3] = host_out_to - host_out_from;
    if port_from >= 0 && port_to > port_from {
        var pv: i64[1] = [0];

        if http_stoi(url, port_from, port_to, pv) < 0 {
            return 4;
        }
        port[0] = pv[0];
    } else if http_eq(scheme, "http") {
        port[0] = 80;
    } else {
        port[0] = 443;
    }
    return 0;
}

fn url_masked(
    url: const u8[:],
    off: i64[:],
    len: i64[:],
    out: u8[:]) -> i64 {
    var p: i64 = 0;
    var i: i64 = 0;

    while i < len[0] {
        out[p] = url[off[0] + i];
        p = p + 1;
        i = i + 1;
    }
    let colon3: const u8[:] = "://";
    i = 0;
    while i < 3 {
        out[p] = colon3[i];
        p = p + 1;
        i = i + 1;
    }
    if len[1] > 0 {
        let mask: const u8[:] = "****:****@";
        i = 0;
        while i < 10 {
            out[p] = mask[i];
            p = p + 1;
            i = i + 1;
        }
    }
    let host = url[off[3] .. off[3] + len[3]];

    p = p + url_format_host(host, out[p ..]);
    if off[4] < 0 {
        out[p] = 47;
        p = p + 1;
    } else {
        i = 0;
        while i < len[4] {
            out[p] = url[off[4] + i];
            p = p + 1;
            i = i + 1;
        }
    }
    return p;
}

// Export wrappers. Goose refuses a call to an export fn from another function,
// so the bodies above are private and the gate calls them directly; the public
// surface stays one export per native entry point.
export fn http_format_host(host: const u8[:], out: u8[:]) -> i64 {
    return url_format_host(host, out);
}

export fn http_is_loopback(host: const u8[:]) -> i64 {
    return url_is_loopback(host);
}

export fn http_proxy_bypass(no_proxy: const u8[:], host: const u8[:]) -> i64 {
    return url_proxy_bypass(no_proxy, host);
}

export fn http_parse_url(url: const u8[:], off: i64[:], len: i64[:], port: i64[:]) -> i64 {
    return url_parse(url, off, len, port);
}

export fn http_masked_url(url: const u8[:], off: i64[:], len: i64[:], out: u8[:]) -> i64 {
    return url_masked(url, off, len, out);
}
