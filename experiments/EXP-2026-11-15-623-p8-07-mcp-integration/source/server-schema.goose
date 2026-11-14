// Request-schema field policy, ported from tools/server/server-schema.cpp.
//
// Owned decisions:
//   - the 62-entry completion schema in native order (row 62 is the
//     stream_options/include_usage subfield): primary name, alias order, value
//     kind, numeric type, inclusive limits, hard-limit flag, doc string;
//   - the task_params baseline and which values are copied from common_params
//     (sampling as a whole, n_keep, n_predict, n_cache_reuse, cache_prompt,
//     antiprompt, sse_ping_interval, reasoning_format, verbose from verbosity);
//   - the eval rules: null means absent, primary name before aliases,
//     clamp for soft limits, throw for hard limits, boolean/json/string
//     dispatch, and the nested object rule;
//   - handler semantics: response_fields, lora id set, dry_sequence_breakers,
//     json_schema/grammar selection and grammar type, reasoning_format names,
//     continue_final_message, echo, ignore_eos with the EOG bias, stop with
//     the CLI fallback, samplers by name or char; dry_base below 1.0;
//   - the TTS schema and its baseline (repeat_penalty 1.05, n_predict -1,
//     top_k 0, top_p 0, seed from the base);
//   - every error text: hard-limit range messages and the handler-owned ones.
//
// Not owned:
//   - JSON parsing and the nlohmann conversions: the gate driver supplies a
//     tagged value (kind, int, float, text) and this file decides what the
//     schema does with it;
//   - json_schema_to_grammar: only the selection and the grammar type are here,
//     the grammar text is D1;
//   - preserved_tokens, grammar_triggers, reasoning_budget_start_tag,
//     reasoning_budget_end_tags, reasoning_budget_message and logit_bias are
//     token-level handlers that assert a llama_vocab natively; the table rows
//     and their defaults are carried, the handlers are not run (D6);
//   - lora scale parsing per entry: the driver supplies element ids, this file
//     owns the array check and the resulting id-set size.
//
// Row layout: one row per line in SC_NAMES/SC_DESC; a line lists the primary name
// first and the aliases after it, comma separated, in add_alias() order.

// ---------------------------------------------------------------- tables ---

let SC_NAMES = "verbose\ntimings_per_token\nstream\nstream_options\ncache_prompt\nreturn_tokens\nreturn_progress\nsse_ping_interval\nn_predict,max_completion_tokens,max_tokens\nn_indent\nn_keep\nn_discard\nn_cmpl,n\nn_cache_reuse\nt_max_predict_ms\nresponse_fields\ntop_k\ntop_p\nmin_p\ntop_n_sigma\nxtc_probability\nxtc_threshold\ntypical_p\ntemperature\ndynatemp_range\ndynatemp_exponent\nrepeat_last_n\nrepeat_penalty\nfrequency_penalty\npresence_penalty\ndry_multiplier\ndry_base\ndry_allowed_length\ndry_penalty_last_n\nmirostat\nmirostat_tau\nmirostat_eta\nadaptive_target\nadaptive_decay\nseed\nn_probs,logprobs\nmin_keep\nbackend_sampling\npost_sampling_probs\nlora\ndry_sequence_breakers\njson_schema,grammar\ngrammar_lazy\nreasoning_format\ncontinue_final_message\necho\npreserved_tokens\ngrammar_triggers\nreasoning_control\nreasoning_budget_tokens\nreasoning_budget_start_tag\nreasoning_budget_end_tags,reasoning_budget_end_tag\nreasoning_budget_message\nlogit_bias\nignore_eos\nstop\nsamplers\ninclude_usage";

let SC_DESC = "Include __verbose field in the response with additional debug information\nInclude prompt processing and text generation speed information in each response\nAllows receiving each predicted token in real-time instead of waiting for the completion to finish\nAdditional options for streaming responses\nRe-use KV cache from a previous request if possible. This way the common prefix does not have to be re-processed, only the suffix that differs between the requests\nReturn the raw generated token ids in the `tokens` field\nInclude prompt processing progress events in stream mode\nInterval in seconds between SSE comment pings emitted while the stream stays silent, -1 disables pings\nSet the maximum number of tokens to predict. When 0, no tokens will be generated but the prompt is evaluated into the cache\nSpecify the minimum line indentation for the generated text in number of whitespace characters. Useful for code completion tasks\nSpecify the number of tokens from the initial prompt to retain when context size is exceeded. Use -1 to retain all tokens from the prompt\nNumber of tokens after n_keep that may be discarded when shifting context (0 = half context)\nNumber of completions to generate. If the input has multiple prompts, total outputs will be N prompts times n_cmpl\nMin chunk size to attempt reusing from the cache via KV shifting. See --cache-reuse arg\nSet a time limit in milliseconds for the prediction phase. The timeout triggers if generation exceeds this time (measured since the first token) and a newline has been generated. Useful for FIM applications\nA list of response fields to return. Missing fields are omitted without error. Fields with a slash are unnested (e.g. generation_settings/n_predict moves n_predict to the root)\nLimit the next token selection to the K most probable tokens (0 = disabled)\nLimit the next token selection to a subset of tokens with cumulative probability above threshold P (1.0 = disabled)\nThe minimum probability for a token to be considered, relative to the probability of the most likely token (0 = disabled)\nKeep tokens within n standard deviations of the top token logit (< 0 = disabled)\nSet the chance for token removal via XTC sampler (0 = disabled)\nSet a minimum probability threshold for tokens to be removed via XTC sampler (> 0.5 disables XTC)\nEnable locally typical sampling with parameter p (1.0 = disabled)\nAdjust the randomness of the generated text (0 = greedy)\nDynamic temperature range. The final temperature will be in [temperature - range, temperature + range] (0 = disabled)\nDynamic temperature exponent, controls how entropy maps to temperature\nLast n tokens to consider for penalizing repetition (0 = disabled)\nControl the repetition of token sequences in the generated text (1.0 = disabled)\nRepeat alpha frequency penalty (0 = disabled)\nRepeat alpha presence penalty (0 = disabled)\nSet the DRY (Don't Repeat Yourself) repetition penalty multiplier (0 = disabled)\nSet the DRY repetition penalty base value (must be >= 1.0, any values < 1.0 will be replaced with the default value)\nTokens that extend repetition beyond this length receive exponentially increasing penalty: multiplier * base ^ (sequence_length - allowed_length)\nHow many tokens to scan for repetitions (0 = disabled)\nEnable Mirostat sampling, controlling perplexity during text generation (0 = disabled, 1 = Mirostat, 2 = Mirostat 2.0)\nSet the Mirostat target entropy, parameter tau\nSet the Mirostat learning rate, parameter eta\nAdaptive sampling target entropy (valid range 0.0 to 1.0; negative = disabled)\nEMA decay for adaptive sampling; history approximates 1/(1-decay) tokens\nSet the random number generator (RNG) seed (-1 = random)\nIf greater than 0, output the probabilities of top N tokens for each generated token\nIf greater than 0, force samplers to return at least N possible tokens\nUse backend sampling instead of llama.cpp sampling\nReturn probabilities of top n_probs tokens after applying the sampling chain\nA list of LoRA adapters to apply to this request. Each entry must have `id` and `scale` fields. Adapters not listed default to scale 0.0\nSpecify an array of sequence breakers for DRY sampling. Only a JSON array of strings is accepted\nSet a JSON schema (json_schema) or GBNF grammar string (grammar) for constrained generation. json_schema takes precedence if both are provided\nWhether to apply grammar constraints lazily, only when triggered (instead of at every step)\nReasoning format for chain-of-thought models\nWhether to continue the final message of the chat template\nWhether to include the continued assistant message in the output\nList of token strings that must not be split during tokenization\nList of strings or patterns that trigger grammar-constrained generation\nCreate the budget sampler on demand so reasoning can be ended at runtime\nNumber of tokens in the reasoning budget (-1 = disabled)\nToken string marking the start of the reasoning budget section\nToken strings marking the end of the reasoning budget section; the first is forced when the budget expires\nMessage to prepend to the reasoning budget end tag when forcing it\nModify the likelihood of specific tokens. Accepts an array of [token, bias] pairs or an object mapping token to bias. Use false as bias to ban a token\nIgnore the end-of-sequence token and continue generating\nSpecify stopping strings. Generation stops when one is produced, and the string is not included in the output\nThe order in which samplers are applied. An array of sampler type names, or a single string of sampler chars\nWhether to include usage information in the stream";

