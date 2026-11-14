#!/usr/bin/env python3
# Generates both sides of the P7-04 S2 gate from one fixture table:
#   tests/probe.cpp  native oracle: common_imatrix_load, load_imatrix,
#                    prepare_imatrix, remap_layer and remap_imatrix over the
#                    same bytes, handed to the native loaders through a memfd
#   build/gt.goose   the port plus a driver over the same cases
#
# No weights and no test files on disk: every fixture is a byte buffer built
# here. Each native case runs in a child process, so a native exit(1) or
# GGML_ABORT is observable as a line instead of killing the probe.
import hashlib
import os
import struct
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = Path(os.environ.get("GOOSE_SRC", ROOT / "source" / "imatrix.goose"))
OUT = Path(os.environ.get("OUT_DIR", ROOT / "build"))

F32, F16, UINT32, INT32, STRING, ARRAY = 0, 1, 4, 5, 8, 9


def f(x):
    """The f32 bit pattern of a decimal value."""
    return struct.unpack("<I", struct.pack("<f", x))[0]


def fbytes(u):
    return struct.pack("<I", u)


# ---------------------------------------------------------------- fixtures

def legacy(entries, chunk=None, dataset=None, tail=b""):
    """n_entries, then (len, name, ncall, nval, sums), then the optional tail."""
    b = struct.pack("<i", len(entries))
    for name, ncall, sums in entries:
        nb = name.encode()
        b += struct.pack("<i", len(nb)) + nb
        b += struct.pack("<i", ncall) + struct.pack("<i", len(sums))
        b += b"".join(fbytes(s) for s in sums)
    if chunk is not None:
        b += struct.pack("<i", chunk)
    if dataset is not None:
        b += struct.pack("<i", len(dataset)) + dataset
    b += tail
    return b


def enc_scalar(t, v):
    if t == 0:
        return struct.pack("<B", v)
    if t == 1:
        return struct.pack("<b", v)
    if t == 2:
        return struct.pack("<H", v)
    if t == 3:
        return struct.pack("<h", v)
    if t == 4:
        return struct.pack("<I", v)
    if t == 5:
        return struct.pack("<i", v)
    if t == 6:
        return struct.pack("<f", v)
    if t == 7:
        return struct.pack("<B", 1 if v else 0)
    if t == 8:
        return struct.pack("<Q", len(v)) + v
    if t == 10:
        return struct.pack("<Q", v)
    if t == 11:
        return struct.pack("<q", v)
    if t == 12:
        return struct.pack("<d", v)
    raise ValueError(t)


def gguf(kv, tensors, align=32, version=3, magic=b"GGUF"):
    """A GGUF image: header, KVs, tensor infos, then the aligned data section."""
    b = magic + struct.pack("<I", version)
    b += struct.pack("<Q", len(tensors)) + struct.pack("<Q", len(kv))
    for key, typ, val in kv:
        kb = key.encode()
        b += struct.pack("<Q", len(kb)) + kb
        if typ == ARRAY:
            et, items = val
            b += struct.pack("<I", ARRAY) + struct.pack("<I", et) + struct.pack("<Q", len(items))
            for it in items:
                b += enc_scalar(et, it)
        else:
            b += struct.pack("<I", typ) + enc_scalar(typ, val)
    data = b""
    for name, ne, typ, blob in tensors:
        nb = name.encode()
        b += struct.pack("<Q", len(nb)) + nb
        b += struct.pack("<I", len(ne))
        for d in ne:
            b += struct.pack("<Q", d)
        b += struct.pack("<I", typ)
        b += struct.pack("<Q", len(data))
        data += blob + b"\x00" * ((-len(blob)) % align)
    return b + b"\x00" * ((-len(b)) % align) + data


def f32t(vals):
    return b"".join(fbytes(v) for v in vals)


def pair(name, sums, counts, stype=F32, ctype=F32):
    return [
        (name + ".in_sum2", [len(sums)], stype, f32t(sums)),
        (name + ".counts", [len(counts)], ctype, f32t(counts)),
    ]


def meta_kv(chunk=12, size=512, datasets=(b"train", b"valid")):
    return [("imatrix.datasets", ARRAY, (STRING, list(datasets))),
            ("imatrix.chunk_count", UINT32, chunk),
            ("imatrix.chunk_size", UINT32, size)]


A = f(1.0)
B = f(3.0)
C = f(0.5)
D = f(0.25)
E = f(4.0)
NAN = 0x7fc00001
INF = 0x7f800000
NEG = 0xbf800000
SUB = 0x00000001
NZERO = 0x80000000

