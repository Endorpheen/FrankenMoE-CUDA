#!/usr/bin/env python3
# Generates the shared case table for the request-schema slice (P8-07 S1):
#   tests/probe_schema.cpp   - native oracle, includes tools/server/server-schema.cpp
#                              and evaluates the same request bodies
#   build/gate_schema.goose  - the port plus a driver over the same cases
# Templates use __TOKEN__ replacement so no percent or brace formatting leaks
# into the generated C++ or Goose source.
from pathlib import Path

# Entry forms:
#   ("i", key, v) ("f", key, v) ("b", key, v) ("null", key) ("s", key, text)
#   ("arr", key, [str]) ("arrnum", key, [int]) ("obj", key)
#   ("lora", [ids]) ("lora_bad",)
#   ("schema", js, gr, tc, grammar_text, schema_text)
#   ("nested", "b"|"null"|"s"|"arr", value)
CASES = [
    ("empty0", 0, []),
    ("empty1", 1, []),
    ("nulls", 1, [("null", "top_k"), ("null", "stream"), ("null", "verbose")]),
    ("top_k_100", 0, [("i", "top_k", 100)]),
    ("top_k_neg", 0, [("i", "top_k", -5)]),
    ("n_predict_neg", 0, [("i", "n_predict", -5)]),
    ("n_cmpl_0", 0, [("i", "n_cmpl", 0)]),
    ("n_cmpl_2_v0", 0, [("i", "n_cmpl", 2)]),
    ("n_cmpl_2_v1", 1, [("i", "n_cmpl", 2)]),
    ("n_cmpl_5_v1", 1, [("i", "n_cmpl", 5)]),
    ("n_indent_neg", 0, [("i", "n_indent", -1)]),
    ("n_keep_neg2", 0, [("i", "n_keep", -2)]),
    ("n_keep_neg1", 0, [("i", "n_keep", -1)]),
    ("top_p_hi", 0, [("f", "top_p", 2.0)]),
    ("top_p_lo", 0, [("f", "top_p", -0.5)]),
    ("temp_neg", 0, [("f", "temperature", -1.0)]),
    ("mirostat_hi", 0, [("i", "mirostat", 3)]),
    ("mirostat_lo", 0, [("i", "mirostat", -1)]),
    ("decay_hi", 0, [("f", "adaptive_decay", 1.5)]),
    ("decay_ok", 0, [("f", "adaptive_decay", 0.5)]),
    ("atarget_hi", 0, [("f", "adaptive_target", 2.0)]),
    ("seed_42", 0, [("i", "seed", 42)]),
    ("seed_neg", 0, [("i", "seed", -1)]),
    ("seed_wrap", 0, [("i", "seed", 4294967296)]),
    ("top_k_float", 0, [("f", "top_k", 1.5)]),
    ("top_k_str", 0, [("s", "top_k", "x")]),
    ("stream_str", 0, [("s", "stream", "yes")]),
    ("stream_true", 1, [("b", "stream", 1), ("b", "cache_prompt", 1)]),
    ("t_max", 0, [("i", "t_max_predict_ms", 9000000000)]),
    ("repeat_last_neg", 0, [("i", "repeat_last_n", -1)]),
    ("dry_base_lo", 0, [("f", "dry_base", 0.5)]),
    ("dry_base_hi", 1, [("f", "dry_base", 2.5)]),
    ("dry_base_1", 1, [("f", "dry_base", 1.0)]),
    ("resp_fields", 0, [("arr", "response_fields", ["n_predict", "model"])]),
    ("resp_fields_str", 0, [("s", "response_fields", "x")]),
    ("resp_fields_num", 0, [("arrnum", "response_fields", [1, 2])]),
    ("lora_empty", 0, [("lora", [])]),
    ("lora_str", 0, [("lora_bad",)]),
    ("lora_one", 0, [("lora", [1])]),
    ("lora_dups", 0, [("lora", [-1, -1])]),
    ("breakers_ok", 0, [("arr", "dry_sequence_breakers", ["a", "bb"])]),
    ("breakers_empty", 0, [("arr", "dry_sequence_breakers", [])]),
    ("breakers_str", 0, [("s", "dry_sequence_breakers", "x")]),
    ("breakers_blank", 0, [("arr", "dry_sequence_breakers", [""])]),
    ("stop_arr", 0, [("arr", "stop", ["a", "b"])]),
    ("stop_empty_arr", 1, [("arr", "stop", [])]),
    ("stop_str", 0, [("s", "stop", "END")]),
    ("stop_empty_str", 1, [("s", "stop", "")]),
    ("stop_filter", 0, [("arr", "stop", ["", "a"])]),
    ("stop_num", 0, [("i", "stop", 5)]),
    ("stop_obj", 1, [("obj", "stop")]),
    ("samplers_names", 0, [("arr", "samplers", ["top_k", "top-p", "nucleus", "temp"])]),
    ("samplers_upper", 0, [("arr", "samplers", ["TOP_K", "Typ_P", "topnsigma"])]),
    ("samplers_unknown", 0, [("arr", "samplers", ["bogus", "minp"])]),
    ("samplers_chars", 0, [("s", "samplers", "kyp")]),
    ("samplers_chars_unknown", 0, [("s", "samplers", "kzq")]),
    ("samplers_num", 1, [("i", "samplers", 5)]),
    ("json_schema_empty", 0, [("schema", 1, 0, 0, "", "{}")]),
    ("json_schema_and_grammar", 0, [("schema", 1, 1, 0, 'root ::= "a"', '{"type":"object"}')]),
    ("grammar_user", 0, [("schema", 0, 1, 0, 'root ::= "a"', "")]),
    ("grammar_tool", 0, [("schema", 0, 1, 1, 'root ::= "a"', "")]),
    ("grammar_empty", 0, [("schema", 0, 1, 0, "", "")]),
    ("schema_null", 0, [("null", "json_schema")]),
    ("reasoning_deepseek", 0, [("s", "reasoning_format", "deepseek")]),
    ("reasoning_legacy", 0, [("s", "reasoning_format", "deepseek-legacy")]),
    ("reasoning_auto", 0, [("s", "reasoning_format", "auto")]),
    ("reasoning_none", 0, [("s", "reasoning_format", "none")]),
    ("reasoning_bad", 0, [("s", "reasoning_format", "nope")]),
    ("reasoning_num", 0, [("i", "reasoning_format", 1)]),
    ("cont_true", 0, [("b", "continue_final_message", 1)]),
    ("cont_content", 0, [("s", "continue_final_message", "content")]),
    ("cont_reason", 0, [("s", "continue_final_message", "reasoning_content")]),
    ("cont_bad", 0, [("s", "continue_final_message", "x")]),
    ("cont_num", 0, [("i", "continue_final_message", 1)]),
    ("echo_true", 0, [("b", "echo", 1)]),
    ("echo_num", 0, [("i", "echo", 1)]),
    ("nested_usage", 0, [("nested", "b", 1)]),
    ("nested_false", 0, [("nested", "b", 0)]),
    ("nested_null", 1, [("nested", "null", 0)]),
    ("nested_str", 0, [("nested", "s", "x")]),
    ("nested_arr", 0, [("nested", "arr", 0)]),
    ("alias_max_tokens", 0, [("i", "max_tokens", 32)]),
    ("alias_both", 0, [("i", "n_predict", 10), ("i", "max_tokens", 32)]),
    ("alias_alias_both", 0, [("i", "max_completion_tokens", 16), ("i", "max_tokens", 32)]),
    ("alias_n", 1, [("i", "n", 3)]),
    ("alias_logprobs", 0, [("i", "logprobs", 3)]),
    ("ignore_eos_t", 0, [("b", "ignore_eos", 1)]),
    ("ignore_eos_f", 1, [("b", "ignore_eos", 0)]),
    ("bools", 0, [("b", "backend_sampling", 1), ("b", "post_sampling_probs", 1),
                  ("b", "grammar_lazy", 1), ("b", "reasoning_control", 1)]),
    ("budget_tokens", 0, [("i", "reasoning_budget_tokens", 128), ("b", "reasoning_control", 1)]),
    ("int_to_float", 0, [("i", "temperature", 1)]),
    ("float_to_int", 0, [("f", "n_keep", 3.9)]),
    ("n_probs_min", 0, [("i", "n_probs", -2147483648)]),
    ("min_keep_neg", 0, [("i", "min_keep", -1)]),
    ("backend_str", 0, [("s", "backend_sampling", "x")]),
    ("timings_true", 0, [("b", "timings_per_token", 1)]),
    ("n_discard_neg", 0, [("i", "n_discard", -1)]),
    ("n_cache_v1", 1, [("i", "n_cache_reuse", 32)]),
    ("dry_allowed_neg", 0, [("i", "dry_allowed_length", -1)]),
    ("pos_freq", 0, [("f", "frequency_penalty", 0.25), ("f", "presence_penalty", 0.5)]),
    ("nested_extra", 0, [("nested", "b", 1)]),
]