// kind: 0 num, 1 str, 2 bool, 3 json, 4 nested (row 3 only).
let SC_KIND = [2, 2, 2, 4, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 3, 3, 3, 2, 1, 3, 3, 3, 3, 2, 0, 1, 3, 1, 3, 2, 3, 3, 2];
let SC_TYPE = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 3, 3, 3, 3, 3, 0, 0, 0, 3, 3, 3, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
let SC_ILIM = [0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 2, 3, 2, 4, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 6, 0, 0, 0, 0, 5, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0];
let SC_FLIM = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
let SC_HARD = [0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0];
let SC_HAND = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 0, 5, 6, 7, 8, 9, 0, 0, 10, 11, 12, 13, 16, 14, 15, 0];

// Integer limit pairs, inclusive (sc_ilim); 3 takes n_parallel from the base.
let SC_ILIM_COUNT = 7;

// Float limit pairs, inclusive (sc_flim).  Pair 0 is numeric_limits<float>
// lowest()/max() natively; the port uses -inf/+inf, which no f32 request value
// can cross, so the clamping outcome is identical.
let SC_FLIM_COUNT = 5;

// The DRY sequence breakers that common_params ships with, joined by 0xFF.
let SC_BRK = "\x0a\xff\x3a\xff\x22\xff\x2a\xff";

let TTS_NAMES = "top_k\ntop_p\nseed\nn_predict,max_tokens\nrepeat_penalty\nstream";
let TTS_DESC = "Top-k for the acoustic code predictor, 0 to use the model default\nTop-p for the acoustic code predictor, 0.0 to use the model default\nRNG seed for the backbone sampler and the codec/vocoder (-1 = random)\nMax number of audio frames to generate\nRepetition penalty applied to the backbone sampler over the whole generation\nStream the audio as it becomes available instead of waiting for the full generation";
let TTS_KIND = [0, 0, 0, 0, 0, 2];
let TTS_TYPE = [0, 3, 2, 0, 3, 0];
let TTS_ILIM = [2, 0, 5, 1, 0, 0];
let TTS_FLIM = [0, 1, 0, 0, 0, 0];
let TTS_HARD = [0, 0, 0, 1, 0, 0];

// --------------------------------------------------------- byte helpers ---

fn sc_copy(dst: u8[:], at: i64, s: const u8[:], from: i64, n: i64) -> i64 {
    var i: i64 = 0;

    while i < n {
        dst[at + i] = s[from + i];
        i = i + 1;
    }
    return at + n;
}

fn sc_puts(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return sc_copy(dst, at, s, 0, s.len);
}

