// C host for the Goose draft: prints `name<TAB>id<TAB>len<TAB>hex` for every
// template in templates.h, using the same conversation as probe.
#include "draft.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string t((const char *) s.data, (size_t) s.len);
    return std::strtod(t.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
}
extern "C" double gs_frexp_mant(double x) { int e = 0; return std::frexp(x, &e); }
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    const std::string t = std::to_string(x);
    memcpy(out.data, t.data(), t.size());
    return (int64_t) t.size();
}
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    const std::string f((const char *) fmt.data, (size_t) fmt.len);
    const std::time_t t = epoch < 0 ? std::time(nullptr) : (std::time_t) epoch;
    const std::tm * tmv = std::localtime(&t);
    if (!tmv) return -1;
    return (int64_t) std::strftime((char *) out.data, (size_t) out.len, f.c_str(), tmv);
}

#include "templates.h"

#include <stdio.h>
#include <string.h>

static const char * ROLE[] = { "system", "user", "assistant", "user" };
static const char * CONT[] = {
    "You are a helpful assistant.", "Hello", "Hi there", "Who are you?"
};

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    static unsigned char dst[65536];
    static int64_t meta[2];
    static int64_t roffs[8], rlens[8], coffs[8], clens[8];
    static unsigned char roles[256], cs[1024];
    int ro = 0, co = 0;
    for (int i = 0; i < 4; i++) {
        roffs[i] = ro; rlens[i] = (int64_t) strlen(ROLE[i]);
        memcpy(roles + ro, ROLE[i], (size_t) rlens[i]); ro += (int) rlens[i];
        coffs[i] = co; clens[i] = (int64_t) strlen(CONT[i]);
        memcpy(cs + co, CONT[i], (size_t) clens[i]); co += (int) clens[i];
    }
    sl_u8 sroles = { roles, ro }, scs = { cs, co };
    sl_i64 sroff = { roffs, 4 }, srl = { rlens, 4 }, scoff = { coffs, 4 }, scl = { clens, 4 };
    sl_i64 smeta = { meta, 2 };
    for (int i = 0; i < TPL_N; i++) {
        sl_u8 stmpl = { (unsigned char *) TPL_SRC[i], (int64_t) strlen(TPL_SRC[i]) };
        sl_u8 sdst = { dst, (int64_t) sizeof dst };
        int64_t len = legacy_apply(stmpl, sroles, sroff, srl, scs, scoff, scl, 4, 1, sdst, smeta);
        printf("%s\t%lld\t%lld\t", TPL_NAME[i], (long long) meta[0], (long long) len);
        for (int64_t k = 0; k < len && k < (int64_t) sizeof dst; k++) printf("%02x", dst[k]);
        printf("\n");
    }
    // Add-generation-prompt off, and an unsupported source.
    sl_u8 stmpl = { (unsigned char *) TPL_SRC[4], (int64_t) strlen(TPL_SRC[4]) };
    sl_u8 sdst = { dst, (int64_t) sizeof dst };
    ro = 0; co = 0;
    for (int i = 0; i < 4; i++) {
        roffs[i] = ro; rlens[i] = (int64_t) strlen(ROLE[i]);
        memcpy(roles + ro, ROLE[i], (size_t) rlens[i]); ro += (int) rlens[i];
        coffs[i] = co; clens[i] = (int64_t) strlen(CONT[i]);
        memcpy(cs + co, CONT[i], (size_t) clens[i]); co += (int) clens[i];
    }
    sroles = sl_u8{ roles, ro }; scs = sl_u8{ cs, co };
    sroff = sl_i64{ roffs, 4 }; srl = sl_i64{ rlens, 4 };
    scoff = sl_i64{ coffs, 4 }; scl = sl_i64{ clens, 4 };
    int64_t len = legacy_apply(stmpl, sroles, sroff, srl, scs, scoff, scl, 4, 0, sdst, smeta);
    printf("noass\t%lld\t%lld\t", (long long) meta[0], (long long) len);
    for (int64_t k = 0; k < len; k++) printf("%02x", dst[k]);
    printf("\n");
    // All 54 builtin names plus a DOTS1 source; detection only.
    {
        static const char * NM[] = {"chatml","llama2","llama2-sys","llama2-sys-bos","llama2-sys-strip","mistral-v1","mistral-v3","mistral-v3-tekken","mistral-v7","mistral-v7-tekken","phi3","phi4","falcon3","zephyr","monarch","gemma","orion","openchat","vicuna","vicuna-orca","deepseek","deepseek2","deepseek3","deepseek-ocr","command-r","llama3","chatglm3","chatglm4","glmedge","minicpm","exaone3","exaone4","exaone-moe","rwkv-world","granite","granite-4.0","granite-4.1","gigachat","megrez","yandex","bailing","bailing-think","bailing2","llama4","smolvlm","hunyuan-moe","gpt-oss","hunyuan-dense","hunyuan-vl","kimi-k2","seed_oss","grok-2","pangu-embedded","solar-open","{{ '<|userprompt|>' }}<|endofuserprompt|>"};
        for (int c = 0; c < 55; c++) {
            sl_u8 st = {(unsigned char *) NM[c], (int64_t) strlen(NM[c])};
            printf("det\t%s\t%lld\n", NM[c], (long long) legacy_detect(st));
        }
        int built = 0, open = 0;
        for (int c = 0; c < 54; c++) {
            sl_u8 st = {(unsigned char *) NM[c], (int64_t) strlen(NM[c])};
            sl_u8 sd = {dst, (int64_t) sizeof dst};
            int64_t l3 = legacy_apply(st, sroles, sroff, srl, scs, scoff, scl, 4, 1, sd, smeta);
            if (l3 >= 0) built++; else { open++; printf("gap\t%s\t%lld\n", NM[c], (long long) meta[0]); }
        }
        printf("built\t%d\topen\t%d\n", built, open);
    }
    // Cases mirroring probe --cases-tsv, including role quirks and an empty chat.
    {
        struct tcase {
            const char * label;
            const char * tmpl;
            int add;
            int n;
            const char * r[6];
            const char * c[6];
        };
        static const tcase CASES[] = {
            {"case-01-chatml","chatml",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-02-llama3","llama3",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-03-gemma","gemma",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-04-llama2-sys","llama2-sys",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-05-mistral-v1","mistral-v1",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-06-phi3","phi3",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-07-deepseek3","deepseek3",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-08-vicuna-orca","vicuna-orca",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-09-chatml-noass","chatml",0,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-10-yandex","yandex",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-11-tools-chatml","chatml",0,6,{"system","user","assistant","tool","function","assistant_tool_call"},{"S","U","A","T","F","C"}},
            {"case-12-tools-granite","granite",0,6,{"system","user","assistant","tool","function","assistant_tool_call"},{"S","U","A","T","F","C"}},
            {"case-13-tools-kimi","kimi-k2",0,6,{"system","user","assistant","tool","function","assistant_tool_call"},{"S","U","A","T","F","C"}},
            {"case-14-empty","chatml",1,0,{},{}},
            {"case-15-pangu","pangu-embedded",1,2,{"user","assistant"},{"Hello","Hi there"}},
            {"case-16-chatglm3","chatglm3",1,2,{"system","user"},{"Be brief.","Hello"}},
            {"case-17-openchat","openchat",1,4,{"system","user","assistant","user"},{"You are a helpful assistant.","Hello","Hi there","Who are you?"}},
            {"case-18-gigachat","gigachat",1,4,{"system","user","assistant","user"},{"S","U","A","U2"}},
            {"case-19-trim","llama3",1,1,{"user"},{"  padded  "}},
            {"case-20-parts-chatml","chatml",0,1,{"user"},{"C"}},
            {"case-21-deepseek-ocr","deepseek-ocr",0,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-22-chatglm4","chatglm4",1,2,{"system","user"},{"S","U"}},
            {"case-23-minicpm","minicpm",0,3,{"system","user","assistant"},{"S"," U "," A "}},
            {"case-24-exaone3","exaone3",1,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-25-exaone4","exaone4",1,4,{"system","user","tool","assistant"},{"S","U","T","A"}},
            {"case-26-exaone-moe","exaone-moe",1,4,{"system","user","tool","assistant"},{"S","U","T","A"}},
            {"case-27-megrez","megrez",1,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-28-dots1","{{ '<|userprompt|>' }}<|endofuserprompt|>",1,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-29-seed-oss","seed_oss",1,3,{"user","assistant","user"},{"U","  A  ","U2"}},
            {"case-30-rwkv-world","rwkv-world",1,3,{"user","assistant","user"},{"U","A","U2"}},
            {"case-31-smolvlm","smolvlm",1,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-32-bailing","bailing",1,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-33-bailing-think","bailing-think",1,2,{"user","assistant"},{"U","A"}},
            {"case-34-bailing2","bailing2",1,2,{"user","assistant"},{"U","A"}},
            {"case-35-hunyuan-dense","hunyuan-dense",0,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-36-hunyuan-vl","hunyuan-vl",0,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-37-hunyuan-moe","hunyuan-moe",0,3,{"system","user","assistant"},{"S","U","A"}},
            {"case-38-grok-2","grok-2",1,3,{"system","user","assistant"},{"S"," U ","A"}},
            {"case-39-gpt-oss","gpt-oss",1,3,{"user","assistant","user"},{"U","A","U2"}},
        };
        for (const tcase & tc : CASES) {
            int nn = tc.n;
            int ro = 0, co2 = 0;
            for (int i = 0; i < nn; i++) {
                const char * r = tc.r[i];
                const char * t = tc.c[i];
                roffs[i] = ro; rlens[i] = (int64_t) strlen(r);
                memcpy(roles + ro, r, (size_t) rlens[i]); ro += (int) rlens[i];
                coffs[i] = co2; clens[i] = (int64_t) strlen(t);
                memcpy(cs + co2, t, (size_t) clens[i]); co2 += (int) clens[i];
            }
            sl_i64 a = {roffs,nn}, b = {rlens,nn}, d = {coffs,nn}, e = {clens,nn};
            sl_u8 sr = {roles,ro}, sc = {cs,co2};
            sl_u8 st = {(unsigned char *) tc.tmpl, (int64_t) strlen(tc.tmpl)};
            sl_u8 sd = {dst, (int64_t) sizeof dst};
            int64_t l2 = legacy_apply(st, sr, a, b, sc, d, e, nn, tc.add, sd, smeta);
            printf("%s\t%lld\t%lld\t", tc.label, (long long) meta[0], (long long) l2);
            for (int64_t k = 0; k < l2; k++) printf("%02x", dst[k]);
            printf("\n");
        }
    }
    // Content-parts cases: content "C" and empty content, with two text parts
    // and one media marker. The native separator rule depends on emptiness.
    {
        static const char * PT[] = {"P1","P2","<image>"};
        static int64_t poff[3], plen[3];
        int po = 0;
        unsigned char pbuf[64];
        for (int i = 0; i < 3; i++) {
            poff[i] = po; plen[i] = (int64_t) strlen(PT[i]);
            memcpy(pbuf + po, PT[i], (size_t) plen[i]); po += (int) plen[i];
        }
        static unsigned char merged[128];
        sl_u8 mt = {pbuf, po};
        sl_i64 mo = {poff, 3}, ml = {plen, 3};
        sl_u8 md = {merged, (int64_t) sizeof merged};
        for (int pc = 0; pc < 2; pc++) {
            const char * base_content = pc == 0 ? "C" : "";
            sl_u8 mc = {(unsigned char *) base_content, (int64_t) strlen(base_content)};
            int64_t mlen = legacy_merge(mc, mt, mo, ml, 3, md);
            sl_u8 st = {(unsigned char *) "chatml", 6};
            static int64_t proff[1] = {0}, prlen[1] = {4}, pcoff[1] = {0}, pclen[1];
            pclen[0] = mlen;
            memcpy(roles, "user", 4);
            memcpy(cs, merged, (size_t) mlen);
            sl_i64 a = {proff,1}, b = {prlen,1}, d = {pcoff,1}, e = {pclen,1};
            sl_u8 sr = {roles,4}, sc = {cs,(int) mlen};
            sl_u8 sd = {dst, (int64_t) sizeof dst};
            int64_t l4 = legacy_apply(st, sr, a, b, sc, d, e, 1, 0, sd, smeta);
            printf("%s\t%lld\t%lld\t", pc == 0 ? "parts" : "parts-empty",
                   (long long) meta[0], (long long) l4);
            for (int64_t k = 0; k < l4; k++) printf("%02x", dst[k]);
            printf("\n");
        }
    }
    return 0;
}
