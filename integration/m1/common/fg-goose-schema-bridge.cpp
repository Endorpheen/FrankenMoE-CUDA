// Goose bridge for the JSON-schema to GBNF converter (staged S1.1 trial).
//
// Built only when FG_GOOSE_JSON_SCHEMA=ON. P11-04 D1 deleted the native
// converter body; json-schema-to-grammar.cpp now has one path, this bridge, and
// the removed body survives as the frozen oracle of the S1.1 gate. This file is
// the only consumer of the Goose module.
//
// Contract: the Goose module (goose/json/format-grammar.goose, export
// json_schema_convert) does not parse JSON. The host flattens the parsed
// schema into the module's flat node table here:
//   meta   - 11 int64 per node (kind / unsupported / dump id / value range /
//            property range / items / bounds / additionalProperties mode /
//            $ref target);
//   props  - 4 int64 per property (child id, name id, dumped name id, required);
//   vals   - node ids for anyOf/type-array/allOf/enum/prefixItems;
//   strs   - one byte pool with a (offset, length) pair per string id.
// The flattening mirrors the reference differential fixture
// (experiments/EXP-2026-11-14-570-p7-08-schema/tests/fixture.cpp); it is the
// remaining seam of this stage and is not itself a Goose module.
//
// No model, no GPU, no server: this is a library-level seam.

#include "json-schema-to-grammar.h"
#include "goose-host.h" // fg_goose_host_ensure_init

#include "json.h"

#include "json-schema-grammar.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// ---- flat contract constants (must match goose/json/format-grammar.goose) --

namespace {

const int64_t K_ANY        = 0;
const int64_t K_REF        = 1;
const int64_t K_ANY_OF     = 2;
const int64_t K_ALLOF      = 3;
const int64_t K_CONST      = 4;
const int64_t K_ENUM       = 5;
const int64_t K_NULL       = 6;
const int64_t K_BOOLEAN    = 7;
const int64_t K_NUMBER     = 8;
const int64_t K_INTEGER    = 9;
const int64_t K_STRING     = 10;
const int64_t K_ARRAY      = 11;
const int64_t K_OBJECT     = 13;
const int64_t K_EMPTY      = 14;
const int64_t K_STRING_LEN = 15;
const int64_t K_INT_BOUND  = 16;
const int64_t K_TYPE_ARR   = 17;
const int64_t K_PATTERN    = 18;
const int64_t K_TUPLE      = 19;
const int64_t K_FORMAT     = 20;

const int64_t MS = 11;

const int64_t I64_MIN = (-9223372036854775807LL - 1);
const int64_t I64_MAX = 9223372036854775807LL;
const int64_t I32_MAX = 2147483647LL;

struct Flat {
    std::vector<int64_t> meta;
    std::vector<int64_t> props;
    std::vector<int64_t> vals;
    std::vector<std::string> strings;
    std::map<std::string, int> sind;
    std::map<std::string, int> built;
    int64_t root = -1;

    int64_t sid(const std::string & s) {
        auto it = sind.find(s);
        if (it != sind.end()) {
            return it->second;
        }
        const int id = (int) strings.size();
        strings.push_back(s);
        sind[s] = id;
        return id;
    }

