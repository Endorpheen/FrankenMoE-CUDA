#!/usr/bin/env python3
# Generates the shared gate for the quantization allow-cuts slice (P7-04 S2):
#   tests/probe_q4.cpp  - native oracle: llama-quant.cpp statics + std::regex
#   build/gate_q4.goose - the port plus a driver over the same cases and corpus
# 20 policy cases plus a 53-pair regex corpus, compared line by line.
from pathlib import Path

ALLOW = [
    ("blk.0.attn_q.weight", 2, 0, 0, [0]),
    ("blk.0.attn_norm.weight", 2, 0, 0, [0]),
    ("output.weight", 2, 0, 0, [0]),
    ("output.weight", 2, 0, 1, [0]),
    ("blk.0.ffn_gate_inp.weight", 2, 0, 0, [0]),
    ("blk.3.ffn_gate_tid2eid.weight", 2, 0, 0, [0]),
    ("blk.0.ssm_conv1d.weight", 2, 0, 0, [0]),
    ("position_embd.weight", 2, 0, 0, [0]),
    ("token_types.weight", 2, 0, 0, [0]),
    ("blk.0.indexer.attn_k.weight", 2, 0, 0, [1, 0]),
    ("decision.head.weight", 2, 0, 0, [2, 0]),
]

OVERRIDE = [
    ("blk.0.attn_v.weight", [".*\\.attn_v.*", "ffn_down"]),
    ("blk.3.attn_k.weight", ["attn_[qv]", "^blk\\.3\\."]),
    ("blk.10.attn_k.weight", ["^blk\\.0\\."]),
]

PRIORITY = [
    ("per_layer_token_embd.weight", 0, 11, 43, 1, 1, ["per_layer.*"]),
    ("per_layer_token_embd.weight", 0, 11, 43, 1, 1, []),
]

CORPUS = [
    ("", "abc"), ("abc", "abc"), ("abc", "xabcx"), ("^abc", "abc"), ("^abc", "xabc"),
    ("abc$", "abc"), ("abc$", "abcx"), ("a.c", "abc"), ("a.c", "a\nc"), ("a\nc", "a\nc"),
    ("a[bc]d", "abd"), ("a[bc]d", "acd"), ("a[bc]d", "aed"),
    ("[a-c]+", "abc"), ("[a-c]+", "xyz"), ("[^a-c]", "d"), ("[^a-c]", "a"),
    ("ab*c", "ac"), ("ab*c", "abbbc"), ("ab*c", "adc"),
    ("ab+c", "ac"), ("ab+c", "abc"), ("ab?c", "ac"), ("ab?c", "abc"),
    ("a{2,3}", "aa"), ("a{2,3}", "aaaa"), ("a{2}", "aa"), ("a{2}", "a"), ("a{2,}", "aaaa"),
    ("\\d+", "blk.10"), ("\\d+", "blk.x"),
    ("\\w+\\.weight", "foo.weight"), ("\\w+\\.weight", "foo.body"),
    ("\\.weight$", "x.weight"), ("\\.weight$", "x.weight.bak"),
    ("^blk\\.0\\.", "blk.0.attn"), ("^blk\\.0\\.", "blk.10.attn"),
    ("attn_[qv]", "blk.0.attn_v.weight"), (".*ffn_down.*", "blk.1.ffn_down.weight"),
    ("ssm_[fg]_[ab]\\.weight", "blk.0.ssm_f_a.weight"),
    ("ssm_[fg]_[ab]\\.weight", "blk.0.ssm_x_a.weight"),
    ("[0-9]+\\.attn", "blk.12.attn_q.weight"), ("[0-9]+\\.attn", "blk.aa.attn_q.weight"),
    ("\\s", "no space"), ("\\s", "has space"),
    ("a\\.b", "a.b"), ("a\\.b", "axb"),
    ("blk\\.[0-9]+\\.", "blk.7.attn_q.weight"), ("blk\\.[0-9]+\\.", "layer.7.attn_q.weight"),
    ("[a-z]{3}\\.[a-z]", "abc.d"), ("[a-z]{3}\\.[a-z]", "ab.c"),
    ("x*y*z", "zzz"), ("x*y*z", "xy"),
    ("q[0-9]{1,2}_k", "q4_k"), ("q[0-9]{1,2}_k", "qq4_k"),
    ("blk\\.\\d+\\..*", "blk.3.attn_q.weight"), ("blk\\.\\d+\\..*", "blk.attn_q.weight"),
]


