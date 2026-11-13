#pragma once
static __device__ __forceinline__ void fg435_fail(const char*);
static __device__ __forceinline__ int64_t fg435_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg435_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg435_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg435_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg435_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg435_function_7(uint64_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg435_function_4(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg435_function_3(uint64_t p0, uint64_t p1, int64_t p2, int32_t p3, int64_t base);
static __device__ __forceinline__ int64_t fg435_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t base);
static __device__ __forceinline__ void fg435_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg435_div(int64_t a, int64_t b) { if (!b) fg435_fail("division by zero"); if (a==INT64_MIN && b==-1) fg435_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg435_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg435_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg435_udiv(uint64_t a,uint64_t b) { if(!b)fg435_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg435_umod(uint64_t a,uint64_t b) { if(!b)fg435_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg435_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg435_function_7(uint64_t p0, int64_t p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint64_t t4;
  _Alignas(8) uint64_t t5;
  _Alignas(8) uint32_t t6;
  _Alignas(8) uint32_t t7;
  _Alignas(8) uint32_t t8;
  _Alignas(8) uint64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint64_t t12;
  _Alignas(8) uint32_t t13;
  _Alignas(8) uint32_t t14;
  _Alignas(8) uint32_t t15;
  _Alignas(8) uint64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) uint64_t t19;
  _Alignas(8) uint32_t t20;
  _Alignas(8) uint64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint64_t t24;
  _Alignas(8) uint32_t t25;
  _Alignas(8) uint64_t t26;
  _Alignas(8) uint64_t t27;
  _Alignas(8) uint32_t t28;
  _Alignas(8) uint32_t t29;
  _Alignas(8) uint32_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) uint32_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint32_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) uint32_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) uint64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) uint32_t v4;
  _Alignas(8) uint32_t v5;
  _Alignas(8) uint32_t v6;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((int64_t)(((int64_t)UINT64_C(0))));
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t4 = ((uint64_t)(v0));
  t5 = ((uint64_t)(((uint64_t)(v1))));
#ifndef __cplusplus
  extern uint32_t (fg435_ld_u8)(uint64_t, uint64_t);
#endif
  t6 = fg435_ld_u8(t4, t5);
  t7 = ((uint32_t)(t6));
  t8 = ((uint32_t)(((int64_t)UINT64_C(63))));
  v2 = ((int64_t)(((int64_t)(((t7) & (t8))))));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))))));
#ifndef __cplusplus
  extern uint32_t (fg435_ld_u8)(uint64_t, uint64_t);
#endif
  t13 = fg435_ld_u8(t9, t12);
  t14 = ((uint32_t)(t13));
  t15 = ((uint32_t)(((int64_t)UINT64_C(63))));
  v3 = ((int64_t)(((int64_t)(((t14) & (t15))))));
  }
  } else {
  {
  t16 = ((uint64_t)(v0));
  t17 = ((int64_t)(v1));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))))));
#ifndef __cplusplus
  extern uint32_t (fg435_ld_u8)(uint64_t, uint64_t);
#endif
  t20 = fg435_ld_u8(t16, t19);
  v4 = ((uint32_t)(t20));
  t21 = ((uint64_t)(v0));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t22) - (uint64_t)(t23)))))));
#ifndef __cplusplus
  extern uint32_t (fg435_ld_u8)(uint64_t, uint64_t);
#endif
  t25 = fg435_ld_u8(t21, t24);
  v5 = ((uint32_t)(t25));
  t26 = ((uint64_t)(v0));
  t27 = ((uint64_t)(((uint64_t)(v1))));
#ifndef __cplusplus
  extern uint32_t (fg435_ld_u8)(uint64_t, uint64_t);
