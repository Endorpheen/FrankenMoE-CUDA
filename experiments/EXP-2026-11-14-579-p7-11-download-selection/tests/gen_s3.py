#!/usr/bin/env python3
# Generates the shared case table for the download selection slice (S3):
#   tests/probe_s3.cpp  - native oracle, includes common/download.cpp and calls
#                         its file-local statics directly
#   build/gate_s3.goose - the port plus a driver over the same cases
# Templates use __TOKEN__ replacement so no percent or brace formatting leaks
# into the generated C or Goose source.
from pathlib import Path

CASES = [
    ("split", "model-Q4_K_M.gguf"),
    ("split", "model-Q8_0.gguf"),
    ("split", "model-F16.gguf"),
    ("split", "model-NVFP4.gguf"),
    ("split", "model-Q4_K_M-00003-of-00004.gguf"),
    ("split", "model-00001-of-00002.gguf"),
    ("split", "dir/sub/model-Q4_K_M.gguf"),
    ("split", "model.bin"),
    ("split", "model-Q4_K_M-001-of-00003.gguf"),
    ("split", "model-q4_k_m.gguf"),
    ("split", "model.IQ2_XS.gguf"),
    ("split", "model-Q4_K_M-00001-of-00003.gguf"),
    ("parts", "model-Q4_K_M-00002-of-00004.gguf"),
    ("parts", "model-Q4_K_M.gguf"),
    ("parts", "https://host/m-Q8_0-00001-of-00002.gguf"),
    ("group", ["a-Q4_K_M-00001-of-00003.gguf", "a-Q4_K_M-00002-of-00003.gguf",
               "a-Q8_0.gguf", "b-00001-of-00002.gguf"], 0),
    ("group", ["a-Q4_K_M-00001-of-00003.gguf", "a-Q4_K_M-00002-of-00003.gguf",
               "a-Q8_0.gguf", "b-00001-of-00002.gguf"], 2),
    ("model", ["m-F16.gguf", "m-Q4_K_M.gguf", "m-Q8_0.gguf"], ""),
    ("model", ["m-F16.gguf", "m-Q4_K_M.gguf", "m-Q8_0.gguf"], "Q8_0"),
    ("model", ["m-Q4_K_M-00001-of-00003.gguf", "m-Q4_K_M-00002-of-00003.gguf"], ""),
    ("model", ["mmproj-x.gguf", "imatrix.gguf", "m-Q4_K_M.gguf"], ""),
    ("sibling", ["repo/m-Q4_K_M.gguf", "repo/mmproj-Q4_K_M.gguf", "repo/mmproj-F16.gguf"],
     0, "mmproj", "Q4_K_M"),
    ("sibling", ["repo/m.gguf", "repo/sub/mmproj.gguf", "repo/mmproj.gguf"],
     0, "mmproj", ""),
    ("sibling", ["repo/m-Q4_K_M.gguf", "repo/mmproj-F16.gguf", "repo/mmproj-Q8_0.gguf"],
     0, "mmproj", ""),
    ("sibling", ["repo/m.gguf", "repo/mtp-1.gguf"], 0, "mmproj", ""),
    ("sidecar", "dflash"),
    ("sidecar", "unknown"),
    ("ismodel", ["repo/m-Q4_K_M.gguf", "repo/imatrix.gguf", "repo/mtp-x.gguf",
                 "repo/mmproj.gguf", "repo/M-Q4_K_M.GGUF"]),
    ("plan", [1, 0, 1, 0, 1]),
    ("repo", ["user/model:Q4_K_M", "bad", "a/b:c:d", "a/b:"]),
    ("resume", [(100, 206), (100, 416), (100, 200), (100, 500), (0, 500), (0, 200)]),
]


def zeros(n):
    return ", ".join(["0"] * n)


def cpp_str(s):
    out = []
    for b in s.encode():
        if 32 <= b < 127 and chr(b) not in '"\\':
            out.append(chr(b))
        else:
            out.append("\\x%02x" % b)
    return '"' + "".join(out) + '"'


