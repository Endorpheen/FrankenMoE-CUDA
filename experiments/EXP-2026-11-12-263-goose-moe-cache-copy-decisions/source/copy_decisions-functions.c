static void fg263c_gg_fail(const char*);
static int64_t fg263c_gg_index(int64_t i, int64_t len);
static int64_t fg263c_gg_div(int64_t a, int64_t b);
static int64_t fg263c_gg_mod(int64_t a, int64_t b);
static uint64_t fg263c_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg263c_gg_umod(uint64_t a,uint64_t b);
static double fg263c_gg_double_bits(uint64_t bits);
static int64_t fg263c_gg_function_4(uint64_t p0, int64_t p1, int64_t base);
static void fg263c_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static int64_t fg263c_gg_function_6(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t fg263c_gg_function_8(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static int64_t fg263c_gg_function_7(int64_t p0, int64_t p1, int64_t base);
static int64_t fg263c_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static void fg263c_gg_fail(const char*){asm("trap;");}
static int64_t fg263c_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg263c_gg_fail("array index out of bounds"); return i; }
static int64_t fg263c_gg_div(int64_t a, int64_t b) { if (!b) fg263c_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg263c_gg_fail("division overflow"); return a/b; }
static int64_t fg263c_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg263c_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg263c_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg263c_gg_fail("division by zero");return a/b; }
static uint64_t fg263c_gg_umod(uint64_t a,uint64_t b) { if(!b)fg263c_gg_fail("division by zero");return a%b; }
static double fg263c_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg263c_gg_function_4(uint64_t p0, int64_t p1, int64_t base) {
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
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
#ifndef __cplusplus
  extern int64_t (fg263c_ld_i32)(uint64_t, int64_t);
#endif
  t5 = fg263c_ld_i32(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg263c_gg_done;
  }
fg263c_gg_done:;
  return t0;
}
static void fg263c_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg263c_fail)(int64_t);
#endif
  fg263c_fail(t2);
  }
  }
  }
}
static int64_t fg263c_gg_function_6(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) uint64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) uint64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) uint8_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v3));
  t4 = ((t5) < (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg263c_gg_function_2(t7, t8, base + 0);
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v2));
  v4 = fg263c_gg_function_4(t9, t10, base + 0);
  t11 = ((uint64_t)(v1));
  t12 = ((int64_t)(v2));
  v5 = fg263c_gg_function_4(t11, t12, base + 0);
  v6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
label1:;
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(v6));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(v3));
  t17 = ((t15) < (t16));
  t18 = t17;
  if (t18) {
  t19 = ((uint64_t)(v0));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = fg263c_gg_function_4(t19, t22, base + 0);
  t24 = ((int64_t)(t23));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(v6));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t18 = ((t24) == (t27));
  }
  t28 = t18;
  t29 = t28;
  if (t29) {
  t30 = ((uint64_t)(v1));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(v6));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = fg263c_gg_function_4(t30, t33, base + 0);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t29 = ((t35) == (t38));
  }
  t39 = t29;
  if (!(t39)) goto label2;
  {
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v6));
  }
  return t0;
}
static int64_t fg263c_gg_function_8(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) uint64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(13))));
  fg263c_gg_function_2(t3, t4, base + 0);
  v4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(v2));
  t7 = ((t5) < (t6));
  if (!(t7)) goto label2;
  {
  t8 = ((uint64_t)(v0));
  t9 = ((uint64_t)(v1));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(v2));
  v6 = fg263c_gg_function_6(t8, t9, t10, t11, base + 0);
  t12 = ((int64_t)(v5));
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t15)))));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(v6));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v5));
  }
  return t0;
}
static int64_t fg263c_gg_function_7(int64_t p0, int64_t p1, int64_t base) {
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
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint8_t t13;
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
  t8 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg263c_gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t11 = ((int64_t)(v1));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((t9) <= (t12));
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg263c_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg263c_gg_done;
  }
fg263c_gg_done:;
  return t0;
}
static int64_t fg263c_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint8_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) uint8_t t19;
  _Alignas(8) uint64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg263c_gg_function_2(t7, t8, base + 0);
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v1));
  v3 = fg263c_gg_function_4(t9, t10, base + 0);
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t13 = ((t11) < (t12));
  if (t13) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg263c_gg_done;
  }
  }
  v4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
label1:;
  t14 = ((int64_t)(v1));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t17 = ((int64_t)(v2));
  t18 = ((t16) <= (t17));
  t19 = t18;
  if (t19) {
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = fg263c_gg_function_4(t20, t23, base + 0);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(v3));
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t19 = ((t25) == (t28));
  }
  t29 = t19;
  if (!(t29)) goto label2;
  {
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v4));
  }
fg263c_gg_done:;
  return t0;
}
static int64_t fg263c_run_from_slot(uint64_t p0, int64_t p1, int64_t p2){return fg263c_gg_function_1(p0, p1, p2, 0);}
static int64_t fg263c_run_from_fills(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3){return fg263c_gg_function_6(p0, p1, p2, p3, 0);}
static int64_t fg263c_stats_class(int64_t p0, int64_t p1){return fg263c_gg_function_7(p0, p1, 0);}
static int64_t fg263c_fill_bytes(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3){return fg263c_gg_function_8(p0, p1, p2, p3, 0);}
