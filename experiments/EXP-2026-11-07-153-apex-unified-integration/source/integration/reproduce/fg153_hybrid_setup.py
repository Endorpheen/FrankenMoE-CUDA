from pathlib import Path
import hashlib,json,re,shutil
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/hybrid-memory';D.mkdir(exist_ok=True)
U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/src');native=U/'llama-memory-hybrid.cpp'
(D/'native.cpp').write_text(native.read_text())
(D/'kernel.goose').write_text('''extern "fg153_hm_get" fn hm_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_hm_put" fn hm_put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_hm_call" fn hm_call(op:i64,p:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_hm_witness" fn hm_witness(mode:i64);
fn hm_ensure(ok:bool) {if !ok {let failed=hm_call(99,0,0,0,0,0);assert(false);}}
fn hm_fail(status:i64)->bool {return status==2||status==3;}
fn hm_combine(a:i64,b:i64)->i64 {
 if hm_fail(a) {return a;}if hm_fail(b) {return b;}
 if a==0||b==0 {return 0;}return 1;
}
fn hm_batch(p:i64,n:i64,all:i64)->i64 {
 let reset=hm_call(0,p,0,0,0,0);
 while true {
  var tokens:i64=0;
  if all!=0 {tokens=hm_call(1,p,n,0,0,0);}
  else {var tail=hm_get(p,3,0);hm_ensure(tail>=0&&tail<4294967295);if tail>0 {tail+=1;}var sequential:i64=0;if hm_get(p,2,0)!=1 {sequential=1;}tokens=hm_call(2,p,n,sequential,tail,0);}
  if tokens==0 {break;}
  let pushed=hm_call(3,p,0,0,0,0);
 }
 if hm_get(p,0,0)<hm_get(p,1,0) {return hm_call(8,p,0,0,0,0);}
 if hm_call(5,p,0,0,0,0)==0 {return hm_call(8,p,1,0,0,0);}
 if hm_call(6,p,0,0,0,0)==0 {return hm_call(8,p,2,0,0,0);}
 return hm_call(7,p,0,0,0,0);
}
fn hm_entry(mode:i64,p:i64,a:i64,b:i64,c:i64,d:i64)->i64 {
 hm_witness(mode);
 if mode==0 {return hm_batch(p,a,b);}
 if mode==1 {return hm_call(9,p,0,0,0,0);}
 if mode==2 {return hm_call(10,p,a,b,0,0);}
 if mode==3 {return hm_call(11,p,0,0,0,0);}
 if mode==4 {let x=hm_call(12,p,0,a,0,0);let y=hm_call(12,p,1,a,0,0);return 0;}
 if mode==5 {if hm_call(13,p,1,a,b,c)==0 {return 0;}return hm_call(13,p,0,a,b,c);}
 if mode==6 {let x=hm_call(14,p,0,a,b,c);let y=hm_call(14,p,1,a,b,c);return 0;}
 if mode==7 {let x=hm_call(15,p,0,a,0,0);let y=hm_call(15,p,1,a,0,0);return 0;}
 if mode==8 {let x=hm_call(16,p,0,a,b,c);let y=hm_call(16,p,1,a,b,c);return 0;}
 if mode==9 {let x=hm_call(17,p,0,a,b,c);let y=hm_call(17,p,1,a,b,c);return 0;}
 if mode==10 {let x=hm_call(18,p,0,a,0,0);let y=hm_call(18,p,1,a,0,0);return if x>y {x} else {y};}
 if mode==11 {let x=hm_call(19,p,0,a,0,0);let y=hm_call(19,p,1,a,0,0);return if x<y {x} else {y};}
 if mode==12 {return hm_combine(a,b);}
 if mode==13 {return if hm_fail(a) {1} else {0};}
 if mode==14 {
  hm_ensure(hm_get(p,10,0)==0);let x=hm_call(30,p,0,0,0,0);let y=hm_call(30,p,1,0,0,0);
  let next=hm_get(p,11,0)+1;hm_put(p,11,0,next);return if next<hm_get(p,12,0) {1} else {0};
 }
 if mode==15 {hm_ensure(!hm_fail(hm_get(p,10,0)));let x=hm_call(31,p,0,0,0,0);let y=hm_call(31,p,1,0,0,0);return if x!=0&&y!=0 {1} else {0};}
 if mode==16 {return hm_get(p,10,0);}
 if mode==17 {hm_ensure(hm_get(p,10,0)==0);let index=hm_get(p,11,0);hm_ensure(index>=0&&index<hm_get(p,12,0));return hm_call(33,p,index,0,0,0);}
 if mode==18 {let x=hm_call(42,p,0,a,b,0);let y=hm_call(42,p,1,a,b,0);return 0;}
 if mode==19 {
  if hm_call(43,p,0,a,b,0)!=0 {return hm_call(44,p,0,0,0,0);}
  if hm_call(43,p,1,a,b,0)!=0 {let clear=hm_call(45,p,a,0,0,0);return hm_call(44,p,0,0,0,0);}
  return 0;
 }
 if mode==20 {
  for side in 2 {
   let loaded=hm_call(46,p,side,0,0,0);var index:i64=0;
   while index<hm_get(p,20,0) {
    let key=hm_get(p,21,index);let value=hm_get(p,22,index);let old=hm_call(48,p,key,0,0,0);
    hm_ensure(value>=0&&old>=0&&old<=9223372036854775807-value);
    let stored=hm_call(49,p,key,old+value,0,0);index+=1;
   }
  }
  return 0;
 }
 hm_ensure(false);return 0;
}
fn main() {print(hm_entry(12,0,0,1,0,0));}
''')
(D/'bridge.h').write_text('''#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fg153_hybrid_enabled(void);
int64_t fg153_hybrid(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
int64_t fg153_hm_get(int64_t,int64_t,int64_t);
void fg153_hm_put(int64_t,int64_t,int64_t,int64_t);
int64_t fg153_hm_call(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_hm_witness(int64_t);
#ifdef __cplusplus
}
#endif
''')
(D/'frame.hpp').write_text('''#pragma once
#include "llama-memory-hybrid.h"
#include "bridge.h"
#include <exception>
struct FG153HybridFrame {
 llama_memory_hybrid *hybrid=nullptr;
 llama_kv_cache *attn=nullptr;
 llama_memory_recurrent *recr=nullptr;
 llama_batch_allocr *batch=nullptr;
 llama_ubatch current={};
 std::vector<llama_ubatch> ubatches;
 llama_kv_cache::slot_info_vec_t slots;
 int64_t fourth=0;
 const llama_memory_context_ptr *ca=nullptr,*cr=nullptr;
 size_t *next=nullptr;
 const std::vector<llama_ubatch> *contexts=nullptr;
 llama_memory_status status=LLAMA_MEMORY_STATUS_SUCCESS;
 void *io=nullptr;
 std::exception_ptr failure;
 std::vector<std::pair<ggml_backend_buffer_type_t,size_t>> breakdown;
 std::map<ggml_backend_buffer_type_t,size_t> result;
 explicit FG153HybridFrame(const llama_memory_hybrid *m):hybrid(const_cast<llama_memory_hybrid*>(m)),attn(m->get_mem_attn()),recr(m->get_mem_recr()){}
 FG153HybridFrame(const llama_memory_context_ptr &a,const llama_memory_context_ptr &r,size_t &i,const std::vector<llama_ubatch> &u,llama_memory_status s):ca(&a),cr(&r),next(&i),contexts(&u),status(s){}
};
''')
(D/'services.cpp').write_text('''#include "frame.hpp"
#include "llama-impl.h"
#include <cstdlib>
#include <cstring>
static FG153HybridFrame &hf(int64_t p){GGML_ASSERT(p);return *(FG153HybridFrame*)(intptr_t)p;}
static llama_memory_i *child(FG153HybridFrame &f,int64_t side){GGML_ASSERT(side==0||side==1);return side?(llama_memory_i*)f.recr:(llama_memory_i*)f.attn;}
static llama_memory_context_i *context(FG153HybridFrame &f,int64_t side){GGML_ASSERT(side==0||side==1);auto *v=side?f.cr:f.ca;GGML_ASSERT(v&&v->get());return v->get();}
extern "C" int fg153_hybrid_enabled(void){auto *v=getenv("GGML_GOOSE_HYBRID");return !v||strcmp(v,"0")!=0;}
extern "C" int64_t fg153_hm_get(int64_t p,int64_t k,int64_t i){auto &f=hf(p);switch(k){
case 0:return f.batch->get_n_used();case 1:return f.batch->get_n_tokens();case 2:return f.attn->get_n_stream();case 3:return f.recr->n_rs_seq;
case 10:return f.status;case 11:return *f.next;case 12:return f.contexts->size();case 20:return f.breakdown.size();
case 21:return(intptr_t)f.breakdown.at(i).first;case 22:return f.breakdown.at(i).second;
default:GGML_ABORT("invalid Goose hybrid field");}}
extern "C" void fg153_hm_put(int64_t p,int64_t k,int64_t,int64_t v){auto &f=hf(p);GGML_ASSERT(k==11&&v>=0);*f.next=(size_t)v;}
extern "C" int64_t fg153_hm_call(int64_t op,int64_t p,int64_t a,int64_t b,int64_t c,int64_t d){if(op==99)GGML_ABORT("Goose hybrid invariant failed");auto &f=hf(p);switch(op){
case 0:f.batch->split_reset();return 0;
case 1:f.current=f.batch->split_seq(a);return f.current.n_tokens;
case 2:f.current=f.batch->split_equal(a,b,c);return f.current.n_tokens;
case 3:f.ubatches.push_back(std::move(f.current));return 0;
case 5:return f.recr->prepare(f.ubatches);
case 6:f.slots=f.attn->prepare(f.ubatches);return !f.slots.empty();
case 7:return(intptr_t)new llama_memory_hybrid_context(f.hybrid,std::move(f.slots),std::move(f.ubatches));
case 8:if(a)LLAMA_LOG_ERROR("Goose hybrid: failed to prepare %s ubatches\\n",a==1?"recurrent":"attention");return(intptr_t)new llama_memory_hybrid_context(LLAMA_MEMORY_STATUS_FAILED_PREPARE);
case 9:return(intptr_t)new llama_memory_hybrid_context(f.hybrid);
case 10:return(intptr_t)new llama_memory_hybrid_context(f.hybrid,(llama_context*)(intptr_t)a,b);
case 11:return f.attn->get_can_shift();
case 12:child(f,a)->clear(b);return 0;
case 13:return child(f,a)->seq_rm(b,c,d);
case 14:child(f,a)->seq_cp(b,c,d,f.fourth);return 0;
case 15:child(f,a)->seq_keep(b);return 0;
case 16:child(f,a)->seq_add(b,c,d,f.fourth);return 0;
case 17:child(f,a)->seq_div(b,c,d,f.fourth);return 0;
case 18:return child(f,a)->seq_pos_min(b);
case 19:return child(f,a)->seq_pos_max(b);
case 30:return context(f,a)->next();case 31:return context(f,a)->apply();
case 33:return(intptr_t)&f.contexts->at(a);
case 42:child(f,a)->state_write(*(llama_io_write_i*)f.io,b,c);return 0;
case 43:try{child(f,a)->state_read(*(llama_io_read_i*)f.io,b,c);return 0;}catch(...){f.failure=std::current_exception();return 1;}
case 44:GGML_ASSERT(f.failure);std::rethrow_exception(f.failure);
case 45:f.attn->state_clear(a);return 0;
case 46:{auto values=child(f,a)->memory_breakdown();f.breakdown.assign(values.begin(),values.end());return 0;}
case 48:return f.result[(ggml_backend_buffer_type_t)(intptr_t)a];
case 49:GGML_ASSERT(b>=0);f.result[(ggml_backend_buffer_type_t)(intptr_t)a]=(size_t)b;return 0;
default:GGML_ABORT("invalid Goose hybrid primitive");}}
''')
s=native.read_text();s=s.replace('#include "llama-context.h"','#include "llama-context.h"\n#include "frame.hpp"',1)
def hook(signature,body):
 global s
 assert s.count(signature)==1,signature
 s=s.replace(signature,signature+'\n    if(fg153_hybrid_enabled()){'+body+'}\n',1)
