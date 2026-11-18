// Vocab-type and pre-tokenizer dispatch, ported from llama_vocab::impl::load
// in src/llama-vocab.cpp, the block that starts with "determine vocab type".
//
// Owned decisions:
//   - tokenizer.ggml.model selects the vocab type and its default special ids:
//     no_vocab/none, llama, bert (with add_sep), gpt2/hybriddna/whitespace,
//     t5, rwkv, test, plamo2, plamo3, gemma4; anything else is refused;
//   - the BPE family reads tokenizer.ggml.merges: absent is an error unless
//     tokenizer_pre is kimi-k2, a non-array key or a non-string array is an
//     error, and the present entries are counted (the values are "first second"
//     pairs, which the loader, not this decision, parses into ranks);
//   - the t5 arm reads tokenizer.ggml.precompiled_charsmap: a key that is not
//     an array, an element type that is neither int8 nor uint8, a blob shorter
//     than the 4-byte size header, or a size header that reaches the end of the
//     blob are errors;
//   - tokenizer_pre selects the pre-tokenizer for BPE models and brings the
//     add_space_prefix / escape_whitespaces / clean_spaces / ignore_merges /
//     add_bos / add_eos / add_sep adjustments, the normalizer lower-case flag
//     and the chatglm BOS override with it. An empty name falls back to
//     "default", an unknown one is refused;
//   - the non-BPE families take their fixed defaults (SPM adds the space prefix
//     and BOS, WPM adds BOS and SEP, UGM adds EOS, RWKV adds nothing);
//   - tokenizer.ggml.add_prefix and tokenizer.ggml.remove_extra_ws override
//     add_space_prefix and remove_extra_whitespaces afterwards. They are inputs
//     of this port, not decisions.
//
// Not owned: the merges map, the charsmap bytes, the token list/scores and
// attributes, the big-endian swap inside the charsmap blob, and every file
// read. The pre-tokenizer enum values stay native: this port carries names,
// never numbers.
//
// A refused model is reported at the state the native code had reached when it
// threw: the BPE defaults are written before the pre-tokenizer chain runs, so a
// refused pre-tokenizer name leaves them visible, while a refused merges key
// does not.
//
// Statuses: 0 ok, 1 unknown tokenizer, 2 unknown pre-tokenizer, 3 missing
// merges, 4 bad gguf type for a tokenizer key, 5 charsmap smaller than its size
// header, 6 charsmap size header past the end of the blob.

fn vt_eq(a: const u8[:], b: const u8[:]) -> bool {
    return a == b;
}

fn vt_w(out: u8[:], at: i64, lit: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < lit.len {
        out[at + i] = lit[i];
        i = i + 1;
    }
    return at + lit.len;
}

