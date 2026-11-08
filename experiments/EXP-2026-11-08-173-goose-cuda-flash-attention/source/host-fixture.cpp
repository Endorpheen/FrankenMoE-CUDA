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
static uint64_t plan[8][64]; // plan groups 0..5 x 64 u64 fields; group 4 holds float bits
static jmp_buf g_jb;
static int64_t g_fail = -1, g_role = 2, g_dev = 0;
static FT *g_dst = nullptr;
static int effk[64], neff = 0, nsetattr = 0, ncases = 0, nchecks = 0;
static int64_t effa[64], md[16], mi[16], mv[16], pool_next = 0;
static int nmemo = 0; // memo storage: plain map, no locking (concurrency unverified)

static void req(bool ok, const char *why) { nchecks++; if (!ok) { fprintf(stderr, "FAIL fg175h fixture: %s\n", why); exit(3); } }
static void rec(int k, int64_t a) { req(neff < 64, "effect log full"); effk[neff] = k; effa[neff++] = a; }
static uint64_t *P(int64_t p, int64_t k, int64_t i) { req(p == (int64_t)(intptr_t)plan && k >= 0 && k < 8 && i >= 0 && i < 64, "plan bounds"); return &plan[k][i]; }

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
  dense(tm, 1, kv, nq, 1, ns); dense(tdst, 0, 256, 16, nq, ns);
  tq.data = (void*)0x1000000000ull; tk.data = (void*)0x2000000000ull; tv.data = (void*)0x3000000000ull;
  tm.data = (void*)0x5000000000ull; tdst.data = (void*)0x4000000000ull;
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