TTS_CASES = [
    ("empty0", 0, []),
    ("all", 0, [("i", "top_k", 5), ("f", "top_p", 0.9), ("i", "max_tokens", 64)]),
    ("neg", 0, [("i", "n_predict", -5)]),
    ("top_k_neg", 0, [("i", "top_k", -1)]),
    ("top_p_hi", 0, [("f", "top_p", 2.0)]),
    ("seed_neg", 0, [("i", "seed", -1)]),
    ("stream_rep", 0, [("b", "stream", 1), ("f", "repeat_penalty", 1.5)]),
    ("seed_base", 1, []),
    ("alias_max_tokens", 0, [("i", "max_tokens", 32)]),
    ("top_k_str", 0, [("s", "top_k", "x")]),
    ("n_predict_float", 0, [("f", "n_predict", 7.9)]),
]

# ------------------------------------------------------------------ C++ ----

def cpp_str(s):
    out = []
    for b in s.encode():
        if 32 <= b < 127 and chr(b) not in '"\\':
            out.append(chr(b))
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def json_str(s):
    out = ['"']
    for ch in s:
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif ch == '\n':
            out.append('\\n')
        else:
            out.append(ch)
    out.append('"')
    return ''.join(out)


def json_body(entries):
    parts = []
    for e in entries:
        k = e[0]
        if k == "i":
            parts.append('%s: %d' % (json_str(e[1]), e[2]))
        elif k == "f":
            parts.append('%s: %s' % (json_str(e[1]), repr(float(e[2]))))
        elif k == "b":
            parts.append('%s: %s' % (json_str(e[1]), "true" if e[2] else "false"))
        elif k == "null":
            parts.append('%s: null' % json_str(e[1]))
        elif k == "s":
            parts.append('%s: %s' % (json_str(e[1]), json_str(e[2])))
        elif k == "arr":
            parts.append('%s: [%s]' % (json_str(e[1]), ", ".join(json_str(x) for x in e[2])))
        elif k == "arrnum":
            parts.append('%s: [%s]' % (json_str(e[1]), ", ".join(str(x) for x in e[2])))
        elif k == "obj":
            parts.append('%s: {"a": 1}' % json_str(e[1]))
        elif k == "lora":
            entries = ", ".join('{"id": %d, "scale": 0.5}' % i for i in e[1])
            parts.append('"lora": [%s]' % entries)
        elif k == "lora_bad":
            parts.append('"lora": "x"')
        elif k == "schema":
            js, gr, tc, gtxt, stxt = e[1], e[2], e[3], e[4], e[5]
            if js:
                parts.append('"json_schema": %s' % (stxt if stxt else "{}"))
            if gr:
                parts.append('"grammar": %s' % json_str(gtxt))
            if tc:
                parts.append('"grammar_type": "tool_calls"')
        elif k == "nested":
            sub = e[1]
            if sub == "b":
                parts.append('"stream_options": {"include_usage": %s}' % ("true" if e[2] else "false"))
            elif sub == "null":
                parts.append('"stream_options": {"include_usage": null}')
            elif sub == "s":
                parts.append('"stream_options": {"include_usage": %s}' % json_str(e[2]))
            elif sub == "arr":
                parts.append('"stream_options": []')
    return "{" + ", ".join(parts) + "}"