def files_expr(names):
    return "hf_cache::hf_files{" + ", ".join("{ %s }" % cpp_str(n) for n in names) + "}"


def fill(tpl, **kw):
    for k, v in kw.items():
        tpl = tpl.replace("__%s__" % k, str(v))
    return tpl


CPP_SPLIT = r'''    { const std::string name = __NAME__;
      auto s = get_gguf_split_info(name);
      printf("C__ID__|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }'''

CPP_PARTS = r'''    { auto parts = common_download_get_all_parts(__NAME__);
      printf("C__ID__|parts|%zu", parts.size());
      for (auto & p : parts) printf("|%s", p.c_str());
      printf("\n"); }'''

CPP_GROUP = r'''    { auto fs = __FILES__; auto g = get_split_files(fs, fs[__SEL__]);
      printf("C__ID__|group|%zu", g.size());
      for (auto & f : g) { int at = -1; for (size_t k = 0; k < fs.size(); k++) if (fs[k].path == f.path) at = (int) k; printf("|%d", at); }
      printf("\n"); }'''

CPP_MODEL = r'''    { auto fs = __FILES__; auto m = find_best_model(fs, __TAG__);
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C__ID__|model|%d\n", at); }'''

CPP_SIBLING = r'''    { auto fs = __FILES__; auto m = find_best_sibling(fs, fs[__MODEL__].path, __KEYWORD__, __TAG__);
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C__ID__|sibling|%d\n", at); }'''

CPP_SIDECAR = r'''    { int at = -1; std::string kw;
      auto & sc = common_download_sidecars();
      for (size_t k = 0; k < sc.size(); k++) if (__NAME__ == sc[k].name) { at = (int) k; kw = sc[k].keyword; }
      printf("C__ID__|sidecar|%d|%s|%d|%d\n", at, kw.c_str(),
             (int) name_is_sidecar(std::string("repo/dflash-x.gguf")),
             (int) gguf_filename_is_model(std::string(__FNAME__))); }'''

CPP_ISMODEL_HEAD = r'''    { printf("C__ID__|ismodel");'''
CPP_ISMODEL_ONE = r'''      printf("|%d", (int) gguf_filename_is_model(std::string(__PATH__)));'''
CPP_END = r'''      printf("\n"); }'''

CPP_PLAN = r'''    { common_download_hf_plan plan;
      if (__P0__) plan.mmproj.path = "a";
      if (__P1__) plan.mtp.path = "b";
      if (__P2__) plan.dflash.path = "c";
      if (__P3__) plan.eagle3.path = "d";
      if (__P4__) plan.dspark.path = "e";
      auto names = common_download_plan_sidecars(plan);
      printf("C__ID__|plan|%zu", names.size());
      for (auto & nm : names) { int at = -1; auto & sc = common_download_sidecars();
        for (size_t k = 0; k < sc.size(); k++) if (nm == sc[k].name) at = (int) k;
        printf("|%d", at); }
      printf("\n"); }'''

CPP_REPO_HEAD = r'''    { printf("C__ID__|repo");'''
CPP_REPO_ONE = r'''      try { auto r = common_download_split_repo_tag(std::string(__STR__));
        printf("|0|%s|%s", r.first.c_str(), r.second.c_str()); }
      catch (const std::exception &) { printf("|1||"); }'''

CPP_RESUME_HEAD = r'''    { printf("C__ID__|resume");'''
CPP_RESUME_ONE = r'''      printf("|%d", resume_action(__DL__, __ST__));'''
CPP_RESUME_TAIL = r'''      printf("|%d|%d|%d", resume_header(100, 1), resume_header(0, 1), resume_header(100, 0));'''


