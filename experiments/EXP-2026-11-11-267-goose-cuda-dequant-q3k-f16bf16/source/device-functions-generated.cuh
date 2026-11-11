#pragma once
static __device__ __forceinline__ void fg267dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg267dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg267dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg267dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg267dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg267dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg267dd_gg_function_7(int64_t p0, int64_t base);
static __device__ __forceinline__ int64_t fg267dd_gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static __device__ __forceinline__ void fg267dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg267dd_gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg267dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg267dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg267dd_gg_div(int64_t a, int64_t b) { if (!b) fg267dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg267dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg267dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg267dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg267dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg267dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg267dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg267dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg267dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg267dd_gg_function_7(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg267dd_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  }
fg267dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ int64_t fg267dd_gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint32_t t8;
  uint32_t t9;
  uint32_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  uint32_t t18;
  uint32_t t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint32_t t33;
  uint32_t t34;
  uint32_t t35;
  int64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  uint32_t t43;
  uint32_t t44;
  uint32_t t45;
  uint32_t t46;
  uint32_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  uint64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  uint32_t t60;
  uint32_t t61;
  uint32_t t62;
  int64_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  uint64_t t67;
  uint32_t t68;
  uint32_t t69;
  uint32_t t70;
  uint32_t t71;
  uint32_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint64_t t81;
  uint32_t t82;
  uint32_t t83;
  uint32_t t84;
  int64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  uint32_t t92;
  uint32_t t93;
  uint32_t t94;
  uint32_t t95;
  uint32_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t4 = ((uint64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t8 = fg267d_ld_u8(t4, t7);
  t9 = ((uint32_t)(t8));
  t10 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t11 = ((int64_t)(((int64_t)(((t9) & (t10))))));
  t12 = ((uint64_t)(v0));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t18 = fg267d_ld_u8(t12, t17);
  t19 = ((uint32_t)(t18));
  t20 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t21 = ((uint32_t)(((t19) >> ((uint64_t)(t20) & 31))));
  t22 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t23 = ((int64_t)(((int64_t)(((t21) & (t22))))));
  t24 = ((int64_t)(((int64_t)UINT64_C(4))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) << ((uint64_t)(t24) & 63)))));
  v3 = ((int64_t)(((t11) | (t25))));
  }
  } else {
  {
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t28 = ((t26) < (t27));
  if (t28) {
  {
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v2));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t33 = fg267d_ld_u8(t29, t32);
  t34 = ((uint32_t)(t33));
  t35 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t36 = ((int64_t)(((int64_t)(((t34) & (t35))))));
  t37 = ((uint64_t)(v0));
  t38 = ((int64_t)(v1));
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t43 = fg267d_ld_u8(t37, t42);
  t44 = ((uint32_t)(t43));
  t45 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(2))))));
  t46 = ((uint32_t)(((t44) >> ((uint64_t)(t45) & 31))));
  t47 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t48 = ((int64_t)(((int64_t)(((t46) & (t47))))));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) << ((uint64_t)(t49) & 63)))));
  v3 = ((int64_t)(((t36) | (t50))));
  }
  } else {
  {
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t53 = ((t51) < (t52));
  if (t53) {
  {
  t54 = ((uint64_t)(v0));
  t55 = ((int64_t)(v1));
  t56 = ((int64_t)(v2));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t59 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t57) - (uint64_t)(t58)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t60 = fg267d_ld_u8(t54, t59);
  t61 = ((uint32_t)(t60));
  t62 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t63 = ((int64_t)(((int64_t)(((t61) >> ((uint64_t)(t62) & 31))))));
  t64 = ((uint64_t)(v0));
  t65 = ((int64_t)(v1));
  t66 = ((int64_t)(v2));
  t67 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t68 = fg267d_ld_u8(t64, t67);
  t69 = ((uint32_t)(t68));
  t70 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t71 = ((uint32_t)(((t69) >> ((uint64_t)(t70) & 31))));
  t72 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t73 = ((int64_t)(((int64_t)(((t71) & (t72))))));
  t74 = ((int64_t)(((int64_t)UINT64_C(4))));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) << ((uint64_t)(t74) & 63)))));
  v3 = ((int64_t)(((t63) | (t75))));
  }
  } else {
  {
  t76 = ((uint64_t)(v0));
  t77 = ((int64_t)(v1));
  t78 = ((int64_t)(v2));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t81 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t79) - (uint64_t)(t80)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t82 = fg267d_ld_u8(t76, t81);
  t83 = ((uint32_t)(t82));
  t84 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t85 = ((int64_t)(((int64_t)(((t83) >> ((uint64_t)(t84) & 31))))));
  t86 = ((uint64_t)(v0));
  t87 = ((int64_t)(v1));
  t88 = ((int64_t)(v2));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  t90 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t91 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t89) - (uint64_t)(t90)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t92 = fg267d_ld_u8(t86, t91);
  t93 = ((uint32_t)(t92));
  t94 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(6))))));
  t95 = ((uint32_t)(((t93) >> ((uint64_t)(t94) & 31))));
  t96 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t97 = ((int64_t)(((int64_t)(((t95) & (t96))))));
  t98 = ((int64_t)(((int64_t)UINT64_C(4))));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) << ((uint64_t)(t98) & 63)))));
  v3 = ((int64_t)(((t85) | (t99))));
  }
  }
  }
  }
  }
  }
  t0 = ((int64_t)(v3));
  }