CPP_HEAD = r'''// Native oracle for the request-schema slice. Includes the translation unit so
// the schema table and the eval implementations under test are the native ones.
#include "server-schema.cpp"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <map>
#include <string>
#include <vector>

// tools/server/server-common.cpp owns this ten-line helper.  The probe supplies
// the same body so the translation unit links without libllama-server-impl
// (which carries an unrelated experiment's initialiser and its output).
std::map<int, float> parse_lora_request(const json & data) {
    std::map<int, float> lora;
    for (const auto & entry : data) {
        int   id    = json_value(entry, "id", -1);
        float scale = json_value(entry, "scale", 0.0f);
        lora[id] = scale;
    }
    return lora;
}

static uint32_t fnv_bytes(const char * s, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= (uint8_t) s[i]; h *= 16777619u; }
    return h;
}

// The shared float rendering: fixed six decimals like std::to_string, plus the
// two sentinels the tables carry.
static void f32_repr(char * out, float v) {
    if (v != v) { strcpy(out, "nan"); return; }
    if (v > 3.0e38f) { strcpy(out, "inf"); return; }
    if (v < -3.0e38f) { strcpy(out, "-inf"); return; }
    if (v > 1.0e30f) { strcpy(out, "fmax"); return; }
    if (v < -1.0e30f) { strcpy(out, "-fmax"); return; }
    bool neg = v < 0.0f;
    double a = neg ? -(double) v : (double) v;
    unsigned long long ip = (unsigned long long) a;
    unsigned long long fp = (unsigned long long) ((a - (double) ip) * 1000000.0 + 0.5);
    if (fp >= 1000000ull) { ip++; fp -= 1000000ull; }
    snprintf(out, 64, "%s%llu.%06llu", neg ? "-" : "", ip, fp);
}

static common_params make_base(int v) {
    common_params b;
    if (v == 1) {
        b.verbosity = 10;
        b.n_parallel = 4;
        b.n_keep = 8;
        b.n_predict = 64;
        b.n_cache_reuse = 16;
        b.cache_prompt = false;
        b.sse_ping_interval = 7;
        b.reasoning_format = COMMON_REASONING_FORMAT_AUTO;
        b.antiprompt = {"\nUser:"};
        b.sampling.top_k = 13;
        b.sampling.top_p = 0.5f;
        b.sampling.min_p = 0.1f;
        b.sampling.temp = 0.3f;
        b.sampling.seed = 7;
        b.sampling.penalty_repeat = 1.2f;
        b.sampling.dry_base = 1.5f;
        b.sampling.n_probs = 2;
        b.sampling.mirostat = 1;
    }
    return b;
}

static void dump_field(const server_schema::field * f, int idx, const char * pfx = "T") {
    const char * kind = "?";
    const char * type = "-";
    using namespace server_schema;
    if (dynamic_cast<const field_num<int32_t>*>(f))       { kind = "num"; type = "i32"; }
    else if (dynamic_cast<const field_num<int64_t>*>(f))  { kind = "num"; type = "i64"; }
    else if (dynamic_cast<const field_num<uint32_t>*>(f)) { kind = "num"; type = "u32"; }
    else if (dynamic_cast<const field_num<float>*>(f))    { kind = "num"; type = "f32"; }
    else if (dynamic_cast<const field_bool*>(f))          { kind = "bool"; }
    else if (dynamic_cast<const field_str*>(f))           { kind = "str"; }
    else if (dynamic_cast<const field_json*>(f))          { kind = "json"; }
    else if (dynamic_cast<const field_nested*>(f))        { kind = "nested"; }
    printf("%s%d|n=", pfx, idx);
    for (size_t k = 0; k < f->name.size(); k++) printf("%s%s", k ? "," : "", f->name[k]);
    printf("|k=%s|t=%s|lo=", kind, type);
    if (!strcmp(kind, "num")) {
        char lo[64], hi[64];
        bool hard = false;
        if (dynamic_cast<const field_num<int32_t>*>(f)) {
            auto * n = (const field_num<int32_t>*) f;
            snprintf(lo, sizeof lo, "%d", n->min); snprintf(hi, sizeof hi, "%d", n->max);
            hard = n->is_hard_limit;
        } else if (dynamic_cast<const field_num<int64_t>*>(f)) {
            auto * n = (const field_num<int64_t>*) f;
            snprintf(lo, sizeof lo, "%lld", (long long) n->min); snprintf(hi, sizeof hi, "%lld", (long long) n->max);
            hard = n->is_hard_limit;
        } else if (dynamic_cast<const field_num<uint32_t>*>(f)) {
            auto * n = (const field_num<uint32_t>*) f;
            snprintf(lo, sizeof lo, "%u", n->min); snprintf(hi, sizeof hi, "%u", n->max);
            hard = n->is_hard_limit;
        } else {
            auto * n = (const field_num<float>*) f;
            f32_repr(lo, n->min); f32_repr(hi, n->max);
            hard = n->is_hard_limit;
        }
        printf("%s|hi=%s|hard=%d", lo, hi, (int) hard);
    } else {
        printf("-|hi=-|hard=0");
    }
    printf("|dh=%08x\n", fnv_bytes(f->desc, strlen(f->desc)));
}

static void joined(const std::vector<std::string> & v) {
    std::string j;
    for (auto & s : v) { j += s; j += '\xff'; }
    printf("%zu:%08x", v.size(), fnv_bytes(j.data(), j.size()));
}

static void emit_row(int row, const task_params & p) {
    printf("%d=", row);
    switch (row) {
        case 15: joined(p.response_fields); break;
        case 44: printf("%zu", p.lora.size()); break;
        case 45: joined(p.sampling.dry_sequence_breakers); break;
        case 46: printf("%d", (int) p.sampling.grammar.type); break;
        case 48: printf("%d", (int) p.reasoning_format); break;
        case 49: case 50: printf("0"); break;
        case 51: printf("%zu", p.sampling.preserved_tokens.size()); break;
        case 52: printf("%zu", p.sampling.grammar_triggers.size()); break;
        case 55: printf("%zu", p.sampling.reasoning_budget_start.size()); break;
        case 56: {
            size_t t = 0;
            for (auto & s : p.sampling.reasoning_budget_end) t += s.size();
            printf("%zu:%zu", p.sampling.reasoning_budget_end.size(), t);
            break;
        }
        case 57: printf("%zu", p.sampling.reasoning_budget_forced.size()); break;
        case 58: printf("%zu", p.sampling.logit_bias.size()); break;
        case 60: joined(p.antiprompt); break;
        case 61: {
            printf("%zu", p.sampling.samplers.size());
            for (auto s : p.sampling.samplers) printf(":%d", (int) s);
            break;
        }
        default: printf("?");
    }
    printf(";");
}

static void emit_values(const common_params & base, task_params & r) {
    auto sch = server_schema::make_llama_cmpl_schema(base, r);
    int nested = -1;
    for (size_t i = 0; i < sch.size(); i++) {
        auto * f = sch[i].get();
        if (auto * nf = dynamic_cast<server_schema::field_nested*>(f)) {
            auto * b = dynamic_cast<server_schema::field_bool*>(nf->subfields[0].get());
            nested = b->val ? 1 : 0;
            printf("3=%d;", nested);
            continue;
        }
        if (auto * x = dynamic_cast<server_schema::field_num<int32_t>*>(f))       printf("%zu=%d;", i, x->val);
        else if (auto * x = dynamic_cast<server_schema::field_num<int64_t>*>(f))  printf("%zu=%lld;", i, (long long) x->val);
        else if (auto * x = dynamic_cast<server_schema::field_num<uint32_t>*>(f)) printf("%zu=%u;", i, x->val);
        else if (auto * x = dynamic_cast<server_schema::field_num<float>*>(f))    { char b[64]; f32_repr(b, x->val); printf("%zu=%s;", i, b); }
        else if (auto * x = dynamic_cast<server_schema::field_bool*>(f))          printf("%zu=%d;", i, (int) x->val);
        else emit_row((int) i, r);
    }
}

static void dump_tts_values(const server_task & task) {
    char b[64];
    printf("0=%d;", task.tts_inp.data.top_k);
    f32_repr(b, task.tts_inp.data.top_p); printf("1=%s;", b);
    printf("2=%u;", task.tts_inp.data.seed);
    printf("3=%d;", task.params.n_predict);
    f32_repr(b, task.params.sampling.penalty_repeat); printf("4=%s;", b);
    printf("5=%d;", (int) task.params.stream);
}

struct Case { int variant; const char * body; };
'''

