// Differential gate for the Goose Apex graph-decision module.
//
// The reference is the native expression itself, written out the same way the headers write it,
// so the comparison is against llama-hparams.cpp:260 and llama-graph.h:1086-1094 rather than a
// re-derivation.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <vector>

extern "C" {
int64_t fg200g_is_recr(int64_t, int64_t, uint64_t);
int64_t fg200g_crop_before_nextn(int64_t, int64_t, int64_t);
int64_t fg200g_crop_after_nextn(int64_t, int64_t, int64_t);
int64_t fg200g_capture_layer_input(int64_t, int64_t);
int64_t fg200g_shift_skip(int64_t);
int64_t fg200g_gating_kind(int64_t);
int64_t fg200g_group_select(int64_t, int64_t);
int64_t fg200g_weight_before_ffn(int64_t);
int64_t fg200g_weights_need_scale(int64_t);
int64_t fg200g_kq_scale_auto(int64_t);
int64_t fg200g_gate_elem_offset(int64_t);
int64_t fg200g_q_full_width(int64_t);
int64_t fg200g_head_v_dim(int64_t, int64_t);
int64_t fg200g_conv_channels(int64_t, int64_t, int64_t);
int64_t fg200g_qkv_dim(int64_t, int64_t, int64_t, int64_t);
int64_t fg200g_k_view_offset(int64_t, int64_t);
int64_t fg200g_v_view_offset(int64_t, int64_t);
int64_t fg200g_need_repeat(int64_t, int64_t, int64_t, int64_t);
int64_t fg200g_repeat_divisible(int64_t, int64_t);
int64_t fg200g_final_width(int64_t, int64_t);
int64_t fg200g_final_rows(int64_t, int64_t);
int64_t fg200g_n_ff_exp(int64_t, int64_t, int64_t);
int64_t fg200g_n_ff_shexp(int64_t, int64_t);
int64_t fg200g_key_dim(int64_t, int64_t);
int64_t fg200g_value_dim(int64_t, int64_t);
int64_t fg200g_conv_dim(int64_t, int64_t);
int64_t fg200g_wqkv_width(int64_t, int64_t);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) {
        if (failures < 15) printf("FAIL %s: goose=%lld reference=%lld\n", what, a, b);
        ++failures;
    }
}

