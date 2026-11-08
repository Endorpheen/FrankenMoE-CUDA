#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "apex-ops.h"
#include "../tensor-core/bridge.h"
static atomic_uint_fast64_t fg_counts[67];
static void fg_apex_ops_witness(int64_t mode){if(mode<0||mode>=67)__builtin_trap();atomic_fetch_add_explicit(fg_counts+mode,1,memory_order_relaxed);}
static int64_t fg_apex_ops_alloc(int64_t,int64_t,int64_t,const void*,int64_t,int64_t);
#define main fg_apex_ops_unused_main
#include "../../generated/apex-ops/kernel.c"
#undef main
struct fg_apex_ops_shape_abi {int64_t ne[4];};
_Static_assert(sizeof(gg_type_4)==sizeof(struct fg_apex_ops_shape_abi),"Goose shape size ABI");
_Static_assert(offsetof(gg_type_4,f0)==offsetof(struct fg_apex_ops_shape_abi,ne),"Goose shape offset ABI");
_Static_assert(sizeof(((gg_type_4*)0)->f0.data)==sizeof(((struct fg_apex_ops_shape_abi*)0)->ne),"Goose shape array ABI");
static int64_t fg_apex_ops_alloc(int64_t ctx,int64_t kind,int64_t dims,const void*s,int64_t view,int64_t off){return fg153_entry(5,ctx,kind,dims,(intptr_t)s,view,off);}
__attribute__((constructor))static void fg_apex_ops_initialize_constants(void){
  gg_globals.v0 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_globals.v1 = ((int64_t)(((int64_t)UINT64_C(5))));
  gg_globals.v2 = ((int64_t)(((int64_t)UINT64_C(2))));
  gg_globals.v3 = ((int64_t)(((int64_t)UINT64_C(4))));
  gg_globals.v4 = ((int64_t)(((int64_t)UINT64_C(3))));
  gg_globals.v5 = ((int64_t)(((int64_t)UINT64_C(69))));
  gg_globals.v6 = ((int64_t)(((int64_t)UINT64_C(50))));
  gg_globals.v7 = ((int64_t)(((int64_t)UINT64_C(22))));
  gg_globals.v8 = ((int64_t)(((int64_t)UINT64_C(35))));
  gg_globals.v9 = ((int64_t)(((int64_t)UINT64_C(34))));
  gg_globals.v10 = ((int64_t)(((int64_t)UINT64_C(16))));
  gg_globals.v11 = ((int64_t)(((int64_t)UINT64_C(43))));
  gg_globals.v12 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_globals.v13 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_globals.v14 = ((int64_t)(((int64_t)UINT64_C(73))));
  gg_globals.v15 = ((int64_t)(((int64_t)UINT64_C(74))));
  gg_globals.v16 = ((int64_t)(((int64_t)UINT64_C(86))));
  gg_globals.v17 = ((int64_t)(((int64_t)UINT64_C(40))));
  gg_globals.v18 = ((int64_t)(((int64_t)UINT64_C(100))));
  gg_globals.v19 = ((int64_t)(((int64_t)UINT64_C(28))));
  gg_globals.v20 = ((int64_t)(((int64_t)UINT64_C(17))));
  gg_globals.v21 = ((int64_t)(((int64_t)UINT64_C(7))));
  gg_globals.v22 = ((int64_t)(((int64_t)UINT64_C(29))));
  gg_globals.v23 = ((int64_t)(((int64_t)UINT64_C(30))));
  gg_globals.v24 = ((int64_t)(((int64_t)UINT64_C(24))));
  gg_globals.v25 = ((int64_t)(((int64_t)UINT64_C(64))));
  gg_globals.v26 = ((int64_t)(((int64_t)UINT64_C(38))));
  gg_globals.v27 = ((int64_t)(((int64_t)UINT64_C(20))));
  gg_globals.v28 = ((int64_t)(((int64_t)UINT64_C(36))));
  gg_globals.v29 = ((int64_t)(((int64_t)UINT64_C(25))));
  gg_globals.v30 = ((int64_t)(((int64_t)UINT64_C(48))));
  gg_globals.v31 = ((int64_t)(((int64_t)UINT64_C(32))));
  gg_globals.v32 = ((int64_t)(((int64_t)UINT64_C(33))));
  gg_globals.v33 = ((int64_t)(((int64_t)UINT64_C(42))));
  gg_globals.v34 = ((int64_t)(((int64_t)UINT64_C(46))));
  gg_globals.v35 = ((int64_t)(((int64_t)UINT64_C(85))));
  gg_globals.v36 = ((int64_t)(((int64_t)UINT64_C(9))));
  gg_globals.v37 = ((int64_t)(((int64_t)UINT64_C(10))));
  gg_globals.v38 = ((int64_t)(((int64_t)UINT64_C(76))));
  gg_globals.v39 = ((int64_t)(((int64_t)UINT64_C(6))));
  gg_globals.v40 = ((int64_t)(((int64_t)UINT64_C(14))));
  gg_globals.v41 = ((int64_t)(((int64_t)UINT64_C(15))));
  gg_globals.v42 = ((int64_t)(((int64_t)UINT64_C(70))));
  gg_globals.v43 = ((int64_t)(((int64_t)UINT64_C(39))));
  gg_globals.v44 = ((int64_t)(((int64_t)UINT64_C(72))));
  gg_globals.v45 = ((int64_t)(((int64_t)UINT64_C(91))));
  gg_globals.v46 = ((int64_t)(((int64_t)UINT64_C(37))));
  gg_globals.v47 = ((int64_t)(((int64_t)UINT64_C(8))));
  gg_globals.v48 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_globals.v49 = ((int64_t)(((int64_t)UINT64_C(1))));
  gg_globals.v50 = ((int64_t)(((int64_t)UINT64_C(0))));
  gg_globals.v51 = ((int64_t)(((int64_t)UINT64_C(26))));
  gg_globals.v52 = ((int64_t)(((int64_t)UINT64_C(27))));
}
int64_t fg_apex_ops_entry(int64_t mode,const int64_t*args){return gg_function_1(mode,(intptr_t)args,0);}
__attribute__((destructor))static void fg_apex_ops_report(void){for(int i=0;i<67;i++){uint64_t n=atomic_load_explicit(fg_counts+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_APEX_OP mode=%d calls=%llu\n",i,(unsigned long long)n);}}