LEGACY_CASES = [
    ("legacy_ok", legacy([
        ("blk.0.attn_q.weight", 3, [A, B, E]),
        ("blk.1.ffn_down.weight", 2, [C, D]),
        ("output.weight", 1, [A]),
    ], 9, b"train")),
    ("legacy_notrailer", legacy([
        ("blk.0.attn_q.weight", 2, [A, D]),
        ("blk.2.attn_k.weight", 1, [E]),
    ])),
    ("legacy_short_tail", legacy([
        ("blk.0.attn_q.weight", 1, [A]),
        ("blk.1.attn_v.weight", 1, [C]),
    ], 7, None, b"\x07\x00")),
    ("legacy_tail_len0", legacy([("blk.0.attn_q.weight", 1, [A])], 5, b"")),
    ("legacy_tail_trunc_ds", legacy([("blk.0.attn_q.weight", 1, [A])], 5, None,
                                    struct.pack("<i", 10) + b"abc")),
    ("legacy_dup", legacy([
        ("blk.0.attn_q.weight", 1, [A]),
        ("blk.0.attn_q.weight", 4, [B, C]),
    ])),
    ("legacy_zero", struct.pack("<i", 0)),
    ("legacy_trunc_name", struct.pack("<i", 1) + struct.pack("<i", 5) + b"ab"),
    ("legacy_nval0", struct.pack("<i", 1) + struct.pack("<i", 4) + b"blk." +
     struct.pack("<i", 1) + struct.pack("<i", 0)),
    ("legacy_trunc_sums", struct.pack("<i", 1) + struct.pack("<i", 4) + b"blk." +
     struct.pack("<i", 1) + struct.pack("<i", 3) + fbytes(A) + fbytes(B)),
    ("legacy_empty", b""),
    ("legacy_nul", legacy([("blk.0.attn\x00q.weight", 1, [A])])),
    ("legacy_gguf_magic", b"GGUF" + b"\x00" * 20),
    ("legacy_mixed_names", legacy([
        ("token_embd.weight", 2, [A]),
        ("blk.10.attn_q.weight", 1, [B]),
        ("blk.2.attn_q.weight", 1, [C]),
        ("output.weight", 1, [D]),
    ], 4, b"mix")),
]

GGUF_CASES = [
    ("gguf_ok", gguf(meta_kv(), pair("blk.0.attn_q.weight", [A, B, E, C], [f(4.0)]))),
    ("gguf_multi", gguf(meta_kv(6, 256, [b"d1"]),
                        pair("blk.3.ffn_down.weight", [A, B, C, D, E, A, B, C],
                             [f(2.0), f(4.0)]))),
    ("gguf_indivisible", gguf(meta_kv(3, 64, []),
                              pair("blk.0.attn_v.weight", [A, B, C, D, E, A, B, C, D, E],
                                   [f(2.0), f(2.0), f(2.0)]))),
    ("gguf_sums_lt_counts", gguf(meta_kv(2, 32, [b"x"]),
                                 pair("blk.0.attn_k.weight", [A, B],
                                      [f(1.0), f(1.0), f(1.0), f(1.0)]))),
    ("gguf_missing_counts", gguf(meta_kv(1, 8, [b"x"]),
                                 [("blk.0.attn_q.weight.in_sum2", [4], F32,
                                   f32t([A, B, C, D]))])),
    ("gguf_missing_sums", gguf(meta_kv(1, 8, [b"x"]),
                               [("blk.0.attn_q.weight.counts", [1], F32,
                                 f32t([f(4.0)]))])),
    ("gguf_wrong_type", gguf(meta_kv(1, 8, [b"x"]),
                             pair("blk.0.attn_q.weight", [A, B], [f(2.0)], F32, F16))),
    ("gguf_nometa", gguf([], pair("blk.0.attn_q.weight", [A, B], [f(2.0)]))),
    ("gguf_partial_meta", gguf([("imatrix.datasets", ARRAY, (STRING, [b"x"])),
                                ("imatrix.chunk_count", UINT32, 3)],
                               pair("blk.0.attn_q.weight", [A, B], [f(2.0)]))),
    ("gguf_zero_tensors", gguf(meta_kv(1, 8, [b"x"]), [])),
    ("gguf_align16", gguf([("general.alignment", UINT32, 16)] + meta_kv(2, 16, [b"a"]),
                          pair("blk.0.attn_q.weight", [A, B, C], [f(3.0)]), align=16)),
    ("gguf_align_bad", gguf([("general.alignment", UINT32, 24)] + meta_kv(2, 16, [b"a"]),
                            pair("blk.0.attn_q.weight", [A, B, C], [f(3.0)]), align=24)),
    ("gguf_ds_scalar", gguf([("imatrix.datasets", STRING, b"single"),
                             ("imatrix.chunk_count", UINT32, 1),
                             ("imatrix.chunk_size", UINT32, 8)],
                            pair("blk.0.attn_q.weight", [A, B], [f(2.0)]))),
    ("gguf_suffix_swap", gguf(meta_kv(1, 8, [b"x"]), [
        ("x.in_sum2", [2], F32, f32t([A, B])),
        ("x.in_sum2.counts", [1], F32, f32t([f(2.0)])),
    ])),
    ("gguf_trunc_data", gguf(meta_kv(1, 8, [b"x"]),
                             pair("blk.0.attn_q.weight", [A, B], [f(2.0)]))[:-4]),
    ("gguf_cc_array", gguf([("imatrix.chunk_count", ARRAY, (UINT32, [1, 2])),
                            ("imatrix.chunk_size", UINT32, 8),
                            ("imatrix.datasets", ARRAY, (STRING, [b"x"]))],
                           pair("blk.0.attn_q.weight", [A, B], [f(2.0)]))),
    ("gguf_cc_int32", gguf([("imatrix.chunk_count", INT32, 4),
                            ("imatrix.chunk_size", UINT32, 8),
                            ("imatrix.datasets", ARRAY, (STRING, [b"x"]))],
                           pair("blk.0.attn_q.weight", [A, B], [f(2.0)]))),
    ("gguf_lround", gguf(meta_kv(4, 32, [b"x"]),
                         pair("blk.0.attn_q.weight", [A, B, C, D],
                              [f(2.5), f(3.5), f(-0.5), f(0.0)]))),
    ("gguf_specials", gguf(meta_kv(1, 8, [b"x"]),
                           pair("blk.0.attn_q.weight", [NAN, INF, NEG, SUB, NZERO],
                                [f(1.0)]))),
    ("gguf_unrelated", gguf(meta_kv(1, 8, [b"x"]),
                            [("token_embd.weight", [4], F32, f32t([A, B, C, D]))] +
                            pair("blk.0.attn_q.weight", [A, B], [f(2.0)]))),
    ("gguf_v2", gguf(meta_kv(1, 8, [b"x"]),
                     pair("blk.0.attn_q.weight", [A, B], [f(2.0)]), version=2)),
    ("gguf_unsorted", gguf(meta_kv(1, 8, [b"x"]),
                           pair("blk.10.attn_q.weight", [A], [f(1.0)]) +
                           pair("blk.2.attn_q.weight", [B], [f(1.0)]) +
                           pair("blk.0.attn_q.weight", [C], [f(1.0)]))),
]