#endif
  t28 = fg435_ld_u8(t26, t27);
  v6 = ((uint32_t)(t28));
  t29 = ((uint32_t)(v4));
  t30 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t31 = ((int64_t)(((int64_t)(((t29) & (t30))))));
  t32 = ((uint32_t)(v5));
  t33 = ((int64_t)(((int64_t)UINT64_C(6))));
  t34 = ((int64_t)(((int64_t)(((t32) >> ((uint64_t)(t33) & 31))))));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t36 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t34) << ((uint64_t)(t35) & 63)))))));
  v2 = ((int64_t)(((t31) | (t36))));
  t37 = ((uint32_t)(v4));
  t38 = ((int64_t)(((int64_t)UINT64_C(4))));
  t39 = ((int64_t)(((int64_t)(((t37) >> ((uint64_t)(t38) & 31))))));
  t40 = ((uint32_t)(v6));
  t41 = ((int64_t)(((int64_t)UINT64_C(6))));
  t42 = ((int64_t)(((int64_t)(((t40) >> ((uint64_t)(t41) & 31))))));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t44 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t42) << ((uint64_t)(t43) & 63)))))));
  v3 = ((int64_t)(((t39) | (t44))));
  }
  }
  t45 = ((int64_t)(v2));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) << ((uint64_t)(t46) & 63)))));
  t48 = ((int64_t)(v3));
  t0 = ((int64_t)(((t47) | (t48))));
  goto fg435_done;
  }
fg435_done:;
  return t0;
}
static __device__ __forceinline__ void fg435_function_4(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg435_fail)(int64_t);
#endif
  fg435_fail(t2);
  }
  }
  }
}
static __device__ __forceinline__ void fg435_function_3(uint64_t p0, uint64_t p1, int64_t p2, int32_t p3, int64_t base) {
  _Alignas(8) int32_t t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) uint8_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int32_t t4;
  _Alignas(8) int32_t t5;
  _Alignas(8) uint8_t t6;
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
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) uint64_t t26;
  _Alignas(8) uint64_t t27;
  _Alignas(8) float t28;
  _Alignas(8) uint64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) uint64_t t32;
  _Alignas(8) float t33;
  _Alignas(8) uint64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) uint64_t t45;
  _Alignas(8) uint64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) uint64_t t49;
  _Alignas(8) uint64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) uint64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) uint64_t t55;
  _Alignas(8) uint64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) float t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) float t63;
  _Alignas(8) float t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) float t67;
  _Alignas(8) float t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) float t71;
  _Alignas(8) float t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) float t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) uint8_t t78;
  _Alignas(8) uint64_t t79;
  _Alignas(8) uint64_t t80;
  _Alignas(8) uint32_t t81;
  _Alignas(8) uint32_t t82;
  _Alignas(8) uint32_t t83;
  _Alignas(8) uint32_t t84;
  _Alignas(8) uint32_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) uint64_t t96;
  _Alignas(8) uint64_t t97;
  _Alignas(8) float t98;
  _Alignas(8) float t99;
  _Alignas(8) float t100;
  _Alignas(8) float t101;
  _Alignas(8) float t102;
  _Alignas(8) uint64_t t103;
  _Alignas(8) uint64_t t104;
  _Alignas(8) float t105;
  _Alignas(8) float t106;
  _Alignas(8) float t107;
  _Alignas(8) float t108;
  _Alignas(8) float t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int32_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  _Alignas(8) uint64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) float v15;
  _Alignas(8) float v16;
  _Alignas(8) float v17;
  _Alignas(8) float v18;
  _Alignas(8) int64_t v19;
  _Alignas(8) uint32_t v20;
  _Alignas(8) uint32_t v21;
  _Alignas(8) uint32_t v22;
  _Alignas(8) int64_t v23;
  _Alignas(8) int64_t v24;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int32_t)(v3));
  t1 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int32_t)(v3));
  t5 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg435_function_4(t6, t7, base + 0);
  t8 = ((int64_t)(((int64_t)(v3))));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v4 = ((int64_t)(((int64_t)fg435_div(t8, t9))));
  t10 = ((int64_t)(((int64_t)(v3))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v5 = ((int64_t)(((int64_t)fg435_mod(t10, t11))));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t13 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  v7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t21)))));
  t23 = ((int64_t)(v7));
  t24 = ((int64_t)(v5));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(v0));
  t27 = ((uint64_t)(((uint64_t)(v8))));
