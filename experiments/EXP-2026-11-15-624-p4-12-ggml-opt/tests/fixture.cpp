// Differential gate for the ggml-opt slice (P4-12).
//
// Oracle: the real ggml-opt.cpp from the base tree, compiled into this gate
// (tests/oracle.cpp includes it). It runs one tiny CPU-only training epoch per
// configuration; no GPU, no driver, no real weights. The dataset values are
// synthetic and the model is one mul_mat with a 2x3 weight matrix.
//
// Two values are not observable through the native API and are transcribed
// from the native lines, marked TRANSCRIPTION where used:
//   - the val_split arithmetic of ggml_opt_fit (lines 1025-1026);
//   - the shuffle limit of ggml_opt_dataset_shuffle (lines 158-160).
// Everything else compares against executed native code.
//
// Every case prints one line; the program exits non-zero on any mismatch.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ggml-opt-gen.h"

// ---- oracle API (tests/oracle.cpp) ----
extern "C" {
struct OrcRun;

OrcRun * orc_run_new(int64_t n_in, int64_t n_out, int64_t nbatch, int64_t ndata,
                     int64_t ndata_shard, int32_t opt_period, int32_t loss_type,
                     int32_t optimizer);
void     orc_run_epoch(OrcRun * r, int64_t idata_split);
int      orc_run_nrec(OrcRun * r);
int32_t  orc_run_rec_train(OrcRun * r, int i);
int64_t  orc_run_rec_ibatch(OrcRun * r, int i);
int64_t  orc_run_rec_max(OrcRun * r, int i);
int64_t  orc_run_iter(OrcRun * r);
int32_t  orc_run_opt_i(OrcRun * r);
int64_t  orc_run_params_width(OrcRun * r);
int64_t  orc_run_accumulate(OrcRun * r);
int64_t  orc_run_momenta(OrcRun * r);
double   orc_run_loss_scale(OrcRun * r);
int64_t  orc_run_loss_per_dp(OrcRun * r);
void     orc_run_free(OrcRun * r);

int64_t  orc_run_nbatches(OrcRun * r);
int64_t  orc_run_ncorrect(OrcRun * r);
int64_t  orc_run_loss_size(OrcRun * r);
int64_t  orc_run_train_count(OrcRun * r);
int64_t  orc_run_eval_count(OrcRun * r);
int64_t  orc_run_period(OrcRun * r);
int64_t  orc_run_result_ndata(OrcRun * r);

int64_t  orc_ds_set_identity(OrcRun * r);
int64_t  orc_ds_shard_count_per_batch(OrcRun * r, int64_t nb_data_batch);
int64_t  orc_ds_get_batch_host(OrcRun * r, int64_t ibatch, float * out, int64_t nfloat);
int64_t  orc_run_shuffle_props(OrcRun * r, int64_t idata, int64_t * out);
void     orc_run_set_perm(OrcRun * r, const int64_t * perm, int64_t n);

int64_t  orc_dataset_shard_bytes(int64_t n_in, int64_t n_label, int64_t ndata,
                                 int64_t ndata_shard, int64_t * out);
int64_t  orc_fit_split(int64_t nbatches_logical, float val_split, int64_t opt_period,
                       int64_t nbatch_physical, int64_t * out);
int64_t  orc_shuffle_limit(int64_t idata, int64_t ndata_shard);

void *   orc_res_new(void);
void     orc_res_free(void * p);
void     orc_res_set(void * p, int64_t ndata, int64_t ncorrect, int64_t opt_period, int64_t per_dp);
void     orc_res_push(void * p, float v);
void     orc_res_get(void * p, double * loss, double * unc, double * acc, double * acc_unc);
int64_t  orc_res_ndata(void * p);

const char * orc_optimizer_name(int32_t id);
int64_t      orc_defaults(int64_t * out);
}

// ---------------- reporting ----------------

static int g_pass[3] = {0, 0, 0};
static int g_fail[3] = {0, 0, 0};
static const char * g_slice[3] = {"s1", "s2", "s3"};
static int g_cur = 0;

static void verdict(int slice, const char * name, bool good,
                    const char * got, const char * want) {
    if (good) {
        g_pass[slice]++;
        printf("case %-4s PASS %-34s\n", g_slice[slice], name);
    } else {
        g_fail[slice]++;
        printf("case %-4s FAIL %-34s got=%s want=%s\n", g_slice[slice], name, got, want);
    }
}

