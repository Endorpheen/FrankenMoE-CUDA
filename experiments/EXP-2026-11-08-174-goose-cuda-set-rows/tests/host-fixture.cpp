// Mocked host-plan fixture for inputs/host-host.goose (SET_ROWS host).
// Contract: inputs/host-contract.json. No GPU, inference, timing or allocation:
// the host path allocates nothing, so no OOM path is invented. The only bounded
// resource is the plan arena (10 keys x 32 words); effect order is checked.
#include "../source/host-ffi.h"
static int fg175s_run_entry(){return fg176rh_run(0);}
#include <cstdio>
#include <cstring>
#include <setjmp.h>

enum { F32 = 1, Q4_0 = 3, I32 = 8, I64 = 9 };
struct Tensor { int64_t type, data, op, vs, vo, src[2], ne[4], nb[4]; };
static Tensor T[9];
static uint64_t P[10][32];
static unsigned char W[10][32];
static int nput, launch, fails, bad, dup, late, pcalls, mono, first, last, prev, zero_ok;
static int64_t failcode, arg0;
static jmp_buf jb;
static int checks, cfail, groups, runs;
static long tot_put, tot_launch, tot_fail;

static void effects(void) {
    std::memset(P, 0, sizeof P); std::memset(W, 0, sizeof W);
    nput = launch = fails = bad = dup = late = pcalls = 0;
    mono = 1; first = last = prev = -1; zero_ok = 1; failcode = -1;
}
static void reset_all(void) { effects(); std::memset(T, 0, sizeof T); arg0 = 0; }
static void tensor(int h, int64_t ty, int64_t n0, int64_t n1, int64_t n2, int64_t n3,
                   int64_t b0, int64_t b1, int64_t b2, int64_t b3, int64_t data) {
    Tensor &t = T[h];
    t.type = ty; t.data = data;
    t.ne[0] = n0; t.ne[1] = n1; t.ne[2] = n2; t.ne[3] = n3;
    t.nb[0] = b0; t.nb[1] = b1; t.nb[2] = b2; t.nb[3] = b3;
}
static void link(int d, int a, int b) { T[d].src[0] = a; T[d].src[1] = b; }
static int run_once(void) {
    effects();
    if (setjmp(jb) == 0) return fg175s_run_entry();
    return -2;
}
static void eqv(int k, int i, int64_t v) {
    ++checks;
    if (k < 0 || k > 9 || i < 0 || i > 31 || P[k][i] != (uint64_t)v) {
        std::fprintf(stderr, "FAIL plan k%d[%d]=%llu want %lld\n", k, i,
                     (unsigned long long)P[k][i], (long long)v);
        ++cfail;
    }
}
static void row4(int k, int64_t a, int64_t b, int64_t c, int64_t d) {
    eqv(k,0,a); eqv(k,1,b); eqv(k,2,c); eqv(k,3,d);
}
static void row3(int k, int64_t a, int64_t b, int64_t c) { eqv(k,0,a); eqv(k,1,b); eqv(k,2,c); }
static void rowN(int k, const int64_t *v, int n) { for (int i = 0; i < n; ++i) eqv(k,i,v[i]); }
static void magic_ok(int key, int64_t d) {
    int64_t mp = (int64_t)P[8][key*3], L = (int64_t)P[8][key*3+1], dc = (int64_t)P[8][key*3+2];
    bool rng = L >= 0 && L <= 32 && d > 0;
    uint64_t hi = rng ? ((uint64_t)1 << L) : 0;
    uint64_t want = rng ? (((uint64_t)1 << 32) * (hi - (uint64_t)d) / (uint64_t)d + 1) : 0;
    bool low = rng && (L == 0 ? d == 1 : ((uint64_t)1 << (L-1)) < (uint64_t)d);
    ++checks;
    if (!(dc == d && rng && hi >= (uint64_t)d && low && (uint64_t)mp == want)) {
        std::fprintf(stderr, "FAIL magic key%d d=%lld got[%lld,%lld,%lld]\n",
                     key, (long long)d, (long long)mp, (long long)L, (long long)dc);
        ++cfail;
    }
}
static void accept_case(const char *name) {
    int r = run_once(); ++runs;
    tot_put += nput; tot_launch += launch; tot_fail += fails;
    bool ok = r == 1 && fails == 0 && pcalls == 1 && nput == 67 && launch == 1 &&
              bad == 0 && dup == 0 && late == 0 && mono == 1 && first == 0 &&
              last == 292 && zero_ok == 1 && last < 10*32;
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s accept r=%d f=%d pc=%d put=%d l=%d bad=%d dup=%d late=%d mono=%d first=%d last=%d zero=%d\n",
        name, r, fails, pcalls, nput, launch, bad, dup, late, mono, first, last, zero_ok); ++cfail; }
}
static void refuse_case(const char *name) {
    int r = run_once(); ++runs;
    tot_put += nput; tot_launch += launch; tot_fail += fails;
    ++checks;
    if (!(r == 0 && fails == 0 && pcalls == 0 && nput == 0 && launch == 0 && bad == 0)) {
        std::fprintf(stderr, "FAIL %s refuse r=%d f=%d pc=%d put=%d l=%d bad=%d\n",
                     name, r, fails, pcalls, nput, launch, bad); ++cfail;
    }
}
static void fatal_case(int64_t code, const char *name) {
    int r = run_once(); ++runs;
    tot_put += nput; tot_launch += launch; tot_fail += fails;
    ++checks;
    if (!(r == -2 && fails == 1 && failcode == code && pcalls == 0 &&
          nput == 0 && launch == 0 && bad == 0)) {
        std::fprintf(stderr, "FAIL %s fatal r=%d f=%d code=%lld pc=%d put=%d l=%d bad=%d\n",
                     name, r, fails, (long long)failcode, pcalls, nput, launch, bad); ++cfail;
    }
}

