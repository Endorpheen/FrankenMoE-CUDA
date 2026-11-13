// llama_batch_allocr::init -- entry classification and validation in Goose (APX: P6-07).
//
// Native (llama-batch.cpp:57-98):
//     for each entry: is_tok = id != NULL;  is_emb = has_embd
//         if (!is_tok && !is_emb)                     -> error: neither
//         n_tok_only += is_tok && !is_emb
//         n_embd_only += is_emb && !is_tok
//         n_both     += is_tok &&  is_emb
//     if (n_both > 0 && n_both != n_tok)               -> error: both mixed with others
//     mixed = n_tok_only > 0 && n_embd_only > 0
//     if (mixed && !allow_mixed)                       -> error: model does not allow mixing
//     has_token = n_tok_only > 0 || n_both > 0
//     has_embd  = n_embd_only > 0 || n_both > 0
//
// The counting is trivial; the order of the checks is not. Native reports "neither" as soon as it meets
// such an entry, BEFORE the both-and-n_tok test, so a batch containing both a junk entry and a mixed
// token+embd entry fails with the first reason, not the second. Returning a different code would make an
// error message point at the wrong cause, which is how a real input bug takes an hour instead of a minute.
//
// Ported as a decision returning a code and the six derived flags. Nothing mutates.
extern "fg239b_fail" fn fail(code: i64);
extern "fg239b_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;
extern "fg239b_st_i32" fn st_i32(base: u64, idx: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// classify returns 0 when the batch is valid, or the native error code:
//   1 = entry with neither token nor embedding, 2 = token+embd mixed with other entries,
//   3 = mixed batch not allowed by this model.
// out receives: [n_tok_only, n_embd_only, n_both, mixed, has_token, has_embd]

fn classify(n_tok: i64, is_tok: u64, is_emb: u64, allow_mixed: i64, out: u64) -> i64 {
    ensure(n_tok > 0 as i64, 1900);
    ensure(allow_mixed == 0 as i64 || allow_mixed == 1 as i64, 1901);
    var nto: i64 = 0;
    var neo: i64 = 0;
    var nb: i64 = 0;
    var i: i64 = 0;
    var err: i64 = 0;

    while i < n_tok {
        let t: i64 = ld_i32(is_tok, i);
        let e: i64 = ld_i32(is_emb, i);

        if t == 0 as i64 && e == 0 as i64 {
            // native returns here immediately: this reason wins over any later one
            err = 1 as i64;
            i = n_tok;
        } else {
            if t != 0 as i64 && e == 0 as i64 {
                nto = nto + 1 as i64;
            }

            if e != 0 as i64 && t == 0 as i64 {
                neo = neo + 1 as i64;
            }

            if t != 0 as i64 && e != 0 as i64 {
                nb = nb + 1 as i64;
            }
            i = i + 1 as i64;
        }
    }

    if err != 0 as i64 {
        return err;
    }

    if nb > 0 as i64 && nb != n_tok {
        return 2 as i64;
    }
    var mixed: i64 = 0;

    if nto > 0 as i64 && neo > 0 as i64 {
        mixed = 1 as i64;
    }

    if mixed != 0 as i64 && allow_mixed == 0 as i64 {
        return 3 as i64;
    }
    var has_token: i64 = 0;

    if nto > 0 as i64 || nb > 0 as i64 {
        has_token = 1 as i64;
    }
    var has_embd: i64 = 0;

    if neo > 0 as i64 || nb > 0 as i64 {
        has_embd = 1 as i64;
    }
    st_i32(out, 0 as i64, nto);
    st_i32(out, 1 as i64, neo);
    st_i32(out, 2 as i64, nb);
    st_i32(out, 3 as i64, mixed);
    st_i32(out, 4 as i64, has_token);
    st_i32(out, 5 as i64, has_embd);
    0 as i64
}

fn main() {
    classify(1 as i64, 0 as u64, 8 as u64, 0 as i64, 16 as u64);
    classify(2 as i64, 24 as u64, 32 as u64, 1 as i64, 40 as u64);
}

