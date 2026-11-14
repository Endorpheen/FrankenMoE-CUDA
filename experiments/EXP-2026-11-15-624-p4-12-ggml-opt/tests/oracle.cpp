// Native oracle for the ggml-opt port.
// Includes the real ggml-opt.cpp so the structs and static helpers are visible.
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-opt.h"
#include "ggml-cpu.h"

#define private public
#include "ggml-opt.cpp"
#undef private

struct OrcEpochRec {
    int32_t train;
    int64_t ibatch;
    int64_t ibatch_max;
};

struct OrcRun {
    ggml_context * ctx_model;
    ggml_context * ctx_compute;
    ggml_backend_t cpu;
    ggml_backend_sched_t sched;
    ggml_backend_buffer_t model_buf;
    ggml_tensor * W;
    ggml_tensor * inputs;
    ggml_tensor * outputs;
    ggml_opt_context_t opt;
    ggml_opt_dataset_t ds;
    ggml_opt_result_t rt;
    ggml_opt_result_t rv;
    OrcEpochRec rec[64];
    int nrec;
};

static void rec_cb(bool train, ggml_opt_context_t, ggml_opt_dataset_t, ggml_opt_result_t,
                   int64_t ibatch, int64_t ibatch_max, int64_t) {
    // the callback has no userdata; store into a global current run
    extern OrcRun * g_run;
    if (!g_run || g_run->nrec >= 64) return;
    g_run->rec[g_run->nrec++] = { train ? 1 : 0, ibatch, ibatch_max };
}

OrcRun * g_run = nullptr;