IMX = LEGACY_CASES + GGUF_CASES
IMX_ID = {name: i for i, (name, _) in enumerate(IMX)}

PREP_CASES = [
    ("all", "legacy_ok", [], []),
    ("ex_attn", "legacy_ok", ["attn"], []),
    ("ex_blk1", "legacy_ok", ["blk.1"], []),
    ("in_weight", "legacy_ok", [], ["weight"]),
    ("in_two", "legacy_ok", [], ["ffn", "output"]),
    ("ex_in", "legacy_ok", ["blk"], ["attn"]),
    ("none", "legacy_ok", ["nomatch"], ["nomatch2"]),
    ("nofile", None, [], []),
    ("nometa", "gguf_nometa", [], []),
    ("loadfail", "gguf_missing_counts", [], []),
    ("gguf_ex", "gguf_ok", ["blk.0"], []),
    ("gguf_in", "gguf_unsorted", [], ["blk.0"]),
]

REMAP_CASES = [
    ("empty_prune", [], ["blk.0.attn_q.weight", "blk.1.ffn_down.weight",
                         "output.weight", "blk.10.attn_k.weight"], []),
    ("prune_2_5", [2, 5], ["blk.0.attn_q.weight", "blk.1.attn_q.weight",
                           "blk.2.attn_q.weight", "blk.3.attn_q.weight",
                           "blk.5.attn_q.weight", "blk.7.attn_q.weight"], []),
    ("prune_zero", [0], ["blk.0.attn_q.weight", "blk.1.attn_q.weight", "output.weight"], []),
    ("repeat", [3], ["blk.5.attn", "blk.1.attn", "blk.3.attn", "blk.5.attn", "blk.10.attn"], []),
    ("front_gt", [1, 4], ["token_embd.weight", "blk.0.attn", "blk.1.attn",
                          "blk.4.attn", "blk.6.attn"], []),
    ("leading_zero", [2], ["blk.07.attn"], []),
    ("below_front", [2], ["blk.1.attn"], []),
    ("two_blk", [2], ["blk.3.attn.blk.4.extra"], []),
    ("above_front", [5], ["blk.10.attn"], []),
    ("no_dot", [2], ["blk.0", "blk.attr.weight"], []),
    ("leading_zero_remap", [2], ["blk.1.attn", "blk.07.attn"], []),
    ("stoi_range", [2], ["blk.3000000000.attn"], []),
    # remap_imatrix on names remap_layer never produced: the GGML_ABORT path,
    # which the quantizer's own pipeline cannot reach.
    ("ri_extra_abort", [2], ["blk.0.attn"], ["blk.5.attn"]),
    ("ri_extra_ok", [2], ["blk.0.attn"], ["blk.0.attn", "blk.0.attn_extra"]),
    ("ri_extra_two", [2], ["blk.0.attn", "blk.3.attn"], ["blk.3.attn", "blk.9.attn"]),
]


# --------------------------------------------------------------- C++ probe

def bstr(b):
    return ", ".join(str(x) for x in b)


PROBE_HEAD = r'''// Generated by tests/gen_imx.py - native oracle for P7-04 S2.
// Every fixture is a synthetic byte buffer; the native loaders see it through
// memfd_create, so no test file is written to disk.
#include "native_api.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

static const std::vector<std::vector<unsigned char>> IMX = {
'''