def zeros(n):
    return ", ".join(["0"] * n)


def cpp_str(s):
    out = []
    for b in s.encode():
        if b == 92:
            out.append("\\\\")
        elif b == 34:
            out.append('\\"')
        elif 32 <= b < 127:
            out.append(chr(b))
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def goose_str(s):
    out = []
    for b in s.encode():
        if b == 92:
            out.append("\\\\")
        elif b == 34:
            out.append('\\"')
        elif b == 10:
            out.append("\\n")
        elif b == 9:
            out.append("\\t")
        elif b == 13:
            out.append("\\r")
        elif 32 <= b < 127:
            out.append(chr(b))
        else:
            out.append("\\u{%02x}" % b)
    return '"' + "".join(out) + '"'


def fill(tpl, **kw):
    for k, v in kw.items():
        tpl = tpl.replace("__%s__" % k, str(v))
    return tpl


CPP_ALLOW = r'''    { ggml_tensor t{}; std::strcpy(t.name, __NAME__);
      for (int d = 0; d < 4; d++) t.ne[d] = d < __DIMS__ ? 64 : 1;
      p.only_copy = __OC__ != 0; p.quantize_output_tensor = __QO__ != 0;
      printf("A__ID__|allow|%d\n", (int) tensor_allows_quantization(&p, __ARCH__, &t)); }'''

CPP_OVR_HEAD = r'''    { std::vector<std::pair<std::regex, int>> pats;'''
CPP_OVR_PAT = r'''      pats.emplace_back(std::regex(__PAT__), __TY__);'''
CPP_OVR_TAIL = r'''      int hit = -1; int ty = -1;
      for (auto & pr : pats) { if (std::regex_search(std::string(__NAME__), pr.first)) { hit = 1; ty = pr.second; break; } }
      printf("O__ID__|override|%d|%d\n", hit, ty); }'''

CPP_PRIO_HEAD = r'''    { std::vector<std::pair<std::regex, int>> pats;'''
CPP_PRIO_TAIL = r'''      int src = prio_source(std::string(__NAME__), __CAT__, __TT__, __OT__, __Q__ != 0, __PL__ != 0, pats);
      printf("P__ID__|priority|%d\n", src); }'''

CPP_RX = r'''    { int m = -2;
      try { std::regex re(__PAT__); m = std::regex_search(std::string(__SUBJ__), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R__ID__|regex|%d\n", m); }'''

GO_ALLOW = r'''    {
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 65; p = 1; p = put_int(line, p, __ID__);
        p = put_s(line, p, "|allow|");
        p = put_int(line, p, qa_allows(__OC__, __QO__, __DIMS__, __ARCH__, "position_embd.weight", "token_types.weight", __NAME_G__));
        print(line[0..p]);
    }'''

GO_ALLOW_MULTI = r'''    {
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 65; p = 1; p = put_int(line, p, __ID__);
        p = put_s(line, p, "|allow");
__CALLS__
        print(line[0..p]);
    }'''