static int held=0,locks=0,unlocks=0;
extern "C" void fg175h_lock(int64_t d,int64_t i){req(d>=0&&d<2&&i>=1&&i<=2&&held==0,"bounded init lock");held=1;locks++;}
extern "C" void fg175h_unlock(int64_t,int64_t){req(held==1,"init lock balanced");held=0;unlocks++;}
static int effect_count(int key){int n=0;for(int i=0;i<neff;i++)n+=effk[i]==key;return n;}
static void fail_before_effect(int code){req(!go(2,0,&tdst)&&g_fail==code&&neff==0,"reject before SDK/stream/pool");ncases++;}
int main(){
 group(true);base(1,256,1,true);req(go(2,0,&tdst),"decode1");req(GI(3,1)==100&&GI(3,2)==1&&GI(3,3)==1&&GI(3,9)==4608&&GI(3,13)==1&&GI(3,36)==0,"decode1 plan");{const int e[]={E_SETDEV,E_OCC,E_LAUNCH};effs(e,3);}ncases++;
 base(2,512,1,true);req(go(2,0,&tdst),"decode2");req(GI(3,13)==2&&GI(3,25)==16384&&GI(3,26)==64&&GI(7,0)==4&&GI(7,5)==16&&GI(3,36)==1,"decode2 scratch/combine");{const int e[]={E_SETDEV,E_OCC,E_POOL,E_POOL,E_LAUNCH,E_LFIX};effs(e,6);}ncases++;
 base(1,512,2,true);req(go(2,0,&tdst),"decode two sequences");req(GI(3,20)==1&&GI(3,27)==2&&GI(7,0)==4&&effect_count(E_LFIX)==2,"short-query mask scan plus combine");ncases++;
 base(1,256,1,false);req(go(2,0,&tdst)&&GI(0,3)==0&&GI(3,1)==100,"optional decode mask");ncases++;
 group(true);base(3,256,1,true);req(go(0,0,&tdst)&&GI(3,0)==573440&&neff==0,"pure allocation role");req(go(1,0,&tdst)&&GI(3,29)==1&&neff==0,"pure support role");ncases+=2;
 req(go(2,0,&tdst),"prefill3");req(GI(3,1)==400&&GI(3,2)==4&&GI(3,3)==8&&GI(3,9)==33792&&GI(3,36)==2,"prefill GQA/instance");req(GI(3,14)==16&&GI(3,18)==2&&GI(3,26)==16*32*130&&GI(3,33)==8&&GI(7,0)==2,"uniform stream-K original policy");req(GI(6,14)==1&&GI(6,9)==512&&GI(6,12)==32,"contiguous staging plan");req(effect_count(E_CONV)==2&&nsetattr==1&&locks==unlocks&&held==0,"effects and once lock");ncases++;
 base(4,256,1,true);req(go(2,0,&tdst)&&nsetattr==1,"same MMA instance once cached");ncases++;
 base(5,256,1,true);req(go(2,0,&tdst)&&GI(3,2)==8&&GI(3,3)==8&&GI(3,36)==3&&nsetattr==2,"second MMA instance");ncases++;
 base(4096,256,1,true);req(go(2,0,&tdst)&&GI(3,14)==92&&GI(3,18)==1024&&GI(3,37)==8192&&GI(3,26)==92*64*130,"fractional stream-K");req(GI(3,27)==512&&GI(7,0)==3&&GI(7,12)==8192&&effect_count(E_LFIX)==2,"scan and general fixup");ncases++;
 base(4,32768,1,true);req(go(2,0,&tdst)&&GI(3,14)==92&&GI(3,26)==92*32*130&&GI(7,0)==2&&GI(3,33)==46,"long cache uniform split");ncases++;
 base(4,256,1,true);padkv(256);views(0,777);req(go(2,0,&tdst)&&GI(5,9)==2&&GI(5,10)==2,"strided staging");req(GI(6,9)==1&&GI(6,10)==256&&GI(6,11)==2&&GI(6,12)==256&&GI(6,6)==288,"strided staging dimensions/bytes");ncases++;
 base(4,256,1,true);padkv(256);views(0,0);tv.data=tk.data;req(go(0,0,&tdst)&&GI(3,0)==65536+262144,"shared view allocation");req(go(2,0,&tdst)&&GI(0,7)==GI(0,8)&&effect_count(E_CONV)==1,"shared view staged once");ncases++;
 base(4,256,1,true);tq.data=tk.data=tv.data=tm.data=tdst.data=nullptr;req(go(0,0,&tdst)&&GI(3,0)==589824&&neff==0,"null-data allocation metadata");req(go(1,0,&tdst)&&GI(3,29)==1&&neff==0,"null-data supported metadata");ncases+=2;
 base(3,8192,1,true);tdst.ip[4]=512;req(go(2,0,&tdst)&&GI(3,28)==1&&neff==0,"sparse explicitly deferred");ncases++;
 base(4,256,1,true);tq.ne[1]=-1;fail_before_effect(42);
 base(4,256,1,true);tk.ne[1]=-256;fail_before_effect(42);
 base(4,256,1,true);tm.nb[0]=4;fail_before_effect(43);
 base(4,256,1,true);tdst.nb[2]=16388;tdst.nbytes=tnbytes(tdst);fail_before_effect(50);
 base(4,256,1,true);tq.data=nullptr;fail_before_effect(55);
 base(4,256,1,true);tdst.data=tk.data;fail_before_effect(46);
 base(4,256,1,true);tdst.data=(void*)(uintptr_t)(UINT64_MAX-128);fail_before_effect(2);
 base(4,256,1,true);req(!go(2,-1,&tdst)&&g_fail==27&&neff==0,"negative device");ncases++;
 base(4,256,1,true);req(!go(2,2,&tdst)&&g_fail==27&&neff==0,"device upper bound");ncases++;
 base(4096,262144,1,true);tq.data=tk.data=tv.data=tm.data=tdst.data=nullptr;req(tm.nb[2]==2147483648LL&&tm.ne[2]==1,"real reserve mask singleton stride");req(go(1,0,&tdst)&&GI(3,29)==1&&neff==0,"support valid metadata with unused 2GiB mask stride");req(go(0,0,&tdst)&&neff==0,"allocation metadata with unused large mask stride");ncases+=2;
 req(held==0&&locks==unlocks,"all once scopes balanced");printf("PASS host cases=%d checks=%d locked_once_pairs=%d; mocked SDK only; concurrency not claimed\n",ncases,nchecks,locks);
}