static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2];
    if (pipe(fd) != 0) { ++failures; return; }
    pid_t pid = fork();
    if (pid == 0) { close(fd[0]); dup2(fd[1], 2); close(fd[1]); fn(); _exit(0); }
    close(fd[1]);
    char buf[256] = {0};
    ssize_t n = read(fd[0], buf, sizeof buf - 1);
    close(fd[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    ++checks;
    bool died = WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
    long long code = (n > 0 && strstr(buf, "validation code")) ? atoll(strstr(buf, "validation code") + 16) : -1;
    if (!died || code != expected) {
        if (failures < 15) printf("FAIL trap %s: died=%d code=%lld expected=%d (%s)\n", what, died, code, expected, buf);
        ++failures;
    }
}

static void trap_neg_layer(void) { fg200g_is_recr(-1, 8, 0); }

int main(void) {
    std::mt19937 rng(200001);

    // is_recr over every index of several layer counts with random flags.
    for (int trial = 0; trial < 200; ++trial) {
        const int64_t n_all = 1 + (int64_t)(rng() % 48);
        // Native is std::array<uint32_t, LLAMA_MAX_LAYERS>: one element per layer.
        std::vector<uint32_t> flags((size_t)n_all, 0);
        for (auto & f : flags) f = (uint32_t)(rng() % 2);
        for (int64_t il = 0; il < n_all; ++il) {
            const bool ref = flags[il] != 0;
            const int64_t g = fg200g_is_recr(il, n_all, (uint64_t)(uintptr_t)flags.data());
            ck((g != 0) == ref, "is_recr", g, ref);
        }
        // Out of range returns -1 rather than a flag.
        ck(fg200g_is_recr(n_all, n_all, (uint64_t)(uintptr_t)flags.data()) == -1, "is_recr oob", 0, 0);
    }

    // Crop predicates over all eight combinations.
    for (int h = 0; h < 2; ++h)
    for (int e = 0; e < 2; ++e)
    for (int m = 0; m < 2; ++m) {
        const bool ref_b = h && (!e || m);
        const bool ref_a = h && e && !m;
        ck((fg200g_crop_before_nextn(h, e, m) != 0) == ref_b, "crop_before", 0, ref_b);
        ck((fg200g_crop_after_nextn(h, e, m) != 0) == ref_a, "crop_after", 0, ref_a);
    }

    // capture_layer_input and shift_skip.
    for (int i = 0; i < 200; ++i) {
        const int64_t il = rng() % 60, nl = rng() % 60;
        ck((fg200g_capture_layer_input(il, nl) != 0) == (il < nl), "capture", 0, 0);
        const int64_t sh = (int64_t)(rng() % 21) - 10;
        ck((fg200g_shift_skip(sh) != 0) == (sh == 0), "shift_skip", 0, 0);
    }

    // MoE routing decisions against the native conditions.
    for (int64_t g = 0; g < 4; ++g) {
        ck(fg200g_gating_kind(g) == g, "gating_kind", fg200g_gating_kind(g), g);
    }
    for (int i = 0; i < 200; ++i) {
        const int64_t ng = 1 + (int64_t)(rng() % 8), nt = (int64_t)(rng() % 5);
        ck((fg200g_group_select(ng, nt) != 0) == (ng > 1 && nt > 0), "group_select", 0, 0);

        const int64_t is4 = rng() % 2;
        ck((fg200g_weight_before_ffn(is4) != 0) == (is4 != 0), "weight_before_ffn", 0, 0);

        // Bit patterns: 0.0f, 1.0f, and a few other floats.
        const float vals[] = {0.0f, 1.0f, 0.5f, -1.0f, 2.5f};
        for (float v : vals) {
            int32_t bits; memcpy(&bits, &v, 4);
            const bool ref = (v != 0.0f && v != 1.0f);
            ck((fg200g_weights_need_scale(bits) != 0) == ref, "weights_need_scale", 0, ref);
        }
    }

    // build_layer_attn decisions.
    for (int i = 0; i < 200; ++i) {
        const float scales[] = {0.0f, 1.0f, 0.125f, -2.0f};
        for (float s : scales) {
            int32_t bits; memcpy(&bits, &s, 4);
            ck((fg200g_kq_scale_auto(bits) != 0) == (s == 0.0f), "kq_scale_auto", 0, s == 0.0f);
        }
        const int64_t nh = 1 + (int64_t)(rng() % 256);
        ck(fg200g_gate_elem_offset(nh) == nh, "gate_offset", fg200g_gate_elem_offset(nh), nh);
        ck(fg200g_q_full_width(nh) == 2*nh, "q_full_width", fg200g_q_full_width(nh), 2*nh);
    }

    // Recurrent-branch geometry against the native expressions.
    for (int i = 0; i < 300; ++i) {
        const int64_t di = (int64_t)(rng() % 8192), nvh = 1 + (int64_t)(rng() % 64);
        ck(fg200g_head_v_dim(di, nvh) == di / nvh, "head_v_dim", fg200g_head_v_dim(di,nvh), di/nvh);

        const int64_t ng = (int64_t)(rng() % 32), ds = (int64_t)(rng() % 256);
        ck(fg200g_conv_channels(di, ng, ds) == di + 2*ng*ds, "conv_channels", 0, 0);

        const int64_t hk = (int64_t)(rng() % 256), nk = (int64_t)(rng() % 64);
        const int64_t hv = fg200g_head_v_dim(di, nvh);
        ck(fg200g_qkv_dim(hk, nk, hv, nvh) == hk*nk*2 + hv*nvh, "qkv_dim", 0, 0);
        ck(fg200g_k_view_offset(hk, nk) == hk*nk, "k_off", 0, 0);
        ck(fg200g_v_view_offset(hk, nk) == hk*nk*2, "v_off", 0, 0);
    }

    // Recurrent-branch tail decisions.
    for (int i = 0; i < 300; ++i) {
        const int64_t nk = 1 + (int64_t)(rng() % 16), nv = 1 + (int64_t)(rng() % 16);
        const int64_t ar = rng() % 2, ch = rng() % 2;
        const bool ref_rep = (nk != nv) && !(ar && ch);
        ck((fg200g_need_repeat(nk, nv, ar, ch) != 0) == ref_rep, "need_repeat", 0, ref_rep);
        ck((fg200g_repeat_divisible(nk, nv) != 0) == (nv % nk == 0), "repeat_divisible", 0, 0);

        const int64_t hv = (int64_t)(rng() % 256), nt = (int64_t)(rng() % 64), ns = (int64_t)(rng() % 8);
        ck(fg200g_final_width(hv, nv) == hv*nv, "final_width", 0, 0);
        ck(fg200g_final_rows(nt, ns) == nt*ns, "final_rows", 0, 0);
    }

    // Tensor-shape geometry against the native expressions.
    for (int i = 0; i < 300; ++i) {
        const int64_t nff = 1 + (int64_t)(rng() % 8192), nu = 1 + (int64_t)(rng() % 16);
        const int64_t decl = (rng() % 2) ? (int64_t)(rng() % 4096) : 0;
        ck(fg200g_n_ff_exp(decl, nff, nu) == (decl ? decl : nff / nu), "n_ff_exp", 0, 0);
        const int64_t ds = (rng() % 2) ? (int64_t)(rng() % 4096) : 0;
        ck(fg200g_n_ff_shexp(ds, nff) == (ds ? ds : nff), "n_ff_shexp", 0, 0);

        const int64_t hk = (int64_t)(rng() % 256), nk = (int64_t)(rng() % 32);
        const int64_t hv = (int64_t)(rng() % 256), nv = (int64_t)(rng() % 32);
        ck(fg200g_key_dim(hk, nk) == hk*nk, "key_dim", 0, 0);
        ck(fg200g_value_dim(hv, nv) == hv*nv, "value_dim", 0, 0);
        const int64_t kd = fg200g_key_dim(hk, nk), vd = fg200g_value_dim(hv, nv);
        ck(fg200g_conv_dim(kd, vd) == kd*2 + vd, "conv_dim", 0, 0);
        ck(fg200g_wqkv_width(kd, vd) == kd*2 + vd, "wqkv_width", 0, 0);
    }

    run_trap("is_recr negative layer", 10, trap_neg_layer);

    printf("FG200G_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP200_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP200_LOCAL_GATES_PASS\n");
    return 0;
}
