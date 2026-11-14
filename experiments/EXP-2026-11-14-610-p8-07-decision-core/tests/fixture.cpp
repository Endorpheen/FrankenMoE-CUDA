// Gate for the decision core (P8-07 S4).
//
// Native side: a transcription of tools/server/server-decision.cpp on
// nlohmann::ordered_json, the library behind common_json, with the source lines
// cited at every function. Port side: the exported functions of the Goose
// module. Both evaluate the same fixed corpus and the same deterministic random
// documents; no server, model, tokenizer or thread is involved.
#include "g.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using ojson = nlohmann::ordered_json;

// The two runtime hooks the accepted json modules borrow.
extern "C" double gs_json_strtod(sl_u8 s) {
    std::string t((const char *) s.data, (size_t) s.len);
    return std::strtod(t.c_str(), nullptr);
}

extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return std::snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}

static sl_u8 sl(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

template <typename F>
static std::string run(F f) {
    static char b[1 << 20];
    memset(b, 0, 1 << 16);
    sl_u8 o{(uint8_t *) b, (int64_t) sizeof b};
    int64_t n = f(o);
    if (n < 0) {
        return "!" + std::string(b);
    }
    return std::string(b, (size_t) n);
}

static int cases = 0;
static int fails = 0;

static void eq(const char * name, const std::string & want, const std::string & got) {
    cases++;
    if (want == got) {
        printf("OK   %s\n", name);
        return;
    }
    fails++;
    printf("FAIL %s\n  native: %s\n  port:   %s\n", name, want.c_str(), got.c_str());
}

// ── the Python layouts of server-decision.cpp:556-722 ───────────────────────

// decision_utf8_next, 557
static uint32_t py_utf8_next(const std::string & s, size_t & i) {
    const unsigned char c = s[i];
    size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 1;
    if (i + n > s.size()) n = 1;
    uint32_t cp = n == 1 ? c : n == 2 ? (c & 0x1f) : n == 3 ? (c & 0x0f) : (c & 0x07);
    for (size_t k = 1; k < n; k++) cp = (cp << 6) | (s[i + k] & 0x3f);
    i += n;
    return cp;
}

// decision_py_isspace, 572
static bool py_isspace(uint32_t cp) {
    return (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x20) || cp == 0x85 || cp == 0xa0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200a) || cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000;
}

// decision_py_rstrip, 577
static std::string py_rstrip(const std::string & s) {
    size_t end = 0;
    for (size_t i = 0; i < s.size();) {
        if (!py_isspace(py_utf8_next(s, i))) end = i;
    }
    return s.substr(0, end);
}

// decision_py_lstrip, 587
static std::string py_lstrip(const std::string & s) {
    size_t i = 0;
    while (i < s.size()) {
        size_t next = i;
        if (!py_isspace(py_utf8_next(s, next))) break;
        i = next;
    }
    return s.substr(i);
}

// decision_py_escape_cp, 600
static std::string py_escape_cp(uint32_t cp) {
    char b[32];
    if (cp < 0x10000) {
        snprintf(b, sizeof b, "\\u%04x", cp);
        return b;
    }
    cp -= 0x10000;
    snprintf(b, sizeof b, "\\u%04x\\u%04x", 0xd800 + (cp >> 10), 0xdc00 + (cp & 0x3ff));
    return b;
}

// decision_py_float, 609
static std::string py_float(double v) {
    char buf[64];
    for (int prec = 0; prec < 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*e", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    const std::string sci(buf);
    const size_t pos_e = sci.find('e');
    const int exp = std::atoi(sci.c_str() + pos_e + 1);
    std::string sign;
    std::string digits;
    for (size_t i = 0; i < pos_e; i++) {
        if (sci[i] == '-') {
            sign = "-";
        } else if (sci[i] != '.') {
            digits += sci[i];
        }
    }
    const int n = (int) digits.size();
    if (exp < -4 || exp >= 16) {
        return sign + digits.substr(0, 1) + (n > 1 ? "." + digits.substr(1) : "") +
               std::string(1, 'e') + (exp < 0 ? "-" : "+") + (std::abs(exp) < 10 ? "0" : "") + std::to_string(std::abs(exp));
    }
    if (exp >= n - 1) {
        return sign + digits + std::string(exp - (n - 1), '0') + ".0";
    }
    if (exp >= 0) {
        return sign + digits.substr(0, exp + 1) + "." + digits.substr(exp + 1);
    }
    return sign + "0." + std::string(-exp - 1, '0') + digits;
}

// decision_py_str, 643
static void py_str(std::string & out, const std::string & s, bool ensure_ascii) {
    out += '"';
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = s[i];
        switch (c) {
            case '"':  out += "\\\""; i++; continue;
            case '\\': out += "\\\\"; i++; continue;
            case '\b': out += "\\b";  i++; continue;
            case '\f': out += "\\f";  i++; continue;
            case '\n': out += "\\n";  i++; continue;
            case '\r': out += "\\r";  i++; continue;
            case '\t': out += "\\t";  i++; continue;
            default:   break;
        }
        if (c < 0x20 || (ensure_ascii && c == 0x7f)) {
            char b[16];
            snprintf(b, sizeof b, "\\u%04x", c);
            out += b;
            i++;
        } else if (c < 0x80 || !ensure_ascii) {
            out += (char) c;
            i++;
        } else {
            out += py_escape_cp(py_utf8_next(s, i));
        }
    }
    out += '"';
}

// decision_py_dump, 671
static void py_dump(std::string & out, const ojson & val, int indent, int level,
                    const std::string & item_sep, const std::string & key_sep, bool ensure_ascii) {
    const auto newline = [&](int lvl) {
        return indent < 0 ? std::string() : "\n" + std::string(indent * lvl, ' ');
    };
    if (val.is_null()) {
        out += "null";
    } else if (val.is_boolean()) {
        out += val.get<bool>() ? "true" : "false";
    } else if (val.is_number_float()) {
        out += py_float(val.get<double>());
    } else if (val.is_number()) {
        out += val.dump();
    } else if (val.is_string()) {
        py_str(out, val.get<std::string>(), ensure_ascii);
    } else if (val.is_array()) {
        if (val.empty()) {
            out += "[]";
            return;
        }
        out += "[" + newline(level + 1);
        for (size_t i = 0; i < val.size(); i++) {
            if (i > 0) out += item_sep + newline(level + 1);
            py_dump(out, val.at(i), indent, level + 1, item_sep, key_sep, ensure_ascii);
        }
        out += newline(level) + "]";
    } else if (val.is_object()) {
        if (val.empty()) {
            out += "{}";
            return;
        }
        out += "{" + newline(level + 1);
        bool first = true;
        for (const auto & [key, item] : val.items()) {
            if (!first) out += item_sep + newline(level + 1);
            first = false;
            py_str(out, key, ensure_ascii);
            out += key_sep;
            py_dump(out, item, indent, level + 1, item_sep, key_sep, ensure_ascii);
        }
        out += newline(level) + "}";
    }
}

// decision_py_dumps, 718
static std::string py_dumps(const ojson & val, int indent, const std::string & item_sep,
                            const std::string & key_sep, bool ensure_ascii) {
    std::string out;
    py_dump(out, val, indent, 0, item_sep, key_sep, ensure_ascii);
    return out;
}

// json::dump() as the canonical legend uses it: compact, no ensure_ascii
static std::string nl_dump(const ojson & v) {
    return v.dump(-1, ' ', false, ojson::error_handler_t::replace);
}

// ── the value transforms, 420-550 ───────────────────────────────────────────

// decision_replace_text, 420
static ojson replace_text(const ojson & val, const std::string & search, const std::string & replace) {
    if (val.is_string()) {
        std::string str = val.get<std::string>();
        size_t p = 0;
        while (!search.empty() && (p = str.find(search, p)) != std::string::npos) {
            str.replace(p, search.size(), replace);
            p += replace.size();
        }
        return str;
    }
    if (val.is_array()) {
        ojson out = ojson::array();
        for (const auto & item : val) out.push_back(replace_text(item, search, replace));
        return out;
    }
    if (val.is_object()) {
        ojson out = ojson::object();
        for (const auto & [key, item] : val.items()) out[key] = replace_text(item, search, replace);
        return out;
    }
    return val;
}

// decision_sort_keys, 444
static ojson sort_keys(const ojson & val) {
    if (val.is_array()) {
        ojson out = ojson::array();
        for (const auto & item : val) out.push_back(sort_keys(item));
        return out;
    }
    if (val.is_object()) {
        std::map<std::string, ojson> sorted;
        for (const auto & [key, item] : val.items()) sorted[key] = sort_keys(item);
        ojson out = ojson::object();
        for (const auto & [key, item] : sorted) out[key] = item;
        return out;
    }
    return val;
}