def gen_cpp():
    body = []
    for idx, c in enumerate(CASES):
        k = c[0]
        if k == "split":
            body.append(fill(CPP_SPLIT, ID=idx, NAME=cpp_str(c[1])))
        elif k == "parts":
            body.append(fill(CPP_PARTS, ID=idx, NAME=cpp_str(c[1])))
        elif k == "group":
            body.append(fill(CPP_GROUP, ID=idx, FILES=files_expr(c[1]), SEL=c[2]))
        elif k == "model":
            body.append(fill(CPP_MODEL, ID=idx, FILES=files_expr(c[1]), TAG=cpp_str(c[2])))
        elif k == "sibling":
            body.append(fill(CPP_SIBLING, ID=idx, FILES=files_expr(c[1]), MODEL=c[2],
                             KEYWORD=cpp_str(c[3]), TAG=cpp_str(c[4])))
        elif k == "sidecar":
            fname = "repo/" + c[1] + ".gguf"
            body.append(fill(CPP_SIDECAR, ID=idx, NAME=cpp_str(c[1]), FNAME=cpp_str(fname)))
        elif k == "ismodel":
            body.append(fill(CPP_ISMODEL_HEAD, ID=idx))
            for p in c[1]:
                body.append(fill(CPP_ISMODEL_ONE, PATH=cpp_str(p)))
            body.append(CPP_END)
        elif k == "plan":
            pr = c[1]
            body.append(fill(CPP_PLAN, ID=idx, P0=pr[0], P1=pr[1], P2=pr[2], P3=pr[3], P4=pr[4]))
        elif k == "repo":
            body.append(fill(CPP_REPO_HEAD, ID=idx))
            for s in c[1]:
                body.append(fill(CPP_REPO_ONE, STR=cpp_str(s)))
            body.append(CPP_END)
        elif k == "resume":
            body.append(fill(CPP_RESUME_HEAD, ID=idx))
            for d, st in c[1]:
                body.append(fill(CPP_RESUME_ONE, DL=d, ST=st))
            body.append(CPP_RESUME_TAIL)
            body.append(CPP_END)

    head = r'''// Native oracle for the download selection slice. Includes the translation
// unit so the file-local selection statics are callable directly.
#include "download.cpp"

#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

// common.cpp's formatting helper, supplied here so the translation unit links
// without pulling the whole common object. It is a vsnprintf wrapper, not a
// decision: every compared string still comes from the native call.
std::string string_format(const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int size = vsnprintf(nullptr, 0, fmt, ap) + 1;
    va_end(ap);
    std::string result(size - 1, 0);
    va_start(ap, fmt);
    vsnprintf(result.data(), size, fmt, ap);
    va_end(ap);
    return result;
}

// common_pull_file has no callable decision helper; this is a transcription of
// its response branch, kept next to the probe so the port can be compared.
static int resume_action(long downloaded, int status) {
    if (downloaded > 0 && status != 206) {
        return (status == 416 || status == 200) ? 1 : 2;
    }
    if (downloaded == 0 && status != 200) {
        return 2;
    }
    return 0;
}

static int resume_header(long downloaded, int supports_ranges) {
    return (supports_ranges && downloaded > 0) ? 1 : 0;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
'''
    return head + "\n".join(body) + "\n    return 0;\n}\n"


GO_SPLIT = r'''    {
        var o: i64[6] = [__Z6__];
        var t: u8[128] = [__Z128__];
        let st = d_split_info("__NAME__", o, t);
        let pn = o[1];
        let tn = o[3];
        var line: u8[512] = [__Z512__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|split|");
        p = put_int(line, p, st);
        p = put_s(line, p, "|");
        p = put_fld(line, p, ("__NAME__")[o[0] .. o[0] + o[1]]);
        p = put_s(line, p, "|");
        p = put_fld(line, p, t[0..tn]);
        p = put_s(line, p, "|");
        p = put_int(line, p, o[4]);
        p = put_s(line, p, "|");
        p = put_int(line, p, o[5]);
        p = put_s(line, p, "|");
        p = put_int(line, p, d_quant_bits("__NAME__"));
        print(line[0..p]);
    }'''

GO_PARTS = r'''    {
        let cnt = d_part_count("__NAME__");
        var line: u8[1024] = [__Z1024__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|parts|");
        p = put_int(line, p, cnt);
        var i: i64 = 1;
        while i <= cnt {
            var nb: u8[512] = [__Z512__];
            let nl = d_part_name("__NAME__", i, nb);
            p = put_s(line, p, "|");
            p = put_fld(line, p, nb[0..nl]);
            i = i + 1;
        }
        print(line[0..p]);
    }'''