prefix='FG153HybridFrame frame(this);'
ptr='(intptr_t)&frame'
hook('llama_memory_context_ptr llama_memory_hybrid::init_batch(llama_batch_allocr & balloc, uint32_t n_ubatch, bool embd_all) {',prefix+'frame.batch=&balloc;return llama_memory_context_ptr((llama_memory_context_i*)(intptr_t)fg153_hybrid(0,'+ptr+',n_ubatch,embd_all,0,0));')
for name,signature,mode,args,pre,ret in [
 ('full','llama_memory_context_ptr llama_memory_hybrid::init_full() {',1,'0,0,0,0','','ptr'),
 ('update','llama_memory_context_ptr llama_memory_hybrid::init_update(llama_context * lctx, bool optimize) {',2,'(intptr_t)lctx,optimize,0,0','','ptr'),
 ('shift','bool llama_memory_hybrid::get_can_shift() const {',3,'0,0,0,0','','bool'),
 ('clear','void llama_memory_hybrid::clear(bool data) {',4,'data,0,0,0','','void'),
 ('rm','bool llama_memory_hybrid::seq_rm(llama_seq_id seq_id, llama_pos p0, llama_pos p1) {',5,'seq_id,p0,p1,0','','bool'),
 ('cp','void llama_memory_hybrid::seq_cp(llama_seq_id seq_id_src, llama_seq_id seq_id_dst, llama_pos p0, llama_pos p1) {',6,'seq_id_src,seq_id_dst,p0,0','frame.fourth=p1;','void'),
 ('keep','void llama_memory_hybrid::seq_keep(llama_seq_id seq_id) {',7,'seq_id,0,0,0','','void'),
 ('add','void llama_memory_hybrid::seq_add(llama_seq_id seq_id, llama_pos p0, llama_pos p1, llama_pos shift) {',8,'seq_id,p0,p1,0','frame.fourth=shift;','void'),
 ('div','void llama_memory_hybrid::seq_div(llama_seq_id seq_id, llama_pos p0, llama_pos p1, int d) {',9,'seq_id,p0,p1,0','frame.fourth=d;','void'),
 ('min','llama_pos llama_memory_hybrid::seq_pos_min(llama_seq_id seq_id) const {',10,'seq_id,0,0,0','','value'),
 ('max','llama_pos llama_memory_hybrid::seq_pos_max(llama_seq_id seq_id) const {',11,'seq_id,0,0,0','','value'),
 ('write','void llama_memory_hybrid::state_write(llama_io_write_i & io, llama_seq_id seq_id, llama_state_seq_flags flags) const {',18,'seq_id,flags,0,0','frame.io=&io;','void'),
 ('read','void llama_memory_hybrid::state_read(llama_io_read_i & io, llama_seq_id seq_id, llama_state_seq_flags flags) {',19,'seq_id,flags,0,0','frame.io=&io;','void'),
 ('breakdown','std::map<ggml_backend_buffer_type_t, size_t> llama_memory_hybrid::memory_breakdown() const {',20,'0,0,0,0','','map')]:
 call=f'fg153_hybrid({mode},{ptr},{args})'
 body='return llama_memory_context_ptr((llama_memory_context_i*)(intptr_t)'+call+');' if ret=='ptr' else call+';return;' if ret=='void' else call+';return std::move(frame.result);' if ret=='map' else 'return '+call+('!=0;' if ret=='bool' else ';')
 hook(signature,prefix+pre+body)
