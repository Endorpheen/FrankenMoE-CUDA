// fg175h Apex FlashAttention host fixture: bounded, deterministic, CPU-only mocked-effect
// correctness oracle for the Goose host module (inputs/host.goose, prefix fg175h_). No GPU, no
// timings, no benchmark. Every fg175h_ extern of the source is implemented here with its exact
// declared C ABI (u64/int64/float); the module under test is the parent-built translation of
// host.goose entered via fg175h_host_entry(packet). Tensor POD fields reach the module only
// through ti/tpf/tpi; tc/oc return a self-consistent mock enum space (0 F32,1 F16,2 BF16,3 Q4_0,
// 4 Q4_1,5 Q5_0,6 Q5_1,7 Q8_0; FLASH_ATTN_EXT id 49 shared by op field and oc) - no native ABI is
// invented and no ggml struct is reconstructed. Float plan words are stored as bits via memcpy
// (no float-as-bits casts anywhere). Expected numbers are independently derived from
// inputs/contract.json + fattn.cu (best-kernel/vec-case/alloc) + fattn-common.cuh (launch_fattn
// staging/scan/occupancy/stream-K/fixups) for the Apex shape DKQ=DV=256, Q F32, K/V Q4_0
// (block 32, 18 B), mask F16 [kv,nq,1,ns], GQA 16/2=8, Ada cc 890 / carch 890 / nsm 46 / warp 32,
// occupancy configured 2, ndev 2, KV padded to FATTN_KQ_STRIDE 256.
// Source guards actually exercised: 27 role/device/args, 7 null Q/K/V, 28 op, 2 checked u64 add
// overflow (f16 span), 4 negative cast (itg). Missing preflight guards are flagged, never
// fabricated as PASS: negative ne only fails deep at itg (fail 4, after conv effects); mask
// nb[0] and the sinks tensor are never validated by the module. setattr/memo use a plain linear
// map as storage - concurrency is UNVERIFIED here (parent adds the lock); nothing in this file
// claims thread safety.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <setjmp.h>

extern "C" void fg175h_host_entry(int64_t packet); // parent-built Goose module entry (run(packet))

enum { FA_OP = 49, E_SETDEV = 1, E_CONV, E_POOL, E_LFIX, E_OCC, E_SETATTR, E_LAUNCH };
struct FT { // immutable fake tensor; fields exposed only via ti/tpf/tpi
  int64_t src[5] = {0,0,0,0,0}, type = 0, op = FA_OP, nbytes = 0, view_src = 0, view_offs = 0;
  void *data = nullptr;
  int64_t ne[4] = {1,1,1,1}, nb[4] = {4,4,4,4};
  float fp[5] = {0,0,0,0,0};         // op_params float[i], memcpy semantics
  int32_t ip[8] = {0,0,0,0,0,0,0,0}; // op_params int32[i]; [4] = n_kv_max (0 = dense)
};
static uint64_t plan[6][64]; // plan groups 0..5 x 64 u64 fields; group 4 holds float bits
static jmp_buf g_jb;
static int64_t g_fail = -1, g_role = 2, g_dev = 0;
static FT *g_dst = nullptr;
static int effk[64], neff = 0, nsetattr = 0, ncases = 0, nchecks = 0;
static int64_t effa[64], md[16], mi[16], mv[16], pool_next = 0;
static int nmemo = 0; // memo storage: plain map, no locking (concurrency unverified)

static void req(bool ok, const char *why) { nchecks++; if (!ok) { fprintf(stderr, "FAIL fg175h fixture: %s\n", why); exit(3); } }
static void rec(int k, int64_t a) { req(neff < 64, "effect log full"); effk[neff] = k; effa[neff++] = a; }
static uint64_t *P(int64_t p, int64_t k, int64_t i) { req(p == (int64_t)(intptr_t)plan && k >= 0 && k < 6 && i >= 0 && i < 64, "plan bounds"); return &plan[k][i]; }