// decision_kev_render, 467
static std::string kev_render(const ojson & val, int indent = 0) {
    const std::string pad(2 * indent, ' ');
    if (val.is_null()) return "";
    if (val.is_string()) return val.get<std::string>();
    if (val.is_boolean()) return val.get<bool>() ? "True" : "False";
    if (val.is_array()) {
        std::string out;
        for (const auto & item : val) {
            const std::string text = kev_render(item, indent + 1);
            out += (out.empty() ? "" : "\n") + pad + "- " + text.substr(std::min(text.size(), text.find_first_not_of(" \t\n\r")));
        }
        return out;
    }
    if (val.is_object()) {
        std::string out;
        for (const auto & [key, item] : val.items()) {
            const bool is_nested = item.is_object() || item.is_array();
            out += (out.empty() ? "" : "\n") + pad + key + (is_nested ? ":\n" : ": ") +
                   kev_render(item, is_nested ? indent + 1 : 0);
        }
        return out;
    }
    return val.dump();
}

// server_decision_escape_special_tokens, 499
static std::string escape_special(const std::string & text) {
    const auto is_name = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    };
    std::string out;
    size_t n_done = 0;
    for (size_t i = text.find("<|"); i != std::string::npos; i = text.find("<|", std::max(i + 1, n_done))) {
        size_t j = i + 2;
        while (j < text.size() && is_name(text[j])) j++;
        if (j > i + 2 && text.compare(j, 2, "|>") == 0) {
            out.append(text, n_done, i - n_done);
            out += "<\xC2\xA6";
            out.append(text, i + 2, j - i - 2);
            out += "\xC2\xA6>";
            n_done = j + 2;
        }
    }
    if (n_done == 0) return text;
    out.append(text, n_done, std::string::npos);
    return out;
}

// decision_kev_text, 526
static std::string kev_text(const ojson & val) {
    return escape_special(kev_render(val));
}

// decision_d1omni_escape, 531
static ojson d1omni_escape(const ojson & val) {
    if (val.is_string()) return escape_special(val.get<std::string>());
    if (val.is_array()) {
        ojson out = ojson::array();
        for (const auto & item : val) out.push_back(d1omni_escape(item));
        return out;
    }
    if (val.is_object()) {
        ojson out = ojson::object();
        for (const auto & [key, item] : val.items()) out[escape_special(key)] = d1omni_escape(item);
        return out;
    }
    return val;
}

// ── questions, 216-329 ─────────────────────────────────────────────────────

struct NCfg {
    int  type   = 0;
    bool strict = false;
    bool csort  = false;
    bool ntf    = false; // noul_true_first
    bool nco    = false; // noul_criteria_order
    long nomax  = 52;
};

struct NOpt {
    std::string key;
    ojson       desc;
};

struct NQ {
    std::string           id;
    int                   type = 0;
    ojson                 instr;
    std::vector<NOpt>     opts;
};

static const char * qname(int t) {
    return t == 0 ? "choice" : t == 1 ? "score" : "noul";
}

template <typename T>
static T json_value(const ojson & body, const char * key, const T & dflt) {
    if (body.contains(key) && !body.at(key).is_null()) {
        try {
            return body.at(key).get<T>();
        } catch (const ojson::exception &) {
            return dflt;
        }
    }
    return dflt;
}

// decision_is_payload, 216
static bool is_payload(const ojson & v) {
    return v.is_string() || v.is_object() || v.is_array();
}

// parse_questions, 220
static std::vector<NQ> parse_questions_native(const ojson & body, const NCfg & cfg) {
    const bool is_h2o = cfg.type == 13;
    if (!body.contains("state") ||
        (body.at("state").is_null() && cfg.type != 10 && cfg.type != 11 && cfg.type != 9)) {
        throw std::invalid_argument("\"state\" must be provided");
    }
    if (cfg.strict && !is_payload(body.at("state"))) {
        throw std::invalid_argument("\"state\" must be text, an object or an array");
    }
    if (!body.contains("questions") || !body.at("questions").is_object() || body.at("questions").empty()) {
        throw std::invalid_argument("\"questions\" must be a non-empty object");
    }
    std::vector<NQ> questions;
    for (const auto & [id, q] : body.at("questions").items()) {
        auto err = [&id = id](const std::string & msg) {
            return std::invalid_argument("questions." + id + ": " + msg);
        };
        if (!q.is_object()) {
            throw err("must be an object");
        }
        const ojson instructions = q.contains("instructions") ? q.at("instructions")
                                                             : (is_h2o ? ojson(std::string()) : ojson());
        if (instructions.is_null() && cfg.type != 9 && !is_h2o) {
            throw err("\"instructions\" must be provided");
        }
        if (cfg.strict && (!is_payload(instructions) || instructions == "")) {
            throw err("\"instructions\" must be non-empty text, an object or an array");
        }
        NQ question;
        question.id    = id;
        question.instr = instructions;

        const std::string type_name = json_value(q, "type", std::string(is_h2o ? "choice" : ""));
        const ojson criteria = q.contains("criteria") ? q.at("criteria") : ojson();

        if (type_name == "choice") {
            question.type = 0;
            if (!criteria.is_object() || criteria.empty()) {
                throw err("\"criteria\" must be a non-empty object");
            }
            for (const auto & [key, description] : criteria.items()) {
                if (cfg.strict && (key.empty() || !(description.is_null() || is_payload(description)))) {
                    throw err("option keys must be non-empty, descriptions must be null, text, an object or an array");
                }
                question.opts.push_back({key, description});
            }
            if (cfg.strict && question.opts.size() < 2) {
                throw err("\"criteria\" must have at least 2 options");
            }
            if (cfg.csort) {
                std::sort(question.opts.begin(), question.opts.end(),
                          [](const NOpt & a, const NOpt & b) { return a.key < b.key; });
            }
        } else if (type_name == "score") {
            question.type = 1;
            if (is_h2o && (!criteria.is_array() || criteria.empty())) {
                throw err("\"criteria\" must be a non-empty array");
            }
            if (!is_h2o && (!criteria.is_array() || criteria.size() < 2 || criteria.size() > 10)) {
                throw err("\"criteria\" must be an array of 2 to 10 levels");
            }
            for (size_t i = 0; i < criteria.size(); i++) {
                if (cfg.strict && !is_payload(criteria.at(i))) {
                    throw err("level descriptions must be text, an object or an array");
                }
                question.opts.push_back({std::to_string(i), criteria.at(i)});
            }
        } else if (type_name == "noul") {
            question.type = 2;
            if (!criteria.is_null() && !criteria.is_object()) {
                throw err("\"criteria\" must be an object");
            }
            if (cfg.strict && criteria.is_object()) {
                for (const auto & [key, description] : criteria.items()) {
                    if ((key != "false" && key != "true") || !is_payload(description)) {
                        throw err("\"criteria\" may only describe \"false\" and \"true\" with text, an object or an array");
                    }
                }
            }
            for (const char * key : {"false", "true"}) {
                ojson description;
                if (criteria.is_object() && criteria.contains(key)) {
                    description = criteria.at(key);
                } else if (criteria.is_object() && (cfg.type == 11 || is_h2o)) {
                    const char * alias = std::string(key) == "true" ? "yes" : "no";
                    description = criteria.contains(alias) ? criteria.at(alias) : ojson();
                }
                question.opts.push_back({key, description});
            }
            if (cfg.ntf || (cfg.nco && criteria.is_object() && criteria.size() == 2 &&
                            criteria.begin().key() == "true")) {
                std::swap(question.opts[0], question.opts[1]);
            }
        } else {
            throw err("\"type\" must be one of: choice, score, noul");
        }
        if ((long) question.opts.size() > cfg.nomax) {
            char b[128];
            snprintf(b, sizeof b, "too many options (%zu), this model supports at most %ld",
                     question.opts.size(), cfg.nomax);
            throw err(b);
        }
        questions.push_back(std::move(question));
    }
    return questions;
}

// The canonical questions text the port returns.
static std::string questions_dump(const std::vector<NQ> & qs) {
    ojson arr = ojson::array();
    for (const auto & q : qs) {
        ojson o = ojson::object();
        o["id"]           = q.id;
        o["type"]         = qname(q.type);
        o["instructions"] = q.instr;
        ojson opts = ojson::array();
        for (const auto & op : q.opts) {
            ojson oo = ojson::object();
            oo["key"]         = op.key;
            oo["description"] = op.desc;
            opts.push_back(oo);
        }
        o["options"] = opts;
        arr.push_back(o);
    }
    return py_dumps(arr, -1, ",", ":", false);
}

