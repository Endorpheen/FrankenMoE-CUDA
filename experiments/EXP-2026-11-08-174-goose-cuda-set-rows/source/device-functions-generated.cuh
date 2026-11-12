#pragma once
static __device__ __forceinline__ void fg176rd_gg_fail(const char*);
static __device__ __forceinline__ fg176rd_gg_slice fg176rd_gg_view(void *p, int64_t n);
static __device__ __forceinline__ int64_t fg176rd_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg176rd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg176rd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg176rd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg176rd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg176rd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ uint32_t fg176rd_gg_function_3(uint32_t p0, fg176rd_gg_type_1 p1, int64_t base);
static __device__ __forceinline__ uint32_t fg176rd_gg_function_5(uint32_t p0, fg176rd_gg_type_1 p1, int64_t base);
static __device__ __forceinline__ fg176rd_gg_type_8 fg176rd_gg_function_2(uint32_t p0, fg176rd_gg_type_1 p1, fg176rd_gg_type_1 p2, fg176rd_gg_type_1 p3, int64_t base);
static __device__ __forceinline__ void fg176rd_gg_function_16(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, fg176rd_gg_type_1 p4, fg176rd_gg_type_1 p5, fg176rd_gg_type_1 p6, fg176rd_gg_type_1 p7, fg176rd_gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base);
static __device__ __forceinline__ void fg176rd_gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg176rd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, fg176rd_gg_type_1 p4, fg176rd_gg_type_1 p5, fg176rd_gg_type_1 p6, fg176rd_gg_type_1 p7, fg176rd_gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base);
static __device__ __forceinline__ void fg176rd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ fg176rd_gg_slice fg176rd_gg_view(void *p, int64_t n) { fg176rd_gg_slice v; v.data=p; v.len=n; return v; }
static __device__ __forceinline__ int64_t fg176rd_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg176rd_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg176rd_gg_div(int64_t a, int64_t b) { if (!b) fg176rd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg176rd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg176rd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg176rd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg176rd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg176rd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg176rd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg176rd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg176rd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ uint32_t fg176rd_gg_function_3(uint32_t p0, fg176rd_gg_type_1 p1, int64_t base) {
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
  uint32_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint32_t v0;
  fg176rd_gg_type_1 v1;
  uint32_t v2;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint32_t)(v0));
  t2 = ((uint32_t)((v1).f0));
  t3 = fg176r_mulhi(t1, t2);
  v2 = ((uint32_t)(t3));
  t4 = ((uint64_t)(((uint64_t)(v2))));
  t5 = ((uint64_t)(((uint64_t)(v0))));
  t6 = ((uint64_t)(((uint64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t7 = ((uint64_t)(((uint64_t)((v1).f1))));
  t0 = ((uint32_t)(((uint32_t)(((t6) >> ((uint64_t)(t7) & 63))))));
  goto fg176rd_gg_done;
  }
fg176rd_gg_done:;
  return t0;
}
static __device__ __forceinline__ uint32_t fg176rd_gg_function_5(uint32_t p0, fg176rd_gg_type_1 p1, int64_t base) {
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
  fg176rd_gg_type_1 t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  uint32_t t7;
  uint32_t v0;
  fg176rd_gg_type_1 v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint32_t)(v0));
  t2 = ((uint32_t)(v0));
  t3 = v1;
  t4 = fg176rd_gg_function_3(t2, t3, base + 0);
  t5 = ((uint32_t)(t4));
  t6 = ((uint32_t)((v1).f2));
  t7 = ((uint32_t)(((uint32_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t0 = ((uint32_t)(((uint32_t)((uint64_t)(t1) - (uint64_t)(t7)))));
  goto fg176rd_gg_done;
  }
fg176rd_gg_done:;
  return t0;
}
static __device__ __forceinline__ fg176rd_gg_type_8 fg176rd_gg_function_2(uint32_t p0, fg176rd_gg_type_1 p1, fg176rd_gg_type_1 p2, fg176rd_gg_type_1 p3, int64_t base) {
  fg176rd_gg_type_8 t0;
  uint32_t t1;
  fg176rd_gg_type_1 t2;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  uint32_t t7;
  fg176rd_gg_type_1 t8;
  uint32_t t9;
  uint32_t t10;
  uint32_t t11;
  uint32_t t12;
  uint32_t t13;
  fg176rd_gg_type_1 t14;
  uint32_t t15;
  uint32_t t16;
  uint32_t t17;
  uint32_t t18;
  uint32_t v0;
  fg176rd_gg_type_1 v1;
  fg176rd_gg_type_1 v2;
  fg176rd_gg_type_1 v3;
  uint32_t v4;
  uint32_t v5;
  uint32_t v6;
  uint32_t v7;
  uint32_t v8;
  uint32_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint32_t)(v0));
  t2 = v1;
  v4 = fg176rd_gg_function_3(t1, t2, base + 0);
  t3 = ((uint32_t)(v0));
  t4 = ((uint32_t)(v4));
  t5 = ((uint32_t)((v1).f2));
  t6 = ((uint32_t)(((uint32_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  v5 = ((uint32_t)(((uint32_t)((uint64_t)(t3) - (uint64_t)(t6)))));
  t7 = ((uint32_t)(v4));
  t8 = v2;
  v6 = fg176rd_gg_function_3(t7, t8, base + 0);
  t9 = ((uint32_t)(v4));
  t10 = ((uint32_t)(v6));
  t11 = ((uint32_t)((v2).f2));
  t12 = ((uint32_t)(((uint32_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  v7 = ((uint32_t)(((uint32_t)((uint64_t)(t9) - (uint64_t)(t12)))));
  t13 = ((uint32_t)(v6));
  t14 = v3;
  v8 = fg176rd_gg_function_3(t13, t14, base + 0);
  t15 = ((uint32_t)(v6));
  t16 = ((uint32_t)(v8));
  t17 = ((uint32_t)((v3).f2));
  t18 = ((uint32_t)(((uint32_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  v9 = ((uint32_t)(((uint32_t)((uint64_t)(t15) - (uint64_t)(t18)))));
  memset(&(t0), 0, sizeof(t0));
  (t0).f3 = ((int64_t)(((int64_t)(v5))));
  (t0).f4 = ((int64_t)(((int64_t)(v7))));
  (t0).f5 = ((int64_t)(((int64_t)(v9))));
  (t0).f6 = ((int64_t)(((int64_t)(v8))));
  goto fg176rd_gg_done;
  }
fg176rd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg176rd_gg_function_16(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, fg176rd_gg_type_1 p4, fg176rd_gg_type_1 p5, fg176rd_gg_type_1 p6, fg176rd_gg_type_1 p7, fg176rd_gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint32_t t7;
  fg176rd_gg_type_1 t8;
  fg176rd_gg_type_1 t9;
  fg176rd_gg_type_1 t10;
  uint32_t t11;
  fg176rd_gg_type_1 t12;
  uint32_t t13;
  uint32_t t14;
  fg176rd_gg_type_1 t15;
  uint32_t t16;
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
  uint8_t t27;
  uint64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
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
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint64_t t77;
  float t78;
  float t79;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  fg176rd_gg_type_1 v4;
  fg176rd_gg_type_1 v5;
  fg176rd_gg_type_1 v6;
  fg176rd_gg_type_1 v7;
  fg176rd_gg_type_1 v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  uint8_t v19;
  uint8_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int64_t v24;
  fg176rd_gg_type_8 v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
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
  v21 = p21;
  v22 = p22;
  v23 = p23;
  {
  t0 = ((int64_t)(((int64_t)(v23))));
  t1 = ((int64_t)(((int64_t)(v21))));
  t2 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t3 = ((int64_t)(((int64_t)(v22))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t4 = ((int64_t)(v24));
  t5 = ((int64_t)(v3));
  t6 = ((t4) >= (t5));
  if (t6) {
  {
  goto fg176rd_gg_done;
  }
  }
  t7 = ((uint32_t)(((uint32_t)(v24))));
  t8 = v4;
  t9 = v5;
  t10 = v6;
  v25 = fg176rd_gg_function_2(t7, t8, t9, t10, base + 0);
  v26 = ((int64_t)((v25).f3));
  v27 = ((int64_t)((v25).f4));
  v28 = ((int64_t)((v25).f5));
  v29 = ((int64_t)((v25).f6));
  t11 = ((uint32_t)(((uint32_t)(v29))));
  t12 = v8;
  t13 = fg176rd_gg_function_5(t11, t12, base + 0);
  v30 = ((int64_t)(((int64_t)(t13))));
  t14 = ((uint32_t)(((uint32_t)(v28))));
  t15 = v7;
  t16 = fg176rd_gg_function_5(t14, t15, base + 0);
  v31 = ((int64_t)(((int64_t)(t16))));
  v32 = ((int64_t)(v27));
  fg176r_pdl_sync();
  t17 = ((int64_t)(v32));
  t18 = ((int64_t)(v12));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(v31));
  t21 = ((int64_t)(v13));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t22)))));
  t24 = ((int64_t)(v30));
  t25 = ((int64_t)(v14));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t26)))));
  t27 = v20;
  if (t27) {
  {
  t28 = ((uint64_t)(v1));
  t29 = ((int64_t)(v33));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))))));
  t32 = fg176r_ld_i64(t28, t31);
  v34 = ((int64_t)(t32));
  }
  } else {
  {
  t33 = ((uint64_t)(v1));
  t34 = ((int64_t)(v33));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))))));
  t37 = fg176r_ld_i32(t33, t36);
  v34 = ((int64_t)(t37));
  }
  }
  fg176r_pdl_lc();
  t38 = v19;
  if (t38) {
  {
  t39 = ((int64_t)(v34));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t41 = ((t39) < (t40));
  t42 = t41;
  if (!t42) {
  t43 = ((int64_t)(v34));
  t44 = ((int64_t)(v18));
  t42 = ((t43) >= (t44));
  }
  t45 = t42;
  if (t45) {
  {
  fg176r_bad_index();
  goto fg176rd_gg_done;
  }
  }
  }
  }
  t46 = ((int64_t)(v27));
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v28));
  t50 = ((int64_t)(v10));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))));
  t53 = ((int64_t)(v29));
  t54 = ((int64_t)(v11));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(v26));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t60 = ((int64_t)(v34));
  t61 = ((int64_t)(v15));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(v28));
  t64 = ((int64_t)(v16));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t65)))));
  t67 = ((int64_t)(v29));
  t68 = ((int64_t)(v17));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t71 = ((int64_t)(v26));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))));
  t74 = ((uint64_t)(v2));
  t75 = ((uint64_t)(((uint64_t)(v36))));
  t76 = ((uint64_t)(v0));
  t77 = ((uint64_t)(((uint64_t)(v35))));
  t78 = fg176r_ld_f32(t76, t77);
  t79 = ((float)(t78));
  fg176r_st_f32(t74, t75, t79);
  }