CPP_MAIN_HEAD = r'''
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    {
        common_params base;
        task_params p;
        auto tf = server_schema::make_llama_cmpl_schema(base, p);
        const server_schema::field * sub = nullptr;
        for (size_t i = 0; i < tf.size(); i++) {
            dump_field(tf[i].get(), (int) i);
            if (auto * nf = dynamic_cast<server_schema::field_nested*>(tf[i].get())) {
                sub = nf->subfields[0].get();
            }
        }
        if (sub) dump_field(sub, 62);
    }
    {
        server_task task;
        auto tt = server_schema::make_tts_schema(task);
        for (size_t i = 0; i < tt.size(); i++) dump_field(tt[i].get(), (int) i, "U");
    }
    const std::vector<llama_logit_bias> eog = { {100, -INFINITY}, {101, -INFINITY} };
    const Case cases[] = {
__CASES__
    };
    for (int i = 0; i < (int) (sizeof(cases) / sizeof(cases[0])); i++) {
        common_params base = make_base(cases[i].variant);
        printf("C%d|", i);
        try {
            task_params r = server_schema::eval_llama_cmpl_schema(nullptr, base, eog, json::parse(cases[i].body));
            printf("ok|");
            emit_values(base, r);
        } catch (const std::exception & e) {
            printf("err|%s", e.what());
        }
        printf("\n");
    }
    const Case tts[] = {
__TTS_CASES__
    };
    for (int i = 0; i < (int) (sizeof(tts) / sizeof(tts[0])); i++) {
        common_params base = make_base(tts[i].variant);
        server_task task;
        printf("D%d|", i);
        try {
            server_schema::eval_tts_schema(base, task, json::parse(tts[i].body));
            printf("ok|");
            dump_tts_values(task);
        } catch (const std::exception & e) {
            printf("err|%s", e.what());
        }
        printf("\n");
    }
    return 0;
}
'''