    int64_t new_node() {
        const int64_t id = (int64_t) meta.size() / MS;
        meta.resize(meta.size() + MS, 0);
        return id;
    }
};

const common_json * resolve_pointer(const common_json & doc, const std::string & frag) {
    const common_json * cur = &doc;
    if (frag.empty()) {
        return cur;
    }
    size_t p = 0;
    while (p < frag.size()) {
        size_t q = frag.find('/', p + 1);
        if (q == std::string::npos) {
            q = frag.size();
        }
        const std::string sel = frag.substr(p + 1, q - p - 1);
        if (cur->is_object() && cur->contains(sel)) {
            cur = &cur->at(sel);
        } else if (cur->is_array()) {
            size_t idx = 0;
            try {
                idx = (size_t) std::stoull(sel);
            } catch (...) {
                return nullptr;
            }
            if (idx >= cur->size()) {
                return nullptr;
            }
            cur = &cur->at(idx);
        } else {
            return nullptr;
        }
        p = q;
    }
    return cur;
}

std::set<std::string> string_set(const common_json & arr) {
    std::set<std::string> s;
    if (arr.is_array()) {
        for (const auto & v : arr) {
            if (v.is_string()) {
                s.insert(v.get<std::string>());
            }
        }
    }
    return s;
}

int64_t build_node(Flat & f, const common_json & doc, const common_json & s, const std::string & path);

void fill(Flat & f, int64_t id, int64_t kind, int64_t unsupported, int64_t dump_sid,
          int64_t vf, int64_t vn, int64_t pf, int64_t pc,
          int64_t items, int64_t mn, int64_t mx, int64_t addk,
          int64_t ref_target, int64_t ref_sid) {
    int64_t * m = &f.meta[id * MS];
    m[0] = kind;
    m[1] = unsupported;
    m[2] = dump_sid;
    m[3] = (kind == K_REF) ? ref_target : vf;
    m[4] = (kind == K_REF) ? ref_sid : vn;
    m[5] = pf;
    m[6] = pc;
    m[7] = items;
    m[8] = mn;
    m[9] = mx;
    m[10] = addk;
}

int64_t build_node(Flat & f, const common_json & doc, const common_json & s, const std::string & path) {
    auto found = f.built.find(path);
    if (found != f.built.end()) {
        return found->second;
    }

    int64_t id = f.new_node();
    f.built[path] = id;
    const int64_t dump_sid = f.sid(s.dump());
    const bool has_type = s.contains("type");
    const bool type_null = !has_type || s["type"].is_null();
    const bool type_object = has_type && s["type"].is_string() && s["type"].get<std::string>() == "object";
    const bool type_array = has_type && s["type"].is_string() && s["type"].get<std::string>() == "array";
    const bool type_string = has_type && s["type"].is_string() && s["type"].get<std::string>() == "string";
    const bool type_integer = has_type && s["type"].is_string() && s["type"].get<std::string>() == "integer";

    // 1. $ref
    if (s.contains("$ref") && s["$ref"].is_string()) {
        const std::string ref = s["$ref"].get<std::string>();
        int64_t target = -1;
        int64_t unsupported = 0;
        if (ref.rfind("#/", 0) == 0 || ref == "#") {
            const std::string frag = ref == "#" ? "" : ref.substr(1);
            const common_json * t = resolve_pointer(doc, frag);
            if (t) {
                target = build_node(f, doc, *t, frag);
            } else {
                unsupported = 1;
            }
        } else {
            unsupported = 1;
        }
        fill(f, id, K_REF, unsupported, dump_sid, 0, 0, 0, 0, -1, 0, 0, 0, target, f.sid(ref));
        return id;
    }

    // 2. oneOf / anyOf
    if (s.contains("oneOf") || s.contains("anyOf")) {
        const common_json & alts = s.contains("oneOf") ? s["oneOf"] : s["anyOf"];
        const std::string key = s.contains("oneOf") ? "oneOf" : "anyOf";
        const int64_t vf = (int64_t) f.vals.size();
        const int64_t n = alts.is_array() ? (int64_t) alts.size() : 0;
        f.vals.resize((size_t) (vf + n), 0);
        for (int64_t i = 0; i < n; ++i) {
            f.vals[(size_t) (vf + i)] = build_node(f, doc, alts.at((size_t) i), path + "/" + key + "/" + std::to_string(i));
        }
        fill(f, id, K_ANY_OF, 0, dump_sid, vf, n, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 3. type array
    if (has_type && s["type"].is_array()) {
        const int64_t vf = (int64_t) f.vals.size();
        const int64_t n = (int64_t) s["type"].size();
        f.vals.resize((size_t) (vf + n), 0);
        for (int64_t i = 0; i < n; ++i) {
            common_json copy(s);
            copy["type"] = s["type"].at((size_t) i);
            f.vals[(size_t) (vf + i)] = build_node(f, doc, copy, path + "@type" + std::to_string(i));
        }
        fill(f, id, K_TYPE_ARR, 0, dump_sid, vf, n, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 4. const
    if (s.contains("const")) {
        fill(f, id, K_CONST, 0, f.sid(s["const"].dump()), 0, 0, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 5. enum
    if (s.contains("enum") && s["enum"].is_array()) {
        const int64_t vf = (int64_t) f.vals.size();
        int64_t n = 0;
        for (const auto & v : s["enum"]) {
            f.vals.push_back(f.sid(v.dump()));
            ++n;
        }
        fill(f, id, K_ENUM, 0, dump_sid, vf, n, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 6. object
    const bool add_present = s.contains("additionalProperties");
    const bool add_true = add_present && s["additionalProperties"].is_boolean() && s["additionalProperties"].get<bool>();
    const bool add_false = add_present && s["additionalProperties"].is_boolean() && !s["additionalProperties"].get<bool>();
    const bool add_object = add_present && s["additionalProperties"].is_object();
    const bool has_props = s.contains("properties");
    if ((type_null || type_object) && (has_props || (add_present && !add_true))) {
        const int64_t pf = (int64_t) f.props.size() / 4;
        int64_t pc = 0;
        std::set<std::string> req;
        if (s.contains("required")) {
            req = string_set(s["required"]);
        }
        if (has_props && s["properties"].is_object()) {
            for (const auto & kv : s["properties"].items()) {
                const int64_t child = build_node(f, doc, kv.value(), path + "/properties/" + kv.key());
                f.props.push_back(child);
                f.props.push_back(f.sid(kv.key()));
                f.props.push_back(f.sid(common_json(kv.key()).dump()));
                f.props.push_back(req.count(kv.key()) ? 1 : 0);
                ++pc;
            }
        }
        const int64_t addk = add_present ? (add_true ? 1 : add_false ? 2 : add_object ? 3 : 0) : 0;
        int64_t add_child = -1;
        if (add_object) {
            add_child = build_node(f, doc, s["additionalProperties"], path + "/additionalProperties");
        }
        fill(f, id, K_OBJECT, 0, dump_sid, 0, 0, pf, pc, add_child, 0, 0, addk, -1, 0);
        return id;
    }

    // 7. allOf
    if ((type_null || type_object || type_string) && s.contains("allOf") && s["allOf"].is_array()) {
        const int64_t vf = (int64_t) f.vals.size();
        const int64_t n = (int64_t) s["allOf"].size();
        f.vals.resize((size_t) (vf + n), 0);
        for (int64_t i = 0; i < n; ++i) {
            f.vals[(size_t) (vf + i)] = build_node(f, doc, s["allOf"].at((size_t) i), path + "/allOf/" + std::to_string(i));
        }
        fill(f, id, K_ALLOF, 0, dump_sid, vf, n, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 8. array
    if ((type_null || type_array) && (s.contains("items") || s.contains("prefixItems"))) {
        const bool use_items = s.contains("items");
        const common_json & it = use_items ? s["items"] : s["prefixItems"];
        const std::string key = use_items ? "items" : "prefixItems";
        if (it.is_array()) {
            const int64_t vf = (int64_t) f.vals.size();
            const int64_t n = (int64_t) it.size();
            f.vals.resize((size_t) (vf + n), 0);
            for (int64_t i = 0; i < n; ++i) {
                f.vals[(size_t) (vf + i)] = build_node(f, doc, it.at((size_t) i), path + "/" + key + "/" + std::to_string(i));
            }
            fill(f, id, K_TUPLE, 0, dump_sid, vf, n, 0, 0, -1, 0, 0, 0, -1, 0);
            return id;
        }
        const int64_t items = build_node(f, doc, it, path + "/" + key);
        const int64_t mn = s.contains("minItems") && s["minItems"].is_number_integer() ? s["minItems"].get<int64_t>() : 0;
        const int64_t mx = s.contains("maxItems") && s["maxItems"].is_number_integer() ? s["maxItems"].get<int64_t>() : I32_MAX;
        fill(f, id, K_ARRAY, 0, dump_sid, 0, 0, 0, 0, items, mn, mx, 0, -1, 0);
        return id;
    }

    // 9. pattern
    if ((type_null || type_string) && s.contains("pattern") && s["pattern"].is_string()) {
        fill(f, id, K_PATTERN, 0, dump_sid, f.sid(s["pattern"].get<std::string>()), 0, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 10. supported string formats; anything else falls through
    if ((type_null || type_string) && s.contains("format") && s["format"].is_string()) {
        const std::string fmt = s["format"].get<std::string>();
        const bool uuid = fmt == "uuid" ||
            (fmt.size() == 5 && fmt.compare(0, 4, "uuid") == 0 && fmt[4] >= '1' && fmt[4] <= '5');
        if (uuid || fmt == "date" || fmt == "time" || fmt == "date-time") {
            fill(f, id, K_FORMAT, 0, dump_sid, f.sid(fmt), 0, 0, 0, -1, 0, 0, 0, -1, 0);
            return id;
        }
    }

    // 11. string length
    if (type_string && (s.contains("minLength") || s.contains("maxLength"))) {
        const int64_t mn = s.contains("minLength") && s["minLength"].is_number_integer() ? s["minLength"].get<int64_t>() : 0;
        const int64_t mx = s.contains("maxLength") && s["maxLength"].is_number_integer() ? s["maxLength"].get<int64_t>() : I32_MAX;
        fill(f, id, K_STRING_LEN, 0, dump_sid, 0, 0, 0, 0, -1, mn, mx, 0, -1, 0);
        return id;
    }

    // 12. integer bounds, exclusive ones folded inward by the host
    if (type_integer && (s.contains("minimum") || s.contains("exclusiveMinimum") ||
                         s.contains("maximum") || s.contains("exclusiveMaximum"))) {
        int64_t mn = I64_MIN;
        int64_t mx = I64_MAX;
        if (s.contains("minimum") && s["minimum"].is_number_integer()) {
            mn = s["minimum"].get<int64_t>();
        } else if (s.contains("exclusiveMinimum") && s["exclusiveMinimum"].is_number_integer()) {
            mn = s["exclusiveMinimum"].get<int64_t>() + 1;
        }
        if (s.contains("maximum") && s["maximum"].is_number_integer()) {
            mx = s["maximum"].get<int64_t>();
        } else if (s.contains("exclusiveMaximum") && s["exclusiveMaximum"].is_number_integer()) {
            mx = s["exclusiveMaximum"].get<int64_t>() - 1;
        }
        fill(f, id, K_INT_BOUND, 0, dump_sid, 0, 0, 0, 0, -1, mn, mx, 0, -1, 0);
        return id;
    }

    // 13. empty schema or object fallback
    if (s.empty() || type_object) {
        fill(f, id, K_OBJECT, 0, dump_sid, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 14. no type constraint, non-empty object
    if (type_null && s.is_object()) {
        fill(f, id, K_ANY, 0, dump_sid, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0);
        return id;
    }

    // 16. primitive
    if (has_type && s["type"].is_string()) {
        const std::string t = s["type"].get<std::string>();
        int64_t kind = -1;
        if (t == "string") {
            kind = K_STRING;
        } else if (t == "integer") {
            kind = K_INTEGER;
        } else if (t == "number") {
            kind = K_NUMBER;
        } else if (t == "boolean") {
            kind = K_BOOLEAN;
        } else if (t == "null") {
            kind = K_NULL;
        }
        if (kind >= 0) {
            fill(f, id, kind, 0, dump_sid, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0);
            return id;
        }
    }
    // 15. unrecognized
    fill(f, id, K_ANY, 1, dump_sid, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0);
    return id;
}

} // namespace

// ---- witness ---------------------------------------------------------------

// Counts the calls that actually entered the Goose module. Read by the smoke
// test, which is how the ON build proves dispatch rather than merely resolve.
static long fg_goose_calls = 0;

extern "C" long fg_goose_json_schema_calls(void) {
    return fg_goose_calls;
}

// ---- bridge ----------------------------------------------------------------

std::string fg_goose_json_schema_to_grammar(const common_json & schema) {
    // M1: one start-up for the whole runtime. The per-stage guard of the
    // accepted card would be a tenth pthread_once in this link.
    fg_goose_host_ensure_init();

    Flat f;
    const common_json doc = schema;
    f.root = build_node(f, doc, doc, "");

    std::string strs;
    std::vector<int64_t> sdir;
    for (const auto & s : f.strings) {
        sdir.push_back((int64_t) strs.size());
        sdir.push_back((int64_t) s.size());
        strs += s;
    }

    std::vector<int64_t> koff(4096, 0), klen(4096, 0), toff(4096, 0), tlen(4096, 0);
    std::vector<int64_t> st(16, 0);
    std::vector<int64_t> opt(4096, 0), kvoff(4096, 0), kvlen(4096, 0);
    std::vector<int64_t> choff(4096, 0), chlen(4096, 0);
    std::vector<int64_t> pwork(1 << 18, 0);
    std::vector<int64_t> mprops(4 * 4096, 0), rref(256, 0);
    std::vector<uint8_t> buf(1 << 20, 0), sbuf(1 << 20, 0), out(1 << 20, 0);

    sl_i64 smeta  { f.meta.data(),  (int64_t) f.meta.size()  };
    sl_i64 sprops { f.props.data(), (int64_t) f.props.size() };
    sl_i64 svals  { f.vals.data(),  (int64_t) f.vals.size()  };
    sl_u8  sstrs  { (uint8_t *) strs.data(), (int64_t) strs.size() };
    sl_i64 ssdir  { sdir.data(),    (int64_t) sdir.size()    };
    sl_i64 skoff  { koff.data(),    (int64_t) koff.size()    };
    sl_i64 sklen  { klen.data(),    (int64_t) klen.size()    };
    sl_i64 stoff  { toff.data(),    (int64_t) toff.size()    };
    sl_i64 stlen  { tlen.data(),    (int64_t) tlen.size()    };
    sl_u8  sbufv  { buf.data(),     (int64_t) buf.size()     };
    sl_i64 sst    { st.data(),      (int64_t) st.size()      };
    sl_i64 sopt   { opt.data(),     (int64_t) opt.size()     };
    sl_i64 skvoff { kvoff.data(),   (int64_t) kvoff.size()   };
    sl_i64 skvlen { kvlen.data(),   (int64_t) kvlen.size()   };
    sl_i64 schoff { choff.data(),   (int64_t) choff.size()   };
    sl_i64 schlen { chlen.data(),   (int64_t) chlen.size()   };
    sl_u8  ssbuf  { sbuf.data(),    (int64_t) sbuf.size()    };
    sl_i64 spwork { pwork.data(),   (int64_t) pwork.size()   };
    sl_i64 smprops{ mprops.data(),  (int64_t) mprops.size()  };
    sl_i64 srref  { rref.data(),    (int64_t) rref.size()    };
    sl_u8  sout   { out.data(),     (int64_t) out.size()     };

    ++fg_goose_calls;

    const int64_t n = json_schema_convert(f.root, smeta, sprops, svals, sstrs, ssdir,
                                          skoff, sklen, stoff, stlen, sbufv, sst, sopt,
                                          skvoff, skvlen, schoff, schlen, ssbuf, spwork,
                                          smprops, srref, sout);
    if (n < 0) {
        throw std::invalid_argument("JSON schema conversion failed: Goose json_schema_convert returned -1");
    }
    return std::string((const char *) out.data(), (size_t) n);
}
