#include "ggml.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(2);}}while(0)

/* Defined in the CPU backend TU (inputs/cpu-integrated.c:3974) but absent from
 * this ggml.h snapshot; parent must confirm this prototype against the full
 * tree header (ggml-cpu.h) before building. */
extern enum ggml_status ggml_graph_compute_with_ctx(struct ggml_context *, struct ggml_cgraph *, int);

static uint64_t h = 1469598103934665603ULL;
static double maxdiff = 0.0;
static void mixf(float f) { uint64_t u = 0; memcpy(&u, &f, 4); h ^= u; h *= 1099511628211ULL; }

/* Stride-aware element access; nb[0..3] are honoured exactly like ggml does. */
static float   f_at(struct ggml_tensor*t,int64_t a,int64_t b,int64_t c,int64_t d){return *(float*)   ((char*)t->data+a*t->nb[0]+b*t->nb[1]+c*t->nb[2]+d*t->nb[3]);}
static void    f_set(struct ggml_tensor*t,int64_t a,int64_t b,int64_t c,int64_t d,float v){*(float*)((char*)t->data+a*t->nb[0]+b*t->nb[1]+c*t->nb[2]+d*t->nb[3])=v;}
static int32_t i_at(struct ggml_tensor*t,int64_t a,int64_t b,int64_t c,int64_t d){return *(int32_t*)((char*)t->data+a*t->nb[0]+b*t->nb[1]+c*t->nb[2]+d*t->nb[3]);}
static void    i_set(struct ggml_tensor*t,int64_t a,int64_t b,int64_t c,int64_t d,int32_t v){*(int32_t*)((char*)t->data+a*t->nb[0]+b*t->nb[1]+c*t->nb[2]+d*t->nb[3])=v;}

/* Reference activations. The GEGLU tolerance also absorbs the optional
 * GGML_GELU_FP16 lookup-table path of some CPU builds (see .md). */
static float silu_ref(float x){return x/(1.0f+expf(-x));}
static float gelu_ref(float x){return 0.5f*x*(1.0f+tanhf(0.7978845608028654f*x*(1.0f+0.044715f*x*x)));}

static void rangef(struct ggml_tensor*t,unsigned seed){
  for(int64_t r=0;r<t->ne[1];r++)for(int64_t c=0;c<t->ne[0];c++){
    unsigned u=seed+(unsigned)(r*131+c*17)*2654435761u;
    f_set(t,c,r,0,0,(float)((int)(u%2001u)-1000)/400.0f);
  }
}

static void qfill(struct ggml_tensor*w,unsigned seed){
  const int64_t ne0=w->ne[0],rows=w->ne[1]*w->ne[2]*w->ne[3];
  CHECK(ggml_is_quantized(w->type));
  CHECK(!ggml_quantize_requires_imatrix(w->type));
  CHECK(ne0%ggml_blck_size(w->type)==0);
  float*src=(float*)malloc((size_t)rows*ne0*sizeof(float));CHECK(src);
  for(int64_t r=0;r<rows;r++)for(int64_t c=0;c<ne0;c++){
    unsigned u=seed+(unsigned)(r*ne0+c)*1103515245u+12345u;
    src[r*ne0+c]=(float)((int)(u%2001u)-1000)/500.0f;
  }
  CHECK(ggml_quantize_chunk(w->type,src,w->data,0,rows,ne0,NULL)>0);
  free(src);
}

/* Mirrors ggml_compute_forward_get_rows_q (inputs/ggml-native-ops.cpp:5288):
 * src row offset = i01*nb01 + i11*nb02 + i12*nb03, dequantised with the same
 * ggml_get_type_traits(type)->to_float used by the kernel, hence bit-exact. */
static void check_rows(struct ggml_tensor*w,struct ggml_tensor*ix,struct ggml_tensor*o,double tol){
  CHECK(o->type==GGML_TYPE_F32);
  CHECK(o->ne[0]==w->ne[0]&&o->ne[1]==ix->ne[0]&&o->ne[2]==w->ne[2]&&o->ne[3]==w->ne[3]);
  const struct ggml_type_traits*tr=ggml_get_type_traits(w->type);CHECK(tr&&tr->to_float);
  const int64_t nc=o->ne[0];
  float*ref=(float*)malloc((size_t)nc*sizeof(float));CHECK(ref);
  double dmax=0.0;
  for(int64_t i3=0;i3<o->ne[3];i3++)for(int64_t i2=0;i2<o->ne[2];i2++)for(int64_t i1=0;i1<o->ne[1];i1++){
    const int32_t sr=i_at(ix,i1,i2,i3,0);
    CHECK(sr>=0&&sr<w->ne[1]);
    tr->to_float((const void*)((char*)w->data+sr*w->nb[1]+i2*w->nb[2]+i3*w->nb[3]),ref,nc);
    for(int64_t c=0;c<nc;c++){
      const float got=f_at(o,c,i1,i2,i3);
      const double dd=fabs((double)got-(double)ref[c]);
      if(dd>dmax)dmax=dd;
      mixf(got);CHECK(dd<=tol);
    }
  }
  free(ref);if(dmax>maxdiff)maxdiff=dmax;
  printf("rows %s maxdiff=%.6g\n",ggml_type_name(w->type),dmax);
}