def gen_cpp():
    cases = []
    for i, (label, variant, entries) in enumerate(CASES):
        cases.append('        { %d, %s }, // C%d %s' % (variant, cpp_str(json_body(entries)), i, label))
    tts = []
    for i, (label, variant, entries) in enumerate(TTS_CASES):
        tts.append('        { %d, %s }, // D%d %s' % (variant, cpp_str(json_body(entries)), i, label))
    return (CPP_HEAD + CPP_MAIN_HEAD
            .replace('__CASES__', "\n".join(cases))
            .replace('__TTS_CASES__', "\n".join(tts)))


# ----------------------------------------------------------------- Goose ----

def zeros(n):
    return ", ".join(["0"] * n)


def go_text(text):
    """Declare the value buffer and copy a string into it."""
    lines = ["var tv: u8[256] = [%s];" % zeros(256), "var tl: i64 = 0;"]
    if text:
        lines.append('tl = sc_puts(tv, 0, "%s");' % text.replace('"', '\\"'))
    return lines, "tv[0..tl]"


def go_blob(items):
    """Declare the value buffer and fill it with 0xFF-joined strings."""
    lines = ["var tv: u8[256] = [%s];" % zeros(256), "var tl: i64 = 0;"]
    for s in items:
        lines.append('tl = sc_puts(tv, tl, "%s");' % s.replace('"', '\\"'))
        lines.append("tv[tl] = 255;")
        lines.append("tl = tl + 1;")
    return lines, "tv[0..tl]"