GO_OVR = r'''    {
        var blob: u8[1024] = [__Z1024__];
        var off: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
        var len: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
        var ty: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
        var n: i64 = 0;
        var blen: i64 = 0;
__ADDS__
        var ot: i64[2] = [0, 0];
        let r = qa_override(__NAME_G__, blob[0..blen], off, len, ty, n, ot);
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 79; p = 1; p = put_int(line, p, __ID__);
        p = put_s(line, p, "|override|");
        if r >= 0 { p = put_int(line, p, 1); } else { p = put_int(line, p, r); }
        p = put_s(line, p, "|");
        if r >= 0 { p = put_int(line, p, ot[0]); } else { p = put_int(line, p, 0 - 1); }
        print(line[0..p]);
    }'''

GO_PRIO = r'''    {
        var blob: u8[1024] = [__Z1024__];
        var off: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
        var len: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
        var ty: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
        var n: i64 = 0;
        var blen: i64 = 0;
__ADDS__
        var ot: i64[2] = [0, 0];
        let r = qa_priority(1, __CAT__, __TT__, __OT__, __Q__, __PL__, __NAME_G__, blob[0..blen], off, len, ty, n, ot);
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 80; p = 1; p = put_int(line, p, __ID__);
        p = put_s(line, p, "|priority|");
        p = put_int(line, p, r);
        print(line[0..p]);
    }'''

GO_RX = r'''    {
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 82; p = 1; p = put_int(line, p, __ID__);
        p = put_s(line, p, "|regex|");
        p = put_int(line, p, qr_search(__PAT_G__, __SUBJ_G__));
        print(line[0..p]);
    }'''

GO_ADD = r'''        off[n] = blen; len[n] = len_of(__PAT_G__); ty[n] = __TY__;
        blen = q_copy(blob, blen, __PAT_G__, 0, len_of(__PAT_G__));
        n = n + 1;'''


def gen_cpp():
    body = ['    llama_model_quantize_params p{};']
    arch_names = ["LLM_ARCH_LLAMA", "LLM_ARCH_GLM5_NEXT", "LLM_ARCH_VLLM_DECISION2"]
    for i, (name, dims, oc, qo, arches) in enumerate(ALLOW):
        body.append(f'    {{ ggml_tensor t{{}}; std::strcpy(t.name, {cpp_str(name)});')
        body.append(f'      for (int d = 0; d < 4; d++) t.ne[d] = d < {dims} ? 64 : 1;')
        body.append(f'      p.only_copy = {oc} != 0; p.quantize_output_tensor = {qo} != 0;')
        body.append(f'      printf("A{i}|allow");')
        for a in arches:
            body.append(f'      printf("|%d", (int) tensor_allows_quantization(&p, {arch_names[a]}, &t));')
        body.append('      printf("\\n"); }')
    for i, (name, pats) in enumerate(OVERRIDE):
        body.append(CPP_OVR_HEAD)
        for k, pat in enumerate(pats):
            body.append(fill(CPP_OVR_PAT, PAT=cpp_str(pat), TY=12 + k))
        body.append(fill(CPP_OVR_TAIL, NAME=cpp_str(name), ID=i))
    for i, (name, cat, tt, ot, q, pl, pats) in enumerate(PRIORITY):
        body.append(CPP_PRIO_HEAD)
        for k, pat in enumerate(pats):
            body.append(fill(CPP_OVR_PAT, PAT=cpp_str(pat), TY=12 + k))
        body.append(fill(CPP_PRIO_TAIL, NAME=cpp_str(name), CAT=cat, TT=tt, OT=ot, Q=q, PL=pl, ID=i))
    for i, (pat, subj) in enumerate(CORPUS):
        body.append(fill(CPP_RX, PAT=cpp_str(pat), SUBJ=cpp_str(subj), ID=i))

    head = r'''// Native oracle for the quantization allow-cuts slice.
// Includes the translation unit so the file-local policy helpers are callable.
#include "llama-quant.cpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// llama_tensor_get_type's prefix, transcribed: denied, token-embedding type,
// output type, manual override, standard logic. The two real decisions it calls
// (tensor_allows_quantization, regex_search) stay native.
static int prio_source(const std::string & name, int category, int token_type, int output_type,
                       bool is_quant, bool per_layer,
                       const std::vector<std::pair<std::regex, int>> & pats) {
    const int TYPE_COUNT = (int) GGML_TYPE_COUNT;
    const int CAT_TOKEN_EMBD = (int) tensor_category::TOKEN_EMBD;
    const int CAT_OUTPUT = (int) tensor_category::OUTPUT;
    if (token_type < TYPE_COUNT && category == CAT_TOKEN_EMBD) {
        bool named = false;
        if (per_layer) {
            for (auto & pr : pats) if (std::regex_search(name, pr.first)) { named = true; break; }
        }
        if (!named) return 1;
    }
    if (output_type < TYPE_COUNT && category == CAT_OUTPUT) return 2;
    if (is_quant) {
        for (auto & pr : pats) if (std::regex_search(name, pr.first)) return 3;
    }
    return 4;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
'''
    return head + "\n".join(body) + "\n    return 0;\n}\n"