GO_LIST_HEAD = r'''    {
        var blob: u8[4096] = [__Z4096__];
        var off: i64[16] = [__Z16__];
        var len: i64[16] = [__Z16__];
        var n: i64 = 0;
        var blen: i64 = 0;
__ADDS__
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|__KIND__|");
'''

GO_LIST_ADD = r'''        off[n] = blen;
        len[n] = len_of("__NAME__");
        blen = d_copy(blob, blen, "__NAME__", 0, len_of("__NAME__"));
        n = n + 1;'''

GO_GROUP_TAIL = r'''        var g: i64[16] = [__Z16__];
        let gc = d_split_group(blob[0..blen], off, len, n, __SEL__, g);
        p = put_int(line, p, gc);
        var i: i64 = 0;
        while i < gc {
            p = put_s(line, p, "|");
            p = put_int(line, p, g[i]);
            i = i + 1;
        }
        print(line[0..p]);
    }'''

GO_MODEL_TAIL = r'''        p = put_int(line, p, d_best_model(blob[0..blen], off, len, n, "__TAG__"));
        print(line[0..p]);
    }'''

GO_SIBLING_TAIL = r'''        p = put_int(line, p, d_best_sibling(blob[0..blen], off, len, n, __MODEL__, "__KEYWORD__", "__TAG__"));
        print(line[0..p]);
    }'''

GO_SIDECAR = r'''    {
        var kwb: u8[32] = [__Z32__];
        let ci = d_sidecar_index("__NAME__");
        var kn: i64 = 0;
        if ci >= 0 {
            kn = d_sidecar_keyword(ci, kwb);
        }
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|sidecar|");
        p = put_int(line, p, ci);
        p = put_s(line, p, "|");
        p = put_fld(line, p, kwb[0..kn]);
        p = put_s(line, p, "|");
        p = put_int(line, p, d_name_is_sidecar("repo/dflash-x.gguf"));
        p = put_s(line, p, "|");
        p = put_int(line, p, d_filename_is_model("__FNAME__"));
        print(line[0..p]);
    }'''

GO_ISMODEL_HEAD = r'''    {
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|ismodel");
        var i: i64 = 0;
        while i < __N__ {
            p = put_s(line, p, "|");
__CALLS__
            i = i + 1;
        }
        print(line[0..p]);
    }'''

GO_ISMODEL_ONE = r'''            if i == __K__ { p = put_int(line, p, d_filename_is_model("__PATH__")); }'''

GO_PLAN = r'''    {
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|plan|");
        var pr: i64[5] = [__P0__, __P1__, __P2__, __P3__, __P4__];
        var pl: i64[5] = [0, 0, 0, 0, 0];
        let pc = d_plan_sidecars(pr, pl);
        p = put_int(line, p, pc);
        var i: i64 = 0;
        while i < pc {
            p = put_s(line, p, "|");
            p = put_int(line, p, pl[i]);
            i = i + 1;
        }
        print(line[0..p]);
    }'''

GO_REPO_HEAD = r'''    {
        var line: u8[512] = [__Z512__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|repo");'''

GO_REPO_ONE = r'''        var o__K__: i64[4] = [0, 0, 0, 0];
        let st__K__ = d_split_repo_tag("__STR__", o__K__);
        p = put_s(line, p, "|");
        p = put_int(line, p, st__K__);
        if st__K__ == 0 {
            p = put_s(line, p, "|");
            p = put_fld(line, p, ("__STR__")[o__K__[0] .. o__K__[0] + o__K__[1]]);
            p = put_s(line, p, "|");
            p = put_fld(line, p, ("__STR__")[o__K__[2] .. o__K__[2] + o__K__[3]]);
        } else {
            p = put_s(line, p, "||");
        }'''

GO_REPO_TAIL = r'''        print(line[0..p]);
    }'''