def go_entry(e):
    """Returns (row/key/k/iv/fv, build lines, error key).

    The value text is always passed as a mutable slice of one local buffer:
    an export fn gets one concrete signature, so a literal and a slice in the
    same call site would be two specialisations.
    """
    k = e[0]
    if k == "i":
        lines, txt = go_text("")
        return ['schema_find("%s")' % e[1], "0", str(e[2]), "0.0", txt], lines, None
    if k == "f":
        lines, txt = go_text("")
        return ['schema_find("%s")' % e[1], "1", "0", repr(float(e[2])), txt], lines, None
    if k == "b":
        lines, txt = go_text("")
        return ['schema_find("%s")' % e[1], "2", str(e[2]), "0.0", txt], lines, None
    if k == "null":
        lines, txt = go_text("")
        return ['schema_find("%s")' % e[1], "7", "0", "0.0", txt], lines, None
    if k == "s":
        lines, txt = go_text(e[2])
        return ['schema_find("%s")' % e[1], "3", "0", "0.0", txt], lines, None
    if k == "arr":
        lines, txt = go_blob(e[2])
        return ['schema_find("%s")' % e[1], "4", str(len(e[2])), "0.0", txt], lines, None
    if k == "arrnum":
        lines, txt = go_text("")
        return ['schema_find("%s")' % e[1], "5", "0", "0.0", txt], lines, None
    if k == "obj":
        lines, txt = go_text("")
        return ['schema_find("%s")' % e[1], "6", "0", "0.0", txt], lines, None
    if k == "lora":
        lines, txt = go_text(",".join(str(x) for x in e[1]))
        return ['schema_find("lora")', "4", str(len(e[1])), "0.0", txt], lines, None
    if k == "lora_bad":
        lines, txt = go_text("")
        return ['schema_find("lora")', "5", "0", "0.0", txt], lines, None
    if k == "schema":
        js, gr, tc, gtxt = e[1], e[2], e[3], e[4]
        key = "json_schema" if js else "grammar"
        flags = js | (gr << 1) | (tc << 2)
        lines, txt = go_text(gtxt)
        return ['schema_find("%s")' % key, "8", str(flags), "0.0", txt], lines, None
    if k == "nested":
        sub = e[1]
        if sub == "b":
            lines, txt = go_text("")
            return ["3", "10" if e[2] else "11", "0", "0.0", txt], lines, None
        if sub == "null":
            lines, txt = go_text("")
            return ["3", "12", "0", "0.0", txt], lines, None
        if sub == "s":
            lines, txt = go_text(e[2])
            return ["3", "13", "0", "0.0", txt], lines, "include_usage"
        lines, txt = go_text("")
        return ["3", "14", "0", "0.0", txt], lines, None
    raise SystemExit("bad entry " + k)