// ── state, 338-413 ─────────────────────────────────────────────────────────

struct NMedia {
    int         nfiles = 0;
    std::string kinds;
};

// decision_load_image, 338
static void load_image(const ojson & url, NMedia & m) {
    if (!url.is_string() || url.get<std::string>().rfind("data:", 0) != 0) {
        throw std::invalid_argument("images must be data URLs (data:image/...;base64,... or data:audio/...;base64,...)");
    }
    if (m.nfiles >= 8) {
        throw std::invalid_argument("too many images, the maximum is 8");
    }
    m.kinds += url.get<std::string>().rfind("data:audio/", 0) == 0 ? 'a' : 'i';
    m.nfiles++;
}

// decision_load_audio, 349
static void load_audio(const ojson & data, NMedia & m) {
    if (!data.is_string() || data.get<std::string>().rfind("http", 0) == 0 ||
        data.get<std::string>().rfind("file://", 0) == 0) {
        throw std::invalid_argument("audio must be base64 data");
    }
    if (m.nfiles >= 8) {
        throw std::invalid_argument("too many media files, the maximum is 8");
    }
    m.kinds += 'a';
    m.nfiles++;
}

// parse_state, 359, the media decoded here as well (they are counted only)
static ojson parse_state_native(const ojson & body, NMedia & m) {
    if (body.contains("videos") && !body.at("videos").is_null() && !body.at("videos").empty()) {
        throw std::invalid_argument("\"videos\" is not supported");
    }
    for (const char * key : {"files", "images"}) {
        if (!body.contains(key) || body.at(key).is_null()) continue;
        if (!body.at(key).is_array()) {
            throw std::invalid_argument(std::string("\"") + key + "\" must be an array");
        }
        for (const auto & url : body.at(key)) load_image(url, m);
    }
    const ojson & state    = body.at("state");
    const bool    is_wrapped = state.is_object() && state.contains("messages");
    const ojson & messages = is_wrapped ? state.at("messages") : state;
    if (!messages.is_array()) return state;

    ojson messages_out = ojson::array();
    for (const auto & msg : messages) {
        if (!msg.is_object() || !msg.contains("content") || !msg.at("content").is_array()) {
            messages_out.push_back(msg);
            continue;
        }
        ojson content = ojson::array();
        for (const auto & part : msg.at("content")) {
            if (part.is_object() && json_value(part, "type", std::string()) == "image_url" && part.contains("image_url")) {
                const ojson & image_url = part.at("image_url");
                load_image(image_url.is_object() && image_url.contains("url") ? image_url.at("url") : image_url, m);
            } else if (part.is_object() && json_value(part, "type", std::string()) == "input_audio" && part.contains("input_audio")) {
                const ojson input_audio = json_value(part, "input_audio", ojson::object());
                load_audio(input_audio.contains("data") ? input_audio.at("data") : json_value(input_audio, "url", ojson()), m);
            } else {
                content.push_back(part);
            }
        }
        ojson msg_out = msg;
        msg_out["content"] = content;
        messages_out.push_back(msg_out);
    }
    if (!is_wrapped) return messages_out;
    ojson state_out = state;
    state_out["messages"] = messages_out;
    return state_out;
}

static std::string state_dump(const ojson & state, const NMedia & m) {
    return "files=" + std::to_string(m.nfiles) + " kinds=" + m.kinds + "\n" +
           py_dumps(state, -1, ",", ":", false);
}

// ── h2o-lightning, 725-824 ─────────────────────────────────────────────────

// h2o_cut_state, 725, with one token per byte (the tokenizer stays native)
static std::string h2o_cut(const std::string & text, size_t max_state_tokens) {
    if (text.size() <= max_state_tokens) return text;
    if (text.size() <= max_state_tokens) return text;
    const size_t half = max_state_tokens / 2;
    auto text_of = [&](size_t from, size_t to) {
        const std::string piece = text.substr(from, to - from);
        return ojson::parse(ojson(piece).dump(-1, ' ', false, ojson::error_handler_t::replace)).get<std::string>();
    };
    return text_of(0, half) + "\n...\n" + text_of(text.size() - half, text.size());
}

// h2o_quote, 748
static std::string h2o_quote(const std::string & text, const std::vector<std::string> & spellings) {
    std::string quoted_first;
    for (const auto & s : spellings) {
        if (!s.empty() && quoted_first.find(s[0]) == std::string::npos) quoted_first += s[0];
    }
    std::string out;
    for (size_t i = 0; i < text.size();) {
        bool is_quoted = false;
        if (quoted_first.find(text[i]) == std::string::npos) {
            out += text[i++];
            continue;
        }
        for (const auto & spelling : spellings) {
            if (text.compare(i, spelling.size(), spelling) == 0) {
                std::string quoted = spelling;
                for (size_t p = 0; (p = quoted.find('<', p)) != std::string::npos; p += 6) quoted.replace(p, 1, "\\u003c");
                for (size_t p = 0; (p = quoted.find('>', p)) != std::string::npos; p += 6) quoted.replace(p, 1, "\\u003e");
                out += quoted;
                i += spelling.size();
                is_quoted = true;
                break;
            }
        }
        if (!is_quoted) out += text[i++];
    }
    return out;
}

// prepare_state, 774
static ojson prepare_state_native(const ojson & state, int type, const std::vector<std::string> & spellings) {
    if (type != 13) return state;
    const std::string text = state.is_string() ? state.get<std::string>()
                                               : py_dumps(state, -1, ",", ":", false);
    return h2o_quote(h2o_cut(text, 32000), spellings);
}

// render_h2o, 782
static ojson render_h2o_native(const ojson & state, const NQ & q, int type,
                               const std::vector<std::string> & labels,
                               const std::vector<std::string> & spellings) {
    const std::string instructions = py_lstrip(q.instr.is_string()
            ? q.instr.get<std::string>()
            : py_dumps(q.instr, 1, ",", ": ", true));
    ojson options = ojson::array();
    for (size_t i = 0; i < q.opts.size(); i++) {
        const auto & opt = q.opts[i];
        ojson description;
        if (q.type == 1 && opt.desc.is_null()) {
            description = "null";
        } else if (!opt.desc.is_null()) {
            description = h2o_quote(opt.desc.is_string() ? opt.desc.get<std::string>()
                                                         : py_dumps(opt.desc, -1, ", ", ": ", false), spellings);
        }
        const std::string key = h2o_quote(opt.key, spellings);
        if (i + 1 == q.opts.size()) {
            const bool is_key = q.type == 0 && description.is_null();
            if (is_key || description.is_string()) {
                const std::string text = is_key ? key : description.get<std::string>();
                const std::string stripped = py_rstrip(text);
                if (stripped != text) description = stripped.empty() ? std::string(" ") : stripped;
            }
        }
        ojson option = ojson::object();
        option["label"]       = labels[i];
        option["key"]         = key;
        option["description"] = description;
        options.push_back(option);
    }
    ojson inp = ojson::object();
    inp["type"]         = qname(q.type);
    inp["state"]        = state;
    inp["instructions"] = h2o_quote(instructions, spellings);
    inp["options"]      = options;
    return inp;
}

// ── answer, 1442-1582 ──────────────────────────────────────────────────────

// n_variants, 838
static size_t n_variants_native(int type, int qt, size_t nopt) {
    if (type == 2 && qt == 0 && nopt > 1) return 2;
    return 1;
}

// n_outputs, 846
static size_t n_outputs_native(int type, int qt, size_t nopt) {
    if (type == 2 && qt == 2) return 9;
    return nopt;
}

// decision_confidence_choice, 1444
static double conf_choice(const std::vector<double> & probs) {
    if (probs.size() < 2) return 1.0;
    const double uniform = 1.0 / probs.size();
    const double p_max   = *std::max_element(probs.begin(), probs.end());
    return std::max(0.0, (p_max - uniform) / (1.0 - uniform));
}

// decision_confidence_score, 1453
static double conf_score(const std::vector<double> & probs) {
    if (probs.size() < 2) return 1.0;
    const size_t n    = probs.size();
    const size_t mode = std::max_element(probs.begin(), probs.end()) - probs.begin();
    double dist         = 0.0;
    double dist_uniform = 0.0;
    for (size_t i = 0; i < n; i++) {
        dist         += probs[i] * std::fabs((double) i - (double) mode);
        dist_uniform += std::fabs((double) i - (n - 1) / 2.0) / n;
    }
    return std::max(0.0, 1.0 - dist / dist_uniform);
}

