static void fg272s_gg_fail(const char*);
static int64_t fg272s_gg_index(int64_t i, int64_t len);
static int64_t fg272s_gg_div(int64_t a, int64_t b);
static int64_t fg272s_gg_mod(int64_t a, int64_t b);
static uint64_t fg272s_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg272s_gg_umod(uint64_t a,uint64_t b);
static double fg272s_gg_double_bits(uint64_t bits);
static int64_t fg272s_gg_function_5(uint64_t p0, int64_t p1, int64_t base);
static void fg272s_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static int64_t fg272s_gg_function_7(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t base);
static int64_t fg272s_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static void fg272s_gg_fail(const char*){asm("trap;");}
static int64_t fg272s_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg272s_gg_fail("array index out of bounds"); return i; }
static int64_t fg272s_gg_div(int64_t a, int64_t b) { if (!b) fg272s_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg272s_gg_fail("division overflow"); return a/b; }
static int64_t fg272s_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg272s_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg272s_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg272s_gg_fail("division by zero");return a/b; }
static uint64_t fg272s_gg_umod(uint64_t a,uint64_t b) { if(!b)fg272s_gg_fail("division by zero");return a%b; }
static double fg272s_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg272s_gg_function_5(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg272s_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg272s_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg272s_gg_done;
  }
fg272s_gg_done:;
  return t0;
}
static void fg272s_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg272s_fail)(int64_t);
#endif
  fg272s_fail(t2);
  }
  }
  }
}
static int64_t fg272s_gg_function_7(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) uint64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) uint64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) uint8_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) uint8_t t32;
  _Alignas(8) uint8_t t33;
  _Alignas(8) uint64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) uint8_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg272s_gg_function_2(t3, t4, base + 0);
  t5 = ((int64_t)(((int64_t)UINT64_C(1))));
  v4 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t5)))));
  v5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t6 = ((int64_t)(v5));
  t7 = ((int64_t)(v3));
  t8 = ((t6) < (t7));
  if (!(t8)) goto label2;
  {
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v5));
  t11 = fg272s_gg_function_5(t9, t10, base + 0);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((t12) != (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t17 = ((t15) < (t16));
  if (t17) {
  {
  v4 = ((int64_t)(v5));
  }
  } else {
  {
  t18 = ((uint64_t)(v1));
  t19 = ((int64_t)(v4));
  v6 = fg272s_gg_function_5(t18, t19, base + 0);
  t20 = ((uint64_t)(v1));
  t21 = ((int64_t)(v5));
  v7 = fg272s_gg_function_5(t20, t21, base + 0);
  t22 = ((int64_t)(v6));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t24 = ((t22) == (t23));
  t25 = t24;
  if (t25) {
  t26 = ((int64_t)(v7));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t25 = ((t26) != (t27));
  }
  t28 = t25;
  t29 = t28;
  if (!t29) {
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(v7));
  t32 = ((t30) == (t31));
  t33 = t32;
  if (t33) {
  t34 = ((uint64_t)(v2));
  t35 = ((int64_t)(v5));
  t36 = fg272s_gg_function_5(t34, t35, base + 0);
  t37 = ((int64_t)(t36));
  t38 = ((uint64_t)(v2));
  t39 = ((int64_t)(v4));
  t40 = fg272s_gg_function_5(t38, t39, base + 0);
  t41 = ((int64_t)(t40));
  t33 = ((t37) < (t41));
  }
  t29 = t33;
  }
  t42 = t29;
  if (t42) {
  {
  v4 = ((int64_t)(v5));
  }
  }
  }
  }
  }
  }
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  }
  return t0;
}
static int64_t fg272s_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint32_t t9;
  _Alignas(8) float t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) uint64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint8_t t20;
  _Alignas(8) uint64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) float t24;
  _Alignas(8) float t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) float t30;
  _Alignas(8) float t31;
  _Alignas(8) uint8_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) float v5;
  _Alignas(8) float v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) float v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg272s_gg_function_2(t7, t8, base + 0);
  t9 = ((uint32_t)(((uint32_t)(v4))));
#ifndef __cplusplus
  extern float (fg272s_from_bits)(uint32_t);
#endif
  t10 = fg272s_from_bits(t9);
  v5 = ((float)(t10));
  v6 = ((float)(((float)(((int64_t)UINT64_C(0))))));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  v7 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t11)))));
  v8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t12 = ((int64_t)(v8));
  t13 = ((int64_t)(v2));
  t14 = ((t12) < (t13));
  if (!(t14)) goto label2;
  {
  t15 = ((uint64_t)(v0));
  t16 = ((int64_t)(v8));
  t17 = fg272s_gg_function_5(t15, t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t20 = ((t18) != (t19));
  if (t20) {
  {
  t21 = ((uint64_t)(v1));
  t22 = ((int64_t)(v8));
  t23 = fg272s_gg_function_5(t21, t22, base + 0);
  t24 = ((float)(((float)(t23))));
  t25 = ((float)(((float)(v3))));
  v9 = ((float)(((t24) / (t25))));
  t26 = ((float)(v9));
  t27 = ((float)(v6));
  t28 = ((t26) > (t27));
  t29 = t28;
  if (t29) {
  t30 = ((float)(v9));
  t31 = ((float)(v5));
  t29 = ((t30) > (t31));
  }
  t32 = t29;
  if (t32) {
  {
  v6 = ((float)(v9));
  v7 = ((int64_t)(v8));
  }
  }
  }
  }
  t33 = ((int64_t)(v8));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v7));
  }
  return t0;
}
int64_t fg272s_pick_similar(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4){return fg272s_gg_function_1(p0, p1, p2, p3, p4, 0);}
int64_t fg272s_pick_lru(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3){return fg272s_gg_function_7(p0, p1, p2, p3, 0);}