fg176rd_gg_done:;
}
static __device__ __forceinline__ void fg176rd_gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t base) {
  double t0;
  int32_t t1;
  int32_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  uint8_t t14;
  int32_t t15;
  int32_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  uint8_t t21;
  float t22;
  float t23;
  uint64_t t24;
  uint64_t t25;
  float t26;
  int32_t t27;
  int32_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  int32_t t34;
  int32_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  float t41;
  float t42;
  float t43;
  uint64_t t44;
  uint64_t t45;
  float t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  int32_t t52;
  float t53;
  float t54;
  float t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  uint8_t t59;
  int32_t t60;
  int32_t t61;
  uint8_t t62;
  int32_t t63;
  int32_t t64;
  int32_t t65;
  int32_t t66;
  uint32_t t67;
  uint32_t t68;
  uint32_t t69;
  uint32_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  uint32_t t77;
  uint32_t t78;
  uint32_t t79;
  int32_t t80;
  int32_t t81;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  float v4;
  float v5;
  float v6;
  float v7;
  uint64_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  float v14;
  float v15;
  int32_t v16;
  uint64_t v17;
  float v18;
  float v19;
  float v20;
  float v21;
  int32_t v22;
  uint64_t v23;
  uint64_t v24;
  float v25;
  float v26;
  int32_t v27;
  int32_t v28;
  int32_t v29;
  int32_t v30;
  uint32_t v31;
  uint32_t v32;
  uint32_t v33;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  v4 = ((float)(fg176rd_gg_double_bits(UINT64_C(0))));
  t0 = fg176rd_gg_double_bits(UINT64_C(4620693217682128896));
  v5 = ((float)((-(t0))));
  v6 = ((float)(fg176rd_gg_double_bits(UINT64_C(4620974692658839552))));
  v7 = ((float)(fg176rd_gg_double_bits(UINT64_C(4607182418800017408))));
  v8 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v9 = ((int32_t)(((int64_t)UINT64_C(32))));
  v10 = ((int32_t)(((int64_t)UINT64_C(16))));
  v11 = ((int32_t)(((int64_t)UINT64_C(16))));
  v12 = ((int32_t)(((int64_t)UINT64_C(15))));
  v13 = ((int32_t)(((int64_t)UINT64_C(255))));
  v14 = ((float)(v4));
  v15 = ((float)(v4));
  v16 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int32_t)(v16));
  t2 = ((int32_t)(v9));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((uint64_t)(v1));
  t4 = ((uint64_t)(((uint64_t)(((int64_t)(v16))))));
  t5 = ((uint64_t)(v8));
  t6 = ((uint64_t)(((uint64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  v17 = ((uint64_t)(((uint64_t)((uint64_t)(t3) + (uint64_t)(t6)))));
  t7 = ((uint64_t)(v0));
  t8 = ((uint64_t)(v17));
  t9 = fg176r_ld_f32(t7, t8);
  v18 = ((float)(t9));
  t10 = ((float)(v18));
  t11 = fg176r_fabs(t10);
  v19 = ((float)(t11));
  t12 = ((float)(v14));
  t13 = ((float)(v19));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  v14 = ((float)(v19));
  v15 = ((float)(v18));
  }
  }
  t15 = v16;
  t16 = ((int32_t)(((int64_t)UINT64_C(1))));
  v16 = ((int32_t)((uint64_t)(t15) + (uint64_t)(t16)));
  }
  goto label1;
label2:;
  t17 = ((float)(v15));
  t18 = ((float)(v5));
  v20 = ((float)(((t17) / (t18))));
  v21 = ((float)(v4));
  t19 = ((float)(v20));
  t20 = ((float)(v4));
  t21 = ((t19) != (t20));
  if (t21) {
  {
  t22 = ((float)(v7));
  t23 = ((float)(v20));
  v21 = ((float)(((t22) / (t23))));
  }
  }
  t24 = ((uint64_t)(v2));
  t25 = ((uint64_t)(v3));
  t26 = ((float)(v20));
  fg176r_st_f16(t24, t25, t26);
  v22 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t27 = ((int32_t)(v22));
  t28 = ((int32_t)(v11));
  if (!(((t27) < (t28)))) goto label5;
  {
  t29 = ((uint64_t)(v1));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)(v22))))));
  t31 = ((uint64_t)(v8));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  v23 = ((uint64_t)(((uint64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t33 = ((uint64_t)(v1));
  t34 = ((int32_t)(v22));
  t35 = ((int32_t)(v10));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)(((int32_t)((uint64_t)(t34) + (uint64_t)(t35)))))))));
  t37 = ((uint64_t)(v8));
  t38 = ((uint64_t)(((uint64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  v24 = ((uint64_t)(((uint64_t)((uint64_t)(t33) + (uint64_t)(t38)))));
  t39 = ((uint64_t)(v0));
  t40 = ((uint64_t)(v23));
  t41 = fg176r_ld_f32(t39, t40);
  t42 = ((float)(t41));
  t43 = ((float)(v21));
  v25 = ((float)(((t42) * (t43))));
  t44 = ((uint64_t)(v0));
  t45 = ((uint64_t)(v24));
  t46 = fg176r_ld_f32(t44, t45);
  t47 = ((float)(t46));
  t48 = ((float)(v21));
  v26 = ((float)(((t47) * (t48))));
  t49 = ((float)(v25));
  t50 = ((float)(v6));
  t51 = ((float)(((t49) + (t50))));
  t52 = fg176r_i8trunc(t51);
  v27 = ((int32_t)(t52));
  t53 = ((float)(v26));
  t54 = ((float)(v6));
  t55 = ((float)(((t53) + (t54))));
  t56 = fg176r_i8trunc(t55);
  v28 = ((int32_t)(t56));
  t57 = ((int32_t)(v27));
  t58 = ((int32_t)(v12));
  t59 = ((t57) > (t58));
  if (t59) {
  {
  v29 = ((int32_t)(v12));
  }
  } else {
  {
  v29 = ((int32_t)(v27));
  }
  }
  t60 = ((int32_t)(v28));
  t61 = ((int32_t)(v12));
  t62 = ((t60) > (t61));
  if (t62) {
  {
  v30 = ((int32_t)(v12));
  }
  } else {
  {
  v30 = ((int32_t)(v28));
  }
  }
  t63 = ((int32_t)(v29));
  t64 = ((int32_t)(v13));
  v31 = ((uint32_t)(((uint32_t)(((t63) & (t64))))));
  t65 = ((int32_t)(v30));
  t66 = ((int32_t)(v13));
  v32 = ((uint32_t)(((uint32_t)(((t65) & (t66))))));
  t67 = ((uint32_t)(v32));
  t68 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint32_t)(((uint32_t)((uint64_t)(t67) << ((uint64_t)(t68) & 31)))));
  t70 = ((uint32_t)(((uint32_t)(v13))));
  v33 = ((uint32_t)(((t69) & (t70))));
  t71 = ((uint64_t)(v2));
  t72 = ((uint64_t)(v3));
  t73 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(2))))));
  t74 = ((uint64_t)(((uint64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = ((uint64_t)(((uint64_t)(((int64_t)(v22))))));
  t76 = ((uint64_t)(((uint64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t77 = ((uint32_t)(v31));
  t78 = ((uint32_t)(v33));
  t79 = ((uint32_t)(((t77) | (t78))));
  fg176r_st_u8(t71, t76, t79);
  t80 = v22;
  t81 = ((int32_t)(((int64_t)UINT64_C(1))));
  v22 = ((int32_t)((uint64_t)(t80) + (uint64_t)(t81)));
  }
  goto label4;
label5:;
  }
fg176rd_gg_done:;
}
static __device__ __forceinline__ void fg176rd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, fg176rd_gg_type_1 p4, fg176rd_gg_type_1 p5, fg176rd_gg_type_1 p6, fg176rd_gg_type_1 p7, fg176rd_gg_type_1 p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, uint8_t p19, uint8_t p20, int32_t p21, int32_t p22, int32_t p23, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint32_t t9;
  fg176rd_gg_type_1 t10;
  fg176rd_gg_type_1 t11;
  fg176rd_gg_type_1 t12;
  uint32_t t13;
  fg176rd_gg_type_1 t14;
  uint32_t t15;
  uint32_t t16;
  fg176rd_gg_type_1 t17;
  uint32_t t18;
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
  uint8_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
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
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  fg176rd_gg_type_1 v4;
  fg176rd_gg_type_1 v5;
  fg176rd_gg_type_1 v6;
  fg176rd_gg_type_1 v7;
  fg176rd_gg_type_1 v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  uint8_t v19;
  uint8_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int64_t v24;
  fg176rd_gg_type_8 v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
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
  v21 = p21;
  v22 = p22;
  v23 = p23;
  {
  t0 = ((int64_t)(((int64_t)(v23))));
  t1 = ((int64_t)(((int64_t)(v21))));
  t2 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t3 = ((int64_t)(((int64_t)(v22))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t4 = ((int64_t)(v24));
  t5 = ((int64_t)(v3));
  t6 = ((t4) >= (t5));
  if (t6) {
  {
  goto fg176rd_gg_done;
  }
  }
  t7 = ((int64_t)(v24));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t9 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))))));
  t10 = v4;
  t11 = v5;
  t12 = v6;
  v25 = fg176rd_gg_function_2(t9, t10, t11, t12, base + 0);
  v26 = ((int64_t)((v25).f3));
  v27 = ((int64_t)((v25).f4));
  v28 = ((int64_t)((v25).f5));
  v29 = ((int64_t)((v25).f6));
  t13 = ((uint32_t)(((uint32_t)(v29))));
  t14 = v8;
  t15 = fg176rd_gg_function_5(t13, t14, base + 0);
  v30 = ((int64_t)(((int64_t)(t15))));
  t16 = ((uint32_t)(((uint32_t)(v28))));
  t17 = v7;
  t18 = fg176rd_gg_function_5(t16, t17, base + 0);
  v31 = ((int64_t)(((int64_t)(t18))));
  v32 = ((int64_t)(v27));
  fg176r_pdl_sync();
  t19 = ((int64_t)(v32));
  t20 = ((int64_t)(v12));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v31));
  t23 = ((int64_t)(v13));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t24)))));
  t26 = ((int64_t)(v30));
  t27 = ((int64_t)(v14));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t28)))));
  t29 = v20;
  if (t29) {
  {
  t30 = ((uint64_t)(v1));
  t31 = ((int64_t)(v33));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))))));
  t34 = fg176r_ld_i64(t30, t33);
  v34 = ((int64_t)(t34));
  }
  } else {
  {
  t35 = ((uint64_t)(v1));
  t36 = ((int64_t)(v33));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))))));
  t39 = fg176r_ld_i32(t35, t38);
  v34 = ((int64_t)(t39));
  }
  }
  t40 = v19;
  if (t40) {
  {
  t41 = ((int64_t)(v34));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t43 = ((t41) < (t42));
  t44 = t43;
  if (!t44) {
  t45 = ((int64_t)(v34));
  t46 = ((int64_t)(v18));
  t44 = ((t45) >= (t46));
  }
  t47 = t44;
  if (t47) {
  {
  fg176r_bad_index();
  goto fg176rd_gg_done;
  }
  }
  }
  }
  t48 = ((int64_t)(v34));
  t49 = ((int64_t)(v15));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  t51 = ((int64_t)(v28));
  t52 = ((int64_t)(v16));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v29));
  t56 = ((int64_t)(v17));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v35 = ((int64_t)(((int64_t)fg176rd_gg_div(t58, t59))));
  t60 = ((int64_t)(v35));
  t61 = ((int64_t)(v26));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t63 = ((int64_t)(((int64_t)fg176rd_gg_div(t61, t62))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t66 = ((int64_t)(v27));
  t67 = ((int64_t)(v9));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t69 = ((int64_t)(v28));
  t70 = ((int64_t)(v10));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t71)))));
  t73 = ((int64_t)(v29));
  t74 = ((int64_t)(v11));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t75)))));
  t77 = ((int64_t)(v26));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  t79 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t80 = ((uint64_t)(v0));
  t81 = ((uint64_t)(((uint64_t)(v37))));
  t82 = ((uint64_t)(v2));
  t83 = ((uint64_t)(((uint64_t)(v36))));
  fg176rd_gg_function_10(t80, t81, t82, t83, base + 0);
  }
fg176rd_gg_done:;
}
#define FG176R_Q4 fg176rd_gg_function_1
#define FG176R_F32 fg176rd_gg_function_16