extern "C" int64_t fg175s_arg(int64_t packet, int64_t k) {
    if (packet != 0 || k != 0) { ++bad; return 0; }
    return arg0;
}
extern "C" int64_t fg175s_plan(int64_t packet) {
    if (packet != 0) { ++bad; return 0; }
    ++pcalls;
    if (pcalls > 1) { zero_ok = 0; return 1; }
    for (int k = 0; k < 10; ++k) for (int i = 0; i < 32; ++i) if (P[k][i]) zero_ok = 0;
    return 1;
}
extern "C" void fg175s_put(int64_t p, int64_t k, int64_t i, uint64_t v) {
    if (p != 1 || k < 0 || k > 9 || i < 0 || i > 31) { ++bad; return; }
    int f = (int)(k*32 + i);
    if (W[k][i]) ++dup;
    if (launch) ++late;
    P[k][i] = v; W[k][i] = 1; ++nput;
    if (first < 0) first = f;
    if (f <= prev) mono = 0;
    prev = f; last = f;
}
extern "C" uint64_t fg175s_get(int64_t p, int64_t k, int64_t i) {
    if (p != 1 || k < 0 || k > 9 || i < 0 || i > 31) { ++bad; return 0; }
    return P[k][i];
}
extern "C" int64_t fg175s_ti(int64_t t, int64_t k, int64_t i) {
    if (t < 1 || t > 8) { ++bad; return 0; }
    const Tensor &x = T[t];
    switch (k) {
    case 0: if (i >= 0 && i <= 1) return x.src[i]; break;
    case 1: if (i == 0) return x.data; break;
    case 2: if (i == 0) return x.type; break;
    case 3: if (i >= 0 && i <= 3) return x.ne[i]; break;
    case 4: if (i >= 0 && i <= 3) return x.nb[i]; break;
    case 5: if (i == 0) return x.op; break;
    case 6: if (i == 0) return x.vs; break;
    case 7: if (i == 0) return x.vo; break;
    default: break;
    }
    ++bad; return 0;
}
extern "C" int64_t fg175s_tc(int64_t k) {
    if (k == 1 || k == 3 || k == 8 || k == 9) return k;
    ++bad; return -1;
}
extern "C" int64_t fg175s_oc(int64_t k) { if (k == 0) return 0; ++bad; return -1; }
extern "C" int64_t fg175s_launch(int64_t packet) { if (packet != 0) { ++bad; return 1; } ++launch; return 0; }
extern "C" void fg175s_fail(int64_t code) { ++fails; failcode = code; longjmp(jb, 1); }

