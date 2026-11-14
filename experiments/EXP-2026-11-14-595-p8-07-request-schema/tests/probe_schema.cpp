// Native oracle for the request-schema slice. Includes the translation unit so
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
        { 0, "{}" }, // C0 empty0
        { 1, "{}" }, // C1 empty1
        { 1, "{\042top_k\042: null, \042stream\042: null, \042verbose\042: null}" }, // C2 nulls
        { 0, "{\042top_k\042: 100}" }, // C3 top_k_100
        { 0, "{\042top_k\042: -5}" }, // C4 top_k_neg
        { 0, "{\042n_predict\042: -5}" }, // C5 n_predict_neg
        { 0, "{\042n_cmpl\042: 0}" }, // C6 n_cmpl_0
        { 0, "{\042n_cmpl\042: 2}" }, // C7 n_cmpl_2_v0
        { 1, "{\042n_cmpl\042: 2}" }, // C8 n_cmpl_2_v1
        { 1, "{\042n_cmpl\042: 5}" }, // C9 n_cmpl_5_v1
        { 0, "{\042n_indent\042: -1}" }, // C10 n_indent_neg
        { 0, "{\042n_keep\042: -2}" }, // C11 n_keep_neg2
        { 0, "{\042n_keep\042: -1}" }, // C12 n_keep_neg1
        { 0, "{\042top_p\042: 2.0}" }, // C13 top_p_hi
        { 0, "{\042top_p\042: -0.5}" }, // C14 top_p_lo
        { 0, "{\042temperature\042: -1.0}" }, // C15 temp_neg
        { 0, "{\042mirostat\042: 3}" }, // C16 mirostat_hi
        { 0, "{\042mirostat\042: -1}" }, // C17 mirostat_lo
        { 0, "{\042adaptive_decay\042: 1.5}" }, // C18 decay_hi
        { 0, "{\042adaptive_decay\042: 0.5}" }, // C19 decay_ok
        { 0, "{\042adaptive_target\042: 2.0}" }, // C20 atarget_hi
        { 0, "{\042seed\042: 42}" }, // C21 seed_42
        { 0, "{\042seed\042: -1}" }, // C22 seed_neg
        { 0, "{\042seed\042: 4294967296}" }, // C23 seed_wrap
        { 0, "{\042top_k\042: 1.5}" }, // C24 top_k_float
        { 0, "{\042top_k\042: \042x\042}" }, // C25 top_k_str
        { 0, "{\042stream\042: \042yes\042}" }, // C26 stream_str
        { 1, "{\042stream\042: true, \042cache_prompt\042: true}" }, // C27 stream_true
        { 0, "{\042t_max_predict_ms\042: 9000000000}" }, // C28 t_max
        { 0, "{\042repeat_last_n\042: -1}" }, // C29 repeat_last_neg
        { 0, "{\042dry_base\042: 0.5}" }, // C30 dry_base_lo
        { 1, "{\042dry_base\042: 2.5}" }, // C31 dry_base_hi
        { 1, "{\042dry_base\042: 1.0}" }, // C32 dry_base_1
        { 0, "{\042response_fields\042: [\042n_predict\042, \042model\042]}" }, // C33 resp_fields
        { 0, "{\042response_fields\042: \042x\042}" }, // C34 resp_fields_str
        { 0, "{\042response_fields\042: [1, 2]}" }, // C35 resp_fields_num
        { 0, "{\042lora\042: []}" }, // C36 lora_empty
        { 0, "{\042lora\042: \042x\042}" }, // C37 lora_str
        { 0, "{\042lora\042: [{\042id\042: 1, \042scale\042: 0.5}]}" }, // C38 lora_one
        { 0, "{\042lora\042: [{\042id\042: -1, \042scale\042: 0.5}, {\042id\042: -1, \042scale\042: 0.5}]}" }, // C39 lora_dups
        { 0, "{\042dry_sequence_breakers\042: [\042a\042, \042bb\042]}" }, // C40 breakers_ok
        { 0, "{\042dry_sequence_breakers\042: []}" }, // C41 breakers_empty
        { 0, "{\042dry_sequence_breakers\042: \042x\042}" }, // C42 breakers_str
        { 0, "{\042dry_sequence_breakers\042: [\042\042]}" }, // C43 breakers_blank
        { 0, "{\042stop\042: [\042a\042, \042b\042]}" }, // C44 stop_arr
        { 1, "{\042stop\042: []}" }, // C45 stop_empty_arr
        { 0, "{\042stop\042: \042END\042}" }, // C46 stop_str
        { 1, "{\042stop\042: \042\042}" }, // C47 stop_empty_str
        { 0, "{\042stop\042: [\042\042, \042a\042]}" }, // C48 stop_filter
        { 0, "{\042stop\042: 5}" }, // C49 stop_num
        { 1, "{\042stop\042: {\042a\042: 1}}" }, // C50 stop_obj
        { 0, "{\042samplers\042: [\042top_k\042, \042top-p\042, \042nucleus\042, \042temp\042]}" }, // C51 samplers_names
        { 0, "{\042samplers\042: [\042TOP_K\042, \042Typ_P\042, \042topnsigma\042]}" }, // C52 samplers_upper
        { 0, "{\042samplers\042: [\042bogus\042, \042minp\042]}" }, // C53 samplers_unknown
        { 0, "{\042samplers\042: \042kyp\042}" }, // C54 samplers_chars
        { 0, "{\042samplers\042: \042kzq\042}" }, // C55 samplers_chars_unknown
        { 1, "{\042samplers\042: 5}" }, // C56 samplers_num
        { 0, "{\042json_schema\042: {}}" }, // C57 json_schema_empty
        { 0, "{\042json_schema\042: {\042type\042:\042object\042}, \042grammar\042: \042root ::= \134\042a\134\042\042}" }, // C58 json_schema_and_grammar
        { 0, "{\042grammar\042: \042root ::= \134\042a\134\042\042}" }, // C59 grammar_user
        { 0, "{\042grammar\042: \042root ::= \134\042a\134\042\042, \042grammar_type\042: \042tool_calls\042}" }, // C60 grammar_tool
        { 0, "{\042grammar\042: \042\042}" }, // C61 grammar_empty
        { 0, "{\042json_schema\042: null}" }, // C62 schema_null
        { 0, "{\042reasoning_format\042: \042deepseek\042}" }, // C63 reasoning_deepseek
        { 0, "{\042reasoning_format\042: \042deepseek-legacy\042}" }, // C64 reasoning_legacy
        { 0, "{\042reasoning_format\042: \042auto\042}" }, // C65 reasoning_auto
        { 0, "{\042reasoning_format\042: \042none\042}" }, // C66 reasoning_none
        { 0, "{\042reasoning_format\042: \042nope\042}" }, // C67 reasoning_bad
        { 0, "{\042reasoning_format\042: 1}" }, // C68 reasoning_num
        { 0, "{\042continue_final_message\042: true}" }, // C69 cont_true
        { 0, "{\042continue_final_message\042: \042content\042}" }, // C70 cont_content
        { 0, "{\042continue_final_message\042: \042reasoning_content\042}" }, // C71 cont_reason
        { 0, "{\042continue_final_message\042: \042x\042}" }, // C72 cont_bad
        { 0, "{\042continue_final_message\042: 1}" }, // C73 cont_num
        { 0, "{\042echo\042: true}" }, // C74 echo_true
        { 0, "{\042echo\042: 1}" }, // C75 echo_num
        { 0, "{\042stream_options\042: {\042include_usage\042: true}}" }, // C76 nested_usage
        { 0, "{\042stream_options\042: {\042include_usage\042: false}}" }, // C77 nested_false
        { 1, "{\042stream_options\042: {\042include_usage\042: null}}" }, // C78 nested_null
        { 0, "{\042stream_options\042: {\042include_usage\042: \042x\042}}" }, // C79 nested_str
        { 0, "{\042stream_options\042: []}" }, // C80 nested_arr
        { 0, "{\042max_tokens\042: 32}" }, // C81 alias_max_tokens
        { 0, "{\042n_predict\042: 10, \042max_tokens\042: 32}" }, // C82 alias_both
        { 0, "{\042max_completion_tokens\042: 16, \042max_tokens\042: 32}" }, // C83 alias_alias_both
        { 1, "{\042n\042: 3}" }, // C84 alias_n
        { 0, "{\042logprobs\042: 3}" }, // C85 alias_logprobs
        { 0, "{\042ignore_eos\042: true}" }, // C86 ignore_eos_t
        { 1, "{\042ignore_eos\042: false}" }, // C87 ignore_eos_f
        { 0, "{\042backend_sampling\042: true, \042post_sampling_probs\042: true, \042grammar_lazy\042: true, \042reasoning_control\042: true}" }, // C88 bools
        { 0, "{\042reasoning_budget_tokens\042: 128, \042reasoning_control\042: true}" }, // C89 budget_tokens
        { 0, "{\042temperature\042: 1}" }, // C90 int_to_float
        { 0, "{\042n_keep\042: 3.9}" }, // C91 float_to_int
        { 0, "{\042n_probs\042: -2147483648}" }, // C92 n_probs_min
        { 0, "{\042min_keep\042: -1}" }, // C93 min_keep_neg
        { 0, "{\042backend_sampling\042: \042x\042}" }, // C94 backend_str
        { 0, "{\042timings_per_token\042: true}" }, // C95 timings_true
        { 0, "{\042n_discard\042: -1}" }, // C96 n_discard_neg
        { 1, "{\042n_cache_reuse\042: 32}" }, // C97 n_cache_v1
        { 0, "{\042dry_allowed_length\042: -1}" }, // C98 dry_allowed_neg
        { 0, "{\042frequency_penalty\042: 0.25, \042presence_penalty\042: 0.5}" }, // C99 pos_freq
        { 0, "{\042stream_options\042: {\042include_usage\042: true}}" }, // C100 nested_extra
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
        { 0, "{}" }, // D0 empty0
        { 0, "{\042top_k\042: 5, \042top_p\042: 0.9, \042max_tokens\042: 64}" }, // D1 all
        { 0, "{\042n_predict\042: -5}" }, // D2 neg
        { 0, "{\042top_k\042: -1}" }, // D3 top_k_neg
        { 0, "{\042top_p\042: 2.0}" }, // D4 top_p_hi
        { 0, "{\042seed\042: -1}" }, // D5 seed_neg
        { 0, "{\042stream\042: true, \042repeat_penalty\042: 1.5}" }, // D6 stream_rep
        { 1, "{}" }, // D7 seed_base
        { 0, "{\042max_tokens\042: 32}" }, // D8 alias_max_tokens
        { 0, "{\042top_k\042: \042x\042}" }, // D9 top_k_str
        { 0, "{\042n_predict\042: 7.9}" }, // D10 n_predict_float
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