// decision_confidence_entropy, 1471
static double conf_entropy(const std::vector<double> & probs) {
    if (probs.size() < 2) return 1.0;
    double entropy = 0.0;
    for (const double p : probs) {
        if (p > 0.0) entropy -= p * std::log(p);
    }
    return std::max(0.0, std::min(1.0, 1.0 - entropy / std::log((double) probs.size())));
}

// get_decision_confidence, 1484
static double confidence_native(int type, int qt, const std::vector<double> & probs) {
    if (type == 8 || type == 9) return conf_entropy(probs);
    switch (qt) {
        case 0: return conf_choice(probs);
        case 1: return conf_score(probs);
        case 2: break;
    }
    throw std::runtime_error("no confidence for this question type");
}

// format_answer, 1496
static ojson format_answer_native(const NQ & q, const std::vector<std::vector<float>> & scores,
                                  int type, float temperature, float itemp, bool has_itemp,
                                  bool has_media, bool canonical_legend, const std::vector<float> & bias) {
    const size_t n = n_outputs_native(type, q.type, q.opts.size());
    if (scores.size() != n_variants_native(type, q.type, q.opts.size())) {
        throw std::runtime_error("decision result does not match the number of variants");
    }
    if (has_media && type == 11) {
        temperature = 1.0f;
    }
    if (has_media && has_itemp) {
        temperature = itemp;
    }
    std::vector<double> probs(n, 0.0);
    for (size_t v = 0; v < scores.size(); v++) {
        const auto & s = scores[v];
        if (s.size() != n) {
            throw std::runtime_error("decision result does not match the number of options");
        }
        if (std::any_of(s.begin(), s.end(), [](float v) { return std::isnan(v); })) {
            throw std::runtime_error("the model could not evaluate the decision");
        }
        std::vector<double> logits(s.begin(), s.end());
        if (q.type == 1 && bias.size() == n) {
            for (size_t i = 0; i < n; i++) logits[i] += bias[i];
        }
        const double logit_max = *std::max_element(logits.begin(), logits.end());
        std::vector<double> p(n);
        double sum = 0.0;
        for (size_t i = 0; i < n; i++) {
            p[i] = std::exp((logits[i] - logit_max) / temperature);
            sum += p[i];
        }
        for (size_t i = 0; i < n; i++) {
            probs[v == 0 ? i : n - 1 - i] += p[i] / sum / scores.size();
        }
    }
    ojson answer = ojson::object();
    answer["type"] = qname(q.type);
    if (q.type == 2) {
        if (type == 2) {
            double expected = 0.0;
            for (size_t i = 0; i < n; i++) expected += probs[i] * i / (n - 1);
            answer["noul"] = expected;
            return answer;
        }
        for (size_t i = 0; i < n; i++) {
            if (q.opts[i].key == "true") answer["noul"] = probs[i];
        }
        return answer;
    }
    ojson probabilities = ojson::object();
    for (size_t i = 0; i < n; i++) probabilities[q.opts[i].key] = probs[i];
    if (q.type == 0) {
        const size_t best = std::max_element(probs.begin(), probs.end()) - probs.begin();
        answer["choice"]        = q.opts[best].key;
        answer["probabilities"] = probabilities;
        answer["confidence"]    = confidence_native(type, q.type, probs);
    } else {
        double expected = 0.0;
        ojson legend = ojson::object();
        for (size_t i = 0; i < n; i++) {
            expected += i * probs[i];
            const ojson & description = q.opts[i].desc;
            legend[q.opts[i].key] = canonical_legend && !description.is_string()
                    ? ojson(sort_keys(description).dump()) : description;
        }
        answer["score"]         = expected;
        answer["legend"]        = legend;
        answer["probabilities"] = probabilities;
        answer["confidence"]    = confidence_native(type, q.type, probs);
    }
    return answer;
}

// ── group_tasks, 1588 ──────────────────────────────────────────────────────

struct NTask {
    int              id = 0;
    std::vector<int> tokens;
    size_t           n_shared = 0;
    std::vector<NTask> children;
};

static std::vector<NTask> group_tasks_native(std::vector<NTask> tasks, size_t n_slots) {
    n_slots = std::max<size_t>(n_slots, 1);
    std::vector<NTask> groups;
    for (size_t i = 0; i < tasks.size(); i += n_slots) {
        const size_t end = std::min(tasks.size(), i + n_slots);
        NTask & parent = tasks[i];
        size_t n_shared = parent.tokens.size() - 1;
        for (size_t j = i + 1; j < end; j++) {
            size_t common = 0;
            while (common < parent.tokens.size() && common < tasks[j].tokens.size() &&
                   parent.tokens[common] == tasks[j].tokens[common]) {
                common++;
            }
            n_shared = std::min(n_shared, common);
            n_shared = std::min(n_shared, tasks[j].tokens.size() - 1);
        }
        if (end - i < 2 || n_shared == 0) {
            for (size_t j = i; j < end; j++) groups.push_back(std::move(tasks[j]));
            continue;
        }
        parent.n_shared = n_shared;
        for (size_t j = i + 1; j < end; j++) {
            parent.children.push_back(std::move(tasks[j]));
        }
        groups.push_back(std::move(parent));
    }
    return groups;
}

static std::string group_dump(const std::vector<NTask> & groups) {
    std::string out;
    for (const auto & g : groups) {
        if (g.children.empty()) {
            out += "T " + std::to_string(g.id) + "\n";
        } else {
            out += "G " + std::to_string(g.id) + " " + std::to_string(g.n_shared) + " " +
                   std::to_string(g.children.size()) + " ";
            for (size_t i = 0; i < g.children.size(); i++) {
                if (i > 0) out += ",";
                out += std::to_string(g.children[i].id);
            }
            out += "\n";
        }
    }
    return out;
}

// ── the corpus ─────────────────────────────────────────────────────────────

struct Case {
    const char * name;
    std::string  body;
    NCfg         cfg;
};

static std::string q_body(const std::string & state, const std::string & questions) {
    return "{\"state\":" + state + ",\"questions\":" + questions + "}";
}

static const std::vector<std::string> SPELLINGS = {"<|endoftext|>", "<|end|>"};