static void check_i64(int slice, const char * name, int64_t got, int64_t want) {
    char a[64], b[64];
    snprintf(a, sizeof a, "%lld", (long long) got);
    snprintf(b, sizeof b, "%lld", (long long) want);
    verdict(slice, name, got == want, a, b);
}

static bool same_f64(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b);
    }
    if (std::isinf(a) || std::isinf(b)) {
        return a == b;
    }
    const double scale = std::fabs(b) > 1.0 ? std::fabs(b) : 1.0;
    return std::fabs(a - b) <= 1e-12 * scale;
}

static void check_f64(int slice, const char * name, double got, double want) {
    char a[64], b[64];
    snprintf(a, sizeof a, "%.12g%s", got, std::isnan(got) ? " (nan)" : "");
    snprintf(b, sizeof b, "%.12g%s", want, std::isnan(want) ? " (nan)" : "");
    verdict(slice, name, same_f64(got, want), a, b);
}

static sl_i64 sl_i(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f32 sl_f(std::vector<float> & v)    { return sl_f32{ v.data(), (int64_t) v.size() }; }
static sl_f64 sl_d(std::vector<double> & v)   { return sl_f64{ v.data(), (int64_t) v.size() }; }

// ---------------- port simulation of the eval state machine ----------------

struct Sim {
    int64_t iter;
    int64_t opt_i;
};

static Sim sim_epoch(int64_t period, int64_t ntrain, int64_t nval) {
    Sim s{1, 0};
    for (int64_t i = 0; i < ntrain; ++i) {
        const int64_t bt = op_build_type_next(s.opt_i, period);
        s.iter = op_iter_advance(s.iter, bt == 30 ? 1 : 0);
        s.opt_i = op_opt_i_advance(s.opt_i, period);
    }
    for (int64_t i = 0; i < nval; ++i) {
        s.opt_i = op_opt_i_advance(s.opt_i, period);
    }
    return s;
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    std::vector<int64_t> out4(8, 0);
    std::vector<double>  out2(4, 0.0);
    std::vector<float>   outf(2, 0.0f);

    // ================= S1: dataset and epoch scheduling =================

    OrcRun * a = orc_run_new(2, 3, 4, 16, 4, 2, 0, 0);
    check_i64(0, "nbatches", op_nbatches(16, 4), orc_run_nbatches(a));
    check_i64(0, "opt_period_from_run", op_opt_period(8, 4), orc_run_period(a));

    orc_run_epoch(a, 8);
    const int64_t native_train = orc_run_train_count(a);
    const int64_t native_eval  = orc_run_eval_count(a);

    check_i64(0, "batches_split_explicit", op_batches_split(16, 4, 8), native_train);
    check_i64(0, "split_train_count", native_train, 2);
    check_i64(0, "split_eval_count", native_eval, 2);

    // callback order and bounds, native records vs the port's split plan
    {
        const int64_t ibatch_split = op_batches_split(16, 4, 8);
        const int64_t nbatches = op_nbatches(16, 4);
        bool good = orc_run_nrec(a) == (int) (ibatch_split + (nbatches - ibatch_split));
        for (int i = 0; good && i < orc_run_nrec(a); ++i) {
            const bool train = orc_run_rec_train(a, i) != 0;
            const int64_t want_i = i + 1;
            const int64_t want_max = train ? ibatch_split : (nbatches - ibatch_split);
            if (i >= ibatch_split) {
                good = !train && orc_run_rec_ibatch(a, i) == want_i - ibatch_split &&
                       orc_run_rec_max(a, i) == want_max;
            } else {
                good = train && orc_run_rec_ibatch(a, i) == want_i && orc_run_rec_max(a, i) == want_max;
            }
        }
        verdict(0, "callback_sequence", good, good ? "match" : "mismatch", "native");
    }

    OrcRun * b = orc_run_new(2, 3, 4, 16, 4, 2, 0, 0);
    orc_run_epoch(b, -1);
    check_i64(0, "batches_split_negative_all", op_batches_split(16, 4, -1), orc_run_train_count(b));
    check_i64(0, "negative_split_no_eval", orc_run_eval_count(b), 0);

    // val_split arithmetic: TRANSCRIPTION of ggml_opt_fit lines 1025-1026,
    // cross-checked against a native epoch run at the same split index.
    {
        const float splits[5] = {0.0f, 0.25f, 0.3f, 0.5f, 0.75f};
        for (int i = 0; i < 5; ++i) {
            int64_t ref[2] = {0, 0};
            orc_fit_split(4, splits[i], 1, 4, ref);
            std::vector<int64_t> got(4, 0);
            op_epoch_split(16, 4, 4, splits[i], sl_i(got));
            char name[64];
            snprintf(name, sizeof name, "epoch_split_val_%d", i);
            check_i64(0, name, got[2], ref[0]);
            check_i64(0, name, got[3], ref[1]);
            check_i64(0, name, got[0], 1);
            check_i64(0, name, got[1], 4);
        }
        int64_t ref[2] = {0, 0};
        orc_fit_split(4, 0.25f, 1, 4, ref);
        OrcRun * c = orc_run_new(2, 3, 4, 16, 4, 1, 0, 0);
        orc_run_epoch(c, ref[1]);
        check_i64(0, "epoch_split_native_train", orc_run_train_count(c), ref[0]);
        check_i64(0, "epoch_split_native_eval", orc_run_eval_count(c), 4 - ref[0]);
        orc_run_free(c);
    }

    // epoch_split with a physical opt period above one
    {
        int64_t ref[2] = {0, 0};
        orc_fit_split(4, 0.25f, 2, 4, ref);
        std::vector<int64_t> got(4, 0);
        op_epoch_split(32, 8, 4, 0.25f, sl_i(got));
        check_i64(0, "epoch_split_period", got[0], 2);
        check_i64(0, "epoch_split_period", got[2], ref[0]);
        check_i64(0, "epoch_split_period", got[3], ref[1]);
    }

    // shard byte counts, native dataset tensors
    {
        int64_t ref[2] = {0, 0};
        orc_dataset_shard_bytes(2, 3, 16, 4, ref);
        std::vector<int64_t> got(2, 0);
        op_shard_bytes(2 * 4 * 16, 3 * 4 * 16, 16, 4, 1, sl_i(got));
        check_i64(0, "shard_bytes_data", got[0], ref[0]);
        check_i64(0, "shard_bytes_labels", got[1], ref[1]);
        int64_t ref_nolabel[2] = {0, 0};
        orc_dataset_shard_bytes(2, 0, 16, 4, ref_nolabel);
        std::vector<int64_t> got2(2, 0);
        op_shard_bytes(2 * 4 * 16, 0, 16, 4, 0, sl_i(got2));
        check_i64(0, "shard_bytes_no_labels", got2[0], ref_nolabel[0]);
        check_i64(0, "shard_bytes_no_labels", got2[1], ref_nolabel[1]);
    }

    // shuffle limit: transcription plus the native invariant (suffix fixed)
    {
        OrcRun * s = orc_run_new(2, 3, 4, 16, 4, 1, 0, 0);
        for (int t = 0; t < 2; ++t) {
            const int64_t idata = t == 0 ? -1 : 8;
            int64_t ref = orc_shuffle_limit(idata, 4);
            int64_t got = op_shuffle_limit(idata, 4);
            check_i64(0, t == 0 ? "shuffle_limit_all" : "shuffle_limit_prefix", got, ref);
            int64_t props[2] = {0, 0};
            orc_run_shuffle_props(s, idata, props);
            const int64_t limit = got < 0 ? 4 : got;
            check_i64(0, t == 0 ? "shuffle_suffix_all" : "shuffle_suffix_prefix", props[1], 1);
            verdict(0, t == 0 ? "shuffle_moved_all" : "shuffle_moved_prefix",
                    props[0] <= limit, props[0] <= limit ? "within" : "beyond", "within");
        }
        orc_run_free(s);
    }

    // batch shard mapping: native get_batch_host vs the port's index math
    {
        OrcRun * m = orc_run_new(2, 3, 4, 16, 4, 1, 0, 0);
        const int64_t spb = orc_ds_shard_count_per_batch(m, 2 * 4 * 2 * 4); // 2 shards
        check_i64(0, "shards_per_batch", spb, 2);
        const int64_t nshards = orc_ds_set_identity(m);
        std::vector<int64_t> permv((size_t) nshards, 0);
        for (int64_t i = 0; i < nshards; ++i) permv[(size_t) i] = i;
        orc_run_set_perm(m, permv.data(), nshards);
        for (int t = 0; t < 2; ++t) {
            if (t == 1) {
                for (int64_t i = 0; i < nshards; ++i) permv[(size_t) i] = nshards - 1 - i;
                orc_run_set_perm(m, permv.data(), nshards);
            }
            const int64_t ibatch = 1;
            const int64_t nfloat = spb * 4 * 2;
            std::vector<float> nat(nfloat, 0.0f);
            orc_ds_get_batch_host(m, ibatch, nat.data(), nfloat);
            bool good = true;
            for (int64_t k = 0; k < spb; ++k) {
                const int64_t rank = op_batch_shard(sl_i(permv), ibatch, spb, k);
                for (int64_t j = 0; j < 4; ++j) {
                    for (int64_t e = 0; e < 2; ++e) {
                        const float want = (float) (rank * 4 + j) + 0.5f * (float) e;
                        const float got = nat[(k * 4 + j) * 2 + e];
                        if (got != want) good = false;
                    }
                }
            }
            verdict(0, t == 0 ? "batch_mapping_identity" : "batch_mapping_permuted",
                    good, good ? "match" : "mismatch", "native");
        }
        orc_run_free(m);
    }

    check_i64(0, "should_shuffle_yes", op_should_shuffle(4, 16), 1);
    check_i64(0, "should_shuffle_no", op_should_shuffle(16, 16), 0);
    check_i64(0, "should_reshuffle_yes", op_should_reshuffle(4, 8), 1);
    check_i64(0, "should_reshuffle_equal", op_should_reshuffle(8, 8), 0);

    // ---- eval state machine against real epoch runs ----
    {
        struct Cfg { int64_t ndata, nbatch, shard, period, split; };
        const Cfg cfgs[4] = {
            {16, 4, 4, 2,  8},
            {16, 4, 4, 1,  8},
            {24, 4, 4, 5, 12},
            {24, 4, 4, 3, 16},
        };
        const char * names[4] = {
            "epoch_state_p2", "epoch_state_p1", "epoch_state_p5", "epoch_state_p3"
        };
        for (int i = 0; i < 4; ++i) {
            OrcRun * r = orc_run_new(2, 3, cfgs[i].nbatch, cfgs[i].ndata, cfgs[i].shard,
                                     (int32_t) cfgs[i].period, 0, 0);
            orc_run_epoch(r, cfgs[i].split);
            const int64_t nbatches = orc_run_nbatches(r);
            const int64_t train = orc_run_train_count(r);
            Sim s = sim_epoch(cfgs[i].period, train, nbatches - train);
            check_i64(0, names[i], s.iter, orc_run_iter(r));
            check_i64(0, names[i], s.opt_i, orc_run_opt_i(r));
            orc_run_free(r);
        }
    }
    orc_run_free(a);
    orc_run_free(b);

    // ================= S2: optimizer step policy =================

    {
        int64_t nat[10] = {0};
        orc_defaults(nat);
        std::vector<int64_t> got(8, 0);
        op_defaults(0, sl_i(got));
        check_i64(1, "default_loss_type", got[0], nat[0]);
        check_i64(1, "default_build_type", got[1], nat[1]);
        check_i64(1, "default_opt_period", got[2], nat[2]);
        check_i64(1, "default_optimizer", got[3], nat[3]);
        check_i64(1, "default_has_pars", got[4], nat[4]);
    }

    for (int id = 0; id < 3; ++id) {
        const char * s = orc_optimizer_name(id);
        const int64_t want = std::strcmp(s, "adamw") == 0 ? 0 :
                             std::strcmp(s, "sgd") == 0 ? 1 : -1;
        char name[48];
        snprintf(name, sizeof name, "optimizer_code_%d", id);
        check_i64(1, name, op_optimizer_code(id), want);
    }

    // loss scaling and per-datapoint flag, native opt context metadata
    {
        const int64_t types[4] = {0, 1, 2, 3};
        const int64_t periods[2] = {1, 2};
        for (int ti = 0; ti < 4; ++ti) {
            for (int pi = 0; pi < 2; ++pi) {
                OrcRun * r = orc_run_new(2, 3, 4, 16, 4, (int32_t) periods[pi],
                                         (int32_t) types[ti], 0);
                char name[64];
                snprintf(name, sizeof name, "loss_scale_t%lld_p%lld",
                         (long long) types[ti], (long long) periods[pi]);
                check_f64(1, name, (double) op_loss_scale(types[ti], periods[pi], 12),
                          orc_run_loss_scale(r));
                orc_run_free(r);
            }
            OrcRun * r1 = orc_run_new(2, 3, 4, 16, 4, 1, (int32_t) types[ti], 0);
            char name[64];
            snprintf(name, sizeof name, "loss_per_dp_t%lld", (long long) types[ti]);
            check_i64(1, name, op_loss_per_dp(types[ti]), orc_run_loss_per_dp(r1));
            orc_run_free(r1);
        }
    }

    // momenta and optimizer-parameter width
    {
        OrcRun * adamw = orc_run_new(2, 3, 4, 16, 4, 2, 0, 0);
        OrcRun * sgd   = orc_run_new(2, 3, 4, 16, 4, 2, 0, 1);
        check_i64(1, "momenta_adamw", op_need_momenta(30, 0), orc_run_momenta(adamw));
        check_i64(1, "momenta_sgd", op_need_momenta(30, 1), orc_run_momenta(sgd));
        check_i64(1, "params_width_adamw", op_params_width(1), orc_run_params_width(adamw));
        check_i64(1, "params_width_sgd", op_params_width(0), orc_run_params_width(sgd));
        check_i64(1, "params_width_adamw_value", orc_run_params_width(adamw), 7);
        check_i64(1, "params_width_sgd_value", orc_run_params_width(sgd), 2);
        orc_run_free(adamw);
        orc_run_free(sgd);
    }

    // accumulate: static graph period 1 updates in place, period above 1 and
    // non-static graphs accumulate
    {
        OrcRun * p1 = orc_run_new(2, 3, 4, 16, 4, 1, 0, 0);
        OrcRun * p2 = orc_run_new(2, 3, 4, 16, 4, 2, 0, 0);
        check_i64(1, "accumulate_static_p1", op_accumulate(30, 1, 1), orc_run_accumulate(p1));
        check_i64(1, "accumulate_static_p2", op_accumulate(30, 1, 2), orc_run_accumulate(p2));
        check_i64(1, "accumulate_static_p1_value", orc_run_accumulate(p1), 0);
        check_i64(1, "accumulate_static_p2_value", orc_run_accumulate(p2), 1);
        // no static graph: the port formula is the only oracle for this branch
        check_i64(1, "accumulate_dynamic", op_accumulate(30, 0, 1), 1);
        orc_run_free(p1);
        orc_run_free(p2);
    }

    // build_type / opt_i / iter exact sequences, native epoch runs reuse the
    // S1 configurations; the port simulation is compared there. Here the
    // direct per-step values are pinned.
    {
        check_i64(1, "build_type_next_wrap", op_build_type_next(0, 1), 30);
        check_i64(1, "build_type_next_grad", op_build_type_next(0, 2), 20);
        check_i64(1, "build_type_next_opt", op_build_type_next(1, 2), 30);
        check_i64(1, "iter_advance_opt", op_iter_advance(7, 1), 8);
        check_i64(1, "iter_advance_grad", op_iter_advance(7, 0), 7);
        check_i64(1, "opt_i_wrap", op_opt_i_advance(4, 5), 0);
        check_i64(1, "opt_i_mid", op_opt_i_advance(2, 5), 3);
    }

    // ================= S3: result aggregation =================

    {
        struct RC { const char * name; int n; float v[4]; int64_t ndata, ncorrect, period, per_dp; };
        const RC rcs[8] = {
            {"loss_empty",           0, {0},                       0, -1, 2, 1},
            {"loss_single",          1, {0.5f},                    1, -1, 1, 0},
            {"loss_two_sum",         2, {1.0f, 3.0f},              2, -1, 1, 0},
            {"loss_three_sum",       3, {0.5f, 1.5f, 2.0f},        3, -1, 1, 0},
            {"loss_per_dp_p2",       2, {0.25f, 0.75f},            2, -1, 2, 1},
            {"loss_per_dp_p3",       4, {0.1f, 0.2f, 0.3f, 0.4f},  4, -1, 3, 1},
            {"loss_per_dp_single",   1, {0.9f},                    1, -1, 4, 1},
            {"loss_two_per_dp_p1",   2, {0.2f, 0.4f},              2, -1, 1, 1},
        };
        for (int i = 0; i < 8; ++i) {
            void * res = orc_res_new();
            for (int k = 0; k < rcs[i].n; ++k) orc_res_push(res, rcs[i].v[k]);
            orc_res_set(res, rcs[i].ndata, rcs[i].ncorrect, rcs[i].period, rcs[i].per_dp);
            double nl = 0, nu = 0, na = 0, nau = 0;
            orc_res_get(res, &nl, &nu, &na, &nau);
            std::vector<float> lv(rcs[i].v, rcs[i].v + rcs[i].n);
            std::vector<double> got(2, 0.0);
            op_result_loss(sl_f(lv), rcs[i].per_dp, rcs[i].period, sl_d(got));
            check_f64(2, rcs[i].name, got[0], nl);
            check_f64(2, rcs[i].name, got[1], nu);
            orc_res_free(res);
        }
    }

    {
        struct AC { const char * name; int64_t ncorrect, ndata; };
        const AC acs[5] = {
            {"accuracy_zero",  0, 4},
            {"accuracy_full",  4, 4},
            {"accuracy_third", 1, 3},
            {"accuracy_one",   1, 1},
            {"accuracy_none", -1, 4},
        };
        for (int i = 0; i < 5; ++i) {
            void * res = orc_res_new();
            orc_res_set(res, acs[i].ndata, acs[i].ncorrect, 1, 1);
            double nl = 0, nu = 0, na = 0, nau = 0;
            orc_res_get(res, &nl, &nu, &na, &nau);
            std::vector<double> got(2, 0.0);
            op_result_accuracy(acs[i].ncorrect, acs[i].ndata, sl_d(got));
            check_f64(2, acs[i].name, got[0], na);
            check_f64(2, acs[i].name, got[1], nau);
            orc_res_free(res);
        }
    }

    // ndata accumulation and prediction length, against a real epoch result
    {
        OrcRun * r = orc_run_new(2, 3, 4, 16, 4, 2, 0, 0);
        orc_run_epoch(r, 8);
        int64_t acc = 0;
        const int64_t nloss = orc_run_loss_size(r);
        for (int64_t i = 0; i < nloss; ++i) {
            acc = op_ndata_next(acc, 4, i);
        }
        check_i64(2, "ndata_accumulate", acc, orc_run_result_ndata(r));
        check_i64(2, "ndata_accumulate_native", orc_run_result_ndata(r), 8);
        check_i64(2, "ndata_mismatch", op_ndata_next(6, 4, 2), -1);
        check_i64(2, "pred_len", op_pred_len(4, 4), 8);
        // MEAN loss has no predictions, so ncorrect stays at the sentinel
        check_i64(2, "ncorrect_native_sentinel", orc_run_ncorrect(r), -1);
        check_i64(2, "ncorrect_next_sentinel", op_ncorrect_next(0, 3, 0), -1);
        check_i64(2, "ncorrect_next_sticky", op_ncorrect_next(-1, 3, 1), -1);
        check_i64(2, "ncorrect_next_add", op_ncorrect_next(5, 3, 1), 8);
        check_i64(2, "accuracy_defined_yes", op_accuracy_defined(0), 1);
        check_i64(2, "accuracy_defined_no", op_accuracy_defined(-1), 0);
        orc_run_free(r);
    }

    const int total_pass = g_pass[0] + g_pass[1] + g_pass[2];
    const int total_fail = g_fail[0] + g_fail[1] + g_fail[2];
    for (int i = 0; i < 3; ++i) {
        printf("slice %s pass=%d fail=%d\n", g_slice[i], g_pass[i], g_fail[i]);
    }
    printf("summary pass=%d fail=%d total=%d\n", total_pass, total_fail, total_pass + total_fail);
    return total_fail == 0 ? 0 : 1;
}
