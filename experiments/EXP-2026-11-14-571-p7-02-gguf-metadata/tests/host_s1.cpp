// Host for the Goose S1 port: runs the same 20 cases as probe_s1 and prints
// the same canonical TSV.
#include "s1_metadata.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const int MAX = 64;
static int64_t st[4];
static unsigned char pool[8192];
static int64_t ko[MAX], kl[MAX], ty[MAX], ar[MAX], v0[MAX], v1[MAX], v2[MAX];
static int64_t out[2];

static sl_u8 S(void * p, int64_t n) { return sl_u8{ (uint8_t *) p, n }; }
static sl_i64 A(int64_t * p) { return sl_i64{ p, MAX }; }

static void init() {
    s1_init(A(st), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2));
}

static void set_val(const char * key, int t, int64_t bits) {
    s1_set_val(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
               S((void *) key, (int64_t) strlen(key)), t, bits, A(out));
}

static void set_str(const char * key, const char * val) {
    s1_set_str(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
               S((void *) key, (int64_t) strlen(key)), S((void *) val, (int64_t) strlen(val)), A(out));
}

static void set_arr(const char * key, int at, const void * data, int64_t bytes, int64_t n) {
    s1_set_arr(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
               S((void *) key, (int64_t) strlen(key)), at, S((void *) data, bytes), n, A(out));
}

static void set_arr_str(const char * key, const char * blob, int64_t bytes, int64_t n) {
    s1_set_arr_str(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
                   S((void *) key, (int64_t) strlen(key)), S((void *) blob, bytes), n, A(out));
}

static size_t tsize(int t) {
    if (t == 0 || t == 1 || t == 7) return 1;
    if (t == 2 || t == 3) return 2;
    if (t == 4 || t == 5 || t == 6) return 4;
    if (t == 10 || t == 11 || t == 12) return 8;
    return 0;
}

static void hex(const void * p, size_t n) {
    const unsigned char * b = (const unsigned char *) p;
    for (size_t i = 0; i < n; i++) printf("%02x", b[i]);
}

