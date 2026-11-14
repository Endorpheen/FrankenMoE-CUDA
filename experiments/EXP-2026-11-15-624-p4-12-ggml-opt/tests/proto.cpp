#include <cstdio>
#include <cstdint>
extern "C" {
struct OrcRun;
OrcRun * orc_run_new(int64_t,int64_t,int64_t,int64_t,int64_t,int32_t,int32_t,int32_t);
void orc_run_epoch(OrcRun*, int64_t);
int orc_run_nrec(OrcRun*);
int32_t orc_run_rec_train(OrcRun*,int);
int64_t orc_run_rec_ibatch(OrcRun*,int);
int64_t orc_run_rec_max(OrcRun*,int);
int64_t orc_run_iter(OrcRun*);
int32_t orc_run_opt_i(OrcRun*);
int64_t orc_run_params_width(OrcRun*);
int64_t orc_run_accumulate(OrcRun*);
int64_t orc_run_momenta(OrcRun*);
double orc_run_loss_scale(OrcRun*);
int64_t orc_run_loss_per_dp(OrcRun*);
void orc_run_free(OrcRun*);
int64_t orc_ds_set_identity(OrcRun*);
int64_t orc_ds_get_batch_host(OrcRun*, int64_t, float*, int64_t);
int64_t orc_ds_shard_count_per_batch(OrcRun*, int64_t);
}
int main(){
  OrcRun* r = orc_run_new(2, 3, 4, 16, 4, 2, 0 /*MEAN*/, 0 /*ADAMW*/);
  printf("ds=%lld width=%lld acc=%lld mom=%lld scale=%g perdp=%lld\n",
    (long long)orc_ds_set_identity(r), (long long)orc_run_params_width(r),
    (long long)orc_run_accumulate(r), (long long)orc_run_momenta(r),
    orc_run_loss_scale(r), (long long)orc_run_loss_per_dp(r));
  long long spb = (long long)orc_ds_shard_count_per_batch(r, 8*4);
  float out[8];
  orc_ds_get_batch_host(r, 1, out, 8);
  printf("spb=%lld batch1:", spb);
  for(int i=0;i<8;i++) printf(" %g", out[i]);
  printf("\n");
  orc_run_epoch(r, 8);
  printf("nrec=%d iter=%lld opt_i=%d\n", orc_run_nrec(r), (long long)orc_run_iter(r), orc_run_opt_i(r));
  for(int i=0;i<orc_run_nrec(r);i++) printf("  rec train=%d ib=%lld max=%lld\n", orc_run_rec_train(r,i), (long long)orc_run_rec_ibatch(r,i), (long long)orc_run_rec_max(r,i));
  orc_run_free(r);
  return 0;
}
