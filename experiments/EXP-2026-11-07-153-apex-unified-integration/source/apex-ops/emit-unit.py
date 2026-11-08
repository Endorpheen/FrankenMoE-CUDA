from pathlib import Path
import re
r=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107')
g=(r/'generated/apex-ops/kernel.c').read_text()
init='\n'.join(re.findall(r'^  gg_globals\.v\d+ = .*?;$',g,re.M))
assert len(init.splitlines())>40
text='''#include <stdint.h>
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
'''+init+'''
}
int64_t fg_apex_ops_entry(int64_t mode,const int64_t*args){return gg_function_1(mode,(intptr_t)args,0);}
__attribute__((destructor))static void fg_apex_ops_report(void){for(int i=0;i<67;i++){uint64_t n=atomic_load_explicit(fg_counts+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_APEX_OP mode=%d calls=%llu\\n",i,(unsigned long long)n);}}
'''
(r/'source/apex-ops/unit.c').write_text(text)
print('wrote unit with',len(init.splitlines()),'generated constant initializers')