/* Fused GLU: non-swapped layout puts activation a in [0,nc), gate b in [nc,2*nc)
 * (inputs/ggml-native-ops.cpp:3079-3082). */
static void check_fused(struct ggml_tensor*in,struct ggml_tensor*out,enum ggml_glu_op op,double tol){
  CHECK(out->ne[0]*2==in->ne[0]&&out->ne[1]==in->ne[1]);
  const int64_t nc=out->ne[0];
  double dmax=0.0;
  for(int64_t r=0;r<out->ne[1];r++)for(int64_t c=0;c<nc;c++){
    const float a=f_at(in,c,r,0,0),b=f_at(in,c+nc,r,0,0);
    const float want=op==GGML_GLU_OP_REGLU?(a>0.0f?a:0.0f)*b:op==GGML_GLU_OP_GEGLU?gelu_ref(a)*b:silu_ref(a)*b;
    const float got=f_at(out,c,r,0,0);
    CHECK(fabsf(got)<1.0e3f);   /* detects a padded-row poison leak */
    const double dd=fabs((double)got-(double)want);
    if(dd>dmax)dmax=dd;
    mixf(got);CHECK(dd<=tol);
  }
  if(dmax>maxdiff)maxdiff=dmax;
  printf("glu_fused op=%d maxdiff=%.6g\n",(int)op,dmax);
}

/* Split GLU: a = src0 (activation), b = src1 (gate), identical shapes. */
static void check_split(struct ggml_tensor*a,struct ggml_tensor*b,struct ggml_tensor*out,enum ggml_glu_op op,double tol){
  CHECK(a->ne[0]==out->ne[0]&&b->ne[0]==out->ne[0]);
  CHECK(a->ne[1]==out->ne[1]&&b->ne[1]==out->ne[1]);
  double dmax=0.0;
  for(int64_t r=0;r<out->ne[1];r++)for(int64_t c=0;c<out->ne[0];c++){
    const float x=f_at(a,c,r,0,0),y=f_at(b,c,r,0,0);
    const float want=op==GGML_GLU_OP_REGLU?(x>0.0f?x:0.0f)*y:op==GGML_GLU_OP_GEGLU?gelu_ref(x)*y:silu_ref(x)*y;
    const float got=f_at(out,c,r,0,0);
    const double dd=fabs((double)got-(double)want);
    if(dd>dmax)dmax=dd;
    mixf(got);CHECK(dd<=tol);
  }
  if(dmax>maxdiff)maxdiff=dmax;
  printf("glu_split op=%d maxdiff=%.6g\n",(int)op,dmax);
}