fg267dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg267dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern void (fg267d_fail)(int64_t);
#endif
  fg267d_fail(t2);
  }
  }
  }
fg267dd_gg_done:;
}
static __device__ __forceinline__ void fg267dd_gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  float t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  float t56;
  int64_t t57;
  int64_t t58;
  float t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  uint32_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  uint64_t t88;
  uint32_t t89;
  uint32_t t90;
  uint32_t t91;
  uint32_t t92;
  uint32_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint64_t t105;
  float t106;
  float t107;
  float t108;
  int64_t t109;
  int64_t t110;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  int64_t v15;
  float v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  uint32_t v20;
  int64_t v21;
  int64_t v22;
  uint32_t v23;
  uint32_t v24;
  uint32_t v25;
  int64_t v26;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg267dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(4))));
  v4 = ((int64_t)(((int64_t)fg267dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  v5 = ((int64_t)(((int64_t)fg267dd_gg_div(t10, t11))));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  v6 = ((int64_t)(((int64_t)fg267dd_gg_mod(t12, t13))));
  t14 = ((int64_t)(((int64_t)UINT64_C(16))));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)(((int64_t)fg267dd_gg_mod(t18, t19))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t20)))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t21)))));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = ((int64_t)(((int64_t)fg267dd_gg_div(t22, t23))));
  t24 = ((int64_t)(v5));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t24) - (uint64_t)(t27)))));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((int64_t)(v8));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(v9));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t28) << ((uint64_t)(t33) & 63)))));
  t34 = ((int64_t)(((int64_t)UINT64_C(8))));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(v6));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = ((int64_t)(v9));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(110))))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t46 = ((uint64_t)(v0));
  t47 = ((int64_t)(v13));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(108))))));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))))));
#ifndef __cplusplus
  extern float (fg267d_ld_half)(uint64_t, uint64_t);
#endif
  t50 = fg267d_ld_half(t46, t49);
  v14 = ((float)(t50));
  t51 = ((uint64_t)(v0));
  t52 = ((int64_t)(v13));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(96))))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v11));
  v15 = fg267dd_gg_function_5(t51, t54, t55, base + 0);
  t56 = ((float)(v14));
  t57 = ((int64_t)(v15));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t59 = ((float)(((float)(((int64_t)((uint64_t)(t57) - (uint64_t)(t58)))))));
  v16 = ((float)(((t56) * (t59))));
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)UINT64_C(128))));
  t64 = ((int64_t)(v8));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t65)))));
  t67 = ((int64_t)(((int64_t)UINT64_C(32))));
  t68 = ((int64_t)(v9));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t70 = ((int64_t)(v13));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)UINT64_C(32))));
  t74 = ((int64_t)(v8));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t75)))));
  v19 = ((int64_t)(v13));
  v20 = ((uint32_t)(((uint32_t)(v12))));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t76 = ((int64_t)(v21));
  t77 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t76) < (t77)))) goto label2;
  {
  t78 = ((int64_t)(v7));
  t79 = ((int64_t)(v21));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t80 = ((uint64_t)(v0));
  t81 = ((int64_t)(v18));
  t82 = ((int64_t)(v22));
  t83 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t84 = fg267d_ld_u8(t80, t83);
  v23 = ((uint32_t)(t84));
  t85 = ((uint64_t)(v0));
  t86 = ((int64_t)(v19));
  t87 = ((int64_t)(v22));
  t88 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t89 = fg267d_ld_u8(t85, t88);
  v24 = ((uint32_t)(t89));
  t90 = ((uint32_t)(v23));
  t91 = ((uint32_t)(v20));
  t92 = ((uint32_t)(((t90) >> ((uint64_t)(t91) & 31))));
  t93 = ((uint32_t)(((int64_t)UINT64_C(3))));
  v25 = ((uint32_t)(((t92) & (t93))));
  t94 = ((int64_t)(((int64_t)(v25))));
  t95 = ((int64_t)(((int64_t)(v24))));
  t96 = ((int64_t)(v10));
  t97 = ((int64_t)(((t95) & (t96))));
  t98 = fg267dd_gg_function_7(t97, base + 0);
  t99 = ((int64_t)(t98));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t94) - (uint64_t)(t99)))));
  t100 = ((uint64_t)(v1));
  t101 = ((int64_t)(v17));
  t102 = ((int64_t)(v22));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t105 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))))));
  t106 = ((float)(v16));
  t107 = ((float)(((float)(v26))));
  t108 = ((float)(((t106) * (t107))));
#ifndef __cplusplus
  extern void (fg267d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg267d_st_bf16(t100, t105, t108);
  t109 = ((int64_t)(v21));
  t110 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t109) + (uint64_t)(t110)))));
  }
  goto label1;
label2:;
  }
