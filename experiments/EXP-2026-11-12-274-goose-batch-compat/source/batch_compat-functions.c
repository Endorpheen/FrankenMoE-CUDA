static void fg274b_gg_fail(const char*);
static int64_t fg274b_gg_index(int64_t i, int64_t len);
static int64_t fg274b_gg_div(int64_t a, int64_t b);
static int64_t fg274b_gg_mod(int64_t a, int64_t b);
static uint64_t fg274b_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg274b_gg_umod(uint64_t a,uint64_t b);
static double fg274b_gg_double_bits(uint64_t bits);
static int64_t fg274b_gg_function_4(uint64_t p0, int64_t p1, int64_t base);
static void fg274b_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static uint8_t fg274b_gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, uint64_t p6, uint64_t p7, int64_t p8, uint64_t p9, uint64_t p10, int64_t p11, int64_t base);
static void fg274b_gg_fail(const char*){asm("trap;");}
static int64_t fg274b_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg274b_gg_fail("array index out of bounds"); return i; }
static int64_t fg274b_gg_div(int64_t a, int64_t b) { if (!b) fg274b_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg274b_gg_fail("division overflow"); return a/b; }
static int64_t fg274b_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg274b_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg274b_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg274b_gg_fail("division by zero");return a/b; }
static uint64_t fg274b_gg_umod(uint64_t a,uint64_t b) { if(!b)fg274b_gg_fail("division by zero");return a%b; }
static double fg274b_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg274b_gg_function_4(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg274b_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg274b_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg274b_gg_done;
  }
fg274b_gg_done:;
  return t0;
}
static void fg274b_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg274b_fail)(int64_t);
#endif
  fg274b_fail(t2);
  }
  }
  }
}
static uint8_t fg274b_gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, uint64_t p6, uint64_t p7, int64_t p8, uint64_t p9, uint64_t p10, int64_t p11, int64_t base) {
  _Alignas(8) uint8_t t0;
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
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) uint8_t t27;
  _Alignas(8) uint64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) uint64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) uint8_t t36;
  _Alignas(8) uint64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) uint32_t t42;
  _Alignas(8) float t43;
  _Alignas(8) uint64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) uint32_t t49;
  _Alignas(8) float t50;
  _Alignas(8) float t51;
  _Alignas(8) float t52;
  _Alignas(8) uint8_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) uint64_t v6;
  _Alignas(8) uint64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) uint64_t v9;
  _Alignas(8) uint64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) float v13;
  _Alignas(8) float v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  v9 = p9;
  v10 = p10;
  v11 = p11;
  {
  t1 = ((int64_t)(v8));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v11));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg274b_gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) != (t10));
  t12 = t11;
  if (!t12) {
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((t13) != (t14));
  }
  t15 = t12;
  if (t15) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg274b_gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg274b_gg_done;
  }
  }
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(v3));
  t21 = ((t19) != (t20));
  if (t21) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg274b_gg_done;
  }
  }
  t22 = ((int64_t)(v8));
  t23 = ((int64_t)(v11));
  t24 = ((t22) != (t23));
  if (t24) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg274b_gg_done;
  }
  }
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t25 = ((int64_t)(v12));
  t26 = ((int64_t)(v8));
  t27 = ((t25) < (t26));
  if (!(t27)) goto label2;
  {
  t28 = ((uint64_t)(v6));
  t29 = ((int64_t)(v12));
  t30 = fg274b_gg_function_4(t28, t29, base + 0);
  t31 = ((int64_t)(t30));
  t32 = ((uint64_t)(v9));
  t33 = ((int64_t)(v12));
  t34 = fg274b_gg_function_4(t32, t33, base + 0);
  t35 = ((int64_t)(t34));
  t36 = ((t31) != (t35));
  if (t36) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg274b_gg_done;
  }
  }
  t37 = ((uint64_t)(v7));
  t38 = ((int64_t)(v12));
  t39 = fg274b_gg_function_4(t37, t38, base + 0);
  t40 = ((int64_t)(t39));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4294967295))))));
  t42 = ((uint32_t)(((uint32_t)(((t40) & (t41))))));
#ifndef __cplusplus
  extern float (fg274b_from_bits)(uint32_t);
#endif
  t43 = fg274b_from_bits(t42);
  v13 = ((float)(t43));
  t44 = ((uint64_t)(v10));
  t45 = ((int64_t)(v12));
  t46 = fg274b_gg_function_4(t44, t45, base + 0);
  t47 = ((int64_t)(t46));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4294967295))))));
  t49 = ((uint32_t)(((uint32_t)(((t47) & (t48))))));
#ifndef __cplusplus
  extern float (fg274b_from_bits)(uint32_t);
#endif
  t50 = fg274b_from_bits(t49);
  v14 = ((float)(t50));
  t51 = ((float)(v13));
  t52 = ((float)(v14));
  t53 = ((t51) != (t52));
  if (t53) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg274b_gg_done;
  }
  }
  t54 = ((int64_t)(v12));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t55)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  goto fg274b_gg_done;
  }
fg274b_gg_done:;
  return t0;
}
int64_t fg274b_can_batch(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, uint64_t p6, uint64_t p7, int64_t p8, uint64_t p9, uint64_t p10, int64_t p11){return fg274b_gg_function_1(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, 0);}
