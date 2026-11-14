// Native S1 oracle: native gguf_set_* on tiny contexts, no model or weights.
// Prints canonical TSV consumed by host_s1 (Goose port).
#include "gguf.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static size_t type_size(int t) {
    switch (t) {
        case GGUF_TYPE_UINT8:
        case GGUF_TYPE_INT8:
        case GGUF_TYPE_BOOL: return 1;
        case GGUF_TYPE_UINT16:
        case GGUF_TYPE_INT16: return 2;
        case GGUF_TYPE_UINT32:
        case GGUF_TYPE_INT32:
        case GGUF_TYPE_FLOAT32: return 4;
        case GGUF_TYPE_UINT64:
        case GGUF_TYPE_INT64:
        case GGUF_TYPE_FLOAT64: return 8;
        default: return 0;
    }
}

static void hex(const void * p, size_t n) {
    const auto * b = (const unsigned char *) p;
    for (size_t i = 0; i < n; i++) printf("%02x", b[i]);
}

static void dump(const char * label, gguf_context * ctx) {
    printf("CASE\t%s\t%lld\n", label, (long long) gguf_get_n_kv(ctx));
    for (int64_t i = 0; i < gguf_get_n_kv(ctx); i++) {
        const char * key = gguf_get_key(ctx, i);
        int t = (int) gguf_get_kv_type(ctx, i);
        if (t == GGUF_TYPE_ARRAY) {
            int at = (int) gguf_get_arr_type(ctx, i);
            size_t n = gguf_get_arr_n(ctx, i);
            printf("KV\t%s\t%d\t1\t%d\t%zu\t", key, at, at, n);
            if (at == GGUF_TYPE_STRING) {
                for (size_t j = 0; j < n; j++) {
                    const char * s = gguf_get_arr_str(ctx, i, j);
                    if (j) printf(",");
                    hex(s, strlen(s));
                }
            } else {
                hex(gguf_get_arr_data(ctx, i), n * type_size(at));
            }
            printf("\n");
        } else {
            unsigned char buf[8] = {0};
            switch (t) {
                case GGUF_TYPE_UINT8:   { uint8_t v = gguf_get_val_u8(ctx,i); memcpy(buf,&v,1); break; }
                case GGUF_TYPE_INT8:    { int8_t v = gguf_get_val_i8(ctx,i); memcpy(buf,&v,1); break; }
                case GGUF_TYPE_UINT16:  { uint16_t v = gguf_get_val_u16(ctx,i); memcpy(buf,&v,2); break; }
                case GGUF_TYPE_INT16:   { int16_t v = gguf_get_val_i16(ctx,i); memcpy(buf,&v,2); break; }
                case GGUF_TYPE_UINT32:  { uint32_t v = gguf_get_val_u32(ctx,i); memcpy(buf,&v,4); break; }
                case GGUF_TYPE_INT32:   { int32_t v = gguf_get_val_i32(ctx,i); memcpy(buf,&v,4); break; }
                case GGUF_TYPE_FLOAT32: { float v = gguf_get_val_f32(ctx,i); memcpy(buf,&v,4); break; }
                case GGUF_TYPE_UINT64:  { uint64_t v = gguf_get_val_u64(ctx,i); memcpy(buf,&v,8); break; }
                case GGUF_TYPE_INT64:   { int64_t v = gguf_get_val_i64(ctx,i); memcpy(buf,&v,8); break; }
                case GGUF_TYPE_FLOAT64: { double v = gguf_get_val_f64(ctx,i); memcpy(buf,&v,8); break; }
                case GGUF_TYPE_BOOL:    { bool v = gguf_get_val_bool(ctx,i); buf[0] = v; break; }
                case GGUF_TYPE_STRING:  { const char * v = gguf_get_val_str(ctx,i); printf("KV\t%s\t%d\t0\t8\t%zu\t", key, t, strlen(v)); hex(v, strlen(v)); printf("\n"); continue; }
                default: break;
            }
            printf("KV\t%s\t%d\t0\t%d\t%zu\t", key, t, t, type_size(t));
            hex(buf, type_size(t));
            printf("\n");
        }
    }
}

