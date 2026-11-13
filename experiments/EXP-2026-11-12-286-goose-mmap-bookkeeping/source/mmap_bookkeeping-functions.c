static void fg286m_gg_fail(const char*);
static int64_t fg286m_gg_index(int64_t i, int64_t len);
static int64_t fg286m_gg_div(int64_t a, int64_t b);
static int64_t fg286m_gg_mod(int64_t a, int64_t b);
static uint64_t fg286m_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg286m_gg_umod(uint64_t a,uint64_t b);
static double fg286m_gg_double_bits(uint64_t bits);
static void fg286m_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static uint8_t fg286m_gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t base);
static int64_t fg286m_gg_function_1(int64_t p0, int64_t p1, int64_t base);
static void fg286m_gg_fail(const char*){__builtin_trap();}
static int64_t fg286m_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg286m_gg_fail("array index out of bounds"); return i; }
static int64_t fg286m_gg_div(int64_t a, int64_t b) { if (!b) fg286m_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg286m_gg_fail("division overflow"); return a/b; }
static int64_t fg286m_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg286m_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg286m_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg286m_gg_fail("division by zero");return a/b; }
static uint64_t fg286m_gg_umod(uint64_t a,uint64_t b) { if(!b)fg286m_gg_fail("division by zero");return a%b; }
static double fg286m_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static void fg286m_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg286m_fail)(int64_t);
#endif
  fg286m_fail(t2);
  }
  }
  }
}
static uint8_t fg286m_gg_function_4(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t base) {
  _Alignas(8) uint8_t t0;
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
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) uint64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) uint64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg286m_gg_function_2(t3, t4, base + 0);
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v0));
  t7 = ((t5) < (t6));
  if (t7) {
  {
  v5 = ((int64_t)(v2));
  }
  } else {
  {
  v5 = ((int64_t)(v0));
  }
  }
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v3));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(v1));
  t12 = ((t10) > (t11));
  if (t12) {
  {
  v7 = ((int64_t)(v6));
  }
  } else {
  {
  v7 = ((int64_t)(v1));
  }
  }
  t13 = ((uint64_t)(v4));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((int64_t)(v5));
#ifndef __cplusplus
  extern void (fg286m_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg286m_st_i64(t13, t14, t15);
  t16 = ((uint64_t)(v4));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t18 = ((int64_t)(v7));
#ifndef __cplusplus
  extern void (fg286m_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg286m_st_i64(t16, t17, t18);
  t0 = ((uint8_t)UINT64_C(1));
  }
  return t0;
}
static int64_t fg286m_gg_function_1(int64_t p0, int64_t p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg286m_gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) != (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((t13) == (t14));
  }
  t15 = t12;
  if (t15) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg286m_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg286m_gg_done;
  }
fg286m_gg_done:;
  return t0;
}
int64_t fg286m_should_lock(int64_t p0, int64_t p1){return fg286m_gg_function_1(p0, p1, 0);}
int64_t fg286m_range_update(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4){return fg286m_gg_function_4(p0, p1, p2, p3, p4, 0);}
