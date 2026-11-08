/* fixture.c - canonical IO C ABI checks against the local storage-contract shim. */
#include "canonical-io.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int64_t fgc_live_allocs(void);
extern int64_t fgc_live_bytes(void);
static int fails;
static void num(const char * c, const char * f, int64_t g, int64_t n) {
    int ok = g == n; if (!ok) fails++;
    printf("FG153_TRACE case=%s field=%s goose=%lld want=%lld %s\n", c, f,
           (long long) g, (long long) n, ok ? "PASS" : "FAIL");
}
static void owned(const char * c) {
    num(c, "live_allocs", fgc_live_allocs(), 0);
    num(c, "live_bytes", fgc_live_bytes(), 0);
}
int main(void) {
    { int64_t h = fgcio_create(0, 0, 0, 1); fgcio_write(h, (int64_t)(intptr_t)"a", 5);
      fgcio_write_tensor(h, 0, 7, 0); num("dummy-skip", "n", fgcio_n_bytes(h), 5);
      num("dummy-skip", "dispose", fgcio_dispose(h), 0); owned("dummy-skip"); }
    { int64_t h = fgcio_create(0, 0, 0, 0); fgcio_write_tensor(h, 0, 7, 0);
      num("dummy", "n", fgcio_n_bytes(h), 7); fgcio_dispose(h); }
    { uint8_t buf[8]; memset(buf, 0, sizeof buf);
      int64_t h = fgcio_create(1, (int64_t)(intptr_t)buf, 8, 0);
      num("hostw", "write", fgcio_write(h, (int64_t)(intptr_t)"abcd", 4), 0);
      num("hostw", "n", fgcio_n_bytes(h), 4);
      num("hostw", "overflow", fgcio_write(h, (int64_t)(intptr_t)"x", 5), -1);
      num("hostw", "bytes", memcmp(buf, "abcd", 4) == 0 ? 0 : 1, 0);
      num("hostw", "dispose", fgcio_dispose(h), 0); owned("hostw"); }
    { uint8_t buf[4] = {1,2,3,4}; uint8_t out[8] = {0};
      int64_t h = fgcio_create(2, (int64_t)(intptr_t)buf, 4, 0);
      num("hostr", "read", fgcio_read(h, (int64_t)(intptr_t)out, 4), 0);
      num("hostr", "bytes", memcmp(out, buf, 4) == 0 ? 0 : 1, 0);
      num("hostr", "truncated", fgcio_read(h, (int64_t)(intptr_t)out, 1), -1);
      fgcio_dispose(h); owned("hostr"); }
    { uint8_t b[2]; int64_t h = fgcio_create(1, (int64_t)(intptr_t)b, 2, 0);
      num("hostw-tensor", "bounds", fgcio_write_tensor(h, 0, 3, 0), -1);
      num("hostw-tensor", "n", fgcio_n_bytes(h), 0);
      fgcio_dispose(h); }
    { const char * p = "/work/canonical-io/build/tmp-state.bin";
      const uint8_t b[5] = {9,8,7,6,5}; uint8_t o[5] = {0};
      FILE * f = fopen(p, "wb"); int64_t h = fgcio_create(3, (int64_t)(intptr_t)f, 0, 0);
      num("filew", "write", fgcio_write(h, (int64_t)(intptr_t)b, 5), 0);
      num("filew", "dispose", fgcio_dispose(h), 0); fclose(f); owned("filew");
      f = fopen(p, "rb"); h = fgcio_create(4, (int64_t)(intptr_t)f, 0, 0);
      num("filer", "read", fgcio_read(h, (int64_t)(intptr_t)o, 5), 0);
      num("filer", "bytes", memcmp(o, b, 5) == 0 ? 0 : 1, 0);
      num("filer", "truncated", fgcio_read(h, (int64_t)(intptr_t)o, 1), -1);
      fgcio_dispose(h); fclose(f); owned("filer"); }
    { uint8_t db[8]; int64_t h = fgcio_create(5, (int64_t)(intptr_t)db, 8, 0);
      num("device", "tensor-append", fgcio_write_tensor(h, 0, 3, 0), 0);
      num("device", "n-after-tensor", fgcio_n_bytes(h), 0);
      num("device", "write", fgcio_write(h, (int64_t)(intptr_t)"abcd", 4), 0);
      num("device", "overflow", fgcio_write(h, (int64_t)(intptr_t)"x", 5), -1);
      num("device", "dispose", fgcio_dispose(h), 0); owned("device"); }
    for (int mm = 0; mm < 2; ++mm) {
        float in[4] = {1,2,3,4};
        int64_t th = fgcio_raw(40, 16, 0, 4, (int64_t)(intptr_t)in);
        int64_t h = fgcio_create(5, 0, 0, 0);
        num("device-flush", "write_t", fgcio_write_tensor(h, th, 16, 0), 0);
        num("device-flush", "flush_w", fgcio_flush(h), 0);
        int ok = 0;
        if (!mm) {
            unsigned char o[16] = {0};
            int64_t ot = fgcio_raw(40, 16, 0, 4, (int64_t)(intptr_t)o);
            int64_t hr = fgcio_create(6, 0, 0, h);
            num("device-flush", "read_t", fgcio_read_tensor(hr, ot, 16, 0), 0);
            num("device-flush", "flush_r", fgcio_flush(hr), 0);
            ok = memcmp(o, in, 16) == 0;
            fgcio_dispose(hr);
        } else {
            unsigned char o1[8] = {0}, o2[8] = {0};
            int64_t t1 = fgcio_raw(40, 8, 0, 2, (int64_t)(intptr_t)o1);
            int64_t t2 = fgcio_raw(40, 8, 0, 2, (int64_t)(intptr_t)o2);
            int64_t hr = fgcio_create(6, 0, 0, h);
            num("device-chunk", "read1", fgcio_read_tensor(hr, t1, 8, 0), 0);
            num("device-chunk", "read2", fgcio_read_tensor(hr, t2, 8, 0), 0);
            num("device-chunk", "flush", fgcio_flush(hr), 0);
            ok = memcmp(o1, in, 8) == 0 && memcmp(o2, ((unsigned char*)in)+8, 8) == 0;
            fgcio_dispose(hr);
        }
        num(mm ? "device-chunk" : "device-flush", "roundtrip", ok, 1);
        fgcio_dispose(h);
        owned("device-flush");
    }
    printf("FG153_RESULT failures=%d\n", fails);
    return fails ? 1 : 0;
}