GO_RESUME_HEAD = r'''    {
        var line: u8[256] = [__Z256__];
        var p: i64 = 0;
        line[0] = 67; p = 1;
        p = put_int(line, p, __ID__);
        p = put_s(line, p, "|resume");
        var i: i64 = 0;
        while i < __N__ {
            p = put_s(line, p, "|");
__CALLS__
            i = i + 1;
        }'''

GO_RESUME_ONE = r'''            if i == __K__ { p = put_int(line, p, d_resume_action(__DL__, __ST__)); }'''

GO_RESUME_TAIL = r'''        p = put_s(line, p, "|");
        p = put_int(line, p, d_resume_header(100, 1));
        p = put_s(line, p, "|");
        p = put_int(line, p, d_resume_header(0, 1));
        p = put_s(line, p, "|");
        p = put_int(line, p, d_resume_header(100, 0));
        print(line[0..p]);
    }'''


def gen_goose(port_text):
    body = []
    for idx, c in enumerate(CASES):
        k = c[0]
        if k == "split":
            body.append(fill(GO_SPLIT, ID=idx, NAME=c[1], Z6=zeros(6), Z128=zeros(128), Z512=zeros(512)))
        elif k == "parts":
            body.append(fill(GO_PARTS, ID=idx, NAME=c[1], Z1024=zeros(1024), Z512=zeros(512)))
        elif k in ("group", "model", "sibling"):
            adds = "\n".join(fill(GO_LIST_ADD, NAME=n) for n in c[1])
            head = fill(GO_LIST_HEAD, ID=idx, KIND=k, ADDS=adds, Z4096=zeros(4096),
                        Z16=zeros(16), Z256=zeros(256))
            if k == "group":
                tail = fill(GO_GROUP_TAIL, SEL=c[2], Z16=zeros(16))
            elif k == "model":
                tail = fill(GO_MODEL_TAIL, TAG=c[2])
            else:
                tail = fill(GO_SIBLING_TAIL, MODEL=c[2], KEYWORD=c[3], TAG=c[4])
            body.append(head + tail)
        elif k == "sidecar":
            body.append(fill(GO_SIDECAR, ID=idx, NAME=c[1], Z32=zeros(32), Z256=zeros(256),
                             FNAME="repo/" + c[1] + ".gguf"))
        elif k == "ismodel":
            calls = "\n".join(fill(GO_ISMODEL_ONE, K=i, PATH=p) for i, p in enumerate(c[1]))
            body.append(fill(GO_ISMODEL_HEAD, ID=idx, N=len(c[1]), CALLS=calls, Z256=zeros(256)))
        elif k == "plan":
            pr = c[1]
            body.append(fill(GO_PLAN, ID=idx, P0=pr[0], P1=pr[1], P2=pr[2], P3=pr[3], P4=pr[4],
                             Z256=zeros(256)))
        elif k == "repo":
            head = fill(GO_REPO_HEAD, ID=idx, Z512=zeros(512))
            ones = "\n".join(fill(GO_REPO_ONE, K=i, STR=s) for i, s in enumerate(c[1]))
            body.append(head + "\n" + ones + "\n" + GO_REPO_TAIL)
        elif k == "resume":
            calls = "\n".join(fill(GO_RESUME_ONE, K=i, DL=d, ST=st) for i, (d, st) in enumerate(c[1]))
            head = fill(GO_RESUME_HEAD, ID=idx, N=len(c[1]), Z256=zeros(256), CALLS=calls)
            body.append(head + "\n" + GO_RESUME_TAIL)

    head = r'''
// Gate driver, generated by tests/gen_s3.py. Calls the private bodies of the
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
    return d_copy(dst, at, s, 0, s.len);
}

fn put_fld(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;
    while i < s.len {
        let c = s[i];
        if c == 124 || c == 10 || c == 13 {
            dst[p] = 63;
        } else {
            dst[p] = c;
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
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
    (root / 'tests' / 'probe_s3.cpp').write_text(gen_cpp())
    port = (root / 'source' / 'download-selection.goose').read_text()
    (root / 'build' / 'gate_s3.goose').write_text(gen_goose(port))
    print("wrote tests/probe_s3.cpp and build/gate_s3.goose, cases =", len(CASES))


if __name__ == '__main__':
    main()