#ifndef __cplusplus
  extern float (fg435_ld_half)(uint64_t, uint64_t);
#endif
  t28 = fg435_ld_half(t26, t27);
  v10 = ((float)(t28));
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v8));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern float (fg435_ld_half)(uint64_t, uint64_t);
#endif
  t33 = fg435_ld_half(t29, t32);
  v11 = ((float)(t33));
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t39 = ((int64_t)(v4));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t40)))));
  t42 = ((int64_t)(v7));
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t44)))))));
  v12 = ((uint64_t)(((uint64_t)((uint64_t)(t34) + (uint64_t)(t45)))));
  t46 = ((uint64_t)(v0));
  t47 = ((int64_t)(v8));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))))));
  t50 = ((uint64_t)(((uint64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t51 = ((int64_t)(v6));
  v13 = fg435_function_7(t50, t51, base + 0);
  t52 = ((uint64_t)(v0));
  t53 = ((int64_t)(v8));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t55 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))))));
  t56 = ((uint64_t)(((uint64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(v6));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  v14 = fg435_function_7(t56, t59, base + 0);
  t60 = ((float)(v10));
  t61 = ((int64_t)(v13));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t63 = ((float)(((float)(((t61) >> ((uint64_t)(t62) & 63))))));
  v15 = ((float)(((t60) * (t63))));
  t64 = ((float)(v11));
  t65 = ((int64_t)(v13));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  t67 = ((float)(((float)(((t65) & (t66))))));
  v16 = ((float)(((t64) * (t67))));
  t68 = ((float)(v10));
  t69 = ((int64_t)(v14));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t71 = ((float)(((float)(((t69) >> ((uint64_t)(t70) & 63))))));
  v17 = ((float)(((t68) * (t71))));
  t72 = ((float)(v11));
  t73 = ((int64_t)(v14));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  t75 = ((float)(((float)(((t73) & (t74))))));
  v18 = ((float)(((t72) * (t75))));
  v19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t76 = ((int64_t)(v19));
  t77 = ((int64_t)(v7));
  t78 = ((t76) < (t77));
  if (!(t78)) goto label2;
  {
  t79 = ((uint64_t)(v12));
  t80 = ((uint64_t)(((uint64_t)(v19))));
#ifndef __cplusplus
  extern uint32_t (fg435_ld_u8)(uint64_t, uint64_t);
#endif
  t81 = fg435_ld_u8(t79, t80);
  v20 = ((uint32_t)(t81));
  t82 = ((uint32_t)(v20));
  t83 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  v21 = ((uint32_t)(((t82) & (t83))));
  t84 = ((uint32_t)(v20));
  t85 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  v22 = ((uint32_t)(((t84) >> ((uint64_t)(t85) & 31))));
  t86 = ((int64_t)(v9));
  t87 = ((int64_t)(v19));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  t90 = ((int64_t)(v9));
  t91 = ((int64_t)(v19));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t91)))));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  t95 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t96 = ((uint64_t)(v1));
  t97 = ((uint64_t)(((uint64_t)(v23))));
  t98 = ((float)(v15));
  t99 = ((float)(((float)(v21))));
  t100 = ((float)(((t98) * (t99))));
  t101 = ((float)(v16));
  t102 = ((float)(((t100) - (t101))));
#ifndef __cplusplus
  extern void (fg435_st_f32)(uint64_t, uint64_t, float);
#endif
  fg435_st_f32(t96, t97, t102);
  t103 = ((uint64_t)(v1));
  t104 = ((uint64_t)(((uint64_t)(v24))));
  t105 = ((float)(v17));
  t106 = ((float)(((float)(v22))));
  t107 = ((float)(((t105) * (t106))));
  t108 = ((float)(v18));
  t109 = ((float)(((t107) - (t108))));
#ifndef __cplusplus
  extern void (fg435_st_f32)(uint64_t, uint64_t, float);