PROBE_BODY = r'''
static std::string make_memfd(const std::vector<unsigned char> & bytes) {
    int fd = memfd_create("imx", 0);
    if (fd < 0) { perror("memfd_create"); _exit(2); }
    size_t off = 0;
    while (off < bytes.size()) {
        ssize_t w = write(fd, bytes.data() + off, bytes.size() - off);
        if (w <= 0) { perror("write"); _exit(2); }
        off += (size_t) w;
    }
    lseek(fd, 0, SEEK_SET);
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    return std::string(path);
}

static void put_bits(FILE * f, const std::vector<float> & v) {
    for (size_t i = 0; i < v.size(); i++) {
        uint32_t u = 0;
        memcpy(&u, &v[i], 4);
        fprintf(f, "%s%08x", i ? "," : "", u);
    }
}

static void put_counts(FILE * f, const std::vector<int64_t> & v) {
    for (size_t i = 0; i < v.size(); i++) {
        fprintf(f, "%s%lld", i ? "," : "", (long long) v[i]);
    }
}

static void case_load(size_t i, FILE * out) {
    std::string path = make_memfd(IMX[i]);
    n_imx raw;
    if (n_raw(path, raw) != 0) {
        fprintf(out, "I%zu|raw|0\n", i);
        return;
    }
    fprintf(out, "I%zu|raw|1\n", i);
    fprintf(out, "I%zu|meta|leg=%d|hasmeta=%d|chunk=%d|csize=%d|n=%d\n", i,
            raw.is_legacy ? 1 : 0, raw.has_metadata ? 1 : 0,
            (int) raw.chunk_count, (int) raw.chunk_size, (int) raw.entries.size());
    for (const auto & e : raw.entries) {
        fprintf(out, "I%zu|e|%s|%zu|", i, e.name.c_str(), e.sums.size());
        put_bits(out, e.sums);
        fprintf(out, "|%zu|", e.counts.size());
        put_counts(out, e.counts);
        fprintf(out, "\n");
    }
    fprintf(out, "I%zu|ds|%zu", i, raw.datasets.size());
    for (const auto & d : raw.datasets) {
        fprintf(out, "|%s", d.c_str());
    }
    fprintf(out, "\n");

    std::vector<n_entry> q;
    int chunk = 0;
    n_norm(path, q, chunk);   // native load_imatrix: may exit(1)
    for (const auto & e : q) {
        fprintf(out, "I%zu|norm|%s|%zu|", i, e.name.c_str(), e.sums.size());
        put_bits(out, e.sums);
        fprintf(out, "\n");
    }
    fprintf(out, "I%zu|last|%d\n", i, chunk);
}

static void case_prep(size_t i, int imx, const std::vector<std::string> & ex,
                      const std::vector<std::string> & in, FILE * out) {
    std::string path;
    if (imx >= 0) { path = make_memfd(IMX[(size_t) imx]); }
    std::vector<std::string> kept;
    int last = -2;
    n_prep(path, ex, in, kept, last);   // native prepare_imatrix: may exit(1)
    fprintf(out, "P%zu|kept|%zu", i, kept.size());
    for (const auto & k : kept) {
        fprintf(out, "|%s", k.c_str());
    }
    fprintf(out, "\n");
    fprintf(out, "P%zu|last|%d\n", i, last);
}

static void case_remap(size_t i, const std::vector<int> & prune,
                       const std::vector<std::string> & names,
                       const std::vector<std::string> & extra, FILE * out) {
    fprintf(out, "R%zu|np=%zu|n=%zu\n", i, prune.size(), names.size());
    std::map<int, std::string> mapped;
    int next_id = 0;
    std::vector<std::string> kept;
    for (size_t j = 0; j < names.size(); j++) {
        std::string r = n_remap_layer(names[j], prune, mapped, next_id);  // may abort
        fprintf(out, "R%zu|t|%zu|%d|%s\n", i, j, r.empty() ? 0 : 1, r.c_str());
        if (!r.empty()) { kept.push_back(r); }
    }
    fprintf(out, "R%zu|st|%d|%zu", i, next_id, mapped.size());
    for (const auto & p : mapped) {
        fprintf(out, "|%d:%s", p.first, p.second.c_str());
    }
    fprintf(out, "\n");
    size_t k = 0;
    for (const auto & name : kept) {
        std::string r = n_remap_imatrix(name, mapped);      // may GGML_ABORT
        fprintf(out, "R%zu|i|%zu|%d|%s\n", i, k++, r == name ? 0 : 1, r.c_str());
    }
    for (const auto & name : extra) {
        std::string r = n_remap_imatrix(name, mapped);      // may GGML_ABORT
        fprintf(out, "R%zu|i|%zu|%d|%s\n", i, k++, r == name ? 0 : 1, r.c_str());
    }
}

// Run one case in a child, capture its lines through a pipe, then report how
// the child ended when it did not reach its own end.
static void run_case(size_t i, void (*body)(size_t, FILE *)) {
    int fds[2];
    if (pipe(fds) != 0) { perror("pipe"); exit(2); }
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        FILE * out = fdopen(fds[1], "w");
        setvbuf(out, nullptr, _IONBF, 0);
        if (!freopen("/dev/null", "w", stdout)) { _exit(2); }
        if (!freopen("/dev/null", "w", stderr)) { _exit(2); }
        body(i, out);
        fflush(out);
        _exit(0);
    }
    close(fds[1]);
    std::string buf;
    char tmp[4096];
    ssize_t r;
    while ((r = read(fds[0], tmp, sizeof(tmp))) > 0) { buf.append(tmp, (size_t) r); }
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    fwrite(buf.data(), 1, buf.size(), stdout);
    if (WIFSIGNALED(status)) {
        printf("Z%zu|term|sig%d\n", i, (int) WTERMSIG(status));
    } else if (WEXITSTATUS(status) != 0) {
        printf("Z%zu|term|exit%d\n", i, (int) WEXITSTATUS(status));
    }
}
'''


def gen_probe():
    L = [PROBE_HEAD]
    for name, b in IMX:
        L.append("    { /* %s */ %s }," % (name, bstr(b)))
    L.append("};")
    L.append(PROBE_BODY)

    for i, (_, ref, ex, inc) in enumerate(PREP_CASES):
        rid = -1 if ref is None else IMX_ID[ref]
        exs = "{" + ", ".join('"%s"' % p for p in ex) + "}"
        ins = "{" + ", ".join('"%s"' % p for p in inc) + "}"
        L.append("static void prep_%d(size_t, FILE * out) { case_prep(%d, %d, %s, %s, out); }"
                 % (i, i, rid, exs, ins))
    for i, (_, prune, names, extra) in enumerate(REMAP_CASES):
        ps = "{" + ", ".join(str(p) for p in prune) + "}"
        ns = "{" + ", ".join('"%s"' % n for n in names) + "}"
        xs = "{" + ", ".join('"%s"' % n for n in extra) + "}"
        L.append("static void remap_%d(size_t, FILE * out) { case_remap(%d, %s, %s, %s, out); }"
                 % (i, i, ps, ns, xs))

    L.append("")
    L.append("int main() {")
    L.append("    setvbuf(stdout, nullptr, _IONBF, 0);")
    for i in range(len(IMX)):
        L.append("    run_case(%d, case_load);" % i)
    for i in range(len(PREP_CASES)):
        L.append("    run_case(%d, prep_%d);" % (i, i))
    for i in range(len(REMAP_CASES)):
        L.append("    run_case(%d, remap_%d);" % (i, i))
    L.append("    return 0;")
    L.append("}")
    return "\n".join(L) + "\n"