extern "C" {

OrcRun * orc_run_new(int64_t n_in, int64_t n_out, int64_t nbatch, int64_t ndata,
                     int64_t ndata_shard, int32_t opt_period, int32_t loss_type,
                     int32_t optimizer) {
    OrcRun * r = new OrcRun();
    memset(r, 0, sizeof(*r));

    ggml_init_params pm = { 4u*1024u*1024u, nullptr, true };
    r->ctx_model = ggml_init(pm);
    r->W = ggml_new_tensor_2d(r->ctx_model, GGML_TYPE_F32, n_in, n_out);
    ggml_set_param(r->W);
    r->inputs = ggml_new_tensor_2d(r->ctx_model, GGML_TYPE_F32, n_in, nbatch);

    ggml_init_params pc = { 32u*1024u*1024u, nullptr, true };
    r->ctx_compute = ggml_init(pc);
    r->outputs = ggml_mul_mat(r->ctx_compute, r->W, r->inputs);

    r->cpu = ggml_backend_cpu_init();
    ggml_backend_t backs[1] = { r->cpu };
    r->sched = ggml_backend_sched_new(backs, nullptr, 1, GGML_DEFAULT_GRAPH_SIZE, false, true);
    r->model_buf = ggml_backend_alloc_ctx_tensors(r->ctx_model, r->cpu);
    float * wd = (float *) r->W->data;
    for (int64_t i = 0; i < n_in*n_out; ++i) wd[i] = 0.01f*(float)(i%7) - 0.03f;

    ggml_opt_params p = ggml_opt_default_params(r->sched, (ggml_opt_loss_type) loss_type);
    p.ctx_compute = r->ctx_compute;
    p.inputs = r->inputs;
    p.outputs = r->outputs;
    p.opt_period = opt_period;
    p.optimizer = (ggml_opt_optimizer_type) optimizer;
    r->opt = ggml_opt_init(p);

    r->ds = ggml_opt_dataset_init(GGML_TYPE_F32, GGML_TYPE_F32, n_in, 0, ndata, ndata_shard);
    float * dd = (float *) r->ds->data->data;
    for (int64_t d = 0; d < ndata; ++d) {
        for (int64_t j = 0; j < n_in; ++j) {
            dd[d*n_in + j] = (float) d + 0.5f*(float) j;
        }
    }

    r->rt = ggml_opt_result_init();
    r->rv = ggml_opt_result_init();
    g_run = r;
    return r;
}

void orc_run_epoch(OrcRun * r, int64_t idata_split) {
    r->nrec = 0;
    g_run = r;
    ggml_opt_epoch(r->opt, r->ds, r->rt, r->rv, idata_split, rec_cb, rec_cb);
}

int orc_run_nrec(OrcRun * r) { return r->nrec; }
int32_t orc_run_rec_train(OrcRun * r, int i) { return r->rec[i].train; }
int64_t orc_run_rec_ibatch(OrcRun * r, int i) { return r->rec[i].ibatch; }
int64_t orc_run_rec_max(OrcRun * r, int i) { return r->rec[i].ibatch_max; }
int64_t orc_run_iter(OrcRun * r) { return r->opt->iter; }
int32_t orc_run_opt_i(OrcRun * r) { return r->opt->opt_i; }
int64_t orc_run_params_width(OrcRun * r) { return r->opt->opt_step_params->ne[0]; }
int64_t orc_run_accumulate(OrcRun * r) {
    // The loss node always owns a gradient accumulator, so only parameters
    // decide whether gradient accumulation is in use.
    for (size_t i = 0; i < r->opt->grad_accs.size(); ++i) {
        if (!r->opt->grad_accs[i]) continue;
        if (r->opt->gf->nodes[i]->flags & GGML_TENSOR_FLAG_PARAM) return 1;
    }
    return 0;
}
int64_t orc_run_momenta(OrcRun * r) {
    for (size_t i = 0; i < r->opt->grad_m.size(); ++i) {
        if (r->opt->grad_m[i]) return 1;
    }
    return 0;
}
int64_t orc_run_static(OrcRun * r) { return r->opt->static_graphs ? 1 : 0; }
double orc_run_loss_scale(OrcRun * r) {
    ggml_tensor * loss = r->opt->loss;
    if (loss->op == GGML_OP_SCALE) return (double) ggml_get_op_params_f32(loss, 0);
    return 1.0;
}
int64_t orc_run_loss_per_dp(OrcRun * r) { return r->opt->loss_per_datapoint ? 1 : 0; }
int64_t orc_run_ndata(OrcRun * r) { int64_t n = 0; ggml_opt_result_ndata(r->rt, &n); return n; }
int64_t orc_run_nloss(OrcRun * r) { return (int64_t) r->rt->loss.size(); }
double orc_run_loss_at(OrcRun * r, int64_t i) { return (double) r->rt->loss[i]; }
void orc_run_aggregate(OrcRun * r, double * loss, double * unc, double * acc, double * acc_unc) {
    ggml_opt_result_loss(r->rt, loss, unc);
    ggml_opt_result_accuracy(r->rt, acc, acc_unc);
}
void orc_run_free(OrcRun * r) {
    if (!r) return;
    g_run = nullptr;
    ggml_opt_result_free(r->rt);
    ggml_opt_result_free(r->rv);
    ggml_opt_free(r->opt);
    ggml_opt_dataset_free(r->ds);
    ggml_backend_buffer_free(r->model_buf);
    ggml_backend_sched_free(r->sched);
    ggml_backend_free(r->cpu);
    ggml_free(r->ctx_compute);
    ggml_free(r->ctx_model);
    delete r;
}

// dataset mapping helpers
int64_t orc_ds_set_identity(OrcRun * r) {
    for (size_t i = 0; i < r->ds->permutation.size(); ++i) r->ds->permutation[i] = (int64_t) i;
    return (int64_t) r->ds->permutation.size();
}
int64_t orc_ds_nshards(OrcRun * r) { return (int64_t) r->ds->permutation.size(); }
int64_t orc_ds_shard_at(OrcRun * r, int64_t i) { return r->ds->permutation[i]; }
int64_t orc_ds_get_batch_host(OrcRun * r, int64_t ibatch, float * out, int64_t nfloat) {
    size_t nb = (size_t) nfloat * sizeof(float);
    memset(out, 0, nb);
    ggml_opt_dataset_get_batch_host(r->ds, out, nb, nullptr, ibatch);
    return nfloat;
}
int64_t orc_ds_shuffle(OrcRun * r, int64_t idata) {
    ggml_opt_dataset_shuffle(r->opt, r->ds, idata);
    return (int64_t) r->ds->permutation.size();
}
int64_t orc_ds_shard_count_per_batch(OrcRun * r, int64_t nb_data_batch) {
    return (int64_t)((size_t) nb_data_batch / r->ds->nbs_data);
}
int64_t orc_ds_nbs(OrcRun * r) { return (int64_t) r->ds->nbs_data; }

// optimizer name / defaults
const char * orc_optimizer_name(int32_t id) { return ggml_opt_optimizer_name((ggml_opt_optimizer_type) id); }
int64_t orc_defaults(int64_t * out) {
    ggml_opt_params p = ggml_opt_default_params(nullptr, GGML_OPT_LOSS_TYPE_MEAN);
    out[0] = (int64_t) p.loss_type;
    out[1] = (int64_t) p.build_type;
    out[2] = (int64_t) p.opt_period;
    out[3] = (int64_t) p.optimizer;
    out[4] = p.get_opt_pars == ggml_opt_get_default_optimizer_params ? 1 : 0;
    out[5] = p.get_opt_pars_ud == nullptr ? 1 : 0;
    out[6] = p.ctx_compute == nullptr ? 1 : 0;
    out[7] = p.inputs == nullptr ? 1 : 0;
    out[8] = p.outputs == nullptr ? 1 : 0;
    out[9] = p.backend_sched == nullptr ? 1 : 0;
    return 10;
}


// ---- extra accessors: real run state ----

int64_t orc_run_nbatches(OrcRun * r) { return r->ds->data->ne[1] / r->inputs->ne[1]; }
int64_t orc_run_ndata_batch(OrcRun * r) { return r->inputs->ne[1]; }
int64_t orc_run_ncorrect(OrcRun * r) { return r->rt->ncorrect; }
int64_t orc_run_loss_size(OrcRun * r) { return (int64_t) r->rt->loss.size(); }
int64_t orc_run_train_count(OrcRun * r) {
    int64_t n = 0;
    for (int i = 0; i < r->nrec; ++i) if (r->rec[i].train) n++;
    return n;
}
int64_t orc_run_eval_count(OrcRun * r) { return (int64_t) r->nrec - orc_run_train_count(r); }
int64_t orc_run_period(OrcRun * r) { return r->opt->opt_period; }
int64_t orc_run_result_ndata(OrcRun * r) { return r->rt->ndata; }

// dataset with labels, for the shard-byte policy
int64_t orc_dataset_shard_bytes(int64_t n_in, int64_t n_label, int64_t ndata,
                                int64_t ndata_shard, int64_t * out) {
    ggml_opt_dataset_t ds = ggml_opt_dataset_init(GGML_TYPE_F32, GGML_TYPE_F32,
                                                  n_in, n_label, ndata, ndata_shard);
    out[0] = (int64_t) ds->nbs_data;
    out[1] = (int64_t) ds->nbs_labels;
    ggml_opt_dataset_free(ds);
    return 2;
}

// native ggml_opt_fit split arithmetic, lines 1025-1026 (transcription)
int64_t orc_fit_split(int64_t nbatches_logical, float val_split, int64_t opt_period,
                      int64_t nbatch_physical, int64_t * out) {
    const int64_t ibatch_split = int64_t(((1.0f - val_split) * nbatches_logical)) * opt_period;
    out[0] = ibatch_split;
    out[1] = ibatch_split * nbatch_physical;
    return 2;
}

// native shuffle range, lines 153-160 (transcription of the limit only)
int64_t orc_shuffle_limit(int64_t idata, int64_t ndata_shard) {
    if (idata < 0) return -1;
    return idata / ndata_shard;
}

// identity permutation, native shuffle, report how far it moved and whether
// the suffix stayed identity
int64_t orc_run_shuffle_props(OrcRun * r, int64_t idata, int64_t * out) {
    const int64_t n = (int64_t) r->ds->permutation.size();
    for (int64_t i = 0; i < n; ++i) r->ds->permutation[i] = i;
    ggml_opt_dataset_shuffle(r->opt, r->ds, idata);
    int64_t moved = 0;
    int64_t suffix_ok = 1;
    int64_t k = orc_shuffle_limit(idata, r->ds->ndata_shard);
    if (k < 0) k = n;
    for (int64_t i = 0; i < n; ++i) {
        if (r->ds->permutation[i] != i) moved = i + 1;
        if (i >= k && r->ds->permutation[i] != i) suffix_ok = 0;
    }
    out[0] = moved;
    out[1] = suffix_ok;
    return 2;
}

void orc_run_set_perm(OrcRun * r, const int64_t * perm, int64_t n) {
    for (int64_t i = 0; i < n && i < (int64_t) r->ds->permutation.size(); ++i) {
        r->ds->permutation[i] = perm[i];
    }
}

// ---- standalone result object ----

void * orc_res_new(void) { return (void *) ggml_opt_result_init(); }
void orc_res_free(void * p) { ggml_opt_result_free((ggml_opt_result_t) p); }
void orc_res_set(void * p, int64_t ndata, int64_t ncorrect, int64_t opt_period, int64_t per_dp) {
    ggml_opt_result_t r = (ggml_opt_result_t) p;
    r->ndata = ndata;
    r->ncorrect = ncorrect;
    r->opt_period = opt_period;
    r->loss_per_datapoint = per_dp != 0;
}
void orc_res_push(void * p, float v) { ((ggml_opt_result_t) p)->loss.push_back(v); }
void orc_res_get(void * p, double * loss, double * unc, double * acc, double * acc_unc) {
    ggml_opt_result_t r = (ggml_opt_result_t) p;
    ggml_opt_result_loss(r, loss, unc);
    ggml_opt_result_accuracy(r, acc, acc_unc);
}
int64_t orc_res_ndata(void * p) { int64_t n = -1; ggml_opt_result_ndata((ggml_opt_result_t) p, &n); return n; }

} // extern "C"