#endif
  fg435_st_f32(t103, t104, t109);
  t110 = ((int64_t)(v19));
  t111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  }
  goto label1;
label2:;
  }
}
static __device__ __forceinline__ int64_t fg435_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t base) {
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
  _Alignas(8) uint8_t t18;
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
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) uint64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) uint64_t t51;
  _Alignas(8) uint64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) uint64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint8_t t66;
  _Alignas(8) uint64_t t67;
  _Alignas(8) uint64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int32_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
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
  _Alignas(8) int64_t v19;
  _Alignas(8) int64_t v20;
  _Alignas(8) int64_t v21;
  _Alignas(8) int64_t v22;
  _Alignas(8) int64_t v23;
  _Alignas(8) int64_t v24;
  _Alignas(8) int64_t v25;
  _Alignas(8) int64_t v26;
  _Alignas(8) uint64_t v27;
  _Alignas(8) uint64_t v28;
  _Alignas(8) int64_t v29;
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
  v12 = p12;
  v13 = p13;
  v14 = p14;
  v15 = p15;
  v16 = p16;
  v17 = p17;
  v18 = p18;
  v19 = p19;
  v20 = p20;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg435_done;
  }
  }
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  v21 = ((int64_t)(((int64_t)fg435_div(t12, t13))));
  v22 = ((int64_t)(v17));
label1:;
  t14 = ((int64_t)(v22));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((t14) < (t17));
  if (!(t18)) goto label2;
  {
  t19 = ((int64_t)(v22));
  t20 = ((int64_t)(v5));
  v23 = ((int64_t)(((int64_t)fg435_mod(t19, t20))));
  t21 = ((int64_t)(v22));
  t22 = ((int64_t)(v5));
  v24 = ((int64_t)(((int64_t)fg435_div(t21, t22))));
  v25 = ((int64_t)(v15));
  t23 = ((uint64_t)(v1));
  t24 = ((int64_t)(v25));
  t25 = ((int64_t)(v12));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(v23));
  t28 = ((int64_t)(v13));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t29)))));
  t31 = ((int64_t)(v24));
  t32 = ((int64_t)(v14));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))))));
#ifndef __cplusplus
  extern int64_t (fg435_ld_i32)(uint64_t, uint64_t);
#endif
  t37 = fg435_ld_i32(t23, t36);
  v26 = ((int64_t)(t37));
  t38 = ((uint64_t)(v2));
  t39 = ((int64_t)(v25));
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(v23));
  t43 = ((int64_t)(v7));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t44)))));
  t46 = ((int64_t)(v24));
  t47 = ((int64_t)(v8));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t51 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))))));
  v27 = ((uint64_t)(((uint64_t)((uint64_t)(t38) + (uint64_t)(t51)))));
  t52 = ((uint64_t)(v0));
  t53 = ((int64_t)(v26));
  t54 = ((int64_t)(v9));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(v23));
  t57 = ((int64_t)(v10));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t58)))));
  t60 = ((int64_t)(v24));
  t61 = ((int64_t)(v11));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t62)))))));
  v28 = ((uint64_t)(((uint64_t)((uint64_t)(t52) + (uint64_t)(t63)))));
  v29 = ((int64_t)(v16));
label4:;
  t64 = ((int64_t)(v29));
  t65 = ((int64_t)(v21));
  t66 = ((t64) < (t65));
  if (!(t66)) goto label5;
  {
  t67 = ((uint64_t)(v28));
  t68 = ((uint64_t)(v27));
  t69 = ((int64_t)(v29));
  t70 = ((int32_t)(((int32_t)(v18))));
  fg435_function_3(t67, t68, t69, t70, base + 0);
  t71 = ((int64_t)(v29));
  t72 = ((int64_t)(v19));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  }
  goto label4;
label5:;
  t73 = ((int64_t)(v22));
  t74 = ((int64_t)(v20));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg435_done;
  }
fg435_done:;
  return t0;
}