GO_HEAD = r'''
// Gate driver, generated by tests/gen_schema.py. Calls the private bodies of the
// port (Goose cannot call an export fn from another function).
fn base_variant(BI: i64[:], BF: f32[:], B: i64[:], BANT: u8[:], v: i64) {
    if v == 1 {
        schema_base_cfg(B, 10, 4);
        schema_base_set_i(BI, BF, 10, 8);    // n_keep
        schema_base_set_i(BI, BF, 8, 64);    // n_predict
        schema_base_set_i(BI, BF, 13, 16);   // n_cache_reuse
        schema_base_set_i(BI, BF, 4, 0);     // cache_prompt
        schema_base_set_i(BI, BF, 7, 7);     // sse_ping_interval
        schema_base_set_i(BI, BF, 48, 1);    // reasoning_format AUTO
        schema_base_set_i(BI, BF, 16, 13);   // top_k
        schema_base_set_f(BI, BF, 17, 0.5);  // top_p
        schema_base_set_f(BI, BF, 18, 0.1);  // min_p
        schema_base_set_f(BI, BF, 23, 0.3);  // temperature
        schema_base_set_i(BI, BF, 39, 7);    // seed
        schema_base_set_f(BI, BF, 27, 1.2);  // repeat_penalty
        schema_base_set_f(BI, BF, 31, 1.5);  // dry_base
        schema_base_set_i(BI, BF, 40, 2);    // n_probs
        schema_base_set_i(BI, BF, 34, 1);    // mirostat
        schema_base_set_ant(B, BANT, "\nUser:");
    }
}

fn main() {
    var I: i64[80] = [__Z80__];
    var F: f32[80] = [__Z80__];
    var BI: i64[63] = [__Z63__];
    var BF: f32[63] = [__Z63__];
    var B: i64[8] = [__Z8__];
    var SAM: i64[16] = [__Z16__];
    var KEYN: i64[63] = [__Z63__];
    var ANT: u8[512] = [__Z512__];
    var BANT: u8[512] = [__Z512__];
    var MSG: u8[512] = [__Z512__];
    var out: u8[8192] = [__Z8192__];
    var r: i64 = 0;
    var p: i64 = 0;

    while r < schema_row_count() {
        p = schema_tline(r, 1, out, 0);
        print(out[0..p]);
        r = r + 1;
    }
    r = 0;
    while r < schema_tts_row_count() {
        p = schema_tts_tline(r, out, 0);
        print(out[0..p]);
        r = r + 1;
    }
'''

GO_CASE_HEAD = r'''
    {
        schema_reset(I, F, ANT);
        schema_base_std(BI, BF, B, BANT);
        base_variant(BI, BF, B, BANT, __VAR__);
        schema_defaults(I, F, BI, BF, B, ANT, BANT, SAM, KEYN);
        var done: i64 = 0;
        var rc: i64 = 0;
'''

GO_ENTRY = r'''        if done == 0 {
__BUILD__
            rc = schema_apply(I, F, BI, BF, B, ANT, BANT, SAM, KEYN, MSG, __ROW__, "__KEY2__", __K__, __IV__, __FV__, __TXT__);
            if rc != 0 {
                done = 1;
                p = sc_puts(out, 0, "__PREFIX____ID__|err|Field '__KEY__': ");
                p = sc_copy(out, p, MSG, 0, schema_msg_len(I));
                print(out[0..p]);
            }
        }
'''