# ------------------------------------------------------------ Goose driver

DRIVER_HELPERS = r'''
fn dput_dec(dst: u8[:], at: i64, v: i64) -> i64 {
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
    var buf: u8[24] = default<u8[24]>();
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

fn dput_hex(dst: u8[:], at: i64, v: u32) -> i64 {
    var buf: u8[8] = default<u8[8]>();
    var x = v;
    var i: i64 = 8;
    while i > 0 {
        i = i - 1;
        let d = (x & 15) as u8;
        if d < 10 { buf[i] = 48 + d; } else { buf[i] = 87 + d; }
        x = x >> 4;
    }
    i = 0;
    while i < 8 {
        dst[at + i] = buf[i];
        i = i + 1;
    }
    return at + 8;
}

fn dput_s(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len {
        dst[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn dput_sub(dst: u8[:], at: i64, s: const u8[:], from: i64, l: i64) -> i64 {
    var i: i64 = 0;
    while i < l {
        dst[at + i] = s[from + i];
        i = i + 1;
    }
    return at + l;
}

fn dbits32(x: f32) -> u32 {
    let a: f32[1] = [x];
    let b = bytes_of(a);
    return b[0] | (b[1] as u32) << 8 | (b[2] as u32) << 16 | (b[3] as u32) << 24;
}

fn dsame(a: const u8[:], b: const u8[:]) -> i64 {
    if a.len != b.len { return 0; }
    var i: i64 = 0;
    while i < a.len {
        if a[i] != b[i] { return 0; }
        i = i + 1;
    }
    return 1;
}

fn dterm(i: i64, how: const u8[:]) {
    var line: u8[64] = default<u8[64]>();
    var p = dput_s(line, 0, "Z");
    p = dput_dec(line, p, i);
    p = dput_s(line, p, "|term|");
    p = dput_s(line, p, how);
    print(line[0..p]);
}

fn emit_raw(i: i64, buf: const u8[:], noff: i64[:], nlen: i64[:],
            soff: i64[:], slen: i64[:], spool: f32[:],
            coff: i64[:], clen: i64[:], cpool: i64[:],
            dsoff: i64[:], dslen: i64[:], meta: i64[:]) {
    var line: u8[1024] = default<u8[1024]>();
    var p = dput_s(line, 0, "I");
    p = dput_dec(line, p, i);
    p = dput_s(line, p, "|raw|1");
    print(line[0..p]);

    p = dput_s(line, 0, "I");
    p = dput_dec(line, p, i);
    p = dput_s(line, p, "|meta|leg=");
    p = dput_dec(line, p, meta[M_LEGACY]);
    p = dput_s(line, p, "|hasmeta=");
    p = dput_dec(line, p, meta[M_HASMETA]);
    p = dput_s(line, p, "|chunk=");
    p = dput_dec(line, p, meta[M_CHUNK]);
    p = dput_s(line, p, "|csize=");
    p = dput_dec(line, p, meta[M_CHUNKSZ]);
    p = dput_s(line, p, "|n=");
    p = dput_dec(line, p, meta[M_N]);
    print(line[0..p]);

    var k: i64 = 0;
    while k < meta[M_N] {
        p = dput_s(line, 0, "I");
        p = dput_dec(line, p, i);
        p = dput_s(line, p, "|e|");
        p = dput_sub(line, p, buf, noff[k], nlen[k]);
        p = dput_s(line, p, "|");
        p = dput_dec(line, p, slen[k]);
        p = dput_s(line, p, "|");
        var j: i64 = 0;
        while j < slen[k] {
            if j > 0 { p = dput_s(line, p, ","); }
            p = dput_hex(line, p, dbits32(spool[soff[k] + j]));
            j = j + 1;
        }
        p = dput_s(line, p, "|");
        p = dput_dec(line, p, clen[k]);
        p = dput_s(line, p, "|");
        j = 0;
        while j < clen[k] {
            if j > 0 { p = dput_s(line, p, ","); }
            p = dput_dec(line, p, cpool[coff[k] + j]);
            j = j + 1;
        }
        print(line[0..p]);
        k = k + 1;
    }

    p = dput_s(line, 0, "I");
    p = dput_dec(line, p, i);
    p = dput_s(line, p, "|ds|");
    p = dput_dec(line, p, meta[M_NDS]);
    k = 0;
    while k < meta[M_NDS] {
        p = dput_s(line, p, "|");
        p = dput_sub(line, p, buf, dsoff[k], dslen[k]);
        k = k + 1;
    }
    print(line[0..p]);
}

fn emit_norm(i: i64, noff: i64[:], nlen: i64[:], buf: const u8[:],
             qoff: i64[:], qlen: i64[:], qpool: f32[:], n: i64) {
    var line: u8[1024] = default<u8[1024]>();
    var k: i64 = 0;
    while k < n {
        var p = dput_s(line, 0, "I");
        p = dput_dec(line, p, i);
        p = dput_s(line, p, "|norm|");
        p = dput_sub(line, p, buf, noff[k], nlen[k]);
        p = dput_s(line, p, "|");
        p = dput_dec(line, p, qlen[k]);
        p = dput_s(line, p, "|");
        var j: i64 = 0;
        while j < qlen[k] {
            if j > 0 { p = dput_s(line, p, ","); }
            p = dput_hex(line, p, dbits32(qpool[qoff[k] + j]));
            j = j + 1;
        }
        print(line[0..p]);
        k = k + 1;
    }
}

fn emit_prep(i: i64, buf: const u8[:], noff: i64[:], nlen: i64[:],
             out: i64[:], cnt: i64, last: i64) {
    var line: u8[1024] = default<u8[1024]>();
    var p = dput_s(line, 0, "P");
    p = dput_dec(line, p, i);
    p = dput_s(line, p, "|kept|");
    p = dput_dec(line, p, cnt);
    var k: i64 = 0;
    while k < cnt {
        p = dput_s(line, p, "|");
        p = dput_sub(line, p, buf, noff[out[k]], nlen[out[k]]);
        k = k + 1;
    }
    print(line[0..p]);

    p = dput_s(line, 0, "P");
    p = dput_dec(line, p, i);
    p = dput_s(line, p, "|last|");
    p = dput_dec(line, p, last);
    print(line[0..p]);
}
'''