old='llama_memory_status_combine(ctx_attn->get_status(), ctx_recr->get_status())'
assert s.count(old)==3
s=s.replace(old,'(fg153_hybrid_enabled()?(llama_memory_status)fg153_hybrid(12,0,ctx_attn->get_status(),ctx_recr->get_status(),0,0):'+old+')')
cprefix='FG153HybridFrame frame(ctx_attn,ctx_recr,i_next,ubatches,status);'
for signature,mode,ret in [('bool llama_memory_hybrid_context::next() {',14,'bool'),('bool llama_memory_hybrid_context::apply() {',15,'bool'),('const llama_ubatch & llama_memory_hybrid_context::get_ubatch() const {',17,'ubatch')]:
 pre=cprefix
 if ret=='ubatch':pre='size_t current=i_next;FG153HybridFrame frame(ctx_attn,ctx_recr,current,ubatches,status);'
 call=f'fg153_hybrid({mode},{ptr},0,0,0,0)';body='return *(const llama_ubatch*)(intptr_t)'+call+';' if ret=='ubatch' else 'return '+call+'!=0;'
 hook(signature,pre+body)
(D/'hybrid.cpp').write_text(s)
(D/'source-manifest.json').write_text(json.dumps({'native':str(native),'native_sha256':hashlib.sha256(native.read_bytes()).hexdigest(),'source':'hybrid.cpp','owned_methods':18,'native_rollback':'GGML_GOOSE_HYBRID=0','explicit_residuals':['C++ hybrid constructor and typed child object allocation','child attention/recurrent implementations are separate migration units','temporary native vector/map/unique_ptr storage','plain typed context getters and native object destructors'],'model_gate':'PENDING_COMPLETE_BATCH_A'},indent=2)+'\n')
print('Prepared actual Goose hybrid batch/context/sequence/checkpoint policy with native rollback.')