GO_TAIL = r'''        if done == 0 {
            p = sc_puts(out, 0, "__PREFIX____ID__|ok|");
            r = 0;
            while r < 62 {
                p = schema_vline(r, I, F, SAM, ANT, out, p);
                r = r + 1;
            }
            print(out[0..p]);
        }
    }
'''

GO_TTS_ENTRY = r'''        if done == 0 {
            rc = schema_tts_apply(I, F, MSG, __ROW__, __K__, __IV__, __FV__);
            if rc != 0 {
                done = 1;
                p = sc_puts(out, 0, "D__ID__|err|Field '__KEY__': ");
                p = sc_copy(out, p, MSG, 0, schema_msg_len(I));
                print(out[0..p]);
            }
        }
'''

GO_TTS_TAIL = r'''        if done == 0 {
            p = sc_puts(out, 0, "D__ID__|ok|");
            r = 0;
            while r < 6 {
                p = schema_tts_vline(r, I, F, out, p);
                r = r + 1;
            }
            print(out[0..p]);
        }
    }
'''


def gen_goose(port_text):
    body = []
    for i, (label, variant, entries) in enumerate(CASES):
        chunk = GO_CASE_HEAD.replace('__VAR__', str(variant))
        for e in entries:
            head, build, key = go_entry(e)
            row, k, iv, fv, txt = head
            if key is None:
                key = e[1] if e[0] in ("i", "f", "b", "null", "s", "arr", "arrnum", "obj") else \
                      ("lora" if e[0].startswith("lora") else "json_schema")
            b = "".join("            %s\n" % ln for ln in build)
            chunk += (GO_ENTRY.replace('__BUILD__', b)
                               .replace('__ROW__', row).replace('__K__', k)
                               .replace('__IV__', iv).replace('__FV__', fv)
                               .replace('__TXT__', txt).replace('__KEY__', key)
                               .replace('__KEY2__', 'stream_options' if row == "3" else key)
                               .replace('__PREFIX__', 'C').replace('__ID__', str(i)))
        chunk += GO_TAIL.replace('__PREFIX__', 'C').replace('__ID__', str(i))
        body.append(chunk)

    tts_body = []
    for i, (label, variant, entries) in enumerate(TTS_CASES):
        chunk = GO_CASE_HEAD.replace('__VAR__', str(variant))
        chunk += "        schema_tts_defaults(I, F, BI);\n"
        for e in entries:
            head, build, key = go_entry(e)
            row, k, iv, fv, txt = head
            if key is None:
                key = e[1] if e[0] in ("i", "f", "b", "null", "s") else e[1]
            row = 'schema_tfind("%s")' % key
            chunk += (GO_TTS_ENTRY.replace('__ROW__', row).replace('__K__', k)
                                    .replace('__IV__', iv).replace('__FV__', fv)
                                    .replace('__KEY__', key).replace('__ID__', str(i)))
        chunk += GO_TTS_TAIL.replace('__ID__', str(i))
        tts_body.append(chunk)

    head = (GO_HEAD.replace('__Z80__', zeros(80)).replace('__Z63__', zeros(63))
                   .replace('__Z8__', zeros(8)).replace('__Z16__', zeros(16))
                   .replace('__Z512__', zeros(512)).replace('__Z8192__', zeros(8192)))
    return port_text + head + "\n".join(body) + "\n" + "\n".join(tts_body) + "\n}\n"


def main():
    root = Path(__file__).resolve().parent.parent
    (root / 'tests').mkdir(parents=True, exist_ok=True)
    (root / 'build').mkdir(parents=True, exist_ok=True)
    (root / 'tests' / 'probe_schema.cpp').write_text(gen_cpp())
    port = (root / 'source' / 'server-schema.goose').read_text()
    (root / 'build' / 'gate_schema.goose').write_text(gen_goose(port))
    rows = ["# id\tlabel\tbase\trequest"]
    for i, (label, variant, entries) in enumerate(CASES):
        rows.append("C%d\t%s\t%d\t%s" % (i, label, variant, json_body(entries)))
    for i, (label, variant, entries) in enumerate(TTS_CASES):
        rows.append("D%d\t%s\t%d\t%s" % (i, label, variant, json_body(entries)))
    (root / 'tests' / 'cases_schema.tsv').write_text("\n".join(rows) + "\n")
    print("wrote tests/probe_schema.cpp, build/gate_schema.goose, tests/cases_schema.tsv,",
          "cases =", len(CASES) + len(TTS_CASES))


if __name__ == '__main__':
    main()
