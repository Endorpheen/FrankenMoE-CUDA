#pragma once
// worker-4: C ABI between the generated Goose modules and the native bridge.
// Every import returns int64_t; handles are opaque int64_t.
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// execution module imports
int64_t fg107_ctx(int64_t, int64_t, int64_t);
int64_t fg107_ctx_set(int64_t, int64_t, int64_t);
int64_t fg107_perf(int64_t, int64_t, int64_t, int64_t);
int64_t fg107_log(int64_t, int64_t, int64_t, int64_t);
int64_t fg107_batch(int64_t, int64_t, int64_t, int64_t);
int64_t fg107_ub(int64_t, int64_t, int64_t, int64_t);
int64_t fg107_balloc(int64_t, int64_t, int64_t, int64_t);
int64_t fg107_mem(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_mctx(int64_t, int64_t, int64_t);
int64_t fg107_res_prev(int64_t);
int64_t fg107_res_prev_active(int64_t);
int64_t fg107_res_set_prev_active(int64_t, int64_t);
int64_t fg107_res_reset_all_prev(int64_t);
int64_t fg107_res(int64_t, int64_t, int64_t, int64_t);
int64_t fg107_res_sampling_tensor(int64_t, int64_t, int64_t);
int64_t fg107_graph_params(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_model_build_graph(int64_t, int64_t);
int64_t fg107_graph_reserve(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_graph(int64_t, int64_t, int64_t);
int64_t fg107_sched(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_tensor(int64_t, int64_t, int64_t);
int64_t fg107_tensor_get_async(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_tensor_set_async(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_backend_synchronize(int64_t);
int64_t fg107_moe(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_op_mul_mat_id(void);
int64_t fg107_out(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_out_bind(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_out_view_swap(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_exec(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_exec_copy_id_at(int64_t, int64_t);

// resource module imports
int64_t fg107_rq(int64_t, int64_t, int64_t);
int64_t fg107_rq_set(int64_t, int64_t, int64_t);
int64_t fg107_r(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_r_state(int64_t, int64_t, int64_t, int64_t, int64_t);

// policy/control imports (context-policy.goose)
int64_t fg107_pol(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg107_cpi(int64_t, int64_t, int64_t, int64_t);
float   fg107_cpf(int64_t, int64_t, int64_t, float);
int64_t fg107_cpc(int64_t, int64_t);
int64_t fg107_cplog(int64_t, int64_t, int64_t, int64_t);
int64_t fg107_cpmodel(int64_t);

#ifdef __cplusplus
}
#endif
