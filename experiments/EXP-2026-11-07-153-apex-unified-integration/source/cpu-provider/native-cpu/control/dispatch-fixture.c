#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void fg143_cpu_control(int64_t,int64_t,int64_t,int64_t,int64_t*);
static int op,empty,takeover,selected,mask,called;
int64_t fg143_ctl_meta(int64_t c,int64_t n,int64_t field){(void)c;(void)n;if(field==0)return op;if(field>=10&&field<14)return empty?0:1;if(field==8)return mask;return 0;}
int64_t fg143_ctl_extra_work(int64_t c,int64_t n,int64_t t){(void)c;(void)n;(void)t;return -1;}
int64_t fg143_ctl_goose_work(int64_t c,int64_t n,int64_t t){(void)c;(void)n;(void)t;return 0;}
uint8_t fg143_ctl_extra_compute(int64_t c){(void)c;return takeover;}
int64_t fg143_job_get(int64_t c,int64_t f){(void)c;(void)f;abort();}
void fg143_job_set(int64_t c,int64_t f,int64_t v){(void)c;(void)f;(void)v;abort();}
void fg143_job_create(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_free(int64_t c){(void)c;abort();}
void fg143_job_team(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_barrier(void){abort();}
void fg143_job_place(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_compute(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_job_clear_affinity(void){abort();}
void fg149_job_kickoff(int64_t c,int64_t t){(void)c;(void)t;abort();}
void fg143_ctl_leaf_dup(int64_t c){(void)c;selected=31;++called;}
void fg143_ctl_leaf_add(int64_t c){(void)c;selected=1;++called;}
void fg143_ctl_leaf_add_id(int64_t c){(void)c;selected=3;++called;}
void fg143_ctl_leaf_add1(int64_t c){(void)c;selected=2;++called;}
void fg143_ctl_leaf_acc(int64_t c){(void)c;selected=0;++called;}
void fg143_ctl_leaf_sub(int64_t c){(void)c;selected=86;++called;}
void fg143_ctl_leaf_mul(int64_t c){(void)c;selected=53;++called;}
void fg143_ctl_leaf_div(int64_t c){(void)c;selected=27;++called;}
void fg143_ctl_leaf_sqr(int64_t c){(void)c;selected=82;++called;}
void fg143_ctl_leaf_sqrt(int64_t c){(void)c;selected=83;++called;}
void fg143_ctl_leaf_log(int64_t c){(void)c;selected=48;++called;}
void fg143_ctl_leaf_sin(int64_t c){(void)c;selected=78;++called;}
void fg143_ctl_leaf_cos(int64_t c){(void)c;selected=17;++called;}
void fg143_ctl_leaf_sum(int64_t c){(void)c;selected=87;++called;}
void fg143_ctl_leaf_sum_rows(int64_t c){(void)c;selected=88;++called;}
void fg143_ctl_leaf_cumsum(int64_t c){(void)c;selected=22;++called;}
void fg143_ctl_leaf_mean(int64_t c){(void)c;selected=52;++called;}
void fg143_ctl_leaf_argmax(int64_t c){(void)c;selected=6;++called;}
void fg143_ctl_leaf_count_equal(int64_t c){(void)c;selected=18;++called;}
void fg143_ctl_leaf_repeat(int64_t c){(void)c;selected=65;++called;}
void fg143_ctl_leaf_repeat_back(int64_t c){(void)c;selected=66;++called;}
void fg143_ctl_leaf_concat(int64_t c){(void)c;selected=10;++called;}
void fg143_ctl_leaf_silu_back(int64_t c){(void)c;selected=77;++called;}
void fg143_ctl_leaf_norm(int64_t c){(void)c;selected=56;++called;}
void fg143_ctl_leaf_rms_norm(int64_t c){(void)c;selected=67;++called;}
void fg143_ctl_leaf_rms_norm_back(int64_t c){(void)c;selected=68;++called;}
void fg143_ctl_leaf_group_norm(int64_t c){(void)c;selected=41;++called;}
void fg143_ctl_leaf_l2_norm(int64_t c){(void)c;selected=45;++called;}
void fg143_ctl_leaf_mul_mat(int64_t c){(void)c;selected=54;++called;}
void fg143_ctl_leaf_mul_mat_id(int64_t c){(void)c;selected=55;++called;}
void fg143_ctl_leaf_out_prod(int64_t c){(void)c;selected=59;++called;}
void fg143_ctl_leaf_scale(int64_t c){(void)c;selected=74;++called;}
void fg143_ctl_leaf_set(int64_t c){(void)c;selected=75;++called;}
void fg143_ctl_leaf_cpy(int64_t c){(void)c;selected=19;++called;}
void fg143_ctl_leaf_cont(int64_t c){(void)c;selected=11;++called;}
void fg143_ctl_leaf_get_rows(int64_t c){(void)c;selected=37;++called;}
void fg143_ctl_leaf_get_rows_back(int64_t c){(void)c;selected=38;++called;}
void fg143_ctl_leaf_set_rows(int64_t c){(void)c;selected=76;++called;}
void fg143_ctl_leaf_diag(int64_t c){(void)c;selected=24;++called;}
void fg143_ctl_leaf_diag_mask_inf(int64_t c){(void)c;selected=25;++called;}
void fg143_ctl_leaf_diag_mask_zero(int64_t c){(void)c;selected=26;++called;}
void fg143_ctl_leaf_soft_max(int64_t c){(void)c;selected=79;++called;}
void fg143_ctl_leaf_soft_max_back(int64_t c){(void)c;selected=80;++called;}
void fg143_ctl_leaf_rope(int64_t c){(void)c;selected=70;++called;}
void fg143_ctl_leaf_rope_back(int64_t c){(void)c;selected=71;++called;}
void fg143_ctl_leaf_clamp(int64_t c){(void)c;selected=8;++called;}
void fg143_ctl_leaf_conv_transpose_1d(int64_t c){(void)c;selected=15;++called;}
void fg143_ctl_leaf_im2col(int64_t c){(void)c;selected=42;++called;}
void fg143_ctl_leaf_im2col_back(int64_t c){(void)c;selected=44;++called;}
void fg143_ctl_leaf_im2col_3d(int64_t c){(void)c;selected=43;++called;}
void fg143_ctl_leaf_col2im_1d(int64_t c){(void)c;selected=9;++called;}
void fg143_ctl_leaf_conv_2d(int64_t c){(void)c;selected=12;++called;}
void fg143_ctl_leaf_conv_3d(int64_t c){(void)c;selected=14;++called;}
void fg143_ctl_leaf_conv_2d_dw(int64_t c){(void)c;selected=13;++called;}
void fg143_ctl_leaf_conv_transpose_2d(int64_t c){(void)c;selected=16;++called;}
void fg143_ctl_leaf_pool_1d(int64_t c){(void)c;selected=62;++called;}
void fg143_ctl_leaf_pool_2d(int64_t c){(void)c;selected=63;++called;}
void fg143_ctl_leaf_pool_2d_back(int64_t c){(void)c;selected=64;++called;}
void fg143_ctl_leaf_upscale(int64_t c){(void)c;selected=93;++called;}
void fg143_ctl_leaf_pad(int64_t c){(void)c;selected=60;++called;}
void fg143_ctl_leaf_pad_reflect_1d(int64_t c){(void)c;selected=61;++called;}
void fg143_ctl_leaf_roll(int64_t c){(void)c;selected=69;++called;}
void fg143_ctl_leaf_arange(int64_t c){(void)c;selected=5;++called;}
void fg143_ctl_leaf_timestep_embedding(int64_t c){(void)c;selected=89;++called;}
void fg143_ctl_leaf_argsort(int64_t c){(void)c;selected=7;++called;}
void fg143_ctl_leaf_top_k(int64_t c){(void)c;selected=90;++called;}
void fg143_ctl_leaf_leaky_relu(int64_t c){(void)c;selected=46;++called;}
void fg143_ctl_leaf_tri(int64_t c){(void)c;selected=91;++called;}
void fg143_ctl_leaf_fill(int64_t c){(void)c;selected=32;++called;}
void fg143_ctl_leaf_flash_attn_ext(int64_t c){(void)c;selected=34;++called;}
void fg143_ctl_leaf_flash_attn_back(int64_t c,int64_t m){(void)c;if(m!=mask)abort();selected=33;++called;}
void fg143_ctl_leaf_ssm_conv(int64_t c){(void)c;selected=84;++called;}
void fg143_ctl_leaf_ssm_scan(int64_t c){(void)c;selected=85;++called;}
void fg143_ctl_leaf_win_part(int64_t c){(void)c;selected=94;++called;}
void fg143_ctl_leaf_win_unpart(int64_t c){(void)c;selected=95;++called;}
void fg143_ctl_leaf_unary(int64_t c){(void)c;selected=92;++called;}
void fg143_ctl_leaf_glu(int64_t c){(void)c;selected=40;++called;}
void fg143_ctl_leaf_get_rel_pos(int64_t c){(void)c;selected=36;++called;}
void fg143_ctl_leaf_add_rel_pos(int64_t c){(void)c;selected=4;++called;}
void fg143_ctl_leaf_rwkv_wkv6(int64_t c){(void)c;selected=72;++called;}
void fg143_ctl_leaf_gated_linear_attn(int64_t c){(void)c;selected=39;++called;}
void fg143_ctl_leaf_rwkv_wkv7(int64_t c){(void)c;selected=73;++called;}
void fg143_ctl_leaf_solve_tri(int64_t c){(void)c;selected=81;++called;}
void fg143_ctl_leaf_gated_delta_net(int64_t c){(void)c;selected=35;++called;}
void fg143_ctl_leaf_lightning_indexer(int64_t c){(void)c;selected=47;++called;}
void fg143_ctl_leaf_dsv4_hc_comb(int64_t c){(void)c;selected=28;++called;}
void fg143_ctl_leaf_dsv4_hc_pre(int64_t c){(void)c;selected=30;++called;}
void fg143_ctl_leaf_dsv4_hc_post(int64_t c){(void)c;selected=29;++called;}
void fg143_ctl_leaf_map_custom1(int64_t c){(void)c;selected=49;++called;}
void fg143_ctl_leaf_map_custom2(int64_t c){(void)c;selected=50;++called;}
void fg143_ctl_leaf_map_custom3(int64_t c){(void)c;selected=51;++called;}
void fg143_ctl_leaf_custom(int64_t c){(void)c;selected=23;++called;}
void fg143_ctl_leaf_cross_entropy_loss(int64_t c){(void)c;selected=20;++called;}
void fg143_ctl_leaf_cross_entropy_loss_back(int64_t c){(void)c;selected=21;++called;}
void fg143_ctl_leaf_opt_step_adamw(int64_t c){(void)c;selected=57;++called;}
void fg143_ctl_leaf_opt_step_sgd(int64_t c){(void)c;selected=58;++called;}
static const int expected[]={-1,31,1,3,2,0,86,53,27,82,83,48,78,17,87,88,22,52,6,18,65,66,10,77,56,67,68,41,45,54,55,59,74,75,19,11,-1,-1,-1,-1,37,38,76,24,25,26,79,80,70,71,8,15,42,44,43,9,12,14,13,16,62,63,64,93,60,61,69,5,89,7,90,46,91,32,34,33,84,85,94,95,36,4,72,39,73,81,35,47,28,30,29,92,49,50,51,23,20,21,57,58,40};
int main(void){int cases=0;
 for(op=0;op<101;++op)for(empty=0;empty<2;++empty)for(takeover=0;takeover<2;++takeover){
  mask=op==75?1:0;selected=-1;called=0;fg143_cpu_control(2,0,0,0,NULL);
  int want=empty||takeover?-1:expected[op];if(selected!=want||called!=(want>=0)){fprintf(stderr,"bad dispatch op=%d empty=%d extra=%d selected=%d want=%d\n",op,empty,takeover,selected,want);abort();}++cases;
 }
 printf("{\"status\":\"PASS_ALL_101_DISPATCH_ENTRIES\",\"entries\":101,\"leaf_callbacks\":96,\"cases\":%d}\n",cases);return 0;}
