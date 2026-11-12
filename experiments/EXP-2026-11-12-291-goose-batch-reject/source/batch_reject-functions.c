static void fg291b_gg_fail(const char*);
static int64_t fg291b_gg_index(int64_t i, int64_t len);
static int64_t fg291b_gg_div(int64_t a, int64_t b);
static int64_t fg291b_gg_mod(int64_t a, int64_t b);
static uint64_t fg291b_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg291b_gg_umod(uint64_t a,uint64_t b);
static double fg291b_gg_double_bits(uint64_t bits);
static int64_t fg291b_gg_function_4(uint64_t p0, int64_t p1, int64_t base);
static int64_t fg291b_gg_function_8(uint64_t p0, int64_t p1, int64_t base);
static void fg291b_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static int64_t fg291b_gg_function_6(int64_t p0, uint64_t p1, uint64_t p2, int64_t p3, uint64_t p4, uint64_t p5, int64_t base);
static int64_t fg291b_gg_function_1(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, uint64_t p6, int64_t p7, int64_t base);
static void fg291b_gg_fail(const char*){asm("trap;");}
static int64_t fg291b_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg291b_gg_fail("array index out of bounds"); return i; }
static int64_t fg291b_gg_div(int64_t a, int64_t b) { if (!b) fg291b_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg291b_gg_fail("division overflow"); return a/b; }
static int64_t fg291b_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg291b_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg291b_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg291b_gg_fail("division by zero");return a/b; }
static uint64_t fg291b_gg_umod(uint64_t a,uint64_t b) { if(!b)fg291b_gg_fail("division by zero");return a%b; }
static double fg291b_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg291b_gg_function_4(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg291b_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg291b_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg291b_gg_done;
  }
fg291b_gg_done:;
  return t0;
}
static int64_t fg291b_gg_function_8(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg291b_ld_u64)(uint64_t, int64_t);
#endif
  t5 = fg291b_ld_u64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg291b_gg_done;
  }
fg291b_gg_done:;
  return t0;
}
static void fg291b_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg291b_fail)(int64_t);
#endif
  fg291b_fail(t2);
  }
  }
  }
}
static int64_t fg291b_gg_function_6(int64_t p0, uint64_t p1, uint64_t p2, int64_t p3, uint64_t p4, uint64_t p5, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) uint64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) uint64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) uint8_t t34;
  _Alignas(8) uint64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) uint8_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) uint8_t t48;
  _Alignas(8) uint64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) uint8_t t56;
  _Alignas(8) uint64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) uint64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) uint8_t t67;
  _Alignas(8) uint64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) uint64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  t12 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg291b_gg_function_2(t11, t12, base + 0);
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t13) - (uint64_t)(t14)))));
  v7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(v3));
  t17 = ((t15) < (t16));
  if (!(t17)) goto label2;
  {
  t18 = ((uint64_t)(v4));
  t19 = ((int64_t)(v7));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v6));
#ifndef __cplusplus
  extern void (fg291b_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291b_st_i64(t18, t21, t22);
  t23 = ((uint64_t)(v5));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) - (uint64_t)(t28)))));
#ifndef __cplusplus
  extern void (fg291b_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291b_st_i64(t23, t26, t29);
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  }
  goto label1;