def gen_goose(port_text):
    body = []
    for i, (name, dims, oc, qo, arches) in enumerate(ALLOW):
        calls = "\n".join(
            "        p = put_s(line, p, \"|\"); p = put_int(line, p, qa_allows(%d, %d, %d, %d, \"position_embd.weight\", \"token_types.weight\", %s));"
            % (oc, qo, dims, a, goose_str(name)) for a in arches)
        body.append(fill(GO_ALLOW_MULTI, ID=i, CALLS=calls, Z256=zeros(256)))
    for i, (name, pats) in enumerate(OVERRIDE):
        adds = "\n".join(fill(GO_ADD, PAT_G=goose_str(p), TY=12 + k) for k, p in enumerate(pats))
        body.append(fill(GO_OVR, ID=i, NAME_G=goose_str(name), ADDS=adds, Z1024=zeros(1024), Z256=zeros(256)))
    for i, (name, cat, tt, ot, q, pl, pats) in enumerate(PRIORITY):
        adds = "\n".join(fill(GO_ADD, PAT_G=goose_str(p), TY=12 + k) for k, p in enumerate(pats))
        body.append(fill(GO_PRIO, ID=i, NAME_G=goose_str(name), CAT=cat, TT=tt, OT=ot, Q=q, PL=pl,
                         ADDS=adds, Z1024=zeros(1024), Z256=zeros(256)))
    for i, (pat, subj) in enumerate(CORPUS):
        body.append(fill(GO_RX, ID=i, PAT_G=goose_str(pat), SUBJ_G=goose_str(subj), Z256=zeros(256)))

    head = r'''
// Gate driver, generated by tests/gen_q4.py. Calls the private bodies of the
// port (Goose cannot call an export fn from another function).
fn put_int(dst: u8[:], at: i64, v: i64) -> i64 {
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

fn put_s(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return q_copy(dst, at, s, 0, s.len);
}

fn q_copy(dst: u8[:], at: i64, src: const u8[:], from: i64, len: i64) -> i64 {
    var i: i64 = 0;
    while i < len {
        dst[at + i] = src[from + i];
        i = i + 1;
    }
    return at + len;
}

fn len_of(s: const u8[:]) -> i64 {
    return s.len;
}

fn main() {
'''
    return port_text + head + "\n".join(body) + "\n}\n"


def main():
    root = Path(__file__).resolve().parent.parent
    (root / 'tests').mkdir(parents=True, exist_ok=True)
    (root / 'build').mkdir(parents=True, exist_ok=True)
    (root / 'tests' / 'probe_q4.cpp').write_text(gen_cpp())
    port = (root / 'source' / 'quant-allow.goose').read_text()
    (root / 'build' / 'gate_q4.goose').write_text(gen_goose(port))
    print("cases:", len(ALLOW) + len(OVERRIDE) + len(PRIORITY), "corpus:", len(CORPUS))


if __name__ == '__main__':
    main()