fg267dd_gg_done:;
}
static __device__ __forceinline__ void fg267dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  float t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  float t56;
  int64_t t57;
  int64_t t58;
  float t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  uint32_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  uint64_t t88;
  uint32_t t89;
  uint32_t t90;
  uint32_t t91;
  uint32_t t92;
  uint32_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  uint64_t t105;
  float t106;
  float t107;
  float t108;
  int64_t t109;
  int64_t t110;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  int64_t v15;
  float v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  uint32_t v20;
  int64_t v21;
  int64_t v22;
  uint32_t v23;
  uint32_t v24;
  uint32_t v25;
  int64_t v26;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg267dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(4))));
  v4 = ((int64_t)(((int64_t)fg267dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  v5 = ((int64_t)(((int64_t)fg267dd_gg_div(t10, t11))));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  v6 = ((int64_t)(((int64_t)fg267dd_gg_mod(t12, t13))));
  t14 = ((int64_t)(((int64_t)UINT64_C(16))));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(4))));
  t18 = ((int64_t)(v3));
  t19 = ((int64_t)(((int64_t)UINT64_C(4))));
  t20 = ((int64_t)(((int64_t)fg267dd_gg_mod(t18, t19))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t20)))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t21)))));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = ((int64_t)(((int64_t)fg267dd_gg_div(t22, t23))));
  t24 = ((int64_t)(v5));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t24) - (uint64_t)(t27)))));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((int64_t)(v8));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(v9));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t28) << ((uint64_t)(t33) & 63)))));
  t34 = ((int64_t)(((int64_t)UINT64_C(8))));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(v6));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  t42 = ((int64_t)(((int64_t)UINT64_C(2))));
  t43 = ((int64_t)(v9));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t44 = ((int64_t)(v2));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(110))))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t46 = ((uint64_t)(v0));
  t47 = ((int64_t)(v13));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(108))))));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))))));
#ifndef __cplusplus
  extern float (fg267d_ld_half)(uint64_t, uint64_t);
#endif
  t50 = fg267d_ld_half(t46, t49);
  v14 = ((float)(t50));
  t51 = ((uint64_t)(v0));
  t52 = ((int64_t)(v13));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(96))))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v11));
  v15 = fg267dd_gg_function_5(t51, t54, t55, base + 0);
  t56 = ((float)(v14));
  t57 = ((int64_t)(v15));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t59 = ((float)(((float)(((int64_t)((uint64_t)(t57) - (uint64_t)(t58)))))));
  v16 = ((float)(((t56) * (t59))));
  t60 = ((int64_t)(v2));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)UINT64_C(128))));
  t64 = ((int64_t)(v8));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t65)))));
  t67 = ((int64_t)(((int64_t)UINT64_C(32))));
  t68 = ((int64_t)(v9));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t70 = ((int64_t)(v13));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)UINT64_C(32))));
  t74 = ((int64_t)(v8));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t75)))));
  v19 = ((int64_t)(v13));
  v20 = ((uint32_t)(((uint32_t)(v12))));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t76 = ((int64_t)(v21));
  t77 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t76) < (t77)))) goto label2;
  {
  t78 = ((int64_t)(v7));
  t79 = ((int64_t)(v21));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t78) + (uint64_t)(t79)))));
  t80 = ((uint64_t)(v0));
  t81 = ((int64_t)(v18));
  t82 = ((int64_t)(v22));
  t83 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t84 = fg267d_ld_u8(t80, t83);
  v23 = ((uint32_t)(t84));
  t85 = ((uint64_t)(v0));
  t86 = ((int64_t)(v19));
  t87 = ((int64_t)(v22));
  t88 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))))));
#ifndef __cplusplus
  extern uint32_t (fg267d_ld_u8)(uint64_t, uint64_t);
#endif
  t89 = fg267d_ld_u8(t85, t88);
  v24 = ((uint32_t)(t89));
  t90 = ((uint32_t)(v23));
  t91 = ((uint32_t)(v20));
  t92 = ((uint32_t)(((t90) >> ((uint64_t)(t91) & 31))));
  t93 = ((uint32_t)(((int64_t)UINT64_C(3))));
  v25 = ((uint32_t)(((t92) & (t93))));
  t94 = ((int64_t)(((int64_t)(v25))));
  t95 = ((int64_t)(((int64_t)(v24))));
  t96 = ((int64_t)(v10));
  t97 = ((int64_t)(((t95) & (t96))));
  t98 = fg267dd_gg_function_7(t97, base + 0);
  t99 = ((int64_t)(t98));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t94) - (uint64_t)(t99)))));
  t100 = ((uint64_t)(v1));
  t101 = ((int64_t)(v17));
  t102 = ((int64_t)(v22));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t105 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))))));
  t106 = ((float)(v16));
  t107 = ((float)(((float)(v26))));
  t108 = ((float)(((t106) * (t107))));
#ifndef __cplusplus
  extern void (fg267d_st_half)(uint64_t, uint64_t, float);
#endif
  fg267d_st_half(t100, t105, t108);
  t109 = ((int64_t)(v21));
  t110 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t109) + (uint64_t)(t110)))));
  }
  goto label1;
label2:;
  }
fg267dd_gg_done:;
}
