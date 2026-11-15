// P10-02 S2: off-line decisions of the maintained llama-tokenize tool.
//
// Native source: upstream/llama.cpp/tools/tokenize/tokenize.cpp at the base
// commit 137bc40c. Tokenization itself needs a vocabulary, so this slice owns
// only the decisions around it, all of which run with no model:
//
//   prompt-source precedence and the "must have some prompt" refusal  106-115
//   file prompt is read verbatim, escapes applied locally             117-138
//   stdin prompt is read after the model and escapes applied locally  162-178
//   add_bos = model_wants_add_bos && !--no-bos                        180-182
//   ids mode: "[" then ", "-joined %d then "]\n"                       187-211
//   piece mode: "%6d -> '<piece>'\n" plus the Windows invalid-UTF-8
//   suffix "' (utf-8 decode failure)\n"                                197-206
//   --show-count: "Total number of tokens: %zu\n" after the list       213-215
//
// The prefix is tz_. The native output loop is a transcription oracle in the
// gate (the tool's main cannot be linked without a vocabulary), while the
// gate keeps the same printf formats. Only the characters that reach stdout
// are compared: the Windows console branch is represented by the invalid flag,
// because on POSIX write_utf8_cstr_to_stdout is a plain printf.
//
// Decisions that are deliberately exact:
//   - --stdin wins over -f, and -f wins over -p; the source is chosen once;
//   - -p is already escape-processed by common_params_parse, so the tool does
//     not process it twice; -f and --stdin are processed here when --escape;
//   - a file prompt is not stripped of a trailing newline;
//   - ids are printed as %d (signed) with ", " between them, brackets around;
//   - pieces are right-aligned in six columns, then " -> '", the piece, "'";
//   - the count line follows the whole list and has no leading separator.

fn tz_copy(s: const u8[:], out: u8[:], at0: i64) -> i64 {
    var at: i64 = at0;
    var i: i64 = 0;
    while i < s.len && at < out.len {
        out[at] = s[i];
        at = at + 1;
        i = i + 1;
    }
    return at;
}

fn tz_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

fn tz_put_int(out: u8[:], at0: i64, v0: i64) -> i64 {
    var at: i64 = at0;
    var v: i64 = v0;
    if v < 0 {
        at = tz_putc(out, at, 45);
        if v == (0 - 9223372036854775807 - 1) {
            return tz_copy("9223372036854775808", out, at);
        }
        v = 0 - v;
    }
    if v == 0 {
        return tz_putc(out, at, 48);
    }
    var buf: i64[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    while v > 0 {
        buf[n] = v % 10;
        v = v / 10;
        n = n + 1;
    }
    var i: i64 = n;
    while i > 0 {
        i = i - 1;
        at = tz_putc(out, at, (48 + buf[i]) as u8);
    }
    return at;
}

// "%6d": the sign counts as one of the digits, so -12 needs three spaces.
fn tz_put_w6(out: u8[:], at0: i64, v: i64) -> i64 {
    var digits: i64 = 1;
    var mag: i64 = v;
    if mag < 0 {
        mag = 0 - mag;
    }
    while mag >= 10 {
        mag = mag / 10;
        digits = digits + 1;
    }
    if v < 0 {
        digits = digits + 1;
    }
    var at: i64 = at0;
    var sp: i64 = 6 - digits;
    while sp > 0 {
        at = tz_putc(out, at, 32);
        sp = sp - 1;
    }
    return tz_put_int(out, at, v);
}

// ── prompt source (106-115) ────────────────────────────────────────────────

fn tz_pick_source(stdin_flag: i64, file_present: i64, prompt_present: i64) -> i64 {
    if stdin_flag != 0 {
        return 0;
    }
    if file_present != 0 {
        return 1;
    }
    if prompt_present != 0 {
        return 2;
    }
    return 0 - 1;
}

export fn tz_source(stdin_flag: i64, file_present: i64, prompt_present: i64) -> i64 {
    return tz_pick_source(stdin_flag, file_present, prompt_present);
}

// Source 0 (stdin) and 1 (file) are escape-processed locally when --escape is
// on; source 2 (--prompt) was already processed by common_params_parse.
export fn tz_escape_needed(source: i64, escape_param: i64) -> i64 {
    if escape_param == 0 {
        return 0;
    }
    if source == 0 || source == 1 {
        return 1;
    }
    return 0;
}

export fn tz_add_bos(model_wants_add_bos: i64, no_bos: i64) -> i64 {
    if model_wants_add_bos != 0 && no_bos == 0 {
        return 1;
    }
    return 0;
}

// ── the output loop (187-215) ──────────────────────────────────────────────
//
// tokens[i] is the id, pieces[offs[i] .. offs[i]+lens[i]] its piece. invalid[i]
// is the Windows invalid-UTF-8 flag for the same token. ids selects the %d
// list; count appends the --show-count line.

export fn tz_report(tokens: const i64[:], pieces: const u8[:], offs: const i64[:], lens: const i64[:],
                    invalid: const i64[:], ids: i64, count: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    if ids != 0 {
        at = tz_putc(out, at, 91);
    }
    var i: i64 = 0;
    while i < tokens.len {
        if ids != 0 {
            if i > 0 {
                at = tz_copy(", ", out, at);
            }
            at = tz_put_int(out, at, tokens[i]);
        } else {
            at = tz_put_w6(out, at, tokens[i]);
            at = tz_copy(" -> '", out, at);
            let off: i64 = offs[i];
            let len: i64 = lens[i];
            var k: i64 = 0;
            while k < len && off + k < pieces.len {
                at = tz_putc(out, at, pieces[off + k]);
                k = k + 1;
            }
            if invalid[i] != 0 {
                at = tz_copy("' (utf-8 decode failure)\n", out, at);
            } else {
                at = tz_copy("'\n", out, at);
            }
        }
        i = i + 1;
    }
    if ids != 0 {
        at = tz_copy("]\n", out, at);
    }
    if count != 0 {
        at = tz_copy("Total number of tokens: ", out, at);
        at = tz_put_int(out, at, tokens.len);
        at = tz_putc(out, at, 10);
    }
    return at;
}

export fn tz_fmt_ids(tokens: const i64[:], out: u8[:]) -> i64 {
    var at: i64 = tz_putc(out, 0, 91);
    var i: i64 = 0;
    while i < tokens.len {
        if i > 0 {
            at = tz_copy(", ", out, at);
        }
        at = tz_put_int(out, at, tokens[i]);
        i = i + 1;
    }
    return tz_copy("]\n", out, at);
}

export fn tz_fmt_piece(token: i64, piece: const u8[:], invalid: i64, out: u8[:]) -> i64 {
    var at: i64 = tz_put_w6(out, 0, token);
    at = tz_copy(" -> '", out, at);
    at = tz_copy(piece, out, at);
    if invalid != 0 {
        return tz_copy("' (utf-8 decode failure)\n", out, at);
    }
    return tz_copy("'\n", out, at);
}
