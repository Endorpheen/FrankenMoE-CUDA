from pathlib import Path
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107');D=A/'source/expert-cache'
(D/'lifecycle-bridge.h').write_text('''#pragma once
#include <cstdint>
#include <vector>
struct llama_model;
struct fg151_request {
 void*state=nullptr;const llama_model*model=nullptr;
 const std::vector<ggml_backend_t>*backends=nullptr;
 const std::vector<ggml_backend_buffer_type_t>*bufts=nullptr;
 size_t bytes=0;std::vector<ggml_tensor*>experts;std::vector<size_t>tensors;std::vector<int64_t>model_indices;size_t host_tensors=0;
 int64_t(*read)(fg151_request*,int64_t,int64_t,int64_t)=nullptr;
 void(*write)(fg151_request*,int64_t,int64_t,int64_t,int64_t)=nullptr;
 int64_t(*service)(fg151_request*,int64_t,int64_t,int64_t,int64_t)=nullptr;
};
extern "C" int64_t fg151_lifecycle(int64_t,int64_t);
extern "C" int64_t fg151_meta(int64_t,int64_t,int64_t,int64_t);
extern "C" void fg151_put(int64_t,int64_t,int64_t,int64_t,int64_t);
extern "C" int64_t fg151_action(int64_t,int64_t,int64_t,int64_t,int64_t);
''')
(D/'lifecycle.goose').write_text('''extern "fg151_meta" fn get(c:i64,field:i64,i:i64,j:i64)->i64;
extern "fg151_put" fn put(c:i64,field:i64,i:i64,j:i64,value:i64);
extern "fg151_action" fn action(c:i64,op:i64,a:i64,b:i64,d:i64)->i64;
extern "fg151_witness" fn witness(mode:i64);
fn add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);assert(a==0||b<=9223372036854775807/a);return a*b;}
fn aligned(value:i64,alignment:i64)->i64 {assert(alignment>0&&(alignment&(alignment-1))==0);return mul(add(value,alignment-1)/alignment,alignment);}
fn collect(c:i64,layer:i64) {
    let cleared=action(c,0,0,0,0);
    for kind in 4 {let tensor=get(c,58,layer,kind);if tensor!=0 {let ignored=action(c,1,tensor,0,0);}}
}
fn host_weight(c:i64,t:i64)->bool {return get(c,43,t,0)!=0&&get(c,44,t,0)==get(c,11,0,0)&&get(c,45,t,0)!=0;}
fn same_layout(c:i64,group:i64)->bool {
    let count=get(c,24,0,0);if count!=get(c,27,group,0) {return false;}
    for i in count {
        let a=get(c,25,i,0);let b=get(c,28,group,i);
        if get(c,40,a,0)!=get(c,40,b,0)||get(c,42,a,2)!=get(c,42,b,2) {return false;}
        for axis in 4 {if get(c,41,a,axis)!=get(c,41,b,axis) {return false;}}
    }
    return true;
}
fn bank_size(c:i64,group:i64,slots:i64)->i64 {
    let device=get(c,26,group,0);let alignment=get(c,23,device,0);var bytes:i64=0;
    for bank in get(c,27,group,0) {let t=get(c,28,group,bank);bytes=add(bytes,aligned(mul(get(c,42,t,2),add(slots,1)),alignment));}return bytes;
}
fn cleanup(c:i64) {
    // Temporary views precede their bank parents;each buffer precedes its owning tensor context.
    let a=action(c,26,0,0,0);let views=action(c,17,-2,0,0);
    let host=action(c,18,-1,0,0);let host_context=action(c,17,-1,0,0);
    for device in get(c,2,0,0) {let buffer=action(c,18,device,0,0);let context=action(c,17,device,0,0);}
}
fn fail(c:i64,reason:i64)->i64 {cleanup(c);let ignored=action(c,5,reason,0,0);assert(false);return 0;}
fn context(c:i64,id:i64,tensors:i64)->bool {
    let bytes=mul(tensors,get(c,55,0,0));return action(c,8,id,bytes,0)!=0;
}
fn attach_dummy(c:i64,id:i64,ctx:i64)->bool {
    let buffer=action(c,14,id,1,0);if buffer==0 {return false;}
    var tensor=get(c,47,ctx,0);
    while tensor!=0 {put(c,7,tensor,0,buffer);tensor=get(c,48,ctx,tensor);}return true;
}
fn construct(c:i64)->i64 {
    for i in get(c,0,0,0) {
        let kind=get(c,15,i,0);
        if kind==get(c,8,0,0)||kind==get(c,9,0,0) {
            let device=action(c,2,i,0,0);var selected:i64= -1;
            for j in get(c,12,0,0) {if get(c,13,j,0)==get(c,16,i,0) {selected=j;break;}}
            put(c,8,device,0,selected);
        }
    }
    if get(c,2,0,0)==0 {return fail(c,0);}
    if get(c,4,0,0)==get(c,10,0,0) {return fail(c,1);}
    if get(c,5,0,0)==0||get(c,6,0,0)==0 {return fail(c,2);}
    for layer in get(c,1,0,0) {
        collect(c,layer);var eligible=get(c,24,0,0)>0;
        for i in get(c,24,0,0) {if !host_weight(c,get(c,25,i,0)) {eligible=false;}}
        if eligible {
            var device:i64= -1;
            for i in get(c,2,0,0) {if get(c,17,i,0)==get(c,14,layer,0) {device=i;break;}}
            if device>=0 {
                var group:i64= -1;
                for i in get(c,3,0,0) {if get(c,26,i,0)==device&&same_layout(c,i) {group=i;break;}}
                if group<0 {group=action(c,3,device,0,0);}
                let added=action(c,4,group,layer,0);
                for i in get(c,24,0,0) {let bytes=get(c,46,get(c,25,i,0),0);put(c,0,device,0,add(get(c,18,device,0),bytes));put(c,1,group,0,add(get(c,32,group,0),bytes));}
            }
        }
    }
    if get(c,3,0,0)==0 {let ignored=action(c,6,0,0,0);return 1;}
    let budget=action(c,7,0,0,0);
    if get(c,34,0,0)==0 {return fail(c,3);}
    for group in get(c,3,0,0) {
        if get(c,31,group,0)>0 {let ignored=action(c,9,group,0,0);}else {let ignored=action(c,22,group,0,0);}
    }
    for device in get(c,2,0,0) {let count=get(c,33,device,0);if count>0&&!context(c,device,count) {return fail(c,4);}}
    if !context(c,-1,get(c,34,0,0))||!context(c,-2,2) {return fail(c,4);}
    let host_alignment=get(c,56,0,0);
    for group in get(c,3,0,0) {
        let slots=get(c,31,group,0);
        if slots>0 {
            for bank in get(c,27,group,0) {if action(c,10,group,bank,add(slots,1))==0 {return fail(c,4);}}
            for j in get(c,29,group,0) {
                let layer=get(c,30,group,j);put(c,5,layer,0,group);collect(c,layer);let copied=action(c,11,layer,0,0);
                for bank in get(c,37,layer,0) {if action(c,12,group,layer,bank)==0 {return fail(c,4);}}
                let slot_map=action(c,13,group,layer,0);if slot_map==0 {return fail(c,4);}
                put(c,4,0,0,add(get(c,52,0,0),aligned(get(c,46,slot_map,0),host_alignment)));
            }
            let device=get(c,26,group,0);put(c,3,device,0,add(get(c,21,device,0),bank_size(c,group,slots)));
        }
    }
    if get(c,7,0,0)!=0 {
        for device in get(c,2,0,0) {let ctx=get(c,22,device,0);if ctx!=0&&!attach_dummy(c,device,ctx) {return fail(c,5);}}
        if !attach_dummy(c,-1,get(c,49,0,0)) {return fail(c,5);}
    }else {
        for device in get(c,2,0,0) {if get(c,22,device,0)!=0 {
            let buffer=action(c,14,device,0,0);if buffer==0 {return fail(c,5);}
            let cleared=action(c,15,device,0,0);put(c,3,device,0,get(c,57,buffer,0));
        }}
        let host=action(c,14,-1,0,0);if host==0 {return fail(c,5);}
        let cleared=action(c,15,-1,255,0);put(c,4,0,0,get(c,57,host,0));
        for group in get(c,3,0,0) {for j in get(c,29,group,0) {
            let layer=get(c,30,group,j);if get(c,36,layer,0)!=0 {let linked=action(c,19,group,layer,0);}
        }}
    }
    for device in get(c,2,0,0) {if get(c,20,device,0)!=0 {let ignored=action(c,16,device,0,0);}}
    let weights=action(c,16,-1,0,0);
    for device in get(c,2,0,0) {if get(c,18,device,0)>0 {
        let logged=action(c,20,device,0,0);
        for group in get(c,3,0,0) {if get(c,26,group,0)==device {let logged=action(c,21,group,0,0);}}
    }}return 1;
}
fn entry(mode:i64,c:i64)->i64 {
    witness(mode);
    if mode==0 {return construct(c);}
    assert(mode==1);let logged=action(c,23,0,0,0);cleanup(c);return 0;
}
fn main(){print(entry(1,0));}
''')
(D/'lifecycle-unit.c').write_text('''#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
int64_t fg151_meta(int64_t,int64_t,int64_t,int64_t);
void fg151_put(int64_t,int64_t,int64_t,int64_t,int64_t);
int64_t fg151_action(int64_t,int64_t,int64_t,int64_t,int64_t);
static atomic_uint_fast64_t calls[2];
static void fg151_witness(int64_t mode){if(mode<0||mode>=2)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg151_lifecycle_unused_main
#include "../../generated/cache-lifecycle.c"
#undef main
int64_t fg151_lifecycle(int64_t mode,int64_t context){return gg_function_1(mode,context,0);}
__attribute__((destructor))static void fg151_report(void){for(int i=0;i<2;i++){uint64_t n=atomic_load(calls+i);if(n)fprintf(stderr,"FG151_LIFECYCLE mode=%d calls=%llu\\n",i,(unsigned long long)n);}}
''')
print('Whole cache construction/cleanup controller implemented in Goose;native ABI services next')