extern "C" int64_t fg175h_arg(int64_t, int64_t k) { const int64_t v[3] = {g_role, g_dev, (int64_t)(intptr_t)g_dst}; req(k >= 0 && k < 3, "arg key"); return v[k]; }
extern "C" int64_t fg175h_plan_ptr(int64_t) { return (int64_t)(intptr_t)plan; }
extern "C" int64_t fg175h_plan(int64_t p) { return fg175h_plan_ptr(p); } // goose extern alias
extern "C" uint64_t fg175h_get(int64_t p, int64_t k, int64_t i) { return *P(p, k, i); }
extern "C" void fg175h_put(int64_t p, int64_t k, int64_t i, uint64_t v) { *P(p, k, i) = v; }
extern "C" void fg175h_putf(int64_t p, int64_t i, float v) { uint64_t b = 0; memcpy(&b, &v, 4); *P(p, 4, i) = b; }
extern "C" int64_t fg175h_ti(int64_t t, int64_t k, int64_t i) {
  const FT *x = (const FT *)(intptr_t)t; req(x != nullptr, "ti null tensor");
  if (k == 0) { req(i >= 0 && i < 5, "ti src idx"); return (int64_t)(intptr_t)x->src[i]; }
  if (k == 1) return (int64_t)(intptr_t)x->data;
  if (k == 2) return x->type;
  if (k == 3 || k == 4) { req(i >= 0 && i < 4, "ti dim idx"); return k == 3 ? x->ne[i] : x->nb[i]; }
  if (k == 5) return x->op;
  if (k == 7) return x->nbytes; // ggml_nbytes checked span; fixture fills it exactly
  if (k == 8) return x->view_src;
  if (k == 9) return x->view_offs;
  req(false, "ti key"); return 0;
}
extern "C" float fg175h_tpf(int64_t t, int64_t i) { const FT *x = (const FT *)(intptr_t)t; req(i >= 0 && i < 5, "tpf idx"); float f; memcpy(&f, &x->fp[i], 4); return f; }
extern "C" int64_t fg175h_tpi(int64_t t, int64_t i) { const FT *x = (const FT *)(intptr_t)t; req(i >= 0 && i < 8, "tpi idx"); return x->ip[i]; }
extern "C" int64_t fg175h_tc(int64_t k) { req(k >= 0 && k < 8, "tc key"); return k; }
extern "C" int64_t fg175h_oc(int64_t k) { req(k == 0, "oc key"); return FA_OP; }
extern "C" int64_t fg175h_ndev(void) { return 2; }
extern "C" int64_t fg175h_dev(int64_t d, int64_t k) { const int64_t v[5] = {890, 46, 0, 0, 32}; req(d >= 0 && d < 2 && k >= 0 && k < 5, "dev idx"); return v[k]; }
extern "C" int64_t fg175h_carch(int64_t) { return 890; }
extern "C" int64_t fg175h_blck(int64_t ty) { const int64_t v[8] = {1,1,1,32,32,32,32,32}; req(ty >= 0 && ty < 8, "blck idx"); return v[ty]; }
extern "C" int64_t fg175h_tsize(int64_t ty) { const int64_t v[8] = {4,2,2,18,20,22,24,34}; req(ty >= 0 && ty < 8, "tsize idx"); return v[ty]; }
extern "C" int64_t fg175h_nel(int64_t t) { const FT *x = (const FT *)(intptr_t)t; return x->ne[0]*x->ne[1]*x->ne[2]*x->ne[3]; }
extern "C" int64_t fg175h_nrows(int64_t t) { const FT *x = (const FT *)(intptr_t)t; return x->ne[1]*x->ne[2]*x->ne[3]; }
extern "C" void fg175h_setdev(int64_t d) { req(d >= 0 && d < 2, "setdev range"); rec(E_SETDEV, d); }
extern "C" int64_t fg175h_occ(int64_t) { req(plan[3][9] > 0 && plan[3][6] > 0, "occ plan fields"); rec(E_OCC, 0); return 2; }
extern "C" int64_t fg175h_setattr(int64_t) { req(plan[3][9] > 0 && plan[3][34] > 0, "setattr plan fields"); nsetattr++; rec(E_SETATTR, (int64_t)plan[3][34]); return 0; }
extern "C" int64_t fg175h_memo(int64_t d, int64_t inst) { for (int i = 0; i < nmemo; i++) if (md[i] == d && mi[i] == inst) return mv[i]; return 0; }
extern "C" void fg175h_memo_set(int64_t d, int64_t inst, int64_t v) { req(nmemo < 16, "memo map full"); md[nmemo] = d; mi[nmemo] = inst; mv[nmemo] = v; nmemo++; }
extern "C" int64_t fg175h_pool(int64_t, int64_t slot) { // ctx.pool(): fake ptrs only, never malloc
  const int64_t key[3] = {27, 25, 26}; req(slot >= 0 && slot < 3, "pool slot");
  req((int64_t)plan[3][key[slot]] > 0, "pool planned count > 0 at call time");
  rec(E_POOL, slot); pool_next += 1048576; return pool_next;
}
extern "C" int64_t fg175h_conv(int64_t, int64_t which) { // f16 staging; kind 5/9 | 5/10
  req(which == 0 || which == 1, "conv which");
  uint64_t kind = plan[5][9 + which]; req(kind == 1 || kind == 2, "conv kind");
  req(plan[5][1 + which] > 0, "conv nel");
  if (kind == 2) for (int64_t j = 0; j < 3; j++) req(plan[5][(which ? 6 : 3) + j] > 0, "conv nc strides");
  req(plan[0][5 + which] != 0, "conv stage ptr"); rec(E_CONV, which); return 0;
}
extern "C" int64_t fg175h_launch(int64_t) {
  uint64_t gx = plan[3][14], gy = plan[3][15], gz = plan[3][16];
  req(gx > 0 && gx <= 2147483647ull && gy <= 65535ull && gz <= 65535ull, "launch grid limits");
  req(plan[3][17] > 0 && plan[3][30] == 32 && plan[3][6] % 32 == 0 && plan[3][7] > 0, "launch block");
  req(plan[3][9] < 200000 && plan[0][0] && plan[0][7] && plan[0][8] && plan[0][9] && plan[0][12], "launch args");
  rec(E_LAUNCH, (int64_t)gx); return 0;
}
extern "C" int64_t fg175h_lfix(int64_t, int64_t kind) {
  if (kind == 0) req(plan[3][19] > 0 && plan[3][27] > 0 && plan[0][10] && plan[2][9] > 0 && plan[2][11] > 0 && plan[1][5] > 0 && plan[1][3] > 0, "scan fields");
  else if (kind == 1) req(plan[3][23] > 0 && plan[3][24] > 0 && plan[5][11] > 0 && plan[0][10] && (plan[3][2] == 1 || plan[3][2] == 8), "sparse fields");
  else if (kind == 2) req(plan[3][33] > 0 && plan[3][18] > 0 && plan[0][11], "sk uniform fields");
  else if (kind == 3) req(plan[3][37] > 0 && plan[3][14] > 0 && plan[3][2] > 0 && plan[3][3] > 0 && plan[0][11], "sk general fields");
  else if (kind == 4) req(plan[3][13] > 1 && plan[0][13] && plan[0][11] && plan[0][12] && plan[1][1] > 0 && plan[1][2] > 0 && plan[1][3] > 0, "combine fields");
  else req(false, "lfix kind");
  rec(E_LFIX, kind); return 0;
}
extern "C" float fg175h_powf(float a, float b) { return ::powf(a, b); }
extern "C" float fg175h_u2f(uint64_t v) { return (float)v; }
extern "C" void fg175h_fail(int64_t code) { g_fail = code; longjmp(g_jb, 1); } // must not return