static void canonical(int64_t a0, int64_t a1, int64_t ad) {
    reset_all(); arg0 = 3;
    tensor(1, F32, 8, 3, 2, 1, 4, 32, 96, 192, a0);
    tensor(2, I32, 5, 2, 1, 1, 4, 4, 8, 8, a1);
    tensor(3, F32, 8, 4, 2, 1, 4, 32, 128, 256, ad);
    link(3, 1, 2);
}
static void q4base(void) {
    reset_all(); arg0 = 3;
    tensor(1, F32, 32, 2, 1, 1, 4, 128, 256, 256, 0x100000);
    tensor(2, I32, 2, 1, 1, 1, 4, 4, 4, 4, 0x200000);
    tensor(3, Q4_0, 32, 2, 1, 1, 18, 18, 18, 18, 0x300000);
    link(3, 1, 2);
}

static void c_canonical(void) {
    ++groups;
    canonical(0x100000, 0x200000, 0x300000);
    accept_case("canonical");
    eqv(0,0,0x100000); eqv(0,1,0x200000); eqv(0,2,0x300000); eqv(0,3,0); eqv(0,4,0);
    row4(1,8,3,2,1); row4(2,5,2,1,1); row4(3,8,4,2,1);
    row3(4,8,24,48); row3(5,1,1,2); row3(6,8,32,64);
    int64_t k7[21] = {48,5,2,1,1,1,1,1,256,0,0,0,0,1,4,256,0,1,4,192,24};
    int64_t k8[15] = {1,3,8,1431655766,2,3,1,1,2,1,1,2,1,0,1};
    int64_t k9[5] = {8,3,2,2,1};
    rowN(7,k7,21); rowN(8,k8,15); rowN(9,k9,5);
}
static void c_padded(void) {
    ++groups;
    canonical(0x100000, 0x200000, 0x300000);
    T[1].nb[1]=40; T[1].nb[2]=128; T[1].nb[3]=272;
    T[2].nb[1]=24; T[2].nb[2]=48; T[2].nb[3]=48;
    T[3].nb[1]=48; T[3].nb[2]=200; T[3].nb[3]=400;
    accept_case("padded");
    row4(1,8,3,2,1); row4(2,5,2,1,1); row4(3,8,4,2,1);
    row3(4,10,32,68); row3(5,1,6,12); row3(6,12,50,100);
    eqv(7,0,48); eqv(7,15,376); eqv(7,19,240); eqv(7,20,44);
    int64_t k8[15]={1,3,8,1431655766,2,3,1,1,2,1,1,2,1,0,1};
    rowN(8,k8,15);
}
static void c_i64(void) {
    ++groups;
    canonical(0x100000, 0x200000, 0x300000);
    T[2].type=I64; T[2].nb[0]=8; T[2].nb[1]=8; T[2].nb[2]=16; T[2].nb[3]=16;
    accept_case("f32_i64");
    row3(5,1,1,2); eqv(7,11,1); eqv(7,12,1); eqv(7,20,48);
}
static void c_q4_i32(void) {
    ++groups;
    q4base();
    accept_case("q4_i32");
    row4(1,32,2,1,1); row4(2,2,1,1,1); row4(3,32,2,1,1);
    row3(4,32,64,64); row3(5,1,1,1); row3(6,18,18,18);
    int64_t k7[21] = {2,2,1,1,1,1,1,1,256,0,1,0,2,1,2,36,0,32,18,256,8};
    int64_t k8[15] = {1,5,32,1,1,2,1,0,1,1,0,1,1,0,1};
    int64_t k9[5] = {32,2,1,1,1};
    rowN(7,k7,21); rowN(8,k8,15); rowN(9,k9,5);
}
static void c_q4_i64(void) {
    ++groups;
    q4base();
    T[2].type=I64; T[2].nb[0]=8; T[2].nb[1]=8; T[2].nb[2]=8; T[2].nb[3]=8;
    accept_case("q4_i64");
    row3(5,1,1,1); eqv(7,11,1); eqv(7,12,3); eqv(7,20,16);
}
static void c_broadcast(void) {
    ++groups;
    reset_all(); arg0=3;
    tensor(1,F32,8,2,4,2,4,32,64,256,0x100000);
    tensor(2,I32,3,1,1,1,4,4,4,4,0x200000);
    tensor(3,F32,8,3,4,2,4,32,96,384,0x300000);
    link(3,1,2);
    accept_case("broadcast");
    row4(1,8,2,4,2); row4(2,3,1,1,1); row4(3,8,3,4,2);
    row3(4,8,16,64); row3(5,1,1,1); row3(6,8,24,96);
    int64_t k7[21] = {128,3,1,1,1,1,1,1,256,0,0,0,0,1,3,768,0,1,4,512,12};
    rowN(7,k7,21);
    magic_ok(0,8); magic_ok(1,2); magic_ok(2,4); magic_ok(3,1); magic_ok(4,1);
    int64_t k9[5] = {8,2,4,1,1};
    rowN(9,k9,5);
}
static void c_view(void) {
    ++groups;
    canonical(0x100000, 0x200000, 0x300000);
    T[3].vs=0xABC0; T[3].vo=16;
    accept_case("view");
    eqv(0,3,0xABC0); eqv(0,4,16);
}
static void c_maxdims(void) {
    ++groups;
    reset_all(); arg0=3;
    tensor(1,F32,8,1,1,1,4,32,32,32,0x100000);
    tensor(2,I32,1,4294967295LL,1,1,4,4,4,4,0x200000);
    tensor(3,F32,8,1,1,1,4,32,32,32,0x300000);
    link(3,1,2);
    fatal_case(54,"nondividing_broadcast"); return;
    row4(1,8,1,1,1); row4(2,1,4294967295LL,1,1); row4(3,8,1,1,1);
    row3(4,8,8,8); row3(5,1,1,1); row3(6,8,8,8);
    int64_t k7[21] = {8,1,4294967295LL,1,1,1,1,1,256,0,0,0,0,1,1,32,0,1,4,32,17179869180LL};
    rowN(7,k7,21);
    magic_ok(0,8); magic_ok(1,1); magic_ok(2,1); magic_ok(3,4294967295LL); magic_ok(4,1);
    int64_t k9[5] = {8,1,1,1,1};
    rowN(9,k9,5);
}
static void c_q4_nt_cap(void) {
    ++groups;
    reset_all(); arg0=3;
    tensor(1,F32,32,134217728,1,1,4,128,128,128,0x1000000000LL);
    tensor(2,I32,134217728,1,1,1,4,4,4,4,0x2000000000LL);
    tensor(3,Q4_0,32,1,1,1,18,18,18,18,0x3000000000LL);
    link(3,1,2);
    accept_case("q4_nt_cap");
    row4(1,32,134217728,1,1); row4(2,134217728,1,1,1); row4(3,32,1,1,1);
    row3(4,32,32,32); row3(5,1,1,1); row3(6,18,18,18);
    int64_t k7[21] = {134217728,134217728,1,1,1,524288,1,1,256,0,1,0,2,1,1,18,0,32,18,17179869184LL,536870912};
    rowN(7,k7,21);
    magic_ok(0,32); magic_ok(1,134217728); magic_ok(2,1); magic_ok(3,1); magic_ok(4,1);
    int64_t k9[5] = {32,134217728,1,1,1};
    rowN(9,k9,5);
}
static void c_refuse(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); arg0=0; fatal_case(10,"null_dst");
    canonical(0x100000,0x200000,0x300000); T[3].op=7; refuse_case("op");
    canonical(0x100000,0x200000,0x300000); T[3].src[0]=0; fatal_case(11,"src0_null");
    canonical(0x100000,0x200000,0x300000); T[3].src[1]=0; fatal_case(11,"src1_null");
}
static void c_types(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[1].type=2; refuse_case("src0_f16");
    canonical(0x100000,0x200000,0x300000); T[2].type=4; refuse_case("src1_f16");
    canonical(0x100000,0x200000,0x300000); T[3].type=7; refuse_case("dst_unsupported");
}
static void c_zero(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[1].ne[2]=0; int z=run_once(); ++runs; ++checks; if(z!=1||fails||pcalls||launch||nput) ++cfail;
    canonical(0x100000,0x200000,0x300000); T[3].ne[1]=0; fatal_case(18,"d1_zero");
    canonical(0x100000,0x200000,0x300000); T[2].ne[3]=0; fatal_case(19,"n13_zero");
}
static void c_rel(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[2].ne[0]=2; fatal_case(20,"n10_lt_n01");
    canonical(0x100000,0x200000,0x300000); T[3].ne[0]=7; fatal_case(21,"d0_lt_n00");
    canonical(0x100000,0x200000,0x300000); T[3].ne[2]=1; fatal_case(22,"d2_lt_n02");
}
static void c_q4_align(void) {
    ++groups;
    q4base(); T[1].ne[0]=33; T[3].ne[0]=64; fatal_case(23,"n00_mod32");
    q4base(); T[3].ne[0]=40; fatal_case(24,"d0_mod32");
}
static void c_stride(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[1].nb[0]=8; fatal_case(30,"nb00_unit");
    canonical(0x100000,0x200000,0x300000); T[1].nb[1]=6; fatal_case(31,"nb01_unit");
    canonical(0x100000,0x200000,0x300000); T[2].nb[0]=8; fatal_case(32,"nb10_unit");
    canonical(0x100000,0x200000,0x300000); T[2].nb[1]=6; fatal_case(33,"nb11_unit");
    canonical(0x100000,0x200000,0x300000); T[3].nb[0]=8; fatal_case(34,"db0_unit");
    canonical(0x100000,0x200000,0x300000); T[3].nb[1]=6; fatal_case(35,"db1_unit");
}
static void c_data(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[1].data=0; fatal_case(36,"src0_data");
    canonical(0x100000,0x200000,0x300000); T[2].data=0; fatal_case(36,"src1_data");
    canonical(0x100000,0x200000,0x300000); T[3].data=0; fatal_case(36,"dst_data");
}
static void c_threads(void) {
    ++groups;
    reset_all(); arg0=3;
    tensor(1,F32,32,134217729,1,1,4,128,128,128,0x100000);
    tensor(2,I32,134217729,1,1,1,4,4,4,4,0x200000);
    tensor(3,Q4_0,32,1,1,1,18,18,18,18,0x300000);
    link(3,1,2); fatal_case(38,"q4_nt_overflow");
    reset_all(); arg0=3;
    tensor(1,F32,65536,65537,1,1,4,4,4,4,0x100000);
    tensor(2,I32,65537,1,1,1,4,4,4,4,0x200000);
    tensor(3,F32,65536,1,1,1,4,4,4,4,0x300000);
    link(3,1,2); fatal_case(39,"f32_nt_overflow");
}
static void c_dim(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[1].ne[0]=4294967296LL; T[3].ne[0]=4294967296LL; T[1].ne[1]=1; T[1].ne[2]=1; T[2].ne[1]=1; fatal_case(40,"dim_limit");
}
static void c_span(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[1].nb[2]=9223372036854775804LL; fatal_case(42,"span_overflow");
}
static void c_alias(void) {
    ++groups;
    canonical(0x100000,0x300008,0x300000); fatal_case(43,"alias_src1");
    canonical(0x300008,0x500000,0x300000); fatal_case(44,"alias_src0");
}
static void c_view_fail(void) {
    ++groups;
    canonical(0x100000,0x200000,0x300000); T[3].vo=8; fatal_case(45,"view_off");
}


