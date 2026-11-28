extern float __nv_tanhf(float);
extern float __nv_expf(float);

#define i2_epilogue_tanh __nv_tanhf
#define i2_epilogue_exp __nv_expf
static float i2_epilogue_exp_approx(float value) {
    float result;
    __asm__("ex2.approx.ftz.f32 %0, %1;" : "=f"(result) : "f"(value * 1.4426950408889634f));
    return result;
}
static float i2_epilogue_rcp_approx(float value) {
    float result;
    __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(result) : "f"(value));
    return result;
}
