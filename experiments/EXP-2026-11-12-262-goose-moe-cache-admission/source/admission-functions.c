static void fg262a_gg_fail(const char*);
static int64_t fg262a_gg_index(int64_t i, int64_t len);
static int64_t fg262a_gg_div(int64_t a, int64_t b);
static int64_t fg262a_gg_mod(int64_t a, int64_t b);
static uint64_t fg262a_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg262a_gg_umod(uint64_t a,uint64_t b);
static double fg262a_gg_double_bits(uint64_t bits);
static int64_t fg262a_gg_function_4(uint64_t p0, int64_t p1, int64_t base);
static void fg262a_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static uint8_t fg262a_gg_function_1(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, uint64_t p10, int64_t p11, int64_t base);
static void fg262a_gg_fail(const char*){asm("trap;");}
static int64_t fg262a_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg262a_gg_fail("array index out of bounds"); return i; }
static int64_t fg262a_gg_div(int64_t a, int64_t b) { if (!b) fg262a_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg262a_gg_fail("division overflow"); return a/b; }
static int64_t fg262a_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg262a_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg262a_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg262a_gg_fail("division by zero");return a/b; }
static uint64_t fg262a_gg_umod(uint64_t a,uint64_t b) { if(!b)fg262a_gg_fail("division by zero");return a%b; }
static double fg262a_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg262a_gg_function_4(uint64_t p0, int64_t p1, int64_t base) {
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
  extern int64_t (fg262a_ld_i64)(uint64_t, int64_t);
#endif
  t5 = fg262a_ld_i64(t1, t4);
  t0 = ((int64_t)(t5));
  goto fg262a_gg_done;
  }
fg262a_gg_done:;
  return t0;
}
static void fg262a_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg262a_fail)(int64_t);
#endif
  fg262a_fail(t2);
  }
  }
  }
}
static uint8_t fg262a_gg_function_1(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, uint64_t p10, int64_t p11, int64_t base) {
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
  _Alignas(8) uint64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) uint64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) uint8_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) uint8_t t25;
  _Alignas(8) uint8_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) uint8_t t32;
  _Alignas(8) uint64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) uint8_t t38;
  _Alignas(8) uint8_t t39;
  _Alignas(8) uint64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) uint8_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) uint8_t t53;
  _Alignas(8) uint64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) uint64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) uint64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) uint64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) uint64_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) uint64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) uint64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) uint64_t t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) uint64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) int64_t t101;
  _Alignas(8) int64_t t102;
  _Alignas(8) int64_t t103;
  _Alignas(8) int64_t t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) uint64_t t106;
  _Alignas(8) int64_t t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) uint64_t v5;
  _Alignas(8) uint64_t v6;
  _Alignas(8) uint64_t v7;
  _Alignas(8) uint64_t v8;
  _Alignas(8) uint64_t v9;
  _Alignas(8) uint64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) int64_t v17;
  _Alignas(8) int64_t v18;
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
  t1 = ((int64_t)(v0));
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
  fg262a_gg_function_2(t7, t8, base + 0);
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t9 = ((int64_t)(v13));
  t10 = ((int64_t)(v0));
  t11 = ((t9) < (t10));
  if (!(t11)) goto label2;
  {
  t12 = ((uint64_t)(v1));
  t13 = ((int64_t)(v13));
  v14 = fg262a_gg_function_4(t12, t13, base + 0);
  t14 = ((uint64_t)(v2));
  t15 = ((int64_t)(v13));
  v15 = fg262a_gg_function_4(t14, t15, base + 0);
  t16 = ((uint64_t)(v3));
  t17 = ((int64_t)(v13));
  v16 = fg262a_gg_function_4(t16, t17, base + 0);
  t18 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t18)))));
  t19 = ((int64_t)(v14));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t21 = ((t19) > (t20));
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v15));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t22 = ((t23) >= (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v16));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t26 = ((t27) != (t28));
  }
  t29 = t26;
  if (t29) {
  {
  v18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t30 = ((int64_t)(v18));
  t31 = ((int64_t)(v12));
  t32 = ((t30) < (t31));
  if (!(t32)) goto label5;
  {
  t33 = ((uint64_t)(v6));
  t34 = ((int64_t)(v18));
  t35 = fg262a_gg_function_4(t33, t34, base + 0);
  t36 = ((int64_t)(t35));
  t37 = ((int64_t)(v15));
  t38 = ((t36) == (t37));
  t39 = t38;
  if (t39) {
  t40 = ((uint64_t)(v7));
  t41 = ((int64_t)(v18));
  t42 = fg262a_gg_function_4(t40, t41, base + 0);
  t43 = ((int64_t)(t42));
  t44 = ((int64_t)(v16));
  t39 = ((t43) == (t44));
  }
  t45 = t39;
  if (t45) {
  {
  v17 = ((int64_t)(v18));
  }
  }
  t46 = ((int64_t)(v18));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  }
  goto label4;
label5:;
  t48 = ((int64_t)(v17));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t50 = ((t48) < (t49));
  if (t50) {
  {
  t51 = ((int64_t)(v12));
  t52 = ((int64_t)(v11));
  t53 = ((t51) >= (t52));
  if (t53) {
  {
  t0 = ((uint8_t)UINT64_C(0));
  goto fg262a_gg_done;
  }
  }
  v17 = ((int64_t)(v12));
  t54 = ((uint64_t)(v6));
  t55 = ((int64_t)(v12));
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(v15));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t54, t57, t58);
  t59 = ((uint64_t)(v7));
  t60 = ((int64_t)(v12));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(v16));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t59, t62, t63);
  t64 = ((uint64_t)(v8));
  t65 = ((int64_t)(v12));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t64, t67, t68);
  t69 = ((uint64_t)(v9));
  t70 = ((int64_t)(v12));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t69, t72, t73);
  t74 = ((int64_t)(v12));
  t75 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  }
  }
  t76 = ((uint64_t)(v8));
  t77 = ((int64_t)(v17));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t78)))));
  t80 = ((uint64_t)(v8));
  t81 = ((int64_t)(v17));
  t82 = fg262a_gg_function_4(t80, t81, base + 0);
  t83 = ((int64_t)(t82));
  t84 = ((uint64_t)(v4));
  t85 = ((int64_t)(v13));
  t86 = fg262a_gg_function_4(t84, t85, base + 0);
  t87 = ((int64_t)(t86));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t87)))));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t76, t79, t88);
  t89 = ((uint64_t)(v9));
  t90 = ((int64_t)(v17));
  t91 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((uint64_t)(v9));
  t94 = ((int64_t)(v17));
  t95 = fg262a_gg_function_4(t93, t94, base + 0);
  t96 = ((int64_t)(t95));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t89, t92, t98);
  }
  }
  t99 = ((uint64_t)(v5));
  t100 = ((int64_t)(v13));
  t101 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) * (uint64_t)(t101)))));
  t103 = ((int64_t)(v17));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t99, t102, t103);
  t104 = ((int64_t)(v13));
  t105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  }
  goto label1;
label2:;
  t106 = ((uint64_t)(v10));
  t107 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t108 = ((int64_t)(v12));
#ifndef __cplusplus
  extern void (fg262a_st_i64)(uint64_t, int64_t, int64_t);
#endif
  fg262a_st_i64(t106, t107, t108);
  t0 = ((uint8_t)UINT64_C(1));
  }
fg262a_gg_done:;
  return t0;
}
uint8_t fg262a_run(int64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, uint64_t p10, int64_t p11){return fg262a_gg_function_1(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, 0);}