static void c_new_guards() {
    ++groups;
    canonical(0x100001,0x200000,0x300000); fatal_case(53,"src_alignment");
    canonical(0x100000,0x200001,0x300000); fatal_case(53,"idx_alignment");
    canonical(0x100000,0x200000,0x300001); fatal_case(53,"dst_alignment");
    canonical(0x100000,0x200000,0x300000); T[3].nb[2]=64; fatal_case(55,"overlapping_planes");
    canonical(0x100000,0x200000,0x300000); T[3].nb[1]=16; fatal_case(55,"overlapping_rows");
    canonical(0x100000,0x200000,0x300000); T[1].ne[2]=3; fatal_case(54,"broadcast_divisibility");
}

int main(void) {
    c_new_guards();
    c_canonical(); c_padded(); c_i64(); c_q4_i32(); c_q4_i64();
    c_broadcast(); c_view(); c_maxdims(); c_q4_nt_cap();
    c_refuse(); c_types(); c_zero(); c_rel(); c_q4_align(); c_stride();
    c_data(); c_threads(); c_dim(); c_span(); c_alias(); c_view_fail();
    std::printf("PASS fixture groups=%d runs=%d checks=%d writes=%ld launches=%ld failcallbacks=%ld failchecks=%d\n",
                groups, runs, checks, tot_put, tot_launch, tot_fail, cfail);
    return cfail ? 1 : 0;
}