int main(void){
  struct ggml_context*c=ggml_init((struct ggml_init_params){16777216,NULL,false});CHECK(c);
  struct ggml_tensor*w4 =ggml_new_tensor_2d(c,GGML_TYPE_Q4_K,256,8);
  struct ggml_tensor*w6 =ggml_new_tensor_2d(c,GGML_TYPE_Q6_K,256,8);
  struct ggml_tensor*w4b=ggml_new_tensor_3d(c,GGML_TYPE_Q4_K,256,6,2);
  struct ggml_tensor*ix4=ggml_new_tensor_2d(c,GGML_TYPE_I32,6,1);
  struct ggml_tensor*ix6=ggml_new_tensor_2d(c,GGML_TYPE_I32,5,1);
  struct ggml_tensor*ixw=ggml_new_tensor_3d(c,GGML_TYPE_I32,4,2,2);
  struct ggml_tensor*ixb=ggml_view_3d(c,ixw,4,2,1,ixw->nb[1],ixw->nb[2],0);
  struct ggml_tensor*gf =ggml_new_tensor_2d(c,GGML_TYPE_F32,128,4);
  struct ggml_tensor*gp =ggml_new_tensor_2d(c,GGML_TYPE_F32,140,4);
  struct ggml_tensor*gv =ggml_view_2d(c,gp,128,4,gp->nb[1],0);
  struct ggml_tensor*ga =ggml_new_tensor_2d(c,GGML_TYPE_F32,32,3);
  struct ggml_tensor*gb =ggml_new_tensor_2d(c,GGML_TYPE_F32,32,3);
  struct ggml_tensor*ge =ggml_new_tensor_2d(c,GGML_TYPE_F32,64,2);
  struct ggml_tensor*leaves[]={w4,w6,w4b,ix4,ix6,ixw,gf,gp,ga,gb,ge};
  for(size_t i=0;i<sizeof(leaves)/sizeof(leaves[0]);i++){CHECK(leaves[i]);ggml_set_input(leaves[i]);}
  CHECK(ixb&&ixb->ne[0]==4&&ixb->ne[1]==2&&ixb->ne[2]==1&&ixb->ne[3]==1);
  CHECK(gv&&gv->ne[0]==128&&gv->ne[1]==4&&gv->nb[1]==gp->nb[1]&&gp->nb[1]>128*sizeof(float));

  qfill(w4,11);qfill(w6,23);qfill(w4b,37);

  static const int32_t s4[6]={5,0,5,2,7,0};          /* repeated, unordered */
  static const int32_t s6[5]={7,7,1,0,3};
  for(int i=0;i<6;i++)i_set(ix4,i,0,0,0,s4[i]);
  for(int i=0;i<5;i++)i_set(ix6,i,0,0,0,s6[i]);
  /* 3D indices: batch 0 = {4,0,4,3}, batch 1 = {1,5,2,1}; the third plane is
   * filled with out-of-range values so a wrong read trips the bounds assert. */
  static const int32_t sw[12]={4,0,4,3,1,5,2,1,99,-1,99,-1};
  for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<4;x++)i_set(ixw,x,y,z,0,sw[z*8+y*4+x]);

  rangef(gf,101);rangef(ga,201);rangef(gb,301);rangef(ge,401);
  /* Padded view: columns [0,128) real, [128,140) poison must never be read. */
  for(int64_t r=0;r<gp->ne[1];r++)for(int64_t cc=0;cc<gp->ne[0];cc++){
    if(cc<128){unsigned u=501u+(unsigned)(r*131+cc*17)*2654435761u;f_set(gp,cc,r,0,0,(float)((int)(u%2001u)-1000)/400.0f);}
    else f_set(gp,cc,r,0,0,1.0e6f);
  }

  struct ggml_tensor*o4 =ggml_get_rows(c,w4,ix4);
  struct ggml_tensor*o6 =ggml_get_rows(c,w6,ix6);
  struct ggml_tensor*o4b=ggml_get_rows(c,w4b,ixb);
  struct ggml_tensor*og1=ggml_swiglu(c,gf);
  struct ggml_tensor*og2=ggml_reglu(c,gv);
  struct ggml_tensor*og3=ggml_geglu(c,ge);
  struct ggml_tensor*og4=ggml_swiglu_split(c,ga,gb);
  struct ggml_tensor*outs[]={o4,o6,o4b,og1,og2,og3,og4};
  for(size_t i=0;i<sizeof(outs)/sizeof(outs[0]);i++){CHECK(outs[i]);ggml_set_output(outs[i]);}

  CHECK(o4->type==GGML_TYPE_F32&&o4->ne[0]==256&&o4->ne[1]==6&&o4->ne[2]==1&&o4->ne[3]==1);
  CHECK(o6->type==GGML_TYPE_F32&&o6->ne[0]==256&&o6->ne[1]==5);
  CHECK(o4b->type==GGML_TYPE_F32&&o4b->ne[0]==256&&o4b->ne[1]==4&&o4b->ne[2]==2&&o4b->ne[3]==1);
  CHECK(og1->ne[0]==64&&og1->ne[1]==4&&og1->ne[2]==1);
  CHECK(og2->ne[0]==64&&og2->ne[1]==4);
  CHECK(og3->ne[0]==32&&og3->ne[1]==2);
  CHECK(og4->ne[0]==32&&og4->ne[1]==3);

  struct ggml_cgraph*g=ggml_new_graph(c);CHECK(g);
  for(size_t i=0;i<sizeof(outs)/sizeof(outs[0]);i++)ggml_build_forward_expand(g,outs[i]);
  CHECK(ggml_graph_n_nodes(g)>=7);
  CHECK(ggml_graph_compute_with_ctx(c,g,1)==GGML_STATUS_SUCCESS);

  check_rows(w4,ix4,o4,0.0);      /* same to_float as the kernel: expect exact */
  check_rows(w6,ix6,o6,0.0);
  check_rows(w4b,ixb,o4b,0.0);
  check_fused(gf,og1,GGML_GLU_OP_SWIGLU,1e-4);
  check_fused(gv,og2,GGML_GLU_OP_REGLU,1e-6);
  check_fused(ge,og3,GGML_GLU_OP_GEGLU,2e-2);
  check_split(ga,gb,og4,GGML_GLU_OP_SWIGLU,1e-4);

  printf("{\"status\":\"PASS_LOCAL_APEX_FIXTURE\",\"cases\":7,\"maxdiff\":%.6g,\"hash\":\"%016llx\"}\n",
         maxdiff,(unsigned long long)h);
  ggml_free(c);
  return 0;
}