static gguf_context * fresh() { return gguf_init_empty(); }
static void set_scalar(gguf_context * c, const char * k, int t, uint64_t bits) {
    switch (t) {
        case GGUF_TYPE_UINT8:   gguf_set_val_u8(c,k,(uint8_t)bits); break;
        case GGUF_TYPE_INT8:    gguf_set_val_i8(c,k,(int8_t)bits); break;
        case GGUF_TYPE_UINT16:  gguf_set_val_u16(c,k,(uint16_t)bits); break;
        case GGUF_TYPE_INT16:   gguf_set_val_i16(c,k,(int16_t)bits); break;
        case GGUF_TYPE_UINT32:  gguf_set_val_u32(c,k,(uint32_t)bits); break;
        case GGUF_TYPE_INT32:   gguf_set_val_i32(c,k,(int32_t)bits); break;
        case GGUF_TYPE_FLOAT32: { float v; uint32_t b=(uint32_t)bits; memcpy(&v,&b,4); gguf_set_val_f32(c,k,v); break; }
        case GGUF_TYPE_UINT64:  gguf_set_val_u64(c,k,bits); break;
        case GGUF_TYPE_INT64:   gguf_set_val_i64(c,k,(int64_t)bits); break;
        case GGUF_TYPE_FLOAT64: { double v; memcpy(&v,&bits,8); gguf_set_val_f64(c,k,v); break; }
        case GGUF_TYPE_BOOL:    gguf_set_val_bool(c,k,bits!=0); break;
        case GGUF_TYPE_STRING:  gguf_set_val_str(c,k,"hello"); break;
    }
}

int main() {
    struct { const char * label; int t; uint64_t bits; } scalars[] = {
        {"u8", GGUF_TYPE_UINT8, 250}, {"i8", GGUF_TYPE_INT8, (uint64_t)(int64_t)-5},
        {"u16", GGUF_TYPE_UINT16, 65000}, {"i16", GGUF_TYPE_INT16, (uint64_t)(int64_t)-1234},
        {"u32", GGUF_TYPE_UINT32, 4000000000ull}, {"i32", GGUF_TYPE_INT32, (uint64_t)(int64_t)-2000000000},
        {"f32", GGUF_TYPE_FLOAT32, 0}, {"u64", GGUF_TYPE_UINT64, 0x8000000000000005ull},
        {"i64", GGUF_TYPE_INT64, 0xC000000000000000ull}, {"f64", GGUF_TYPE_FLOAT64, 0},
        {"bool", GGUF_TYPE_BOOL, 1}, {"str", GGUF_TYPE_STRING, 0},
    };
    for (auto & s : scalars) {
        gguf_context * c = fresh();
        uint64_t bits = s.bits;
        if (s.t == GGUF_TYPE_FLOAT32) { float v = 1.5f; uint32_t b; memcpy(&b,&v,4); bits=b; }
        if (s.t == GGUF_TYPE_FLOAT64) { double v = -3.25; memcpy(&bits,&v,8); }
        set_scalar(c, "key", s.t, bits);
        dump(s.label, c);
        gguf_free(c);
    }

    // numeric arrays
    { gguf_context*c=fresh(); uint8_t d[]={1,2,255}; gguf_set_arr_data(c,"a",GGUF_TYPE_UINT8,d,3); dump("arr_u8",c); gguf_free(c); }
    { gguf_context*c=fresh(); int32_t d[]={-1,0,7}; gguf_set_arr_data(c,"a",GGUF_TYPE_INT32,d,3); dump("arr_i32",c); gguf_free(c); }
    { gguf_context*c=fresh(); float d[]={0.5f,-1.5f}; gguf_set_arr_data(c,"a",GGUF_TYPE_FLOAT32,d,2); dump("arr_f32",c); gguf_free(c); }
    { gguf_context*c=fresh(); uint8_t d[]={1,0,1}; gguf_set_arr_data(c,"a",GGUF_TYPE_BOOL,d,3); dump("arr_bool",c); gguf_free(c); }
    { gguf_context*c=fresh(); const char * d[]={"a","bc",""}; gguf_set_arr_str(c,"a",d,3); dump("arr_str",c); gguf_free(c); }

    // replace + valid reserved key
    { gguf_context*c=fresh(); gguf_set_val_u32(c,"k",1); gguf_set_val_u32(c,"k",2);
      gguf_set_val_u32(c,"general.alignment",64); dump("replace_reserved",c); gguf_free(c); }

    // remove existing and missing
    { gguf_context*c=fresh(); gguf_set_val_u8(c,"a",1); gguf_set_val_u8(c,"b",2); gguf_set_val_u8(c,"c",3);
      int64_t r1=gguf_remove_key(c,"b"); int64_t r2=gguf_remove_key(c,"zz");
      printf("RM\t%lld\t%lld\n",(long long)r1,(long long)r2); dump("remove",c); gguf_free(c); }

    // set_kv copies all types, arrays and strings; destination key is replaced
    { gguf_context*src=fresh();
      gguf_set_val_u16(src,"s_u16",60000);
      gguf_set_val_str(src,"s_str","copy");
      uint32_t ad[]={7,8}; gguf_set_arr_data(src,"s_arr",GGUF_TYPE_UINT32,ad,2);
      const char * sd[]={"x","yz"}; gguf_set_arr_str(src,"s_sarr",sd,2);
      gguf_context*dst=fresh(); gguf_set_val_u16(dst,"s_u16",1);
      gguf_set_kv(dst,src); dump("set_kv",dst); gguf_free(dst); gguf_free(src); }
    return 0;
}
