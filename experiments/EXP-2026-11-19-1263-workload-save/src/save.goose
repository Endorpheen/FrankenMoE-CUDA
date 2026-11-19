// DeterministicWorkload::save - the writer that produces the format load() reads.
// Byte-exact on purpose: the file is an interchange artefact, so a space that differs is a defect.
// Routes are passed flat, four integers per record followed by its id block:
//   phase base_pos batch_n layer top_k token_n n_ids  then n_ids ids
// ply[] holds n_prompt, n_output, n_routes and the running cursor into routes.
fn wl_put(out: u8[:], at: i64, c: i64) -> i64 {
    if at < out.len {
        out[at] = (c as u8);
    }
    return at + 1;
}

fn wl_put_bytes(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var k: i64 = 0;
    var p: i64 = at;
    while k < s.len {
        p = wl_put(out, p, s[k] as i64);
        k = k + 1;
    }
    return p;
}

// Iterative on purpose: a recursive form needs a declaration this compiler does not accept here,
// and the digits are cheapest written into a scratch buffer and copied back.
fn wl_put_int(out: u8[:], at: i64, v: i64) -> i64 {
    var p: i64 = at;
    var n: i64 = v;
    if n < 0 {
        p = wl_put(out, p, 45);
        n = 0 - n;
    }
    var digits: u8[24] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var used: i64 = 0;
    if n == 0 {
        digits[0] = 48;
        used = 1;
    } else {
        while n > 0 {
            digits[used] = (48 + n - 10 * (n / 10)) as u8;
            n = n / 10;
            used = used + 1;
        }
    }
    var k: i64 = used;
    while k > 0 {
        k = k - 1;
        p = wl_put(out, p, digits[k] as i64);
    }
    return p;
}

fn wl_save(arch: const u8[:], n_layer: i64, top_k: i64, prompt: const i64[:], n_prompt: i64,
           output_tokens: const i64[:], n_output: i64, routes: const i64[:], n_routes: i64,
           out: u8[:], marker: i64) -> i64 {
    if marker != 213 {
        return -1;
    }
    var p: i64 = 0;
    p = wl_put_bytes(out, p, "BMOE_DETERMINISTIC_WORKLOAD 1\n");
    p = wl_put_bytes(out, p, "arch ");
    p = wl_put_bytes(out, p, arch);
    p = wl_put_bytes(out, p, "\nlayers ");
    p = wl_put_int(out, p, n_layer);
    p = wl_put_bytes(out, p, "\ntop_k ");
    p = wl_put_int(out, p, top_k);
    p = wl_put_bytes(out, p, "\nprompt ");
    p = wl_put_int(out, p, n_prompt);
    var i: i64 = 0;
    while i < n_prompt {
        p = wl_put(out, p, 32);
        p = wl_put_int(out, p, prompt[i]);
        i = i + 1;
    }
    p = wl_put_bytes(out, p, "\noutput ");
    p = wl_put_int(out, p, n_output);
    i = 0;
    while i < n_output {
        p = wl_put(out, p, 32);
        p = wl_put_int(out, p, output_tokens[i]);
        i = i + 1;
    }
    p = wl_put_bytes(out, p, "\nroutes ");
    p = wl_put_int(out, p, n_routes);
    p = wl_put_bytes(out, p, "\n");
    var at: i64 = 0;
    var r: i64 = 0;
    while r < n_routes {
        p = wl_put_bytes(out, p, "route");
        var f: i64 = 0;
        while f < 7 {
            p = wl_put(out, p, 32);
            p = wl_put_int(out, p, routes[at + f]);
            f = f + 1;
        }
        at = at + 7;
        let n_ids = routes[at - 1];
        var j: i64 = 0;
        while j < n_ids {
            p = wl_put(out, p, 32);
            p = wl_put_int(out, p, routes[at + j]);
            j = j + 1;
        }
        at = at + n_ids;
        p = wl_put(out, p, 10);
        r = r + 1;
    }
    return p;
}

fn main() {
    let arch: u8[4] = [97, 98, 99, 100];
    let pr: i64[2] = [1, 2];
    let ou: i64[2] = [3, 4];
    let ro: i64[9] = [0, 0, 1, 0, 2, 1, 2, 7, 8];
    let o: u8[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    let r = wl_save(arch, 2, 4, pr, 2, ou, 2, ro, 1, o, 213);
    if r < 0 {
        return;
    }
}