int main() {
    goose_init(0, nullptr);

    // ── dumps, floats, strings, whitespace ────────────────────────────────
    const std::string docs[] = {
        "{\"b\":1,\"a\":2.5,\"c\":[true,false,null,\"x\"]}",
        "{\"z\":{\"y\":[1,2,{\"k\":\"v\"}],\"x\":0},\"a\":[]}",
        "[[],{},[[]],{}]",
        "{\"s\":\"a\\\"b\\\\c\\n\\t\\u0001\"}",
        "{\"u\":\"caf\xC3\xA9\",\"j\":\"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\",\"e\":\"\xF0\x9F\x98\x80\"}",
        "{\"n\":[0.0,-0.0,1.25,1e15,1e16,1e-5,0.1,123.456,1e300]}",
        "{\"t\":\"  pad  \",\"m\":\"a\\u00a0b\"}",
        "{\"\":1,\" \":2}",
        "null",
        "{\"k\":\"<|end|> and <|b|> and <|no close\"}",
        "{\"arr\":[\"\\tlead\",\"  lead\",\"\\nlead\",\"\\rlead\"]}",
        "{\"arr\":[\"\\ttab\",\" x\",[\"  nested\"]]}",
    };
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        char name[64];
        ojson d = ojson::parse(docs[i]);
        snprintf(name, sizeof name, "dump_compact_%zu", i);
        eq(name, py_dumps(d, -1, ",", ":", false), run([&](sl_u8 o) { return dc_py_dumps(sl(docs[i]), -1, 0, o); }));
        snprintf(name, sizeof name, "dump_indent1_%zu", i);
        eq(name, py_dumps(d, 1, ",", ": ", true), run([&](sl_u8 o) { return dc_py_dumps(sl(docs[i]), 1, 1, o); }));
        snprintf(name, sizeof name, "dump_spaces_%zu", i);
        eq(name, py_dumps(d, -1, ", ", ": ", false), run([&](sl_u8 o) { return dc_py_dumps(sl(docs[i]), -2, 0, o); }));
        snprintf(name, sizeof name, "sort_%zu", i);
        eq(name, py_dumps(sort_keys(d), -1, ",", ":", false),
           run([&](sl_u8 o) { return dc_sort_keys(sl(docs[i]), o); }));
        snprintf(name, sizeof name, "kev_%zu", i);
        eq(name, kev_render(d), run([&](sl_u8 o) { return dc_kev_render(sl(docs[i]), o); }));
        snprintf(name, sizeof name, "kevtext_%zu", i);
        eq(name, kev_text(d), run([&](sl_u8 o) { return dc_kev_text(sl(docs[i]), o); }));
        snprintf(name, sizeof name, "d1omni_%zu", i);
        eq(name, py_dumps(d1omni_escape(d), -1, ",", ":", false),
           run([&](sl_u8 o) { return dc_d1omni_escape(sl(docs[i]), o); }));
        snprintf(name, sizeof name, "replace_%zu", i);
        eq(name, py_dumps(replace_text(d, "a", "AA"), -1, ",", ":", false),
           run([&](sl_u8 o) { return dc_replace_text(sl(docs[i]), sl("a"), sl("AA"), o); }));
        snprintf(name, sizeof name, "replace_marker_%zu", i);
        eq(name, py_dumps(replace_text(d, "<|end|>", " "), -1, ",", ":", false),
           run([&](sl_u8 o) { return dc_replace_text(sl(docs[i]), sl("<|end|>"), sl(" "), o); }));
    }

    const double floats[] = {0.0, -0.0, 1.25, 0.1, 1.0 / 3.0, 1e15, 1e16, 1e-5, 123.456, 2.0 / 7.0, 1e300, 5e-324};
    for (double v : floats) {
        char name[64];
        snprintf(name, sizeof name, "py_float_%.3g", v);
        eq(name, py_float(v), run([&](sl_u8 o) { return dc_py_float_of(v, o); }));
    }

    const std::string raw[] = {
        "plain",
        "a\"b\\c\b\f\n\r\t",
        "\x01\x1f\x7f",
        "caf\xC3\xA9 \xE6\x97\xA5 \xF0\x9F\x98\x80",
        "  \t\n lead and trail \r\n ",
        "\xC2\xA0nbsp\xE3\x80\x80ideographic",
    };
    for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); i++) {
        char name[64];
        snprintf(name, sizeof name, "str_plain_%zu", i);
        eq(name, [&] { std::string o; py_str(o, raw[i], true); return o; }(),
           run([&](sl_u8 o) { return dc_py_str_of(sl(raw[i]), 1, o); }));
        snprintf(name, sizeof name, "str_raw_%zu", i);
        eq(name, [&] { std::string o; py_str(o, raw[i], false); return o; }(),
           run([&](sl_u8 o) { return dc_py_str_of(sl(raw[i]), 0, o); }));
        snprintf(name, sizeof name, "space_%zu", i);
        eq(name, [&] {
               std::string o;
               py_str(o, py_lstrip(raw[i]), false);
               o += "\n";
               py_str(o, py_rstrip(raw[i]), false);
               return o;
           }(),
           run([&](sl_u8 o) { return dc_py_space(sl(raw[i]), o); }));
    }

    const std::string toks[] = {
        "<|end|>",
        "a<|end|>b<|end|>c",
        "<|no close",
        "<|>",
        "<|a-b|>",
        "<||>",
        "<<|x|>>",
        "plain text",
    };
    for (size_t i = 0; i < sizeof(toks) / sizeof(toks[0]); i++) {
        char name[64];
        snprintf(name, sizeof name, "escape_%zu", i);
        eq(name, escape_special(toks[i]), run([&](sl_u8 o) { return dc_escape_tokens(sl(toks[i]), o); }));
    }

    // ── h2o cut and quote ────────────────────────────────────────────────
    eq("cut_short", h2o_cut("short state", 32),
       run([&](sl_u8 o) { return dc_h2o_cut(sl("short state"), 32, o); }));
    {
        std::string t;
        for (int i = 0; i < 40; i++) t += "abcdefghij";
        eq("cut_exact", h2o_cut(t, 400), run([&](sl_u8 o) { return dc_h2o_cut(sl(t), 400, o); }));
        eq("cut_ascii", h2o_cut(t, 100), run([&](sl_u8 o) { return dc_h2o_cut(sl(t), 100, o); }));
    }
    {
        // a cut inside a three-byte character on both sides
        std::string t;
        for (int i = 0; i < 10; i++) t += "\xE2\x82\xAC";
        eq("cut_utf8", h2o_cut(t, 11), run([&](sl_u8 o) { return dc_h2o_cut(sl(t), 11, o); }));
    }
    {
        std::string spell;
        for (const auto & s : SPELLINGS) spell += s + "\n";
        const std::string q1 = "x <|end|> y <|endoftext|> z";
        eq("quote_1", h2o_quote(q1, SPELLINGS), run([&](sl_u8 o) { return dc_h2o_quote(sl(q1), sl(spell), o); }));
        const std::string q2 = "<|end|>";
        eq("quote_2", h2o_quote(q2, SPELLINGS), run([&](sl_u8 o) { return dc_h2o_quote(sl(q2), sl(spell), o); }));
        const std::string q3 = "a<|endb<|end|>";
        eq("quote_3", h2o_quote(q3, SPELLINGS), run([&](sl_u8 o) { return dc_h2o_quote(sl(q3), sl(spell), o); }));
    }

    // ── questions ────────────────────────────────────────────────────────
    NCfg loose;
    NCfg strict;
    strict.strict = true;
    NCfg sorted_cfg = strict;
    sorted_cfg.csort = true;
    NCfg h2o;
    h2o.type   = 13;
    h2o.strict = true;
    NCfg omni;
    omni.type   = 11;
    omni.strict = true;

    NCfg h2o_loose;
    h2o_loose.type = 13;
    NCfg omni_loose;
    omni_loose.type = 11;

    const std::vector<Case> qcases = {
        {"q_choice", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Pick\",\"type\":\"choice\","
                                        "\"criteria\":{\"B\":\"bee\",\"A\":\"ay\",\"C\":null}}}"), strict},
        {"q_choice_sorted", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Pick\",\"type\":\"choice\","
                                               "\"criteria\":{\"B\":\"bee\",\"A\":\"ay\",\"C\":null}}}"), sorted_cfg},
        {"q_choice_one", q_body("\"s\"", "{\"q1\":{\"instructions\":\"Pick\",\"type\":\"choice\","
                                        "\"criteria\":{\"A\":\"ay\"}}}"), strict},
        {"q_choice_one_loose", q_body("\"s\"", "{\"q1\":{\"instructions\":\"Pick\",\"type\":\"choice\","
                                              "\"criteria\":{\"A\":\"ay\"}}}"), loose},
        {"q_choice_desc_num", q_body("\"s\"", "{\"q1\":{\"instructions\":\"Pick\",\"type\":\"choice\","
                                             "\"criteria\":{\"A\":1,\"B\":\"b\"}}}"), strict},
        {"q_choice_desc_num_loose", q_body("\"s\"", "{\"q1\":{\"instructions\":\"Pick\",\"type\":\"choice\","
                                                   "\"criteria\":{\"A\":1,\"B\":\"b\"}}}"), loose},
        {"q_score", q_body("[1,2]", "{\"q1\":{\"instructions\":\"Rate\",\"type\":\"score\","
                                     "\"criteria\":[\"bad\",\"ok\",\"good\"]}}"), strict},
        {"q_score_eleven", q_body("[1,2]", "{\"q1\":{\"instructions\":\"Rate\",\"type\":\"score\","
                                          "\"criteria\":[1,2,3,4,5,6,7,8,9,10,11]}}"), strict},
        {"q_score_one", q_body("[1,2]", "{\"q1\":{\"instructions\":\"Rate\",\"type\":\"score\","
                                       "\"criteria\":[1]}}"), strict},
        {"q_score_h2o_one", q_body("[1,2]", "{\"q1\":{\"type\":\"score\",\"criteria\":[\"only\"]}}"), h2o},
        {"q_score_desc_num", q_body("[1,2]", "{\"q1\":{\"instructions\":\"Rate\",\"type\":\"score\","
                                            "\"criteria\":[1,2]}}"), strict},
        {"q_noul", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                       "\"criteria\":{\"false\":\"no\",\"true\":\"yes\"}}}"), strict},
        {"q_noul_true_first", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                                 "\"criteria\":{\"false\":\"no\",\"true\":\"yes\"}}}"), [&] {
             NCfg c = strict;
             c.ntf  = true;
             return c;
         }()},
        {"q_noul_order", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                            "\"criteria\":{\"true\":\"yes\",\"false\":\"no\"}}}"), [&] {
             NCfg c = strict;
             c.nco  = true;
             return c;
         }()},
        {"q_noul_alias", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                            "\"criteria\":{\"no\":\"n\",\"yes\":\"y\"}}}"), omni},
        {"q_noul_alias_loose", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                                  "\"criteria\":{\"no\":\"n\",\"yes\":\"y\"}}}"), omni_loose},
        {"q_noul_alias_h2o", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                                "\"criteria\":{\"no\":\"n\",\"yes\":\"y\"}}}"), h2o_loose},
        {"q_noul_alias_lev", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                                "\"criteria\":{\"no\":\"n\",\"yes\":\"y\"}}}"), [&] {
             NCfg c = loose;
             c.type = 2;
             return c;
         }()},
        {"q_noul_badkey", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\","
                                             "\"criteria\":{\"maybe\":\"m\"}}}"), strict},
        {"q_noul_no_criteria", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"Yes?\",\"type\":\"noul\"}}"), strict},
        {"q_bad_type", q_body("{\"a\":1}", "{\"q1\":{\"instructions\":\"x\",\"type\":\"rank\"}}"), strict},
        {"q_no_state", "{\"questions\":{\"q1\":{\"instructions\":\"x\",\"type\":\"noul\"}}}", strict},
        {"q_state_null", "{\"state\":null,\"questions\":{\"q1\":{\"instructions\":\"x\",\"type\":\"noul\"}}}", strict},
        {"q_state_number", q_body("5", "{\"q1\":{\"instructions\":\"x\",\"type\":\"noul\"}}"), strict},
        {"q_no_questions", "{\"state\":\"s\"}", strict},
        {"q_empty_questions", "{\"state\":\"s\",\"questions\":{}}", strict},
        {"q_questions_array", "{\"state\":\"s\",\"questions\":[1]}", strict},
        {"q_two", q_body("{\"s\":\"text\"}", "{\"q1\":{\"instructions\":{\"k\":[1,2]},\"type\":\"choice\","
                                            "\"criteria\":{\"x\":\"ex\",\"y\":\"why\"}},"
                                            "\"q2\":{\"instructions\":\"how\",\"type\":\"score\","
                                            "\"criteria\":[\"low\",\"high\"]}}"), strict},
        {"q_no_instr", q_body("{\"s\":1}", "{\"q1\":{\"type\":\"choice\",\"criteria\":{\"a\":\"A\",\"b\":\"B\"}}}"), strict},
        {"q_no_instr_h2o", q_body("{\"s\":1}", "{\"q1\":{\"type\":\"choice\",\"criteria\":{\"a\":\"A\",\"b\":\"B\"}}}"), h2o},
        {"q_instr_null_h2o", q_body("{\"s\":1}", "{\"q1\":{\"instructions\":null,\"type\":\"choice\",\"criteria\":{\"a\":\"A\",\"b\":\"B\"}}}"), h2o_loose},
        {"q_no_instr_h2o_loose", q_body("{\"s\":1}", "{\"q1\":{\"type\":\"choice\",\"criteria\":{\"a\":\"A\",\"b\":\"B\"}}}"), h2o_loose},
        {"q_no_instr_vllm3", q_body("{\"s\":1}", "{\"q1\":{\"type\":\"choice\",\"criteria\":{\"a\":\"A\",\"b\":\"B\"}}}"), [&] {
             NCfg c = strict;
             c.type = 9;
             return c;
         }()},
        {"q_instr_num", q_body("{\"s\":1}", "{\"q1\":{\"instructions\":5,\"type\":\"noul\"}}"), strict},
        {"q_instr_empty", q_body("{\"s\":1}", "{\"q1\":{\"instructions\":\"\",\"type\":\"noul\"}}"), strict},
        {"q_limit", q_body("{\"s\":1}", "{\"q1\":{\"instructions\":\"x\",\"type\":\"choice\","
                                       "\"criteria\":{\"a\":1,\"b\":2,\"c\":3}}}"), [&] {
             NCfg c = strict;
             c.nomax = 2;
             return c;
         }()},
    };
    for (const auto & c : qcases) {
        ojson body = ojson::parse(c.body);
        std::string want;
        try {
            want = questions_dump(parse_questions_native(body, c.cfg));
        } catch (const std::exception & e) {
            want = std::string("!") + e.what();
        }
        std::string got = run([&](sl_u8 o) {
            return dc_parse_questions(sl(c.body), c.cfg.type, c.cfg.strict ? 1 : 0, c.cfg.csort ? 1 : 0,
                                      c.cfg.ntf ? 1 : 0, c.cfg.nco ? 1 : 0, c.cfg.nomax, o);
        });
        eq(c.name, want, got);
    }

    // ── state ────────────────────────────────────────────────────────────
    const std::string states[] = {
        "{\"state\":{\"a\":1,\"b\":[true,null]},\"questions\":{\"q\":{\"instructions\":\"x\",\"type\":\"noul\"}}}",
        "{\"state\":[{\"role\":\"user\",\"content\":\"hello\"},{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"hi\"}]}],"
        "\"questions\":{\"q\":{\"instructions\":\"x\",\"type\":\"noul\"}}}",
        "{\"state\":{\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64,AA\"}},"
        "{\"type\":\"text\",\"text\":\"what\"}]}]},\"questions\":{\"q\":{\"instructions\":\"x\",\"type\":\"noul\"}}}",
        "{\"state\":{\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"input_audio\",\"input_audio\":{\"data\":\"AAAA\"}},"
        "{\"type\":\"text\",\"text\":\"and\"}]}]},\"questions\":{\"q\":{\"instructions\":\"x\",\"type\":\"noul\"}}}",
        "{\"state\":\"plain\",\"files\":[\"data:image/png;base64,AA\",\"data:audio/wav;base64,BB\"],"
        "\"questions\":{\"q\":{\"instructions\":\"x\",\"type\":\"noul\"}}}",
    };
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        char name[64];
        snprintf(name, sizeof name, "state_%zu", i);
        ojson body = ojson::parse(states[i]);
        std::string want;
        try {
            NMedia m;
            const ojson st = parse_state_native(body, m);
            want = state_dump(st, m);
        } catch (const std::exception & e) {
            want = std::string("!") + e.what();
        }
        eq(name, want, run([&](sl_u8 o) { return dc_parse_state(sl(states[i]), o); }));
    }
    {
        const std::vector<std::pair<const char *, std::string>> bad = {
            {"state_videos", "{\"state\":\"s\",\"videos\":[1]}"},
            {"state_videos_null", "{\"state\":\"s\",\"videos\":null}"},
            {"state_videos_empty", "{\"state\":\"s\",\"videos\":[]}"},
            {"state_files_num", "{\"state\":\"s\",\"files\":5}"},
            {"state_images_url", "{\"state\":\"s\",\"images\":[\"http://x\"]}"},
            {"state_audio_url", "{\"state\":{\"messages\":[{\"role\":\"u\",\"content\":[{\"type\":\"input_audio\","
                                "\"input_audio\":{\"url\":\"http://x\"}}]}]}}"},
            {"state_audio_string", "{\"state\":{\"messages\":[{\"role\":\"u\",\"content\":[{\"type\":\"input_audio\","
                                   "\"input_audio\":\"AAAA\"}]}]}}"},
            {"state_too_many", "{\"state\":\"s\",\"files\":[\"data:i,1\",\"data:i,2\",\"data:i,3\",\"data:i,4\","
                               "\"data:i,5\",\"data:i,6\",\"data:i,7\",\"data:i,8\",\"data:i,9\"]}"},
            {"state_msg_content_str", "{\"state\":[{\"role\":\"u\",\"content\":\"text\"}]}"},
            {"state_msg_num", "{\"state\":[1,2]}"},
            {"state_wrapped_not_array", "{\"state\":{\"messages\":\"x\",\"a\":1}}"},
        };
        for (const auto & c : bad) {
            ojson body = ojson::parse(c.second);
            std::string want;
            try {
                NMedia m;
                const ojson st = parse_state_native(body, m);
                want = state_dump(st, m);
            } catch (const std::exception & e) {
                want = std::string("!") + e.what();
            }
            eq(c.first, want, run([&](sl_u8 o) { return dc_parse_state(sl(c.second), o); }));
        }
    }

    // ── prepare_state ────────────────────────────────────────────────────
    {
        std::string spell;
        for (const auto & s : SPELLINGS) spell += s + "\n";
        const std::string st[] = {
            "{\"a\":1,\"b\":\"text\"}",
            "\"a string state\"",
            "[1,{\"x\":\"<|end|>\"}]",
        };
        for (size_t i = 0; i < sizeof(st) / sizeof(st[0]); i++) {
            char name[64];
            ojson v = ojson::parse(st[i]);
            snprintf(name, sizeof name, "prepare_plain_%zu", i);
            eq(name, py_dumps(prepare_state_native(v, 2, SPELLINGS), -1, ",", ":", false),
               run([&](sl_u8 o) { return dc_prepare_state(sl(st[i]), 2, sl(spell), o); }));
            snprintf(name, sizeof name, "prepare_h2o_%zu", i);
            eq(name, py_dumps(prepare_state_native(v, 13, SPELLINGS), -1, ",", ":", false),
               run([&](sl_u8 o) { return dc_prepare_state(sl(st[i]), 13, sl(spell), o); }));
        }
    }

    // ── render_h2o ───────────────────────────────────────────────────────
    {
        std::string spell;
        for (const auto & s : SPELLINGS) spell += s + "\n";
        const std::vector<std::string> labels = {"A", "B", "C", "D"};
        struct HCase {
            std::string body;
            NCfg        cfg;
        };
        const std::vector<HCase> hcases = {
            {q_body("{\"k\":\"v\"}", "{\"q1\":{\"instructions\":\"Pick one\",\"type\":\"choice\","
                                        "\"criteria\":{\"a\":\"first\",\"b\":\"second\"}}}"), h2o},
            {q_body("{\"k\":\"v\"}", "{\"q1\":{\"instructions\":{\"z\":1,\"a\":[1,2]},\"type\":\"choice\","
                                        "\"criteria\":{\"a\":null,\"b\":\"trail   \"}}}"), h2o},
            {q_body("\"state text\"", "{\"q1\":{\"instructions\":\"Rate it\",\"type\":\"score\","
                                       "\"criteria\":[\"low\",null,\"high\"]}}"), h2o},
            {q_body("[1,2,3]", "{\"q1\":{\"instructions\":\"Yes\",\"type\":\"noul\","
                                "\"criteria\":{\"false\":\"n\",\"true\":\"y\"}}}"), h2o},
            {q_body("{\"k\":\"v\"}", "{\"q1\":{\"instructions\":\"  lead and <|end|>\",\"type\":\"choice\","
                                        "\"criteria\":{\"a\":\"A\",\"b\":\"   \"}}}"), h2o},
            {q_body("{\"k\":\"v\"}", "{\"q1\":{\"instructions\":null,\"type\":\"choice\","
                                        "\"criteria\":{\"a\":\"A\",\"b\":\"B\"}}}"), h2o_loose},
        };
        for (size_t i = 0; i < hcases.size(); i++) {
            char name[64];
            ojson body = ojson::parse(hcases[i].body);
            snprintf(name, sizeof name, "h2o_%zu", i);
            std::string want;
            try {
                const auto qs = parse_questions_native(body, hcases[i].cfg);
                NMedia m;
                const ojson st = prepare_state_native(parse_state_native(body, m), 13, SPELLINGS);
                want = py_dumps(render_h2o_native(st, qs[0], 13, labels, SPELLINGS), -1, ",", ":", false);
            } catch (const std::exception & e) {
                want = std::string("!") + e.what();
            }
            std::string got = run([&](sl_u8 o) {
                return dc_render_h2o(sl(hcases[i].body), 13, hcases[i].cfg.strict ? 1 : 0, 0, 0, 0, 52, 0,
                                     sl("A,B,C,D"), sl(spell), o);
            });
            eq(name, want, got);
        }
    }

    // ── variants, outputs, confidence ────────────────────────────────────
    {
        const int types[] = {0, 2, 3, 9, 13};
        const int qts[]   = {0, 1, 2};
        const long nopts[] = {1, 2, 9};
        for (int t : types) {
            for (int qt : qts) {
                for (long n : nopts) {
                    char name[64];
                    snprintf(name, sizeof name, "variants_%d_%d_%ld", t, qt, n);
                    eq(name, std::to_string(n_variants_native(t, qt, (size_t) n)),
                       std::to_string(dc_n_variants(t, qt, n)));
                    snprintf(name, sizeof name, "outputs_%d_%d_%ld", t, qt, n);
                    eq(name, std::to_string(n_outputs_native(t, qt, (size_t) n)),
                       std::to_string(dc_n_outputs(t, qt, n)));
                }
            }
        }
    }
    {
        const std::vector<std::pair<std::pair<int, int>, std::string>> conf = {
            {{2, 0}, "0.6,0.25,0.15"},
            {{2, 0}, "0.5,0.5"},
            {{2, 0}, "1"},
            {{2, 1}, "0.1,0.2,0.6,0.1"},
            {{2, 1}, "0.25,0.25,0.25,0.25"},
            {{8, 0}, "0.6,0.25,0.15"},
            {{9, 1}, "0.1,0.2,0.6,0.1"},
            {{2, 2}, "0.5,0.5"},
            {{8, 2}, "0.5,0.25,0.25"},
            {{9, 0}, "0.5,0.25,0.25"},
        };
        for (size_t i = 0; i < conf.size(); i++) {
            char name[64];
            snprintf(name, sizeof name, "conf_%zu", i);
            std::vector<double> pr;
            {
                size_t p = 0;
                while (p <= conf[i].second.size()) {
                    size_t e = conf[i].second.find(',', p);
                    if (e == std::string::npos) e = conf[i].second.size();
                    pr.push_back(std::stod(conf[i].second.substr(p, e - p)));
                    if (e == conf[i].second.size()) break;
                    p = e + 1;
                }
            }
            std::string want;
            try {
                want = py_float(confidence_native(conf[i].first.first, conf[i].first.second, pr));
            } catch (const std::exception & e) {
                want = std::string("!") + e.what();
            }
            eq(name, want, run([&](sl_u8 o) {
                return dc_confidence(conf[i].first.first, conf[i].first.second, sl(conf[i].second), o);
            }));
        }
    }

    // ── format_answer ────────────────────────────────────────────────────
    {
        struct ACase {
            const char * name;
            std::string  body;
            NCfg         cfg;
            std::string  scores;
            float        temp   = 1.0f;
            float        itemp  = -1.0f;
            bool         media  = false;
            bool         canon  = false;
            std::string  bias;
        };
        auto bias_of = [](const std::string & t) {
            std::vector<float> b;
            size_t p = 0;
            while (!t.empty() && p <= t.size()) {
                size_t e = t.find(',', p);
                if (e == std::string::npos) e = t.size();
                b.push_back(std::stof(t.substr(p, e - p)));
                if (e == t.size()) break;
                p = e + 1;
            }
            return b;
        };
        auto scores_of = [](const std::string & t) {
            std::vector<std::vector<float>> out;
            size_t p = 0;
            while (p <= t.size()) {
                size_t e = t.find(';', p);
                if (e == std::string::npos) e = t.size();
                std::vector<float> v;
                size_t q = p;
                while (q <= e) {
                    size_t c = t.find(',', q);
                    if (c == std::string::npos || c > e) c = e;
                    v.push_back(std::stof(t.substr(q, c - q)));
                    if (c == e) break;
                    q = c + 1;
                }
                out.push_back(v);
                if (e == t.size()) break;
                p = e + 1;
            }
            return out;
        };
        NCfg lev;
        lev.type   = 2;
        lev.strict = true;
        const std::string choice_body = q_body("{\"s\":1}", "{\"q1\":{\"instructions\":\"Pick\",\"type\":\"choice\","
                                                      "\"criteria\":{\"x\":\"ex\",\"y\":\"why\"}}}");
        const std::string score_body = q_body("{\"s\":1}", "{\"q1\":{\"instructions\":\"Rate\",\"type\":\"score\","
                                                     "\"criteria\":[\"low\",{\"k\":2},\"high\"]}}");
        const std::string noul_body = q_body("{\"s\":1}", "{\"q1\":{\"instructions\":\"Yes\",\"type\":\"noul\","
                                                    "\"criteria\":{\"false\":\"n\",\"true\":\"y\"}}}");
        const std::string num_body = q_body("{\"s\":1}", "{\"q1\":{\"instructions\":\"Rate\",\"type\":\"score\","
                                                   "\"criteria\":[1.5,\"mid\",2,0.125]}}");
        const std::vector<ACase> ac = {
            {"ans_choice", choice_body, strict, "2.0,1.0", 1.0f, -1.0f, false, false, ""},
            {"ans_choice_lev", choice_body, lev, "2.0,1.0;1.0,2.0", 1.0f, -1.0f, false, false, ""},
            {"ans_choice_hot", choice_body, strict, "10.0,-4.0", 0.5f, -1.0f, false, false, ""},
            {"ans_choice_media", choice_body, [&] {
                 NCfg c = strict;
                 c.type = 11;
                 return c;
             }(), "3.0,1.0", 0.7f, 2.0f, true, false, ""},
            {"ans_score", score_body, strict, "1.0,2.0,3.0", 1.0f, -1.0f, false, false, ""},
            {"ans_score_bias", score_body, strict, "1.0,2.0,3.0", 1.0f, -1.0f, false, false, "0.5,-0.25,0.0"},
            {"ans_score_canon", score_body, strict, "1.0,2.0,3.0", 1.0f, -1.0f, false, true, ""},
            {"ans_noul", noul_body, strict, "1.0,3.0", 1.0f, -1.0f, false, false, ""},
            {"ans_noul_lev", noul_body, lev, "0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0", 1.0f, -1.0f, false, false, ""},
            {"ans_score_num", num_body, loose, "0.5,1.0,2.0,0.25", 1.0f, -1.0f, false, false, ""},
            {"ans_score_num_canon", num_body, loose, "0.5,1.0,2.0,0.25", 1.0f, -1.0f, false, true, ""},
            {"ans_nan", choice_body, strict, "nan,1.0", 1.0f, -1.0f, false, false, ""},
            {"ans_short", choice_body, strict, "2.0", 1.0f, -1.0f, false, false, ""},
            {"ans_variants", choice_body, lev, "2.0,1.0", 1.0f, -1.0f, false, false, ""},
        };
        for (const auto & c : ac) {
            ojson body = ojson::parse(c.body);
            std::string want;
            try {
                const auto qs = parse_questions_native(body, c.cfg);
                const ojson ans = format_answer_native(qs[0], scores_of(c.scores), c.cfg.type, c.temp, c.itemp,
                                                       c.itemp >= 0.0f, c.media, c.canon, bias_of(c.bias));
                want = py_dumps(ans, -1, ",", ":", false);
            } catch (const std::exception & e) {
                want = std::string("!") + e.what();
            }
            std::string got = run([&](sl_u8 o) {
                return dc_format_answer(sl(c.body), c.cfg.type, c.cfg.strict ? 1 : 0, c.cfg.csort ? 1 : 0,
                                        c.cfg.ntf ? 1 : 0, c.cfg.nco ? 1 : 0, c.cfg.nomax, 0, sl(c.scores),
                                        (double) c.temp, (double) c.itemp, c.media ? 1 : 0, c.canon ? 1 : 0,
                                        sl(c.bias), o);
            });
            eq(c.name, want, got);
        }
    }

    // ── group_tasks ──────────────────────────────────────────────────────
    {
        struct GCase {
            const char * name;
            const char * tokens;
            size_t       slots;
        };
        const GCase gc[] = {
            {"grp_one", "1:1,2,3", 4},
            {"grp_shared", "1:1,2,3,4\n2:1,2,3,5\n3:1,2,9", 4},
            {"grp_split", "1:1,2,3,4\n2:9,8,7\n3:1,2,3,4\n4:1,2,3,9", 2},
            {"grp_slots1", "1:1,2,3\n2:1,2,4", 1},
            {"grp_zero_shared", "1:1,2,3\n2:4,5,6", 4},
            {"grp_prefix_one", "1:7,1,2,3\n2:7,1,2,9", 3},
            {"grp_three", "1:5,5,5,1\n2:5,5,5,2\n3:5,5,5,3", 8},
            {"grp_mixed", "1:1,2,3\n2:1,2,3\n3:8,9,10\n4:8,9,11\n5:1,2,3", 3},
        };
        for (const auto & c : gc) {
            std::vector<NTask> tasks;
            std::string t = c.tokens;
            size_t p = 0;
            while (p <= t.size()) {
                size_t e = t.find('\n', p);
                if (e == std::string::npos) e = t.size();
                const std::string line = t.substr(p, e - p);
                if (!line.empty()) {
                    const size_t colon = line.find(':');
                    NTask task;
                    task.id = std::stoi(line.substr(0, colon));
                    size_t q = colon + 1;
                    while (q <= line.size()) {
                        size_t cm = line.find(',', q);
                        if (cm == std::string::npos) cm = line.size();
                        task.tokens.push_back(std::stoi(line.substr(q, cm - q)));
                        if (cm == line.size()) break;
                        q = cm + 1;
                    }
                    tasks.push_back(task);
                }
                if (e == t.size()) break;
                p = e + 1;
            }
            eq(c.name, group_dump(group_tasks_native(tasks, c.slots)),
               run([&](sl_u8 o) { return dc_group_tasks(sl(t), (int64_t) c.slots, o); }));
        }
    }

    // ── markers ──────────────────────────────────────────────────────────
    eq("markers",
       "D1OMNI_MARKER=<<d1omni:\n"
       "D1OMNI_SEP=<<d1omni:sep>>\n"
       "D1OMNI_MARK_STATE=<<d1omni:state>>\n"
       "D1OMNI_MARK_QUESTION=<<d1omni:question>>\n"
       "D1OMNI_MARK_OPTION=<<d1omni:option>>\n"
       "D1OMNI_MAX_TOKENS=16384\n"
       "D1OMNI_MAX_TOKENS_IMAGE=896\n"
       "D1OMNI_MAX_TOKENS_AUDIO=15360\n"
       "CLEF_MARKER=<<clef:\n"
       "CLEF_SEP=<<clef:sep>>\n"
       "CLEF_MARK_QUESTION=<<clef:question>>\n"
       "CLEF_MARK_OPTION=<<clef:option>>\n"
       "H2O_MAX_STATE_TOKENS=32000",
       run([&](sl_u8 o) { return dc_markers(o); }));

    // ── deterministic random documents ───────────────────────────────────
    {
        uint64_t seed = 0x9E3779B97F4A7C15ull;
        auto rnd = [&]() {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            return (uint32_t) (seed >> 33);
        };
        auto pick = [&](const std::vector<std::string> & v) { return v[rnd() % v.size()]; };
        const std::vector<std::string> keys = {"alpha", "Beta", "g", "k k", "z9", "\xC3\xA9"};
        const std::vector<std::string> vals = {"text", "two words", "  pad  ", "<|end|>", ""};
        std::function<std::string(int)> node = [&](int depth) -> std::string {
            const uint32_t k = rnd() % (depth > 0 ? 6 : 4);
            if (k == 0) return std::to_string((int) (rnd() % 200) - 100);
            if (k == 1) return py_dumps(ojson(1.0 + (double) (rnd() % 1000) / 8.0), -1, ",", ":", false);
            if (k == 2) return rnd() % 2 ? "true" : "null";
            if (k == 3) return [&] { std::string o; py_str(o, pick(vals), false); return o; }();
            if (k == 4) {
                std::string s = "[";
                const int n = 1 + (int) (rnd() % 3);
                for (int i = 0; i < n; i++) {
                    if (i) s += ",";
                    s += node(depth - 1);
                }
                return s + "]";
            }
            std::string s = "{";
            const int n = 1 + (int) (rnd() % 3);
            for (int i = 0; i < n; i++) {
                if (i) s += ",";
                std::string key;
                py_str(key, pick(keys), false);
                s += key + ":" + node(depth - 1);
            }
            return s + "}";
        };
        for (int i = 0; i < 10; i++) {
            const std::string doc = node(2);
            ojson d = ojson::parse(doc);
            char name[64];
            snprintf(name, sizeof name, "rnd_kev_%d", i);
            eq(name, kev_render(d), run([&](sl_u8 o) { return dc_kev_render(sl(doc), o); }));
            snprintf(name, sizeof name, "rnd_dump_%d", i);
            eq(name, py_dumps(d, -1, ",", ":", false), run([&](sl_u8 o) { return dc_py_dumps(sl(doc), -1, 0, o); }));
            snprintf(name, sizeof name, "rnd_dump1_%d", i);
            eq(name, py_dumps(d, 1, ",", ": ", true), run([&](sl_u8 o) { return dc_py_dumps(sl(doc), 1, 1, o); }));
            snprintf(name, sizeof name, "rnd_sort_%d", i);
            eq(name, py_dumps(sort_keys(d), -1, ",", ":", false),
               run([&](sl_u8 o) { return dc_sort_keys(sl(doc), o); }));
            snprintf(name, sizeof name, "rnd_esc_%d", i);
            eq(name, py_dumps(d1omni_escape(d), -1, ",", ":", false),
               run([&](sl_u8 o) { return dc_d1omni_escape(sl(doc), o); }));
            snprintf(name, sizeof name, "rnd_rep_%d", i);
            eq(name, py_dumps(replace_text(d, "text", "TEXT"), -1, ",", ":", false),
               run([&](sl_u8 o) { return dc_replace_text(sl(doc), sl("text"), sl("TEXT"), o); }));
        }
    }

    printf("FG_DECISION cases=%d failures=%d\n", cases, fails);
    return fails == 0 ? 0 : 1;
}
