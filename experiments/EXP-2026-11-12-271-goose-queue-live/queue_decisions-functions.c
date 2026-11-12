static void fg270q_gg_fail(const char*);
static int64_t fg270q_gg_index(int64_t i, int64_t len);
static int64_t fg270q_gg_div(int64_t a, int64_t b);
static int64_t fg270q_gg_mod(int64_t a, int64_t b);
static uint64_t fg270q_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg270q_gg_umod(uint64_t a,uint64_t b);
static double fg270q_gg_double_bits(uint64_t bits);
static int64_t fg270q_gg_function_4(uint64_t p0, int64_t p1, int64_t base);
static void fg270q_gg_function_7(uint64_t p0, int64_t p1, int64_t base);
static void fg270q_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static int64_t fg270q_gg_function_6(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static int64_t fg270q_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg270q_gg_fail(const char*){asm("trap;");}
static int64_t fg270q_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg270q_gg_fail("array index out of bounds"); return i; }
static int64_t fg270q_gg_div(int64_t a, int64_t b) { if (!b) fg270q_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg270q_gg_fail("division overflow"); return a/b; }
static int64_t fg270q_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg270q_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg270q_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg270q_gg_fail("division by zero");return a/b; }
static uint64_t fg270q_gg_umod(uint64_t a,uint64_t b) { if(!b)fg270q_gg_fail("division by zero");return a%b; }
static double fg270q_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg270q_gg_function_4(uint64_t p0, int64_t p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
#ifndef __cplusplus
  extern int64_t (fg270q_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg270q_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg270q_gg_done;
  }
fg270q_gg_done:;
  return t0;
}
static void fg270q_gg_function_7(uint64_t p0, int64_t p1, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t3 = ((int64_t)(((int64_t)fg270q_gg_div(t1, t2))));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((uint64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t9 = ((int64_t)(((int64_t)fg270q_gg_div(t7, t8))));
  t10 = fg270q_gg_function_4(t6, t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t15 = ((int64_t)(((int64_t)fg270q_gg_mod(t13, t14))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t12) << ((uint64_t)(t15) & 63)))));
  t17 = ((int64_t)(((t11) | (t16))));
#ifndef __cplusplus
  extern void (fg270q_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg270q_st_i64(t0, t5, t17);
  }
}
static void fg270q_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern void (fg270q_fail)(int64_t);
#endif
  fg270q_fail(t2);
  }
  }
  }
}
static int64_t fg270q_gg_function_6(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint8_t t13;
  _Alignas(8) uint64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg270q_gg_function_2(t3, t4, base + 0);
  v4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v1));
  t7 = ((t5) < (t6));
  if (!(t7)) goto label2;
  {
  t8 = ((uint64_t)(v0));
  t9 = ((int64_t)(v5));
  t10 = fg270q_gg_function_4(t8, t9, base + 0);
  t11 = ((int64_t)(t10));
  t12 = ((int64_t)(v2));
  t13 = ((t11) == (t12));
  if (t13) {
  {
  t14 = ((uint64_t)(v3));
  t15 = ((int64_t)(v5));
  fg270q_gg_function_7(t14, t15, base + 0);
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  }
  }
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  }
  return t0;
}
static int64_t fg270q_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg270q_gg_function_2(t3, t4, base + 0);
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  goto fg270q_gg_done;
  }
  }
  v3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(v1));
  t11 = ((t9) < (t10));
  if (!(t11)) goto label2;
  {
  t12 = ((uint64_t)(v0));
  t13 = ((int64_t)(v3));
  t14 = fg270q_gg_function_4(t12, t13, base + 0);
  t15 = ((int64_t)(t14));
  t16 = ((int64_t)(v2));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t0 = ((int64_t)(v3));
  goto fg270q_gg_done;
  }
  }
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v3 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg270q_gg_done;
  }
fg270q_gg_done:;
  return t0;
}
int64_t fg270q_pick_deferred(uint64_t p0, int64_t p1, int64_t p2){return fg270q_gg_function_1(p0, p1, p2, 0);}
int64_t fg270q_cleanup_mask(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3){return fg270q_gg_function_6(p0, p1, p2, p3, 0);}