fn sc_put_i(dst: u8[:], at: i64, v: i64) -> i64 {
    var p = at;
    var x = v;

    if x < 0 {
        dst[p] = 45;
        p = p + 1;
        x = 0 - x;
    }
    if x == 0 {
        dst[p] = 48;
        return p + 1;
    }
    var buf: u8[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;

    while x > 0 {
        buf[n] = (48 + (x % 10)) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;

    while i > 0 {
        i = i - 1;
        dst[p] = buf[i];
        p = p + 1;
    }
    return p;
}

fn sc_put_hex8(dst: u8[:], at: i64, v: i64) -> i64 {
    var p = at;
    var i: i64 = 8;

    while i > 0 {
        i = i - 1;
        let nib = (v >> (i * 4)) & 15;
        if nib < 10 {
            dst[p] = (48 + nib) as u8;
        } else {
            dst[p] = (87 + nib) as u8;
        }
        p = p + 1;
    }
    return p;
}

// The shared float rendering: the same fixed six decimals std::to_string uses,
// with the two sentinels the tables carry.
fn sc_put_f32(dst: u8[:], at: i64, v: f32) -> i64 {
    var p = at;

    if v != v {
        return sc_puts(dst, p, "nan");
    }
    if v > 3.0e38 {
        return sc_puts(dst, p, "inf");
    }
    if v < -3.0e38 {
        return sc_puts(dst, p, "-inf");
    }
    if v > 1.0e30 {
        return sc_puts(dst, p, "fmax");
    }
    if v < -1.0e30 {
        return sc_puts(dst, p, "-fmax");
    }
    var a: f64 = v;

    if a < 0.0 {
        dst[p] = 45;
        p = p + 1;
        a = 0.0 - a;
    }
    var ip: i64 = a as i64;
    var fp: i64 = ((a - ip) * 1000000.0 + 0.5) as i64;

    if fp >= 1000000 {
        ip = ip + 1;
        fp = fp - 1000000;
    }
    p = sc_put_i(dst, p, ip);
    dst[p] = 46;
    p = p + 1;
    var div: i64 = 100000;

    while div > 0 {
        dst[p] = (48 + ((fp / div) % 10)) as u8;
        p = p + 1;
        div = div / 10;
    }
    return p;
}

fn sc_fnv(s: const u8[:]) -> i64 {
    var h: i64 = 2166136261;
    var i: i64 = 0;

    while i < s.len {
        h = (h ^ s[i]) % 4294967296;
        h = (h * 16777619) % 4294967296;
        i = i + 1;
    }
    return h;
}

fn sc_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn sc_lower(c: u8) -> u8 {
    if c >= 65 && c <= 90 {
        return c + 32;
    }
    return c;
}

// Case-insensitive compare against a lowercase literal, the sampler name rule.
fn sc_eq_lower(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if sc_lower(a[i]) != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn sc_line(blob: const u8[:], idx: i64) -> const u8[:] {
    var start: i64 = 0;
    var k: i64 = 0;
    var at: i64 = 0;

    while at <= blob.len {
        if at == blob.len || blob[at] == 10 {
            if k == idx {
                return blob[start .. at];
            }
            k = k + 1;
            start = at + 1;
        }
        at = at + 1;
    }
    return blob[0 .. 0];
}

fn sc_names(row: i64) -> const u8[:] {
    return sc_line(SC_NAMES, row);
}

fn sc_desc(row: i64) -> const u8[:] {
    return sc_line(SC_DESC, row);
}

fn sc_tname(row: i64) -> const u8[:] {
    return sc_line(TTS_NAMES, row);
}

fn sc_tdesc(row: i64) -> const u8[:] {
    return sc_line(TTS_DESC, row);
}

fn sc_has_name(row: i64, name: const u8[:]) -> bool {
    let line = sc_names(row);
    var start: i64 = 0;
    var i: i64 = 0;

    while i <= line.len {
        if i == line.len || line[i] == 44 {
            if i - start == name.len {
                var ok: bool = true;
                var k: i64 = 0;

                while k < name.len {
                    if line[start + k] != name[k] {
                        ok = false;
                        k = name.len;
                    } else {
                        k = k + 1;
                    }
                }
                if ok {
                    return true;
                }
            }
            start = i + 1;
        }
        i = i + 1;
    }
    return false;
}

// 1-based position of a key inside a row's name list, 0 when absent.
fn sc_alias_pos(row: i64, key: const u8[:]) -> i64 {
    let line = sc_names(row);
    var start: i64 = 0;
    var pos: i64 = 1;
    var i: i64 = 0;

    while i <= line.len {
        if i == line.len || line[i] == 44 {
            if i - start == key.len {
                var ok: bool = true;
                var k: i64 = 0;

                while k < key.len {
                    if line[start + k] != key[k] {
                        ok = false;
                        k = key.len;
                    } else {
                        k = k + 1;
                    }
                }
                if ok {
                    return pos;
                }
            }
            pos = pos + 1;
            start = i + 1;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// TTS row for a request key, aliases included.
fn sc_tfind(name: const u8[:]) -> i64 {
    var r: i64 = 0;

    while r < 6 {
        let line = sc_tname(r);
        var start: i64 = 0;
        var i: i64 = 0;

        while i <= line.len {
            if i == line.len || line[i] == 44 {
                if i - start == name.len {
                    var ok: bool = true;
                    var k: i64 = 0;

                    while k < name.len {
                        if line[start + k] != name[k] {
                            ok = false;
                            k = name.len;
                        } else {
                            k = k + 1;
                        }
                    }
                    if ok {
                        return r;
                    }
                }
                start = i + 1;
            }
            i = i + 1;
        }
        r = r + 1;
    }
    return 0 - 1;
}

// Row for a request key, aliases included; -1 when the key is not in the schema.
fn sc_find(name: const u8[:]) -> i64 {
    var r: i64 = 0;

    while r < 63 {
        if sc_has_name(r, name) {
            return r;
        }
        r = r + 1;
    }
    return 0 - 1;
}

// -------------------------------------------------------------- limits ----

fn sc_ilim(id: i64, np: i64, out: i64[:]) {
    if id == 0 {
        out[0] = 0 - 2147483648;
        out[1] = 2147483647;
    } else if id == 1 {
        out[0] = 0 - 1;
        out[1] = 2147483647;
    } else if id == 2 {
        out[0] = 0;
        out[1] = 2147483647;
    } else if id == 3 {
        out[0] = 1;
        out[1] = np;
    } else if id == 4 {
        out[0] = 0 - 1;
        out[1] = 9223372036854775807;
    } else if id == 5 {
        out[0] = 0;
        out[1] = 4294967295;
    } else if id == 6 {
        out[0] = 0;
        out[1] = 2;
    }
}

fn sc_inf() -> f32 {
    let b: f32 = 3.4e38;
    return b * b;
}

fn sc_flim(id: i64, out: f32[:]) {
    if id == 0 {
        out[0] = 0.0 - sc_inf();
        out[1] = sc_inf();
    } else if id == 1 {
        out[0] = 0.0;
        out[1] = 1.0;
    } else if id == 2 {
        out[0] = 0.0;
        out[1] = sc_inf();
    } else if id == 3 {
        out[0] = 0.0 - sc_inf();
        out[1] = 1.0;
    } else if id == 4 {
        out[0] = 0.0;
        out[1] = 0.99;
    }
}

fn sc_wrap32(v: i64) -> i64 {
    var x = v % 4294967296;

    if x >= 2147483648 {
        x = x - 4294967296;
    }
    if x < -2147483648 {
        x = x + 4294967296;
    }
    return x;
}

fn sc_wrapu32(v: i64) -> i64 {
    var x = v % 4294967296;

    if x < 0 {
        x = x + 4294967296;
    }
    return x;
}

fn sc_kind_name(k: i64) -> const u8[:] {
    if k == 0 {
        return "num";
    }
    if k == 1 {
        return "str";
    }
    if k == 2 {
        return "bool";
    }
    if k == 3 {
        return "json";
    }
    return "nested";
}

fn sc_type_name(t: i64) -> const u8[:] {
    if t == 0 {
        return "i32";
    }
    if t == 1 {
        return "i64";
    }
    if t == 2 {
        return "u32";
    }
    if t == 3 {
        return "f32";
    }
    return "-";
}

// Name of the JSON value the driver tagged, for the type-error text.
fn sc_kname(k: i64) -> const u8[:] {
    if k == 0 || k == 1 {
        return "number";
    }
    if k == 2 {
        return "boolean";
    }
    if k == 3 {
        return "string";
    }
    if k == 6 {
        return "object";
    }
    if k == 7 {
        return "null";
    }
    return "array";
}

// --------------------------------------------------------------- errors ----

fn sc_msg_s(I: i64[:], MSG: u8[:], s: const u8[:]) -> i64 {
    I[79] = sc_puts(MSG, 0, s);
    return 3;
}

fn sc_msg_pref(I: i64[:], MSG: u8[:], pre: const u8[:], s: const u8[:]) -> i64 {
    var p = sc_puts(MSG, 0, pre);
    I[79] = sc_puts(MSG, p, s);
    return 3;
}

fn sc_type_err(I: i64[:], MSG: u8[:], want: const u8[:], k: i64) -> i64 {
    var p = sc_puts(MSG, 0, "[json.exception.type_error.302] type must be ");
    p = sc_puts(MSG, p, want);
    p = sc_puts(MSG, p, ", but is ");
    I[79] = sc_puts(MSG, p, sc_kname(k));
    return 2;
}

fn sc_range_i(I: i64[:], MSG: u8[:], lo: i64, hi: i64, v: i64) -> i64 {
    var p = sc_puts(MSG, 0, "Value must be between ");
    p = sc_put_i(MSG, p, lo);
    p = sc_puts(MSG, p, " <= value <= ");
    p = sc_put_i(MSG, p, hi);
    p = sc_puts(MSG, p, ", but got ");
    I[79] = sc_put_i(MSG, p, v);
    return 1;
}

fn sc_range_f(I: i64[:], MSG: u8[:], lo: f32, hi: f32, v: f32) -> i64 {
    var p = sc_puts(MSG, 0, "Value must be between ");
    p = sc_put_f32(MSG, p, lo);
    p = sc_puts(MSG, p, " <= value <= ");
    p = sc_put_f32(MSG, p, hi);
    p = sc_puts(MSG, p, ", but got ");
    I[79] = sc_put_f32(MSG, p, v);
    return 1;
}

// ------------------------------------------------------ task baseline -----

fn sc_reset(I: i64[:], F: f32[:], ANT: u8[:]) {
    var i: i64 = 0;

    while i < 80 {
        I[i] = 0;
        F[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < 512 {
        ANT[i] = 0;
        i = i + 1;
    }
}

// common_params defaults for the values the schema reads from the base.
fn sc_base_std(BI: i64[:], BF: f32[:], B: i64[:], BANT: u8[:]) {
    var i: i64 = 0;

    while i < 63 {
        BI[i] = 0;
        BF[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < 8 {
        B[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < 512 {
        BANT[i] = 0;
        i = i + 1;
    }
    BI[4] = 1;             // cache_prompt
    BI[7] = 30;            // sse_ping_interval
    BI[8] = 0 - 1;         // n_predict
    BI[48] = 3;            // COMMON_REASONING_FORMAT_DEEPSEEK
    BI[16] = 40;           // top_k
    BF[17] = 0.95;         // top_p
    BF[18] = 0.05;         // min_p
    BF[19] = 0.0 - 1.0;    // top_n_sigma
    BF[20] = 0.0;          // xtc_probability
    BF[21] = 0.1;          // xtc_threshold
    BF[22] = 1.0;          // typical_p
    BF[23] = 0.8;          // temperature
    BF[24] = 0.0;          // dynatemp_range
    BF[25] = 1.0;          // dynatemp_exponent
    BI[26] = 64;           // penalty_last_n
    BF[27] = 1.0;          // penalty_repeat
    BF[28] = 0.0;          // penalty_freq
    BF[29] = 0.0;          // penalty_present
    BF[30] = 0.0;          // dry_multiplier
    BF[31] = 1.75;         // dry_base
    BI[32] = 2;            // dry_allowed_length
    BI[33] = 64;           // dry_penalty_last_n
    BF[35] = 5.0;          // mirostat_tau
    BF[36] = 0.1;          // mirostat_eta
    BF[37] = 0.0 - 1.0;    // adaptive_target
    BF[38] = 0.9;          // adaptive_decay
    BI[39] = 4294967295;   // seed = LLAMA_DEFAULT_SEED
    BI[40] = 0;            // n_probs
    BI[54] = 0 - 1;        // reasoning_budget_tokens
    B[2] = 3;              // verbosity (LOG_LEVEL_INFO)
    B[3] = 1;              // n_parallel
    B[4] = 4;              // dry_sequence_breakers count
    B[5] = sc_fnv(SC_BRK);   // dry_sequence_breakers hash
    B[6] = 9;              // default sampler chain length
}

fn sc_base_set_i(BI: i64[:], BF: f32[:], row: i64, v: i64) {
    BI[row] = v;
}

fn sc_base_set_f(BI: i64[:], BF: f32[:], row: i64, v: f32) {
    BF[row] = v;
}

fn sc_base_cfg(B: i64[:], verbosity: i64, np: i64) {
    B[2] = verbosity;
    B[3] = np;
}

fn sc_base_set_ant(B: i64[:], BANT: u8[:], txt: const u8[:]) {
    let p = sc_copy(BANT, 0, txt, 0, txt.len);
    BANT[p] = 255;
    B[0] = 1;
    B[1] = p + 1;
}

// The default sampler chain common_params_sampling ships with.
fn sc_samp_std(i: i64) -> i64 {
    if i == 0 {
        return 10;
    }
    if i == 1 {
        return 1;
    }
    if i == 2 {
        return 11;
    }
    if i == 3 {
        return 2;
    }
    if i == 4 {
        return 6;
    }
    if i == 5 {
        return 3;
    }
    if i == 6 {
        return 4;
    }
    if i == 7 {
        return 8;
    }
    return 7;
}

// task_params defaults, then the values eval_llama_cmpl_schema copies from the
// base.  I[63..79] hold the handler payloads, see sc_vline.
fn sc_defaults(I: i64[:], F: f32[:], BI: const i64[:], BF: const f32[:], B: const i64[:],
              ANT: u8[:], BANT: const u8[:], SAM: i64[:], KEYN: i64[:]) {
    var i: i64 = 0;

    while i < 80 {
        I[i] = 0;
        F[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < 63 {
        KEYN[i] = 0;
        i = i + 1;
    }
    I[12] = 1;         // n_cmpl
    I[14] = 0 - 1;     // t_max_predict_ms
    // copied one by one in eval_llama_cmpl_schema
    I[4] = BI[4];
    I[7] = BI[7];
    I[8] = BI[8];
    I[10] = BI[10];
    I[13] = BI[13];
    I[48] = BI[48];
    // params.sampling = params_base.sampling
    I[16] = BI[16];
    I[26] = BI[26];
    I[32] = BI[32];
    I[33] = BI[33];
    I[34] = BI[34];
    I[39] = BI[39];
    I[40] = BI[40];
    I[41] = BI[41];
    I[42] = BI[42];
    I[47] = BI[47];
    I[53] = BI[53];
    I[54] = BI[54];
    I[59] = BI[59];
    F[17] = BF[17];
    F[18] = BF[18];
    F[19] = BF[19];
    F[20] = BF[20];
    F[21] = BF[21];
    F[22] = BF[22];
    F[23] = BF[23];
    F[24] = BF[24];
    F[25] = BF[25];
    F[27] = BF[27];
    F[28] = BF[28];
    F[29] = BF[29];
    F[30] = BF[30];
    F[31] = BF[31];
    F[35] = BF[35];
    F[36] = BF[36];
    F[37] = BF[37];
    F[38] = BF[38];
    // handler payloads from the base
    I[64] = 2166136261;    // empty response_fields hash
    I[66] = B[4];
    I[67] = B[5];
    I[68] = 0;             // grammar type NONE
    I[76] = B[0];
    I[77] = B[1];
    i = 0;
    while i < B[1] {
        ANT[i] = BANT[i];
        i = i + 1;
    }
    I[78] = B[6];
    i = 0;
    while i < B[6] {
        SAM[i] = sc_samp_std(i);
        i = i + 1;
    }
    if B[2] > 9 {
        I[0] = 1;          // verbose = verbosity > 9
    }
}

// ------------------------------------------------------- handler values ---

fn sc_samp_code(name: const u8[:]) -> i64 {
    if sc_eq_lower(name, "dry") {
        return 1;
    }
    if sc_eq_lower(name, "top_k") || sc_eq_lower(name, "top-k") || sc_eq_lower(name, "topk") {
        return 2;
    }
    if sc_eq_lower(name, "top_p") || sc_eq_lower(name, "top-p") || sc_eq_lower(name, "topp") ||
       sc_eq_lower(name, "nucleus") {
        return 3;
    }
    if sc_eq_lower(name, "top_n_sigma") || sc_eq_lower(name, "top-n-sigma") ||
       sc_eq_lower(name, "topnsigma") {
        return 11;
    }
    if sc_eq_lower(name, "typ_p") || sc_eq_lower(name, "typ-p") || sc_eq_lower(name, "typp") ||
       sc_eq_lower(name, "typ") {
        return 6;
    }
    if sc_eq_lower(name, "min_p") || sc_eq_lower(name, "min-p") || sc_eq_lower(name, "minp") {
        return 4;
    }
    if sc_eq_lower(name, "temperature") || sc_eq_lower(name, "temp") {
        return 7;
    }
    if sc_eq_lower(name, "xtc") {
        return 8;
    }
    if sc_eq_lower(name, "infill") {
        return 9;
    }
    if sc_eq_lower(name, "penalties") {
        return 10;
    }
    if sc_eq_lower(name, "adaptive_p") || sc_eq_lower(name, "adaptive-p") ||
       sc_eq_lower(name, "adaptivep") {
        return 12;
    }
    return 0;
}

// Names joined by 0xFF; unknown names are skipped like the native warning path.
fn sc_samplers_names(txt: const u8[:], SAM: i64[:]) -> i64 {
    var n: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;

    while i <= txt.len {
        if i == txt.len || txt[i] == 255 {
            if i > start {
                let c = sc_samp_code(txt[start .. i]);
                if c != 0 {
                    SAM[n] = c;
                    n = n + 1;
                }
            }
            start = i + 1;
        }
        i = i + 1;
    }
    return n;
}

fn sc_samplers_chars(s: const u8[:], SAM: i64[:]) -> i64 {
    var n: i64 = 0;
    var i: i64 = 0;

    while i < s.len {
        var c: i64 = 0;

        if s[i] == 100 {
            c = 1;
        } else if s[i] == 107 {
            c = 2;
        } else if s[i] == 121 {
            c = 6;
        } else if s[i] == 112 {
            c = 3;
        } else if s[i] == 115 {
            c = 11;
        } else if s[i] == 109 {
            c = 4;
        } else if s[i] == 116 {
            c = 7;
        } else if s[i] == 120 {
            c = 8;
        } else if s[i] == 105 {
            c = 9;
        } else if s[i] == 101 {
            c = 10;
        } else if s[i] == 97 {
            c = 12;
        }
        if c != 0 {
            SAM[n] = c;
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

// Number of distinct ids in parse_lora_request()'s map: the driver hands the
// per-entry ids (json_value(entry,"id",-1)) as signed decimals separated by ','.
fn sc_lora_count(txt: const u8[:]) -> i64 {
    if txt.len == 0 {
        return 0;
    }
    var ids: i64[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    var cur: i64 = 0;
    var neg: i64 = 0;
    var i: i64 = 0;

    while i <= txt.len {
        if i == txt.len || txt[i] == 44 {
            if neg != 0 {
                ids[n] = 0 - cur;
            } else {
                ids[n] = cur;
            }
            n = n + 1;
            cur = 0;
            neg = 0;
        } else if txt[i] == 45 {
            neg = 1;
        } else {
            cur = cur * 10 + txt[i] - 48;
        }
        i = i + 1;
    }
    var d: i64 = 0;
    var j: i64 = 0;

    while j < n {
        var dup: i64 = 0;
        var k: i64 = 0;

        while k < j {
            if ids[k] == ids[j] {
                dup = 1;
            }
            k = k + 1;
        }
        if dup == 0 {
            d = d + 1;
        }
        j = j + 1;
    }
    return d;
}

// Rebuild the stopping list from zero.  The native filter is
// `!word.empty()`, and this nlohmann's empty() is false for every string, so
// empty request strings are kept as well.
fn sc_stop_set(I: i64[:], ANT: u8[:], txt: const u8[:]) -> i64 {
    var n: i64 = 0;
    var p: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;

    while i < txt.len {
        if txt[i] == 255 {
            p = sc_copy(ANT, p, txt, start, i - start);
            ANT[p] = 255;
            p = p + 1;
            n = n + 1;
            start = i + 1;
        }
        i = i + 1;
    }
    I[77] = p;
    return n;
}

fn sc_stop_fallback(I: i64[:], ANT: u8[:], BANT: const u8[:], B: const i64[:]) {
    I[76] = B[0];
    I[77] = B[1];
    var i: i64 = 0;

    while i < B[1] {
        ANT[i] = BANT[i];
        i = i + 1;
    }
}

// ----------------------------------------------------------- field eval ---

fn sc_apply_num(I: i64[:], F: f32[:], BF: const f32[:], B: const i64[:], MSG: u8[:],
               row: i64, k: i64, iv: i64, fv: f32) -> i64 {
    if SC_TYPE[row] == 3 {
        if k == 1 {
            F[row] = fv;
        } else if k == 0 {
            F[row] = iv;
        } else {
            return sc_type_err(I, MSG, "number", k);
        }
        if SC_HAND[row] == 17 {
            // dry_base: a value below 1.0 is replaced by the base value
            if F[row] < 1.0 {
                F[row] = BF[row];
            }
            return 0;
        }
        var fl: f32[2] = [0.0, 0.0];

        sc_flim(SC_FLIM[row], fl);
        if SC_HARD[row] != 0 {
            if F[row] < fl[0] || F[row] > fl[1] {
                return sc_range_f(I, MSG, fl[0], fl[1], F[row]);
            }
        } else {
            if F[row] < fl[0] {
                F[row] = fl[0];
            }
            if F[row] > fl[1] {
                F[row] = fl[1];
            }
        }
        return 0;
    }
    var v: i64 = 0;

    if k == 0 {
        v = iv;
    } else if k == 1 {
        v = fv as i64;
    } else {
        return sc_type_err(I, MSG, "number", k);
    }
    if SC_TYPE[row] == 0 {
        v = sc_wrap32(v);
    } else if SC_TYPE[row] == 2 {
        v = sc_wrapu32(v);
    }
    var li: i64[2] = [0, 0];

    sc_ilim(SC_ILIM[row], B[3], li);
    if SC_HARD[row] != 0 {
        if v < li[0] || v > li[1] {
            return sc_range_i(I, MSG, li[0], li[1], v);
        }
    } else {
        if v < li[0] {
            v = li[0];
        }
        if v > li[1] {
            v = li[1];
        }
    }
    I[row] = v;
    return 0;
}

fn sc_apply_bool(I: i64[:], MSG: u8[:], row: i64, k: i64, iv: i64) -> i64 {
    if k != 2 {
        return sc_type_err(I, MSG, "boolean", k);
    }
    I[row] = iv;
    if SC_HAND[row] == 16 && iv != 0 {
        I[75] = 2;   // the two EOG biases common_tokenizer supplies
    }
    return 0;
}

fn sc_apply_handler(I: i64[:], B: const i64[:], ANT: u8[:], BANT: const u8[:], SAM: i64[:],
                   MSG: u8[:], row: i64, k: i64, iv: i64, txt: const u8[:]) -> i64 {
    let h = SC_HAND[row];

    if h == 1 {
        // response_fields: a wrong type falls back to the empty default
        if k == 4 {
            I[63] = iv;
            I[64] = sc_fnv(txt);
        } else {
            I[63] = 0;
            I[64] = 2166136261;
        }
        return 0;
    }
    if h == 2 {
        if k != 4 {
            return sc_msg_s(I, MSG, "Error: 'lora' must be an array of objects with 'id' and 'scale' fields");
        }
        I[65] = sc_lora_count(txt);
        return 0;
    }
    if h == 3 {
        if k == 4 && iv > 0 {
            I[66] = iv;
            I[67] = sc_fnv(txt);
            return 0;
        }
        return sc_msg_s(I, MSG, "Error: dry_sequence_breakers must be a non-empty array of strings");
    }
    if h == 4 {
        // json_schema wins only when "grammar" is absent
        if (iv & 1) != 0 && (iv & 2) == 0 {
            I[68] = 2;   // COMMON_GRAMMAR_TYPE_OUTPUT_FORMAT
            return 0;
        }
        if txt.len > 0 {
            if (iv & 4) != 0 {
                I[68] = 3;   // COMMON_GRAMMAR_TYPE_TOOL_CALLS
            } else {
                I[68] = 1;   // COMMON_GRAMMAR_TYPE_USER
            }
        }
        return 0;
    }
    if h == 5 {
        if k != 3 {
            return sc_type_err(I, MSG, "string", k);
        }
        if sc_eq(txt, "none") {
            I[48] = 0;
        } else if sc_eq(txt, "auto") {
            I[48] = 1;
        } else if sc_eq(txt, "deepseek") {
            I[48] = 3;
        } else if sc_eq(txt, "deepseek-legacy") {
            I[48] = 2;
        } else {
            return sc_msg_pref(I, MSG, "Unknown reasoning format: ", txt);
        }
        return 0;
    }
    if h == 6 {
        if k == 2 {
            return 0;
        }
        if k == 3 && (sc_eq(txt, "content") || sc_eq(txt, "reasoning_content")) {
            return 0;
        }
        return sc_msg_s(I, MSG, "Invalid continue_final_message: expected a boolean, \"content\" or \"reasoning_content\"");
    }
    if h == 7 {
        if k != 2 {
            return sc_type_err(I, MSG, "boolean", k);
        }
        return 0;
    }
    if h == 14 {
        if k == 4 {
            let n = sc_stop_set(I, ANT, txt);
            if n == 0 {
                sc_stop_fallback(I, ANT, BANT, B);
            } else {
                I[76] = n;
            }
            return 0;
        }
        if k == 3 {
            var p = sc_copy(ANT, 0, txt, 0, txt.len);
            ANT[p] = 255;
            I[76] = 1;
            I[77] = p + 1;
            return 0;
        }
        sc_stop_fallback(I, ANT, BANT, B);
        return 0;
    }
    if h == 15 {
        if k == 4 {
            I[78] = sc_samplers_names(txt, SAM);
        } else if k == 3 {
            I[78] = sc_samplers_chars(txt, SAM);
        }
        return 0;
    }
    // handlers 8..13 are token-level and never run here
    return 0;
}

// kind: 0 int, 1 float, 2 bool, 3 string, 4 string array, 5 non-string array,
// 6 object, 7 null, 8 json_schema flags, 10/11 nested bool, 12 nested null,
// 13 nested string, 14 nested non-object.
fn sc_apply(I: i64[:], F: f32[:], BI: const i64[:], BF: const f32[:], B: const i64[:],
           ANT: u8[:], BANT: const u8[:], SAM: i64[:], KEYN: i64[:], MSG: u8[:],
           row: i64, key: const u8[:], k: i64, iv: i64, fv: f32, txt: const u8[:]) -> i64 {
    I[79] = 0;
    if k == 7 {
        return 0;   // null means absent, the alias chain continues
    }
    if row != 3 {
        let pos = sc_alias_pos(row, key);
        if pos < 0 {
            return 0;
        }
        if KEYN[row] != 0 && KEYN[row] < pos {
            return 0;   // a higher-priority name already supplied this field
        }
        KEYN[row] = pos;
    }
    if row == 3 {
        if k == 10 {
            I[62] = 1;
            return 0;
        }
        if k == 11 {
            I[62] = 0;
            return 0;
        }
        if k == 12 || k == 14 {
            return 0;
        }
        if k == 13 {
            return sc_type_err(I, MSG, "boolean", 3);
        }
        return sc_type_err(I, MSG, "boolean", k);
    }
    if SC_KIND[row] == 0 {
        return sc_apply_num(I, F, BF, B, MSG, row, k, iv, fv);
    }
    if SC_KIND[row] == 2 {
        return sc_apply_bool(I, MSG, row, k, iv);
    }
    return sc_apply_handler(I, B, ANT, BANT, SAM, MSG, row, k, iv, txt);
}

// ------------------------------------------------------------ gate text ---

fn sc_tline(row: i64, np: i64, out: u8[:], at: i64) -> i64 {
    var p = sc_puts(out, at, "T");
    p = sc_put_i(out, p, row);
    p = sc_puts(out, p, "|n=");
    p = sc_puts(out, p, sc_names(row));
    p = sc_puts(out, p, "|k=");
    p = sc_puts(out, p, sc_kind_name(SC_KIND[row]));
    p = sc_puts(out, p, "|t=");
    if SC_KIND[row] == 0 {
        p = sc_puts(out, p, sc_type_name(SC_TYPE[row]));
    } else {
        p = sc_puts(out, p, "-");
    }
    p = sc_puts(out, p, "|lo=");
    if SC_KIND[row] == 0 {
        if SC_TYPE[row] == 3 {
            var fl: f32[2] = [0.0, 0.0];
            sc_flim(SC_FLIM[row], fl);
            p = sc_put_f32(out, p, fl[0]);
            p = sc_puts(out, p, "|hi=");
            p = sc_put_f32(out, p, fl[1]);
        } else {
            var li: i64[2] = [0, 0];
            sc_ilim(SC_ILIM[row], np, li);
            p = sc_put_i(out, p, li[0]);
            p = sc_puts(out, p, "|hi=");
            p = sc_put_i(out, p, li[1]);
        }
    } else {
        p = sc_puts(out, p, "-|hi=-");
    }
    p = sc_puts(out, p, "|hard=");
    p = sc_put_i(out, p, SC_HARD[row]);
    p = sc_puts(out, p, "|dh=");
    p = sc_put_hex8(out, p, sc_fnv(sc_desc(row)));
    return p;
}

fn sc_vline(row: i64, I: const i64[:], F: const f32[:], SAM: const i64[:], ANT: const u8[:],
           out: u8[:], at: i64) -> i64 {
    var p = sc_put_i(out, at, row);

    out[p] = 61;
    p = p + 1;
    if row == 3 {
        p = sc_put_i(out, p, I[62]);
    } else if row == 15 {
        p = sc_put_i(out, p, I[63]);
        out[p] = 58;
        p = p + 1;
        p = sc_put_hex8(out, p, I[64]);
    } else if row == 44 {
        p = sc_put_i(out, p, I[65]);
    } else if row == 45 {
        p = sc_put_i(out, p, I[66]);
        out[p] = 58;
        p = p + 1;
        p = sc_put_hex8(out, p, I[67]);
    } else if row == 46 {
        p = sc_put_i(out, p, I[68]);
    } else if row == 48 {
        p = sc_put_i(out, p, I[48]);
    } else if row == 49 || row == 50 {
        p = sc_put_i(out, p, 0);
    } else if row == 51 {
        p = sc_put_i(out, p, I[69]);
    } else if row == 52 {
        p = sc_put_i(out, p, I[70]);
    } else if row == 55 {
        p = sc_put_i(out, p, I[71]);
    } else if row == 56 {
        p = sc_put_i(out, p, I[72]);
        out[p] = 58;
        p = p + 1;
        p = sc_put_i(out, p, I[73]);
    } else if row == 57 {
        p = sc_put_i(out, p, I[74]);
    } else if row == 58 {
        p = sc_put_i(out, p, I[75]);
    } else if row == 60 {
        p = sc_put_i(out, p, I[76]);
        out[p] = 58;
        p = p + 1;
        p = sc_put_hex8(out, p, sc_fnv(ANT[0 .. I[77]]));
    } else if row == 61 {
        p = sc_put_i(out, p, I[78]);
        var i: i64 = 0;
        while i < I[78] {
            out[p] = 58;
            p = p + 1;
            p = sc_put_i(out, p, SAM[i]);
            i = i + 1;
        }
    } else if SC_KIND[row] == 0 {
        if SC_TYPE[row] == 3 {
            p = sc_put_f32(out, p, F[row]);
        } else {
            p = sc_put_i(out, p, I[row]);
        }
    } else if SC_KIND[row] == 2 {
        p = sc_put_i(out, p, I[row]);
    } else {
        p = sc_put_i(out, p, 0);
    }
    out[p] = 59;
    return p + 1;
}

// ------------------------------------------------------------------ TTS ---

fn sc_tts_defaults(I: i64[:], F: f32[:], BI: const i64[:]) {
    var i: i64 = 0;

    while i < 8 {
        I[i] = 0;
        F[i] = 0;
        i = i + 1;
    }
    I[2] = BI[39];   // seed defaults to the base sampling seed
    I[3] = 0 - 1;
    F[4] = 1.05;     // penalty_repeat
}

fn sc_tts_apply(I: i64[:], F: f32[:], MSG: u8[:], row: i64, k: i64, iv: i64, fv: f32) -> i64 {
    if k == 7 {
        return 0;
    }
    if row == 5 {
        if k != 2 {
            return sc_type_err(I, MSG, "boolean", k);
        }
        I[5] = iv;
        return 0;
    }
    if TTS_TYPE[row] == 3 {
        if k == 1 {
            F[row] = fv;
        } else if k == 0 {
            F[row] = iv;
        } else {
            return sc_type_err(I, MSG, "number", k);
        }
        var fl: f32[2] = [0.0, 0.0];
        sc_flim(TTS_FLIM[row], fl);
        if F[row] < fl[0] {
            F[row] = fl[0];
        }
        if F[row] > fl[1] {
            F[row] = fl[1];
        }
        return 0;
    }
    var v: i64 = 0;

    if k == 0 {
        v = iv;
    } else if k == 1 {
        v = fv as i64;
    } else {
        return sc_type_err(I, MSG, "number", k);
    }
    if TTS_TYPE[row] == 0 {
        v = sc_wrap32(v);
    } else if TTS_TYPE[row] == 2 {
        v = sc_wrapu32(v);
    }
    var li: i64[2] = [0, 0];
    sc_ilim(TTS_ILIM[row], 1, li);
    if TTS_HARD[row] != 0 {
        if v < li[0] || v > li[1] {
            return sc_range_i(I, MSG, li[0], li[1], v);
        }
    } else {
        if v < li[0] {
            v = li[0];
        }
        if v > li[1] {
            v = li[1];
        }
    }
    I[row] = v;
    return 0;
}

fn sc_tts_tline(row: i64, out: u8[:], at: i64) -> i64 {
    var p = sc_puts(out, at, "U");
    p = sc_put_i(out, p, row);
    p = sc_puts(out, p, "|n=");
    p = sc_puts(out, p, sc_tname(row));
    p = sc_puts(out, p, "|k=");
    p = sc_puts(out, p, sc_kind_name(TTS_KIND[row]));
    p = sc_puts(out, p, "|t=");
    if TTS_KIND[row] == 0 {
        p = sc_puts(out, p, sc_type_name(TTS_TYPE[row]));
    } else {
        p = sc_puts(out, p, "-");
    }
    p = sc_puts(out, p, "|lo=");
    if TTS_KIND[row] != 0 {
        p = sc_puts(out, p, "-|hi=-");
    } else if TTS_TYPE[row] == 3 {
        var fl: f32[2] = [0.0, 0.0];
        sc_flim(TTS_FLIM[row], fl);
        p = sc_put_f32(out, p, fl[0]);
        p = sc_puts(out, p, "|hi=");
        p = sc_put_f32(out, p, fl[1]);
    } else {
        var li: i64[2] = [0, 0];
        sc_ilim(TTS_ILIM[row], 1, li);
        p = sc_put_i(out, p, li[0]);
        p = sc_puts(out, p, "|hi=");
        p = sc_put_i(out, p, li[1]);
    }
    p = sc_puts(out, p, "|hard=");
    p = sc_put_i(out, p, TTS_HARD[row]);
    p = sc_puts(out, p, "|dh=");
    p = sc_put_hex8(out, p, sc_fnv(sc_tdesc(row)));
    return p;
}

fn sc_tts_vline(row: i64, I: const i64[:], F: const f32[:], out: u8[:], at: i64) -> i64 {
    var p = sc_put_i(out, at, row);

    out[p] = 61;
    p = p + 1;
    if TTS_KIND[row] == 2 {
        p = sc_put_i(out, p, I[row]);
    } else if TTS_TYPE[row] == 3 {
        p = sc_put_f32(out, p, F[row]);
    } else {
        p = sc_put_i(out, p, I[row]);
    }
    out[p] = 59;
    return p + 1;
}

// ----------------------------------------------------------- public API ---
//
// Thin one-line forwards.  All decisions stay in the sc_* functions above;
// these wrappers are the module's integration surface, and the gate driver
// calls them, so the gate exercises the same entry points a runtime would.
// Return codes of schema_apply/schema_tts_apply: 0 ok, 1 hard-limit range
// error, 2 JSON type error, 3 handler message.  On a non-zero code the message
// sits in MSG and its length is schema_msg_len(I).

export fn schema_row_count() -> i64 { return 63; }
export fn schema_tts_row_count() -> i64 { return 6; }
export fn schema_rc_ok() -> i64 { return 0; }
export fn schema_rc_range() -> i64 { return 1; }
export fn schema_rc_type() -> i64 { return 2; }
export fn schema_rc_message() -> i64 { return 3; }
export fn schema_msg_len(I: const i64[:]) -> i64 { return I[79]; }
export fn schema_find(name: const u8[:]) -> i64 { return sc_find(name); }
export fn schema_tfind(name: const u8[:]) -> i64 { return sc_tfind(name); }
export fn schema_reset(I: i64[:], F: f32[:], ANT: u8[:]) { sc_reset(I, F, ANT); }
export fn schema_base_std(BI: i64[:], BF: f32[:], B: i64[:], BANT: u8[:]) { sc_base_std(BI, BF, B, BANT); }
export fn schema_base_set_i(BI: i64[:], BF: f32[:], row: i64, v: i64) { sc_base_set_i(BI, BF, row, v); }
export fn schema_base_set_f(BI: i64[:], BF: f32[:], row: i64, v: f32) { sc_base_set_f(BI, BF, row, v); }
export fn schema_base_cfg(B: i64[:], verbosity: i64, np: i64) { sc_base_cfg(B, verbosity, np); }
export fn schema_base_set_ant(B: i64[:], BANT: u8[:], txt: const u8[:]) { sc_base_set_ant(B, BANT, txt); }
export fn schema_defaults(I: i64[:], F: f32[:], BI: const i64[:], BF: const f32[:], B: const i64[:], ANT: u8[:], BANT: const u8[:], SAM: i64[:], KEYN: i64[:]) { sc_defaults(I, F, BI, BF, B, ANT, BANT, SAM, KEYN); }
export fn schema_apply(I: i64[:], F: f32[:], BI: const i64[:], BF: const f32[:], B: const i64[:], ANT: u8[:], BANT: const u8[:], SAM: i64[:], KEYN: i64[:], MSG: u8[:], row: i64, key: const u8[:], k: i64, iv: i64, fv: f32, txt: const u8[:]) -> i64 { return sc_apply(I, F, BI, BF, B, ANT, BANT, SAM, KEYN, MSG, row, key, k, iv, fv, txt); }
export fn schema_tline(row: i64, np: i64, out: u8[:], at: i64) -> i64 { return sc_tline(row, np, out, at); }
export fn schema_vline(row: i64, I: const i64[:], F: const f32[:], SAM: const i64[:], ANT: const u8[:], out: u8[:], at: i64) -> i64 { return sc_vline(row, I, F, SAM, ANT, out, at); }
export fn schema_tts_defaults(I: i64[:], F: f32[:], BI: const i64[:]) { sc_tts_defaults(I, F, BI); }
export fn schema_tts_apply(I: i64[:], F: f32[:], MSG: u8[:], row: i64, k: i64, iv: i64, fv: f32) -> i64 { return sc_tts_apply(I, F, MSG, row, k, iv, fv); }
export fn schema_tts_tline(row: i64, out: u8[:], at: i64) -> i64 { return sc_tts_tline(row, out, at); }
export fn schema_tts_vline(row: i64, I: const i64[:], F: const f32[:], out: u8[:], at: i64) -> i64 { return sc_tts_vline(row, I, F, out, at); }