static FT tq, tk, tv, tm, tdst, tcache;
static int64_t tnbytes(const FT &t) { int64_t b = fg175h_blck(t.type) == 1 ? t.ne[0]*t.nb[0] : t.ne[0]*t.nb[0]/fg175h_blck(t.type); for (int i = 1; i < 4; i++) b += (t.ne[i]-1)*t.nb[i]; return b; }
static void dense(FT &t, int64_t ty, int64_t n0, int64_t n1, int64_t n2, int64_t n3) { // ggml layout
  int64_t ts = fg175h_tsize(ty), ne[4] = {n0, n1, n2, n3};
  t.type = ty; for (int i = 0; i < 4; i++) t.ne[i] = ne[i];
  t.nb[0] = ts; t.nb[1] = ne[0]*ts/fg175h_blck(ty); t.nb[2] = ne[1]*t.nb[1]; t.nb[3] = ne[2]*t.nb[2];
  t.nbytes = tnbytes(t);
}
static void base(int nq, int kv, int ns, bool mask) { // Apex graph: Q/K/V/mask/dst + op params
  dense(tq, 0, 256, nq, 16, ns); dense(tk, 3, 256, kv, 2, ns); dense(tv, 3, 256, kv, 2, ns);
  dense(tm, 1, kv, nq, 1, ns); dense(tdst, 0, 256, nq, 16, ns);
  tq.data = (void*)0x100000; tk.data = (void*)0x200000; tv.data = (void*)0x300000;
  tm.data = (void*)0x500000; tdst.data = (void*)0x400000;
  tdst.op = FA_OP; tdst.fp[0] = 0.0625f; tdst.fp[1] = 0.0f; tdst.fp[2] = 0.0f; tdst.ip[4] = 0;
  tdst.src[0] = (int64_t)(intptr_t)&tq; tdst.src[1] = (int64_t)(intptr_t)&tk; tdst.src[2] = (int64_t)(intptr_t)&tv;
  tdst.src[3] = mask ? (int64_t)(intptr_t)&tm : 0; tdst.src[4] = 0;
  tk.view_src = tk.view_offs = tv.view_src = tv.view_offs = 0;
}
static void padkv(int kv) { // padded cache rows: 512 slots/row, kv used -> nb1 = 2*packed = 288
  tk.nb[1] = 288; tk.nb[2] = kv*288; tk.nb[3] = 2*tk.nb[2]; tk.nbytes = tnbytes(tk);
  tv.nb[1] = 288; tv.nb[2] = kv*288; tv.nb[3] = 2*tv.nb[2]; tv.nbytes = tnbytes(tv);
}
static void views(int64_t koffs, int64_t voffs) { tk.view_src = tv.view_src = (int64_t)(intptr_t)&tcache; tk.view_offs = koffs; tv.view_offs = voffs; }
static void group(bool clearmemo) { if (clearmemo) nmemo = 0; nsetattr = 0; }
static bool go(int role, int dev, FT *dst) { // fresh zeroed plan + effect log per run; memo persists
  memset(plan, 0, sizeof plan); neff = 0; pool_next = 0x7000000000ll; g_fail = -1;
  g_role = role; g_dev = dev; g_dst = dst;
  if (setjmp(g_jb)) return false;
  fg175h_host_entry(1);
  return true;
}
static void effs(const int *e, int n) { req(neff == n, "effect count"); for (int i = 0; i < n; i++) req(effk[i] == e[i], "effect order"); }
static int64_t GI(int k, int i) { return (int64_t)plan[k][i]; }
static float GF(int i) { float f; uint64_t b = plan[4][i]; memcpy(&f, &b, 4); return f; }