def zeros_i64(n):
    return "default<i64[%d]>()" % n


def zeros_f32(n):
    return "default<f32[%d]>()" % n


def zeros_u8(n):
    return "default<u8[%d]>()" % n


def case_imx_block(i, b):
    n = len(b)
    return f'''
    {{
        var buf: u8[{n}] = [{bstr(b)}];
        var tko: i64[16] = {zeros_i64(16)}; var tkl: i64[16] = {zeros_i64(16)};
        var tkyo: i64[16] = {zeros_i64(16)}; var tkyl: i64[16] = {zeros_i64(16)};
        var tkind: i64[16] = {zeros_i64(16)}; var tdata: i64[16] = {zeros_i64(16)};
        var tnv: i64[16] = {zeros_i64(16)}; var ttype: i64[16] = {zeros_i64(16)};
        var noff: i64[16] = {zeros_i64(16)}; var nlen: i64[16] = {zeros_i64(16)};
        var soff: i64[16] = {zeros_i64(16)}; var slen: i64[16] = {zeros_i64(16)};
        var spool: f32[64] = {zeros_f32(64)};
        var coff: i64[16] = {zeros_i64(16)}; var clen: i64[16] = {zeros_i64(16)};
        var cpool: i64[16] = {zeros_i64(16)};
        var dsoff: i64[8] = {zeros_i64(8)}; var dslen: i64[8] = {zeros_i64(8)};
        var meta: i64[8] = {zeros_i64(8)};
        var qoff: i64[16] = {zeros_i64(16)}; var qlen: i64[16] = {zeros_i64(16)};
        var qpool: f32[64] = {zeros_f32(64)};
        let id: i64 = {i};
        let st = im_load(buf[0..{n}], tko, tkl, tkyo, tkyl, tkind, tdata, tnv, ttype,
                         noff, nlen, soff, slen, spool, coff, clen, cpool,
                         dsoff, dslen, meta);
        if st == 8 {{
            dterm(id, "sig6");
        }} else if st != 0 {{
            var line: u8[64] = {zeros_u8(64)};
            var p = dput_s(line, 0, "I");
            p = dput_dec(line, p, id);
            p = dput_s(line, p, "|raw|0");
            print(line[0..p]);
        }} else {{
            emit_raw(id, buf[0..{n}], noff, nlen, soff, slen, spool, coff, clen, cpool,
                     dsoff, dslen, meta);
            let ns = im_normalize(meta[M_N], soff, slen, spool, coff, clen, cpool,
                                  meta[M_LEGACY], meta[M_HASMETA], qoff, qlen, qpool);
            if ns != 0 {{
                dterm(id, "exit1");
            }} else {{
                emit_norm(id, noff, nlen, buf[0..{n}], qoff, qlen, qpool, meta[M_N]);
                var line: u8[64] = {zeros_u8(64)};
                var p = dput_s(line, 0, "I");
                p = dput_dec(line, p, id);
                p = dput_s(line, p, "|last|");
                p = dput_dec(line, p, meta[M_CHUNK]);
                print(line[0..p]);
            }}
        }}
    }}'''


def pat_arrays(name, pats):
    """A byte pool plus offset/length arrays for one pattern list."""
    pool = b""
    offs = []
    for p in pats:
        offs.append(len(pool))
        pool += p.encode()
    n = max(1, len(pool))
    pol = [str(o) for o in offs] + ["0"] * (8 - len(offs))
    pll = [str(len(p)) for p in pats] + ["0"] * (8 - len(pats))
    po = ", ".join(pol)
    pl = ", ".join(pll)
    return (f'        var {name}b: u8[{n}] = [{bstr(pool) if pool else "0"}];\n'
            f'        var {name}o: i64[8] = [{po}];\n'
            f'        var {name}l: i64[8] = [{pl}];\n'), len(pool)