static void dump(const char * label) {
    printf("CASE\t%s\t%lld\n", label, (long long) st[0]);
    for (int64_t i = 0; i < st[0]; i++) {
        std::string key((const char *) pool + ko[i], (size_t) kl[i]);
        int t = (int) ty[i];
        if (ar[i] != 0) {
            int at = t;
            int64_t n = v1[i];
            printf("KV\t%s\t%d\t1\t%d\t%lld\t", key.c_str(), at, at, (long long) n);
            if (ar[i] == 2) {
                int64_t p = v0[i], end = v0[i] + v2[i], k = 0;
                for (; p <= end && k < n; ) {
                    int64_t s = p;
                    while (p < end && pool[p] != 0) p++;
                    if (k) printf(",");
                    hex(pool + s, (size_t) (p - s));
                    p++;
                    k++;
                }
            } else {
                hex(pool + v0[i], (size_t) v2[i]);
            }
            printf("\n");
        } else if (t == 8) {
            printf("KV\t%s\t%d\t0\t8\t%lld\t", key.c_str(), t, (long long) v2[i]);
            hex(pool + v0[i], (size_t) v2[i]);
            printf("\n");
        } else {
            uint64_t bits = (uint64_t) v0[i];
            size_t n = tsize(t);
            printf("KV\t%s\t%d\t0\t%d\t%zu\t", key.c_str(), t, t, n);
            unsigned char b[8];
            for (size_t k = 0; k < n; k++) b[k] = (unsigned char) (bits >> (8 * k));
            hex(b, n);
            printf("\n");
        }
    }
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    struct Sc { const char * label; int t; uint64_t bits; } scalars[] = {
        {"u8",0,250},{"i8",1,(uint8_t)-5},{"u16",2,65000},{"i16",3,(uint16_t)-1234},
        {"u32",4,4000000000ull},{"i32",5,(uint32_t)-2000000000},{"f32",6,0},
        {"u64",10,0x8000000000000005ull},{"i64",11,0xC000000000000000ull},{"f64",12,0},
        {"bool",7,1},{"str",8,0},
    };
    for (auto & s : scalars) {
        init();
        if (s.t == 6) { float f = 1.5f; uint32_t b; memcpy(&b, &f, 4); set_val("key", s.t, b); }
        else if (s.t == 12) { double d = -3.25; uint64_t b; memcpy(&b, &d, 8); set_val("key", s.t, (int64_t) b); }
        else if (s.t == 8) { set_str("key", "hello"); }
        else set_val("key", s.t, (int64_t) s.bits);
        dump(s.label);
    }
    { init(); unsigned char d[]={1,2,255}; set_arr("a",0,d,3,3); dump("arr_u8"); }
    { init(); int32_t d[]={-1,0,7}; set_arr("a",5,(void*)d,12,3); dump("arr_i32"); }
    { init(); float d[]={0.5f,-1.5f}; set_arr("a",6,(void*)d,8,2); dump("arr_f32"); }
    { init(); unsigned char d[]={1,0,1}; set_arr("a",7,d,3,3); dump("arr_bool"); }
    { init(); const char blob[] = "a\0bc\0\0"; set_arr_str("a", blob, 6, 3); dump("arr_str"); }

    { init(); set_val("k",4,1); set_val("k",4,2); set_val("general.alignment",4,64); dump("replace_reserved"); }
    { init(); set_val("a",0,1); set_val("b",0,2); set_val("c",0,3);
      int64_t r1 = s1_remove(A(st),S(pool,sizeof pool),A(ko),A(kl),A(ty),A(ar),A(v0),A(v1),A(v2),S((void*)"b",1));
      int64_t r2 = s1_remove(A(st),S(pool,sizeof pool),A(ko),A(kl),A(ty),A(ar),A(v0),A(v1),A(v2),S((void*)"zz",2));
      printf("RM\t%lld\t%lld\n",(long long)r1,(long long)r2); dump("remove"); }

    // set_kv: build source, destination has a duplicate s_u16
    {
        static int64_t sst[4], sko[MAX], skl[MAX], sty[MAX], sar[MAX], sv0[MAX], sv1[MAX], sv2[MAX];
        static unsigned char spool[4096];
        s1_init(sl_i64{sst,4}, sl_i64{sko,MAX}, sl_i64{skl,MAX}, sl_i64{sty,MAX}, sl_i64{sar,MAX},
                sl_i64{sv0,MAX}, sl_i64{sv1,MAX}, sl_i64{sv2,MAX});
        s1_set_val(sl_i64{sst,4}, sl_u8{spool,sizeof spool}, sl_i64{sko,MAX}, sl_i64{skl,MAX}, sl_i64{sty,MAX},
                   sl_i64{sar,MAX}, sl_i64{sv0,MAX}, sl_i64{sv1,MAX}, sl_i64{sv2,MAX},
                   sl_u8{(uint8_t*)"s_u16",5}, 2, 60000, sl_i64{out,2});
        s1_set_str(sl_i64{sst,4}, sl_u8{spool,sizeof spool}, sl_i64{sko,MAX}, sl_i64{skl,MAX}, sl_i64{sty,MAX},
                   sl_i64{sar,MAX}, sl_i64{sv0,MAX}, sl_i64{sv1,MAX}, sl_i64{sv2,MAX},
                   sl_u8{(uint8_t*)"s_str",5}, sl_u8{(uint8_t*)"copy",4}, sl_i64{out,2});
        uint32_t ad[]={7,8};
        s1_set_arr(sl_i64{sst,4}, sl_u8{spool,sizeof spool}, sl_i64{sko,MAX}, sl_i64{skl,MAX}, sl_i64{sty,MAX},
                   sl_i64{sar,MAX}, sl_i64{sv0,MAX}, sl_i64{sv1,MAX}, sl_i64{sv2,MAX},
                   sl_u8{(uint8_t*)"s_arr",5}, 4, sl_u8{(uint8_t*)ad,8}, 2, sl_i64{out,2});
        const char blob[] = "x\0yz\0";
        s1_set_arr_str(sl_i64{sst,4}, sl_u8{spool,sizeof spool}, sl_i64{sko,MAX}, sl_i64{skl,MAX}, sl_i64{sty,MAX},
                       sl_i64{sar,MAX}, sl_i64{sv0,MAX}, sl_i64{sv1,MAX}, sl_i64{sv2,MAX},
                       sl_u8{(uint8_t*)"s_sarr",6}, sl_u8{(uint8_t*)blob,6}, 2, sl_i64{out,2});

        init(); set_val("s_u16",2,1);
        s1_set_kv(A(st),S(pool,sizeof pool),A(ko),A(kl),A(ty),A(ar),A(v0),A(v1),A(v2),
                  sl_i64{sst,4},sl_u8{spool,sizeof spool},sl_i64{sko,MAX},sl_i64{skl,MAX},
                  sl_i64{sty,MAX},sl_i64{sar,MAX},sl_i64{sv0,MAX},sl_i64{sv1,MAX},sl_i64{sv2,MAX},
                  A(out));
        dump("set_kv");
    }
    return 0;
}