label2:;
  v8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(v0));
  t34 = ((t32) < (t33));
  if (!(t34)) goto label5;
  {
  t35 = ((uint64_t)(v2));
  t36 = ((int64_t)(v8));
  v9 = fg291b_gg_function_8(t35, t36, base + 0);
  t37 = ((uint64_t)(v1));
  t38 = ((int64_t)(v8));
  v10 = fg291b_gg_function_4(t37, t38, base + 0);
  v11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t39 = ((int64_t)(v11));
  t40 = ((int64_t)(v3));
  t41 = ((t39) < (t40));
  if (!(t41)) goto label8;
  {
  t42 = ((int64_t)(v9));
  t43 = ((int64_t)(v11));
  t44 = ((int64_t)(((t42) >> ((uint64_t)(t43) & 63))));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t46 = ((int64_t)(((t44) & (t45))));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t48 = ((t46) != (t47));
  if (t48) {
  {
  t49 = ((uint64_t)(v4));
  t50 = ((int64_t)(v11));
  t51 = fg291b_gg_function_4(t49, t50, base + 0);
  t52 = ((int64_t)(t51));
  t53 = ((int64_t)(v9));
  v12 = ((int64_t)(((t52) & (t53))));
  t54 = ((int64_t)(v12));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t56 = ((t54) == (t55));
  if (t56) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  goto fg291b_gg_done;
  }
  }
  t57 = ((uint64_t)(v4));
  t58 = ((int64_t)(v11));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(v12));
#ifndef __cplusplus
  extern void (fg291b_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291b_st_i64(t57, t60, t61);
  t62 = ((int64_t)(v10));
  t63 = ((uint64_t)(v5));
  t64 = ((int64_t)(v11));
  t65 = fg291b_gg_function_4(t63, t64, base + 0);
  t66 = ((int64_t)(t65));
  t67 = ((t62) < (t66));
  if (t67) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  goto fg291b_gg_done;
  }
  }
  t68 = ((uint64_t)(v5));
  t69 = ((int64_t)(v11));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(v10));
#ifndef __cplusplus
  extern void (fg291b_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg291b_st_i64(t68, t71, t72);
  }
  }
  t73 = ((int64_t)(v11));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  }
  goto label7;
label8:;
  t75 = ((int64_t)(v8));
  t76 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  }
  goto label4;
label5:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
fg291b_gg_done:;
  return t0;
}
static int64_t fg291b_gg_function_1(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, uint64_t p6, int64_t p7, int64_t base) {
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
  _Alignas(8) int64_t t24;
  _Alignas(8) uint8_t t25;
  _Alignas(8) uint64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) uint8_t t33;
  _Alignas(8) uint64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) uint8_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) uint64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v7));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg291b_gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg291b_gg_done;
  }
  }
  v8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t12 = ((int64_t)(v8));
  t13 = ((int64_t)(v7));
  t14 = ((t12) < (t13));
  if (!(t14)) goto label2;
  {
  t15 = ((uint64_t)(v4));
  t16 = ((int64_t)(v8));
  t17 = fg291b_gg_function_4(t15, t16, base + 0);
  t18 = ((int64_t)(t17));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t20 = ((t18) > (t19));
  if (t20) {
  {
  t21 = ((uint64_t)(v6));
  t22 = ((int64_t)(v8));
  v9 = fg291b_gg_function_4(t21, t22, base + 0);
  t23 = ((int64_t)(v9));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t25 = ((t23) >= (t24));
  if (t25) {
  {
  t26 = ((uint64_t)(v2));
  t27 = ((int64_t)(v8));
  t28 = fg291b_gg_function_4(t26, t27, base + 0);
  t29 = ((int64_t)(t28));
  t30 = ((int64_t)(v9));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  t33 = ((t29) != (t32));
  if (t33) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg291b_gg_done;
  }
  }
  t34 = ((uint64_t)(v3));
  t35 = ((int64_t)(v8));
  t36 = fg291b_gg_function_4(t34, t35, base + 0);
  t37 = ((int64_t)(t36));
  t38 = ((uint64_t)(v2));
  t39 = ((int64_t)(v8));
  t40 = fg291b_gg_function_4(t38, t39, base + 0);
  t41 = ((int64_t)(t40));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t37) - (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((uint64_t)(v4));
  t46 = ((int64_t)(v8));
  t47 = fg291b_gg_function_4(t45, t46, base + 0);
  t48 = ((int64_t)(t47));
  t49 = ((t44) > (t48));
  if (t49) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg291b_gg_done;
  }
  }
  }
  }
  }
  }
  t50 = ((int64_t)(v8));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
fg291b_gg_done:;
  return t0;
}
int64_t fg291b_check_continuity(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, uint64_t p6, int64_t p7){return fg291b_gg_function_1(p0, p1, p2, p3, p4, p5, p6, p7, 0);}
int64_t fg291b_check_walk(int64_t p0, uint64_t p1, uint64_t p2, int64_t p3, uint64_t p4, uint64_t p5){return fg291b_gg_function_6(p0, p1, p2, p3, p4, p5, 0);}