def case_prep_block(i, spec):
    _, ref, ex, inc = spec
    parts = [f'''
    {{
        var noff: i64[16] = {zeros_i64(16)}; var nlen: i64[16] = {zeros_i64(16)};
        var soff: i64[16] = {zeros_i64(16)}; var slen: i64[16] = {zeros_i64(16)};
        var spool: f32[64] = {zeros_f32(64)};
        var coff: i64[16] = {zeros_i64(16)}; var clen: i64[16] = {zeros_i64(16)};
        var cpool: i64[16] = {zeros_i64(16)};
        var meta: i64[8] = {zeros_i64(8)};
        var qoff: i64[16] = {zeros_i64(16)}; var qlen: i64[16] = {zeros_i64(16)};
        var qpool: f32[64] = {zeros_f32(64)};
        var out: i64[16] = {zeros_i64(16)};
        var tko: i64[16] = {zeros_i64(16)}; var tkl: i64[16] = {zeros_i64(16)};
        var tkyo: i64[16] = {zeros_i64(16)}; var tkyl: i64[16] = {zeros_i64(16)};
        var tkind: i64[16] = {zeros_i64(16)}; var tdata: i64[16] = {zeros_i64(16)};
        var tnv: i64[16] = {zeros_i64(16)}; var ttype: i64[16] = {zeros_i64(16)};
        var dsoff: i64[8] = {zeros_i64(8)}; var dslen: i64[8] = {zeros_i64(8)};
        let id: i64 = {i};''']
    if ref is None:
        b = b""
        n = 0
        parts.append(f'        var buf: u8[1] = [0];')
        tx, exlen = pat_arrays("ex", ex)
        ti, inlen = pat_arrays("in", inc)
        parts.append(tx)
        parts.append(ti)
        parts.append(f'''
        let cnt = im_prepare(0, noff, nlen, buf[0..0], exb[0..{exlen}], exo, exl, {len(ex)},
                             inb[0..{inlen}], ino, inl, {len(inc)}, 0, 0, out, meta);
        emit_prep(id, buf[0..0], noff, nlen, out, cnt, meta[M_LASTCALL]);''')
    else:
        b = dict(IMX)[ref]
        n = len(b)
        parts.append(f'        var buf: u8[{n}] = [{bstr(b)}];')
        tx, exlen = pat_arrays("ex", ex)
        ti, inlen = pat_arrays("in", inc)
        parts.append(tx)
        parts.append(ti)
        parts.append(f'''
        let st = im_load(buf[0..{n}], tko, tkl, tkyo, tkyl, tkind, tdata, tnv, ttype,
                         noff, nlen, soff, slen, spool, coff, clen, cpool,
                         dsoff, dslen, meta);
        if st == 8 {{
            dterm(id, "sig6");
        }} else if st != 0 {{
            dterm(id, "exit1");
        }} else {{
            let ns = im_normalize(meta[M_N], soff, slen, spool, coff, clen, cpool,
                                  meta[M_LEGACY], meta[M_HASMETA], qoff, qlen, qpool);
            if ns != 0 {{
                dterm(id, "exit1");
            }} else {{
                let cnt = im_prepare(meta[M_N], noff, nlen, buf[0..{n}],
                                     exb[0..{exlen}], exo, exl, {len(ex)},
                                     inb[0..{inlen}], ino, inl, {len(inc)},
                                     1, meta[M_CHUNK], out, meta);
                emit_prep(id, buf[0..{n}], noff, nlen, out, cnt, meta[M_LASTCALL]);
            }}
        }}''')
    parts.append("    }")
    return "\n".join(parts)