int main() {
  req(fg175h_blck(3) == 32 && fg175h_tsize(3) == 18 && fg175h_tsize(0) == 4 && fg175h_tsize(1) == 2, "Q4_0 bs32 ts18");
  req(fg175h_ndev() == 2 && fg175h_dev(0,0) == 890 && fg175h_dev(0,1) == 46 && fg175h_dev(0,4) == 32 && fg175h_carch(0) == 890 && fg175h_oc(0) == FA_OP, "device model");

  // decode route 100, Q4_0-Q4_0 vec pair 8: c1=1 at nq=1; no staging; smem 4608; grid (1,1,16)
  group(true); base(1, 256, 1, true);
  req(go(2, 0, &tdst), "decode1 run");
  req(GI(3,1) == 100 && GI(3,2) == 1 && GI(3,3) == 1 && GI(3,4) == 256 && GI(3,21) == 8 && GI(3,34) == 0, "decode route100 c1=1 c2=1");
  req(GI(3,9) == 4608 && GI(3,6) == 128 && GI(3,7) == 4 && GI(3,8) == 256 && GI(3,10) == 0 && GI(3,12) == 0 && GI(3,13) == 1, "vec config");
  req(GI(3,14) == 1 && GI(3,15) == 1 && GI(3,16) == 16 && GI(3,17) == 256 && GI(3,18) == 16 && GI(3,19) == 1 && GI(3,20) == 0, "vec grid");
  req(GI(0,5) == 0 && GI(0,6) == 0 && GI(0,15) == 0x400000 + 16384 && GI(0,7) == 0x200000 && GI(0,8) == 0x300000, "vec no stage");
  req(GI(3,25) == 0 && GI(3,26) == 0 && GI(3,27) == 0 && GI(3,28) == 0 && GI(3,29) == 0 && GI(3,31) == 8 && GI(3,32) == 0, "vec bookkeeping");
  { const int e[] = {E_SETDEV, E_OCC, E_LAUNCH}; effs(e, 3); }
  ncases++;

  // decode nq=2 kv=512: c1=2, pb=2 -> dst_tmp/dst_meta pools + combine fixup, raw Q4_0 strides
  group(true); base(2, 512, 1, true);
  req(go(2, 0, &tdst), "decode2 run");
  req(GI(3,1) == 100 && GI(3,2) == 2 && GI(3,3) == 1 && GI(3,21) == 8 && GI(3,13) == 2, "decode2 c1=2 pb=2");
  req(GI(3,25) == 16384 && GI(3,26) == 64, "decode2 scratch pb*nel/nrows"); // 2*8192, 2*32
  req(GI(3,14) == 1 && GI(3,15) == 2 && GI(3,16) == 16 && GI(0,9) != 0x400000 && GI(0,13) && GI(0,11), "decode2 combine ptrs");
  req(GI(1,1) == 2 && GI(1,2) == 16 && GI(1,3) == 1, "combine grid ne01/ne02/ne03");
  req(GI(1,4) == 256 && GI(1,5) == 512 && GI(1,6) == 2 && GI(2,3) == 144 && GI(2,4) == 73728 && GI(2,5) == 147456, "raw Q4_0 K strides");
  req(GI(1,12) == 512 && GI(1,13) == 2 && GI(2,9) == 1024 && GI(2,10) == 2048 && GI(2,11) == 2048, "mask dims/strides");
  { const int e[] = {E_SETDEV, E_OCC, E_POOL, E_POOL, E_LAUNCH, E_LFIX}; effs(e, 6); req(effa[2] == 1 && effa[3] == 2 && effa[5] == 4, "pool slots then combine"); }
  ncases++;

  // mask optional on decode: route stays 100, mask plan fields zero
  group(true); base(1, 256, 1, false);
  req(go(2, 0, &tdst), "decode nomask run");
  req(GI(3,1) == 100 && GI(0,3) == 0 && GI(1,12) == 0 && GI(1,13) == 0 && GI(1,14) == 0 && GI(1,15) == 0 && GI(2,9) == 0, "mask optional decode");
  { const int e[] = {E_SETDEV, E_OCC, E_LAUNCH}; effs(e, 3); }
  ncases++;

  // prefill mma route 400, GQA grouping preserved: nq 3/4 -> ncols1=4, nq 5/4096 -> 8, ncols2=8
  group(true); base(3, 256, 1, true);
  req(go(0, 0, &tdst), "prefill3 alloc"); // role 0: exact packed f16 stage plan, no effects
  req(GI(3,1) == 400 && GI(3,32) == 0 && GI(3,0) == 573440, "alloc 3/0 exact"); // 49152 dst + pad128 + 131072*2 + pad128 + 131072*2
  req(GI(0,5) == 0x400000 + 49152 && GI(0,6) == 0x400000 + 311296 && GI(0,15) == 0x400000 + 573440, "stage layout");
  req(GI(0,5) % 128 == 0 && GI(0,6) % 128 == 0, "stage 128-alignment");
  req(neff == 0, "role0 no setdev/conv/pool/launch");
  req(go(2, 0, &tdst), "prefill3 run");
  req(GI(3,1) == 400 && GI(3,2) == 4 && GI(3,3) == 8 && GI(3,4) == 256 && GI(3,5) == 256 && GI(3,35) == 32, "prefill route400 c1=4 c2=8");
  req(GI(3,9) == 33792 && GI(3,8) == 32 && GI(3,6) == 128 && GI(3,7) == 4 && GI(3,10) == 1 && GI(3,12) == 1 && GI(3,34) > 0, "mma smem 33792 nstages2");
  req(GI(3,14) == 16 && GI(3,15) == 1 && GI(3,16) == 1 && GI(3,17) == 256 && GI(3,18) == 2 && GI(3,19) == 1, "prefill3 streamk grid"); // min(92,8*2)=16, loss 0
  req(GI(3,26) == 16*32*130 && GI(3,25) == 0 && GI(0,11) && GI(0,13) == 0, "sk fractional meta scratch nonzero");
  req(GI(3,37) == 16 && GI(3,33) == 0, "general fixup total_work");
  req(GI(0,7) == 0x400000 + 49152 && GI(0,8) == 0x400000 + 311296, "mma staged K/V ptrs");
  req(GI(2,3) == 512 && GI(2,4) == 131072 && GI(2,5) == 262144 && GI(2,6) == 512 && GI(2,7) == 131072 && GI(2,8) == 262144, "staged f16 strides");
  req(GI(2,0) == 1024 && GI(2,1) == 3072 && GI(2,2) == 49152, "Q strides");
  req(GI(1,0) == 256 && GI(1,1) == 3 && GI(1,2) == 16 && GI(1,3) == 1 && GI(1,4) == 256 && GI(1,5) == 256 && GI(1,6) == 2, "Q/K dims");
  req(GI(1,12) == 256 && GI(1,13) == 3 && GI(1,14) == 1 && GI(1,15) == 1 && GI(2,9) == 512 && GI(2,10) == 1536 && GI(2,11) == 1536, "mask dims/strides");
  req(GF(0) == 0.0625f && GF(1) == 0.0f && GF(2) == 1.0f && GF(3) == 1.0f && GF(4) == 0.0f && GI(5,0) == 16, "scale/m0/m1/n_head_log2");
  req(GI(5,1) == 131072 && GI(5,2) == 131072 && GI(5,9) == 1 && GI(5,10) == 1, "contig conv kinds + nel");
  req(GI(5,12) == 2 && GI(5,13) == 1 && GI(5,14) == 1 && GI(5,15) == 16 && GI(5,16) == 8 && GI(5,17) == 8 && GI(5,18) == 8, "fastdiv divisors");
  { const int e[] = {E_SETDEV, E_SETATTR, E_CONV, E_CONV, E_OCC, E_POOL, E_LAUNCH, E_LFIX}; effs(e, 8); req(effa[1] > 0 && effa[5] == 2 && effa[7] == 3, "setdev,setattr,convK,convV,occ,pool2,launch,lfix3"); }
  ncases++;

  // memo reuse: identical instance (nq=4, same ncols) -> no second setattr
  base(4, 256, 1, true); int sa = nsetattr;
  req(go(2, 0, &tdst), "prefill4 run");
  req(GI(3,1) == 400 && GI(3,2) == 4 && GI(3,3) == 8 && GI(3,9) == 33792 && GI(3,14) == 16 && GI(3,26) == 16*32*130, "prefill4 same plan");
  req(nsetattr == sa, "setattr memoized per instance");
  ncases++;

  base(5, 256, 1, true); sa = nsetattr;
  req(go(2, 0, &tdst), "prefill5 run");
  req(GI(3,1) == 400 && GI(3,2) == 8 && GI(3,3) == 8 && GI(3,35) == 64 && GI(3,9) == 33792 && GI(3,14) == 16 && GI(3,26) == 16*64*130, "prefill route400 c1=8");
  req(nsetattr == sa + 1, "new instance raises limit once");
  ncases++;

  // mask scan on prefill >=1024 queries: pool0+scan launch before occupancy; 3/27 = ntiles_x*ne03
  base(4096, 256, 1, true);
  req(go(2, 0, &tdst), "prefill4096 run");
  req(GI(3,1) == 400 && GI(3,2) == 8 && GI(3,20) == 1 && GI(3,27) == 512 && GI(5,11) == 0 && GI(0,10) && GI(3,19) == 512, "mask scan prefill >=1024");
  req(GI(3,14) == 92 && GI(3,18) == 1024 && GI(3,26) == 92*64*130 && GI(3,37) == 8192, "scan streamk grid"); // rounded->0, loss 100 -> gx=raw=92
  req(GI(2,9) == 512 && GI(2,11) == 2097152, "scan mask strides (s31/s33 in half2 units)");
  { const int e[] = {E_SETDEV, E_CONV, E_CONV, E_POOL, E_LFIX, E_OCC, E_POOL, E_LAUNCH, E_LFIX}; effs(e, 9); req(effa[3] == 0 && effa[4] == 0 && effa[6] == 2 && effa[8] == 3, "scan before occ; lfix kinds 0/3"); }
  ncases++;

  // KV sweep 512/32768 at nq=4: stage sizing follows nelements; stream-K re-buckets grids
  base(4, 512, 1, true);
  req(go(0, 0, &tdst), "kv512 alloc");
  req(GI(3,0) == 65536 + 524288 + 524288, "kv512 alloc exact"); // dst + K nel 262144*2 + V
  req(go(2, 0, &tdst), "kv512 run");
  req(GI(3,17) == 512 && GI(3,14) == 32 && GI(3,26) == 32*32*130 && GI(3,37) == 32, "kv512 grid"); // min(92,16*2)=32
  ncases++;
  base(4, 32768, 1, true);
  req(go(2, 0, &tdst), "kv32768 run");
  req(GI(3,1) == 400 && GI(3,2) == 4 && GI(3,17) == 32768 && GI(3,14) == 92 && GI(3,26) == 92*32*130 && GI(3,37) == 2048, "kv32768 grid"); // raw=min(92,2048)=92
  req(GI(0,15) == 0x400000 + 65536 + 33554432 + 33554432, "kv32768 stage end");
  { const int e[] = {E_SETDEV, E_CONV, E_CONV, E_OCC, E_POOL, E_LAUNCH, E_LFIX}; effs(e, 7); }
  ncases++;

  // sparse (n_kv_max=512, kv=8192 eligible): pool0 + compact launch between convs and occupancy
  group(true); base(3, 8192, 1, true); tdst.ip[4] = 512;
  req(go(2, 0, &tdst), "sparse run");
  req(GI(3,1) == 400 && GI(3,2) == 1 && GI(3,3) == 8 && GI(3,11) == 1 && GI(3,12) == 0 && GI(3,9) == 65680, "sparse c1=1 smem 65680");
  req(GI(3,23) == 512 && GI(3,24) == 3 && GI(3,27) == 1539 && GI(5,11) == 1536 && GI(3,17) == 512 && GI(3,19) == 3, "sparse lists/scratch");
  req(GI(3,14) == 48 && GI(3,18) == 6 && GI(3,26) == 48*8*130 && GI(3,37) == 48, "sparse streamk grid"); // ntdst=6, ntkv=8, raw=48
  { const int e[] = {E_SETDEV, E_SETATTR, E_CONV, E_CONV, E_POOL, E_LFIX, E_OCC, E_POOL, E_LAUNCH, E_LFIX}; effs(e, 10); req(effa[4] == 0 && effa[5] == 1 && effa[7] == 2 && effa[9] == 3, "sparse pool0+lfix1 order"); }
  ncases++;

  // padded KV cache views vs contiguous: conv kind 2, strides in type-size units, same stage bytes
  group(true); base(4, 256, 1, true); padkv(256); views(0, 777);
  req(go(2, 0, &tdst), "padded run");
  req(GI(3,1) == 400 && GI(3,2) == 4 && GI(5,9) == 2 && GI(5,10) == 2, "padded conv kind 2");
  req(GI(5,3) == 16 && GI(5,4) == 4096 && GI(5,5) == 8192 && GI(5,6) == 16 && GI(5,7) == 4096 && GI(5,8) == 8192, "nc strides in ts units");
  req(GI(2,3) == 512 && GI(2,4) == 131072 && GI(2,5) == 262144 && GI(2,6) == 512 && GI(2,7) == 131072 && GI(2,8) == 262144, "padded effective strides");
  req(tk.nbytes == 144 + 255*288 + 73728, "padded nbytes proper"); // 147312 vs packed 73728 -> contig false
  req(GI(3,14) == 16 && GI(3,26) == 16*32*130, "padded streamk unchanged");
  ncases++;

  // V view of K region (same view_src/view_offs): stage reuse, no V conv
  base(4, 256, 1, true); padkv(256); views(0, 0);
  req(go(0, 0, &tdst), "vkv alloc");
  req(GI(3,0) == 65536 + 262144, "vkv alloc exact (V reuses K stage)");
  req(go(2, 0, &tdst), "vkv run");
  req(GI(0,5) && GI(0,6) == GI(0,5) && GI(2,6) == GI(2,3) && GI(2,7) == GI(2,4) && GI(2,8) == GI(2,5), "V view K reuse");
  { const int e[] = {E_SETDEV, E_CONV, E_OCC, E_POOL, E_LAUNCH, E_LFIX}; effs(e, 6); }
  ncases++;

  // multi-sequence scan (ne03>1) on small queries, vec route: pool0+scan, pb=1, no combine
  group(true); base(2, 256, 2, true);
  req(go(2, 0, &tdst), "ns2 run");
  req(GI(3,1) == 100 && GI(3,20) == 1 && GI(3,27) == 2 && GI(3,18) == 32 && GI(3,14) == 1 && GI(3,15) == 1 && GI(3,16) == 32 && GI(3,13) == 1, "ns2 scan");
  { const int e[] = {E_SETDEV, E_POOL, E_LFIX, E_OCC, E_LAUNCH}; effs(e, 5); req(effa[1] == 0 && effa[2] == 0, "scan order"); }
  ncases++;

  // support/allocation roles never set dev/stream/conv/pool/launch; alloc exact incl. NULL data
  group(true); base(1, 256, 1, true);
  req(go(1, 0, &tdst), "role1 vec");
  req(GI(3,1) == 100 && GI(3,29) == 1 && GI(3,28) == 0 && neff == 0, "role1 supported, no effects");
  req(go(0, 0, &tdst), "role0 vec");
  req(GI(3,0) == 16384 && neff == 0, "role0 vec alloc == dst nbytes");
  base(3, 256, 1, true);
  req(go(1, 0, &tdst), "role1 mma");
  req(GI(3,1) == 400 && GI(3,29) == 1 && GI(3,28) == 0 && neff == 0, "role1 mma supported, no effects");
  tdst.data = nullptr;
  req(go(0, 0, &tdst), "role0 null-data");
  req(GI(3,0) == 573440 && neff == 0, "data NULL allocation metadata allowed");
  tdst.data = (void*)0x400000;
  ncases++;

  // route NONE (V head mismatch): deferred bookkeeping for role 1, abort 29 for role 2, alloc exact
  group(true); base(3, 256, 1, true); tv.ne[0] = 128;
  req(go(1, 0, &tdst), "none role1");
  req(GI(3,1) == 0 && GI(3,29) == 0 && GI(3,28) == 1 && neff == 0, "route none deferred");
  req(go(0, 0, &tdst), "none role0");
  req(GI(3,0) == 49152 && neff == 0, "route none alloc stays exact");
  req(!go(2, 0, &tdst) && g_fail == 29, "route none dispatch abort");
  ncases++;

  // fail paths via setjmp/longjmp (fail must not return); only actually-present guards claimed
  group(true); base(2, 256, 1, true);
  req(!go(2, 5, &tdst) && g_fail == 27 && neff == 0, "invalid GPU id (fail 27)");
  req(!go(2, 0, nullptr) && g_fail == 27 && neff == 0, "null dst (fail 27)");
  tdst.src[1] = 0;
  req(!go(2, 0, &tdst) && g_fail == 7 && neff == 1 && effk[0] == E_SETDEV, "null K (fail 7, after setdev)");
  group(true); base(2, 256, 1, true); tdst.op = 0;
  req(!go(2, 0, &tdst) && g_fail == 28 && neff == 0, "op check before effects (fail 28)");
  group(true); base(1, 256, 1, false); tdst.data = (void*)0xffffffffffffff00ull;
  req(!go(0, 0, &tdst) && g_fail == 2 && neff == 0, "checked u64 add overflow in f16 span (fail 2)");
  tdst.data = (void*)0x400000;
  group(true); base(3, 256, 1, true); tq.ne[1] = -3;
  req(!go(2, 0, &tdst) && g_fail == 4 && neff == 1 && effk[0] == E_SETDEV, "negative count via itg (fail 4; flagged: no earlier preflight guard)");
  ncases++;

  printf("PASS fg175h host fixture cases=%d checks=%d (deterministic CPU mocked effects; memo/setattr concurrency unverified; not compile/GPU acceptance)\n", ncases, nchecks);
  return 0;
}
