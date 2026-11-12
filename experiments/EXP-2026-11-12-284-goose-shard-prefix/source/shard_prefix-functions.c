static void fg284p_gg_fail(const char*);
static int64_t fg284p_gg_index(int64_t i, int64_t len);
static int64_t fg284p_gg_div(int64_t a, int64_t b);
static int64_t fg284p_gg_mod(int64_t a, int64_t b);
static uint64_t fg284p_gg_udiv(uint64_t a,uint64_t b);
static uint64_t fg284p_gg_umod(uint64_t a,uint64_t b);
static double fg284p_gg_double_bits(uint64_t bits);
static int64_t fg284p_gg_function_5(int64_t p0, int64_t base);
static int64_t fg284p_gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t base);
static int64_t fg284p_gg_function_6(uint64_t p0, int64_t p1, int64_t base);
static int64_t fg284p_gg_function_4(int64_t p0, int64_t p1, int64_t base);
static void fg284p_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static int64_t fg284p_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base);
static void fg284p_gg_fail(const char*){__builtin_trap();}
static int64_t fg284p_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg284p_gg_fail("array index out of bounds"); return i; }
static int64_t fg284p_gg_div(int64_t a, int64_t b) { if (!b) fg284p_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg284p_gg_fail("division overflow"); return a/b; }
static int64_t fg284p_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg284p_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static uint64_t fg284p_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg284p_gg_fail("division by zero");return a/b; }
static uint64_t fg284p_gg_umod(uint64_t a,uint64_t b) { if(!b)fg284p_gg_fail("division by zero");return a%b; }
static double fg284p_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static int64_t fg284p_gg_function_5(int64_t p0, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  v0 = p0;
  {
  v1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v2 = ((int64_t)(v0));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t3 = ((t1) >= (t2));
  if (!(t3)) goto label2;
  {
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  v2 = ((int64_t)(((int64_t)fg284p_gg_div(t4, t5))));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v1 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v1));
  }
  return t0;
}
static int64_t fg284p_gg_function_8(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint8_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) uint8_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) uint8_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) uint8_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) uint8_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) uint8_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) uint8_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) uint8_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) uint8_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) uint8_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) int64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) uint8_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) int64_t t101;
  _Alignas(8) int64_t t102;
  _Alignas(8) int64_t t103;
  _Alignas(8) int64_t t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) uint8_t t106;
  _Alignas(8) int64_t t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) uint8_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) uint8_t t112;
  _Alignas(8) int64_t t113;
  _Alignas(8) int64_t t114;
  _Alignas(8) uint8_t t115;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
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
  {
  t1 = ((int64_t)(v1));
  t2 = fg284p_gg_function_5(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t5 = ((t3) > (t4));
  if (t5) {
  {
  t6 = ((int64_t)(v1));
  v3 = fg284p_gg_function_5(t6, base + 0);
  }
  } else {
  {
  v3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  }
  }
  t7 = ((int64_t)(v2));
  t8 = fg284p_gg_function_5(t7, base + 0);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v2));
  v4 = fg284p_gg_function_5(t12, base + 0);
  }
  } else {
  {
  v4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  }
  }
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t14 = ((int64_t)(v3));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t23 = ((t21) < (t22));
  t24 = t23;
  if (!t24) {
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(v5));
  t24 = ((t25) >= (t26));
  }
  t27 = t24;
  if (t27) {
  {
  t28 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t28)))));
  goto fg284p_gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(45))))));
  goto fg284p_gg_done;
  }
  }
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t34 = ((int64_t)(v3));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
  t36 = ((t32) < (t35));
  if (t36) {
  {
  t37 = ((int64_t)(v0));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t37) - (uint64_t)(t38)))));
  t39 = ((int64_t)(v3));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) - (uint64_t)(t40)))));
  t42 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t41) - (uint64_t)(t42)))));
  v8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(v7));
  t45 = ((t43) < (t44));
  if (!(t45)) goto label2;
  {
  t46 = ((int64_t)(v8));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  }
  goto label1;