// Non-negative decimal.
fn vt_wnum(out: u8[:], at0: i64, v: i64) -> i64 {
    var at: i64 = at0;
    if v == 0 {
        out[at] = 48;
        return at + 1;
    }
    var buf: i64[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    var x: i64 = v;
    while x > 0 {
        buf[n] = x % 10;
        x = x / 10;
        n = n + 1;
    }
    var i: i64 = n;
    while i > 0 {
        i = i - 1;
        out[at] = (48 + buf[i]) as u8;
        at = at + 1;
    }
    return at;
}

// Signed decimal for token ids, LLAMA_TOKEN_NULL included.
fn vt_wint(out: u8[:], at: i64, v: i64) -> i64 {
    if v < 0 {
        out[at] = 45;
        return vt_wnum(out, at + 1, 0 - v);
    }
    return vt_wnum(out, at, v);
}

fn vt_flag(out: u8[:], at0: i64, comma: bool, v: i64) -> i64 {
    var at: i64 = at0;
    if comma {
        out[at] = 44;
        at = at + 1;
    }
    out[at] = (48 + v) as u8;
    return at + 1;
}

// One canonical record: status, type name, pre name, six ids, seven flags,
// merge count, message. The gate compares these bytes with the native record.
fn vt_write(
    out: u8[:],
    st: i64,
    tname: const u8[:],
    pname: const u8[:],
    bos: i64, eos: i64, unk: i64, sep: i64, pd: i64, mask: i64,
    prefix: i64, esc: i64, clean: i64, ign: i64,
    abos: i64, aeos: i64, asep: i64,
    lo: i64,
    nm: i64,
    err: const u8[:],
    earg: const u8[:],
    etail: const u8[:]
) -> i64 {
    var at: i64 = 0;
    at = vt_w(out, at, "st=");
    at = vt_wint(out, at, st);
    at = vt_w(out, at, " type=");
    at = vt_w(out, at, tname);
    at = vt_w(out, at, " pre=");
    at = vt_w(out, at, pname);
    at = vt_w(out, at, " ids=");
    at = vt_wint(out, at, bos);
    at = vt_w(out, at, ",");
    at = vt_wint(out, at, eos);
    at = vt_w(out, at, ",");
    at = vt_wint(out, at, unk);
    at = vt_w(out, at, ",");
    at = vt_wint(out, at, sep);
    at = vt_w(out, at, ",");
    at = vt_wint(out, at, pd);
    at = vt_w(out, at, ",");
    at = vt_wint(out, at, mask);
    at = vt_w(out, at, " fl=");
    at = vt_flag(out, at, false, prefix);
    at = vt_flag(out, at, true, esc);
    at = vt_flag(out, at, true, clean);
    at = vt_flag(out, at, true, ign);
    at = vt_flag(out, at, true, abos);
    at = vt_flag(out, at, true, aeos);
    at = vt_flag(out, at, true, asep);
    at = vt_w(out, at, " lo=");
    at = vt_wint(out, at, lo);
    at = vt_w(out, at, " nm=");
    at = vt_wnum(out, at, nm);
    at = vt_w(out, at, " err=");
    at = vt_w(out, at, err);
    at = vt_w(out, at, earg);
    at = vt_w(out, at, etail);
    return at;
}

export fn vt_dispatch(
    model: const u8[:],
    pre_in: const u8[:],
    has_merges: i64,
    merges_kv_is_array: i64,
    merges_arr_is_string: i64,
    n_merges: i64,
    charsmap_present: i64,
    charsmap_kv_is_array: i64,
    charsmap_is_i8_or_u8: i64,
    charsmap_len: i64,
    charsmap_blob: i64,
    out: u8[:]
) -> i64 {
    var pre: const u8[:] = pre_in;

    var tname: const u8[:] = "SPM";
    var pname: const u8[:] = "DEFAULT";

    var bos: i64 = 1;
    var eos: i64 = 2;
    var unk: i64 = 0;
    var sep: i64 = 0 - 1;
    var pd: i64 = 0 - 1;
    var mask: i64 = 0 - 1;

    var add_space_prefix: i64 = 0;
    var add_bos: i64 = 0;
    var add_eos: i64 = 0;
    var add_sep: i64 = 0;
    var ignore_merges: i64 = 0;
    var clean_spaces: i64 = 0;
    var escape_whitespaces: i64 = 1;
    var lower: i64 = 1; // normalizer_opts.lowercase

    var nm: i64 = 0;
    var st: i64 = 0;
    var err: const u8[:] = "";
    var earg: const u8[:] = "";
    var etail: const u8[:] = "";

    if vt_eq(model, "no_vocab") || vt_eq(model, "none") {
        tname = "NONE";
        bos = 0 - 1;
        eos = 0 - 1;
        unk = 0 - 1;
        sep = 0 - 1;
        pd = 0 - 1;
        mask = 0 - 1;
    } else if vt_eq(model, "llama") {
        tname = "SPM";
        bos = 1;
        eos = 2;
        unk = 0;
        sep = 0 - 1;
        pd = 0 - 1;
        mask = 0 - 1;
    } else if vt_eq(model, "bert") {
        tname = "WPM";
        bos = 101;
        eos = 0 - 1;
        unk = 100;
        sep = 102;
        pd = 0;
        mask = 103;
        add_sep = 1;
    } else if vt_eq(model, "gpt2") || vt_eq(model, "hybriddna") || vt_eq(model, "whitespace") {
        tname = "BPE";

        // The merges are read before the special ids are written, so a refused
        // merges key leaves the member-initialized ids in place.
        if has_merges == 0 {
            // Kimi-K2 runs without traditional merges.
            if !vt_eq(pre, "kimi-k2") {
                st = 3;
                err = "cannot find tokenizer merges in model file";
            }
        } else if merges_kv_is_array == 0 || merges_arr_is_string == 0 {
            st = 4;
            err = "invalid gguf type for tokenizer.ggml.merges";
        } else {
            nm = n_merges;
        }

        if st == 0 {
            bos = 11;
            eos = 11;
            unk = 0 - 1;
            sep = 0 - 1;
            pd = 0 - 1;
            mask = 0 - 1;
        }
    } else if vt_eq(model, "t5") {
        tname = "UGM";
        bos = 0 - 1;
        eos = 1;
        unk = 2;
        sep = 0 - 1;
        pd = 0;
        mask = 0 - 1;

        if charsmap_present != 0 {
            if charsmap_kv_is_array == 0 || charsmap_is_i8_or_u8 == 0 {
                st = 4;
                err = "invalid gguf type for tokenizer.ggml.precompiled_charsmap";
            } else if charsmap_len < 4 {
                st = 5;
                err = "precompiled_charsmap too small for xcda_blob_size header!";
            } else if charsmap_blob + 4 >= charsmap_len {
                st = 6;
                err = "Index out of array bounds in precompiled charsmap!";
            }
        }
    } else if vt_eq(model, "rwkv") {
        tname = "RWKV";
        bos = 0 - 1;
        eos = 0 - 1;
        unk = 0 - 1;
        sep = 0 - 1;
        pd = 0 - 1;
        mask = 0 - 1;
    } else if vt_eq(model, "test") {
        tname = "TEST";
        bos = 0 - 1;
        eos = 0 - 1;
        unk = 0 - 1;
        sep = 0 - 1;
        pd = 0 - 1;
        mask = 0 - 1;
    } else if vt_eq(model, "plamo2") || vt_eq(model, "plamo3") {
        if vt_eq(model, "plamo2") {
            tname = "PLAMO2";
        } else {
            tname = "PLAMO3";
        }
        bos = 1;
        eos = 2;
        unk = 0;
        sep = 0 - 1;
        pd = 3;
        mask = 0 - 1;
    } else if vt_eq(model, "gemma4") {
        tname = "BPE";

        if has_merges == 0 {
            st = 3;
            err = "cannot find tokenizer merges in model file";
        } else if merges_kv_is_array == 0 || merges_arr_is_string == 0 {
            st = 4;
            err = "invalid gguf type for tokenizer.ggml.merges";
        } else {
            nm = n_merges;
        }

        // Same order as native: merges, then the ids, then the forced name.
        if st == 0 {
            bos = 0 - 1;
            eos = 0 - 1;
            unk = 0 - 1;
            sep = 0 - 1;
            pd = 0 - 1;
            mask = 0 - 1;
            pre = "gemma4";
        }
    } else {
        st = 1;
        err = "unknown tokenizer: '";
        earg = model;
        etail = "'";
    }

    if st == 0 {
        if vt_eq(tname, "BPE") {
            // BPE models default to no space prefix, literal whitespace and
            // clean_up_tokenization_spaces on; the pre-tokenizer refines it.
            add_space_prefix = 0;
            escape_whitespaces = 0;
            clean_spaces = 1;

            if pre.len == 0 {
                pname = "DEFAULT";
            } else if vt_eq(pre, "flint") {
                pname = "FLINT";
                clean_spaces = 0;
            } else if vt_eq(pre, "default") {
                pname = "DEFAULT";
            } else if vt_eq(pre, "minicpm5") {
                pname = "MINICPM5";
                ignore_merges = 1;
            } else if vt_eq(pre, "llama3") || vt_eq(pre, "llama-v3") || vt_eq(pre, "llama-bpe") || vt_eq(pre, "falcon3") || vt_eq(pre, "falcon-h1") || vt_eq(pre, "pixtral") || vt_eq(pre, "midm-2.0") || vt_eq(pre, "lfm2") || vt_eq(pre, "jina-v5-nano") {
                pname = "LLAMA3";
                ignore_merges = 1;
                add_bos = 1;
            } else if vt_eq(pre, "deepseek-llm") {
                pname = "DEEPSEEK_LLM";
                clean_spaces = 0;
            } else if vt_eq(pre, "deepseek-coder") {
                pname = "DEEPSEEK_CODER";
                clean_spaces = 0;
            } else if vt_eq(pre, "deepseek-v3") {
                pname = "DEEPSEEK3_LLM";
                clean_spaces = 0;
            } else if vt_eq(pre, "spark2_5") {
                pname = "SPARK2_5";
                clean_spaces = 0;
            } else if vt_eq(pre, "youtu") {
                pname = "YOUTU";
                clean_spaces = 0;
                ignore_merges = 1;
            } else if vt_eq(pre, "falcon") {
                pname = "FALCON";
            } else if vt_eq(pre, "mpt") {
                pname = "MPT";
            } else if vt_eq(pre, "starcoder") {
                pname = "STARCODER";
            } else if vt_eq(pre, "gpt-2") || vt_eq(pre, "phi-2") || vt_eq(pre, "jina-es") || vt_eq(pre, "jina-de") || vt_eq(pre, "gigachat") || vt_eq(pre, "jina-v2-es") || vt_eq(pre, "jina-v2-de") || vt_eq(pre, "a.x-4.0") || vt_eq(pre, "mellum") || vt_eq(pre, "modern-bert") {
                pname = "GPT2";
            } else if vt_eq(pre, "jais-2") {
                pname = "JAIS2";
            } else if vt_eq(pre, "gemma4") || vt_eq(pre, "granite-embed-multi-311m") {
                pname = "GEMMA4";
                escape_whitespaces = 1;
            } else if vt_eq(pre, "mmbert") {
                pname = "MMBERT";
                escape_whitespaces = 1;
            } else if vt_eq(pre, "sarvam-moe") {
                pname = "SARVAM_MOE";
                escape_whitespaces = 1;
                clean_spaces = 0;
            } else if vt_eq(pre, "jina-v1-en") || vt_eq(pre, "jina-v2-code") || vt_eq(pre, "roberta-bpe") {
                pname = "GPT2";
                add_sep = 1;
            } else if vt_eq(pre, "whitespace") {
                pname = "WHITESPACE";
                lower = 0;
            } else if vt_eq(pre, "refact") {
                pname = "REFACT";
            } else if vt_eq(pre, "command-r") {
                pname = "COMMAND_R";
                clean_spaces = 0;
            } else if vt_eq(pre, "qwen2") || vt_eq(pre, "deepseek-r1-qwen") || vt_eq(pre, "kormo") || vt_eq(pre, "f2llmv2") {
                pname = "QWEN2";
                clean_spaces = 0;
            } else if vt_eq(pre, "qwen35") {
                pname = "QWEN35";
                clean_spaces = 0;
            } else if vt_eq(pre, "stablelm2") {
                pname = "STABLELM2";
            } else if vt_eq(pre, "olmo") {
                pname = "OLMO";
            } else if vt_eq(pre, "dbrx") {
                pname = "DBRX";
            } else if vt_eq(pre, "smaug-bpe") {
                pname = "SMAUG";
            } else if vt_eq(pre, "poro-chat") {
                pname = "PORO";
                clean_spaces = 0;
            } else if vt_eq(pre, "chatglm-bpe") {
                pname = "CHATGLM4";
                bos = 0 - 1;
            } else if vt_eq(pre, "glm4") || vt_eq(pre, "glm5") {
                pname = "CHATGLM4";
                bos = 0 - 1;
                ignore_merges = 1;
            } else if vt_eq(pre, "viking") {
                pname = "VIKING";
                clean_spaces = 0;
            } else if vt_eq(pre, "jais") {
                pname = "JAIS";
            } else if vt_eq(pre, "tekken") {
                pname = "TEKKEN";
                clean_spaces = 0;
                ignore_merges = 1;
                add_bos = 1;
            } else if vt_eq(pre, "smollm") {
                pname = "SMOLLM";
                clean_spaces = 0;
            } else if vt_eq(pre, "codeshell") {
                pname = "CODESHELL";
            } else if vt_eq(pre, "bloom") {
                pname = "BLOOM";
            } else if vt_eq(pre, "gpt3-finnish") {
                pname = "GPT3_FINNISH";
            } else if vt_eq(pre, "exaone") {
                pname = "EXAONE";
            } else if vt_eq(pre, "exaone4") {
                pname = "GPT2";
            } else if vt_eq(pre, "exaone-moe") {
                pname = "EXAONE_MOE";
            } else if vt_eq(pre, "chameleon") {
                pname = "CHAMELEON";
                add_bos = 1;
                clean_spaces = 0;
            } else if vt_eq(pre, "minerva-7b") {
                pname = "MINERVA";
            } else if vt_eq(pre, "megrez") {
                pname = "QWEN2";
            } else if vt_eq(pre, "gpt-4o") || vt_eq(pre, "llama4") || vt_eq(pre, "kanana2") || vt_eq(pre, "talkie") {
                pname = "GPT4O";
                clean_spaces = 0;
            } else if vt_eq(pre, "granite-embed-multi-97m") {
                pname = "GRANITE_EMB_MULTI";
                clean_spaces = 0;
                ignore_merges = 1;
            } else if vt_eq(pre, "tiny_aya") || vt_eq(pre, "cohere2moe") {
                pname = "TINY_AYA";
                clean_spaces = 0;
            } else if vt_eq(pre, "superbpe") {
                pname = "SUPERBPE";
                clean_spaces = 0;
            } else if vt_eq(pre, "trillion") {
                pname = "TRILLION";
                clean_spaces = 0;
            } else if vt_eq(pre, "granite-docling") {
                pname = "GRANITE_DOCLING";
                clean_spaces = 0;
            } else if vt_eq(pre, "bailingmoe") || vt_eq(pre, "bailingmoe2") || vt_eq(pre, "llada-moe") {
                pname = "BAILINGMOE";
                clean_spaces = 0;
            } else if vt_eq(pre, "seed-coder") {
                pname = "SEED_CODER";
                clean_spaces = 0;
            } else if vt_eq(pre, "hunyuan") {
                pname = "HUNYUAN";
                clean_spaces = 0;
            } else if vt_eq(pre, "hunyuan-dense") {
                pname = "HUNYUAN_DENSE";
                clean_spaces = 0;
            } else if vt_eq(pre, "hy_v4") {
                pname = "HY_V4";
                clean_spaces = 0;
            } else if vt_eq(pre, "joyai-llm") {
                pname = "JOYAI_LLM";
                clean_spaces = 0;
            } else if vt_eq(pre, "kimi-k2") {
                pname = "KIMI_K2";
                clean_spaces = 0;
            } else if vt_eq(pre, "ufakzeka") {
                pname = "UFAKZEKA";
                clean_spaces = 0;
            } else if vt_eq(pre, "grok-2") {
                pname = "GROK_2";
                clean_spaces = 0;
            } else if vt_eq(pre, "afmoe") {
                pname = "AFMOE";
                clean_spaces = 0;
            } else if vt_eq(pre, "laguna") {
                pname = "LAGUNA";
                clean_spaces = 0;
            } else if vt_eq(pre, "minimax-m2") {
                pname = "MINIMAX_M2";
                clean_spaces = 0;
            } else if vt_eq(pre, "solar-open") {
                pname = "SOLAR_OPEN";
                clean_spaces = 0;
            } else if vt_eq(pre, "mellum2") {
                pname = "MELLUM2";
            } else if vt_eq(pre, "k2-horizon") {
                pname = "K2_HORIZON";
                clean_spaces = 0;
            } else {
                st = 2;
                err = "unknown pre-tokenizer type: '";
                earg = pre;
                etail = "'";
            }

        } else if vt_eq(tname, "SPM") {
            pname = "DEFAULT";
            add_space_prefix = 1;
            clean_spaces = 0;
            add_bos = 1;
            add_eos = 0;
        } else if vt_eq(tname, "WPM") {
            pname = "DEFAULT";
            add_space_prefix = 0;
            clean_spaces = 1;
            add_bos = 1;
            add_eos = 0;
            add_sep = 1;
        } else if vt_eq(tname, "UGM") {
            pname = "DEFAULT";
            add_bos = 0;
            add_eos = 1;
        } else if vt_eq(tname, "RWKV") {
            pname = "DEFAULT";
            add_space_prefix = 0;
            clean_spaces = 0;
            add_bos = 0;
            add_eos = 0;
        } else {
            pname = "DEFAULT";
        }
    }

    return vt_write(out, st, tname, pname,
        bos, eos, unk, sep, pd, mask,
        add_space_prefix, escape_whitespaces, clean_spaces, ignore_merges,
        add_bos, add_eos, add_sep, lower, nm, err, earg, etail);
}