def case_remap_block(i, spec):
    _, prune, names, extra = spec
    pool = b""
    offs = []
    for nm in names:
        offs.append(len(pool))
        pool += nm.encode()
    n = max(1, len(pool))
    xpool = b""
    xoffs = []
    for nm in extra:
        xoffs.append(len(xpool))
        xpool += nm.encode()
    xn = max(1, len(xpool))
    noff = ", ".join([str(o) for o in offs] + ["0"] * (32 - len(offs)))
    nlen = ", ".join([str(len(nm)) for nm in names] + ["0"] * (32 - len(names)))
    pr = ", ".join([str(p) for p in prune] + ["0"] * (8 - len(prune)))
    xoff = ", ".join([str(o) for o in xoffs] + ["0"] * (16 - len(xoffs)))
    xlen = ", ".join([str(len(nm)) for nm in extra] + ["0"] * (16 - len(extra)))
    return f'''
    {{
        var nbuf: u8[{n}] = [{bstr(pool) if pool else "0"}];
        var noff: i64[32] = [{noff}];
        var nlen: i64[32] = [{nlen}];
        var prune: i64[8] = [{pr}];
        var mapk: i64[32] = {zeros_i64(32)}; var mapv: i64[32] = {zeros_i64(32)};
        var nmap: i64[2] = {zeros_i64(2)}; var stt: i64[2] = {zeros_i64(2)};
        var out: u8[1024] = {zeros_u8(1024)};
        var ooff: i64[32] = {zeros_i64(32)}; var olen: i64[32] = {zeros_i64(32)};
        var keep: i64[32] = {zeros_i64(32)};
        var rout: u8[1024] = {zeros_u8(1024)};
        var roff: i64[2] = {zeros_i64(2)}; var rlen: i64[2] = {zeros_i64(2)};
        var rp: u8[{n}] = {zeros_u8(n)};
        var rpoff: i64[32] = {zeros_i64(32)}; var rplen: i64[32] = {zeros_i64(32)};
        var rpcur: i64[2] = {zeros_i64(2)};
        var xbuf: u8[{xn}] = [{bstr(xpool) if xpool else "0"}];
        var xoff: i64[16] = [{xoff}];
        var xlen: i64[16] = [{xlen}];
        var line: u8[1024] = {zeros_u8(1024)};
        let id: i64 = {i};
        var p = dput_s(line, 0, "R");
        p = dput_dec(line, p, id);
        p = dput_s(line, p, "|np=");
        p = dput_dec(line, p, {len(prune)});
        p = dput_s(line, p, "|n=");
        p = dput_dec(line, p, {len(names)});
        print(line[0..p]);

        var bad: i64 = 0;
        var j: i64 = 0;
        while j < {len(names)} && bad == 0 {{
            let r = rl_run(nbuf[0..{n}], noff[j..j + 1], nlen[j..j + 1], 1,
                           prune, {len(prune)}, mapk, mapv, nmap, stt, out,
                           ooff[j..j + 1], olen[j..j + 1], keep[j..j + 1]);
            if r != 0 {{
                bad = r;
            }} else {{
                p = dput_s(line, 0, "R");
                p = dput_dec(line, p, id);
                p = dput_s(line, p, "|t|");
                p = dput_dec(line, p, j);
                p = dput_s(line, p, "|");
                p = dput_dec(line, p, keep[j]);
                p = dput_s(line, p, "|");
                p = dput_sub(line, p, out, 0, olen[j]);
                print(line[0..p]);
                // keep the remapped name: remap_imatrix runs on the new name
                rpoff[j] = rpcur[0];
                rplen[j] = olen[j];
                var q: i64 = 0;
                while q < olen[j] {{
                    rp[rpcur[0] + q] = out[q];
                    q = q + 1;
                }}
                rpcur[0] = rpcur[0] + olen[j];
            }}
            j = j + 1;
        }}
        if bad != 0 {{
            if bad == 10 {{ dterm(id, "sig6"); }} else {{ dterm(id, "internal"); }}
        }} else {{
            p = dput_s(line, 0, "R");
            p = dput_dec(line, p, id);
            p = dput_s(line, p, "|st|");
            p = dput_dec(line, p, stt[0]);
            p = dput_s(line, p, "|");
            p = dput_dec(line, p, nmap[0]);
            var k: i64 = 0;
            while k < nmap[0] {{
                p = dput_s(line, p, "|");
                p = dput_dec(line, p, mapk[k]);
                p = dput_s(line, p, ":");
                if mapv[k] >= 0 {{ p = dput_dec(line, p, mapv[k]); }}
                k = k + 1;
            }}
            print(line[0..p]);

            var ki: i64 = 0;
            j = 0;
            while j < {len(names)} {{
                if keep[j] == 1 {{
                    let r = ri_run(rp[0..{n}], rpoff[j], rplen[j], mapk, mapv, nmap[0],
                                   rout[0..1024], roff[0..1], rlen[0..1]);
                    if r < 0 {{
                        dterm(id, "sig6");
                        j = {len(names)};
                    }} else {{
                        p = dput_s(line, 0, "R");
                        p = dput_dec(line, p, id);
                        p = dput_s(line, p, "|i|");
                        p = dput_dec(line, p, ki);
                        p = dput_s(line, p, "|");
                        let same = dsame(rout[0..rlen[0]], rp[rpoff[j]..rpoff[j] + rplen[j]]);
                        p = dput_dec(line, p, 1 - same);
                        p = dput_s(line, p, "|");
                        p = dput_sub(line, p, rout, 0, rlen[0]);
                        print(line[0..p]);
                        ki = ki + 1;
                        j = j + 1;
                    }}
                }} else {{
                    j = j + 1;
                }}
            }}
            var xe: i64 = 0;
            while xe < {len(extra)} {{
                let r2 = ri_run(xbuf[0..{xn}], xoff[xe], xlen[xe], mapk, mapv, nmap[0],
                                rout[0..1024], roff[0..1], rlen[0..1]);
                if r2 < 0 {{
                    dterm(id, "sig6");
                    xe = {len(extra)};
                }} else {{
                    p = dput_s(line, 0, "R");
                    p = dput_dec(line, p, id);
                    p = dput_s(line, p, "|i|");
                    p = dput_dec(line, p, ki);
                    p = dput_s(line, p, "|");
                    let same2 = dsame(rout[0..rlen[0]], xbuf[xoff[xe]..xoff[xe] + xlen[xe]]);
                    p = dput_dec(line, p, 1 - same2);
                    p = dput_s(line, p, "|");
                    p = dput_sub(line, p, rout, 0, rlen[0]);
                    print(line[0..p]);
                    ki = ki + 1;
                    xe = xe + 1;
                }}
            }}
        }}
    }}'''


def gen_driver(port_text):
    L = [port_text, DRIVER_HELPERS, "", "fn main() {"]
    for i, (_, b) in enumerate(IMX):
        L.append(case_imx_block(i, b))
    for i, spec in enumerate(PREP_CASES):
        L.append(case_prep_block(i, spec))
    for i, spec in enumerate(REMAP_CASES):
        L.append(case_remap_block(i, spec))
    L.append("}")
    return "\n".join(L) + "\n"


def main():
    probe = gen_probe()
    port = SRC.read_text()
    driver = gen_driver(port)
    (HERE / "probe.cpp").write_text(probe)
    (OUT / "gt.goose").write_text(driver)

    rows = ["case\tkind\tbytes\tsha256"]
    for name, b in IMX:
        rows.append("%s\t%s\t%d\t%s" % (name, "gguf" if b[:4] == b"GGUF" else "legacy",
                                          len(b), hashlib.sha256(b).hexdigest()[:16]))
    for name, ref, ex, inc in PREP_CASES:
        rows.append("%s\tprep\tref=%s\tex=%s in=%s" % (name, ref, ";".join(ex), ";".join(inc)))
    for name, prune, names, extra in REMAP_CASES:
        rows.append("%s\tremap\tprune=%s\tn=%d extra=%d" %
                    (name, ";".join(str(p) for p in prune), len(names), len(extra)))
    (OUT / "cases.tsv").write_text("\n".join(rows) + "\n")
    print("imx:", len(IMX), "prep:", len(PREP_CASES), "remap:", len(REMAP_CASES))


if __name__ == "__main__":
    main()