label2:;
  t50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t51 = ((int64_t)(v1));
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(((int64_t)fg284p_gg_div(t51, t52))));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t55 = ((int64_t)(((int64_t)fg284p_gg_mod(t53, t54))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t55)))));
  goto fg284p_gg_done;
  }
  }
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t57 = ((int64_t)(v3));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t58 = ((int64_t)(v0));
  t59 = ((int64_t)(v10));
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  t62 = ((t58) < (t61));
  if (t62) {
  {
  t63 = ((int64_t)(v0));
  t64 = ((int64_t)(v10));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t63) - (uint64_t)(t64)))));
  t65 = ((int64_t)(v11));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t67 = ((t65) == (t66));
  if (t67) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(45))))));
  goto fg284p_gg_done;
  }
  }
  t68 = ((int64_t)(v11));
  t69 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t70 = ((t68) == (t69));
  if (t70) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(111))))));
  goto fg284p_gg_done;
  }
  }
  t71 = ((int64_t)(v11));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(102))))));
  goto fg284p_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(45))))));
  goto fg284p_gg_done;
  }
  }
  t74 = ((int64_t)(v10));
  t75 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(v12));
  t78 = ((int64_t)(v4));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((t76) < (t79));
  if (t80) {
  {
  t81 = ((int64_t)(v0));
  t82 = ((int64_t)(v12));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t81) - (uint64_t)(t82)))));
  t83 = ((int64_t)(v4));
  t84 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) - (uint64_t)(t84)))));
  t86 = ((int64_t)(v13));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t85) - (uint64_t)(t86)))));
  v15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t87 = ((int64_t)(v16));
  t88 = ((int64_t)(v14));
  t89 = ((t87) < (t88));
  if (!(t89)) goto label5;
  {
  t90 = ((int64_t)(v15));
  t91 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t92 = ((int64_t)(v16));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  }
  goto label4;
label5:;
  t94 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t95 = ((int64_t)(v2));
  t96 = ((int64_t)(v15));
  t97 = ((int64_t)(((int64_t)fg284p_gg_div(t95, t96))));
  t98 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t99 = ((int64_t)(((int64_t)fg284p_gg_mod(t97, t98))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t94) + (uint64_t)(t99)))));
  goto fg284p_gg_done;
  }
  }
  t100 = ((int64_t)(v12));
  t101 = ((int64_t)(v4));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t102 = ((int64_t)(v0));
  t103 = ((int64_t)(v17));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t102) - (uint64_t)(t103)))));
  t104 = ((int64_t)(v18));
  t105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t106 = ((t104) == (t105));
  if (t106) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(46))))));
  goto fg284p_gg_done;
  }
  }
  t107 = ((int64_t)(v18));
  t108 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t109 = ((t107) == (t108));
  if (t109) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(103))))));
  goto fg284p_gg_done;
  }
  }
  t110 = ((int64_t)(v18));
  t111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t112 = ((t110) == (t111));
  if (t112) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(103))))));
  goto fg284p_gg_done;
  }
  }
  t113 = ((int64_t)(v18));
  t114 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t115 = ((t113) == (t114));
  if (t115) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(117))))));
  goto fg284p_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(102))))));
  goto fg284p_gg_done;
  }
fg284p_gg_done:;
  return t0;
}
static int64_t fg284p_gg_function_6(uint64_t p0, int64_t p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern int64_t (fg284p_ld_u8)(uint64_t, int64_t);
#endif
  t3 = fg284p_ld_u8(t1, t2);
  t0 = ((int64_t)(t3));
  goto fg284p_gg_done;
  }
fg284p_gg_done:;
  return t0;
}
static int64_t fg284p_gg_function_4(int64_t p0, int64_t p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint8_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = fg284p_gg_function_5(t1, base + 0);
  t3 = ((int64_t)(t2));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t5 = ((t3) > (t4));
  if (t5) {
  {
  t6 = ((int64_t)(v0));
  v2 = fg284p_gg_function_5(t6, base + 0);
  }
  } else {
  {
  v2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  }
  }
  t7 = ((int64_t)(v1));
  t8 = fg284p_gg_function_5(t7, base + 0);
  t9 = ((int64_t)(t8));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v1));
  v3 = fg284p_gg_function_5(t12, base + 0);
  }
  } else {
  {
  v3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  }
  }
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  }
  return t0;
}
static void fg284p_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg284p_fail)(int64_t);
#endif
  fg284p_fail(t2);
  }
  }
  }
}
static int64_t fg284p_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) uint8_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
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
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint8_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
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
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((t9) > (t10));
  }
  t11 = t8;
  t12 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg284p_gg_function_2(t11, t12, base + 0);
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(v3));
  v5 = fg284p_gg_function_4(t15, t16, base + 0);
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(v5));
  t19 = ((t17) <= (t18));
  if (t19) {
  {
  t20 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t20)))));
  goto fg284p_gg_done;
  }
  }
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v5));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t21) - (uint64_t)(t22)))));
  v7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t23 = ((int64_t)(v7));
  t24 = ((int64_t)(v5));
  t25 = ((t23) < (t24));
  if (!(t25)) goto label2;
  {
  t26 = ((uint64_t)(v0));
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(v7));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = fg284p_gg_function_6(t26, t29, base + 0);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(v3));
  t35 = fg284p_gg_function_8(t32, t33, t34, base + 0);
  t36 = ((int64_t)(t35));
  t37 = ((t31) != (t36));
  if (t37) {
  {
  t38 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t38)))));
  goto fg284p_gg_done;
  }
  }
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v6));
  }
fg284p_gg_done:;
  return t0;
}
int64_t fg284p_split_prefix(uint64_t p0, int64_t p1, int64_t p2, int64_t p3){return fg284p_gg_function_1(p0, p1, p2, p3, 0);}
