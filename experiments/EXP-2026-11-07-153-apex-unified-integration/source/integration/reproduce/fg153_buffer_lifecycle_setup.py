from pathlib import Path
import json,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/buffer-lifecycle';D.mkdir(exist_ok=True)
(D/'kernel.goose').write_text('''extern "fg153_bl_get" fn bl_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_bl_put" fn bl_put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_bl_call" fn bl_call(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_bl_witness" fn bl_witness(mode:i64);
fn bl_ensure(ok:bool){if !ok {let failed=bl_call(99,0,0,0);assert(false);}}
fn bl_add(a:i64,b:i64)->i64{bl_ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn bl_multi_create(array:i64,count:i64)->i64{
 bl_ensure(array!=0&&count>0&&count<=9223372036854775807/8);
 let ctx=bl_call(0,bl_get(0,20,0),0,0);bl_ensure(ctx!=0);
 let storage=bl_call(0,count*8,0,0);if storage==0{let freed=bl_call(1,ctx,0,0);bl_ensure(false);return 0;}
 bl_put(ctx,0,0,storage);bl_put(ctx,1,0,count);var size:i64=0;
 for i in count {let child=bl_get(array,0,i);bl_ensure(child!=0);bl_put(ctx,2,i,child);size=bl_add(size,bl_get(child,4,0));}
 return bl_call(2,bl_get(array,0,0),ctx,size);
}
fn bl_multi(mode:i64,buffer:i64,value:i64)->i64{
 bl_ensure(buffer!=0);let ctx=bl_get(buffer,1,0);bl_ensure(ctx!=0);let count=bl_get(ctx,2,0);
 if mode==3 {bl_ensure(bl_call(9,buffer,0,0)!=0);}
 for i in count {
  let child=bl_get(ctx,3,i);
  if mode==1 {let freed=bl_call(3,child,0,0);}
  if mode==2 {let cleared=bl_call(4,child,value,0);}
  if mode==3 {let used=bl_call(5,child,value,0);}
 }
 if mode==1 {let freed=bl_call(1,bl_get(ctx,8,0),0,0);let freedctx=bl_call(1,ctx,0,0);}
 return 0;
}
fn bl_entry(mode:i64,a:i64,b:i64)->i64{
 bl_witness(mode);
 if mode==0{return bl_multi_create(a,b);}if mode>=1&&mode<=3{return bl_multi(mode,a,b);}
 if mode==4 {if a!=0 {if bl_get(a,5,0)!=0 {let released=bl_call(6,a,0,0);}let deleted=bl_call(7,a,0,0);}return 0;}
 if mode==5 {bl_ensure(a!=0);if bl_get(a,4,0)!=0 {let clear=bl_call(8,a,b,0);}return 0;}
 if mode==6 {bl_ensure(a!=0);if bl_get(a,6,0)!=0 {let reset=bl_call(10,a,0,0);}return 0;}
 if mode==7 {
  bl_ensure(a!=0);bl_put(a,3,0,b);
  if bl_call(9,a,0,0)!=0 {let used=bl_call(11,a,b,0);}else {if bl_call(12,a,0,0)!=0 {let meta=bl_call(13,a,b,0);}}
  return 0;
 }
 if mode==8 {
  bl_ensure(a!=0);if bl_call(12,a,0,0)==0&&bl_get(a,4,0)==0{return 0;}if bl_get(a,7,0)==0{return 0;}
  let base=bl_call(14,a,0,0);bl_ensure(base!=0);return base;
 }
 if mode==9{return 1;}bl_ensure(false);return 0;
}
fn main(){print(bl_entry(9,0,0));}
''')
(D/'bridge.h').write_text('''#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fg153_buffer_lifecycle_enabled(void);
int64_t fg153_buffer_lifecycle(int64_t,int64_t,int64_t);
int64_t fg153_bl_get(int64_t,int64_t,int64_t);
void fg153_bl_put(int64_t,int64_t,int64_t,int64_t);
int64_t fg153_bl_call(int64_t,int64_t,int64_t,int64_t);
void fg153_bl_witness(int64_t);
#ifdef __cplusplus
}
#endif
''')
(D/'projection.inc').write_text('''int fg153_buffer_lifecycle_enabled(void){const char*v=getenv("GGML_GOOSE_BUFFERS");return !v||strcmp(v,"0")!=0;}
int64_t fg153_bl_get(int64_t p,int64_t k,int64_t i){auto*b=(ggml_backend_buffer_t)(intptr_t)p;auto*c=(ggml_backend_multi_buffer_context*)(intptr_t)p;switch(k){
case 0:return(intptr_t)((ggml_backend_buffer_t*)(intptr_t)p)[i];case 1:return(intptr_t)b->context;case 2:return c->n_buffers;case 3:return(intptr_t)c->buffers[i];case 4:return b->size;case 5:return b->iface.free_buffer!=nullptr;case 6:return b->iface.reset!=nullptr;case 7:return b->iface.get_base!=nullptr;case 8:return(intptr_t)c->buffers;case 20:return sizeof(ggml_backend_multi_buffer_context);default:GGML_ABORT("bad Goose buffer lifecycle read");}}
void fg153_bl_put(int64_t p,int64_t k,int64_t i,int64_t v){auto*c=(ggml_backend_multi_buffer_context*)(intptr_t)p;switch(k){case 0:c->buffers=(ggml_backend_buffer_t*)(intptr_t)v;return;case 1:c->n_buffers=v;return;case 2:c->buffers[i]=(ggml_backend_buffer_t)(intptr_t)v;return;case 3:((ggml_backend_buffer_t)(intptr_t)p)->usage=(ggml_backend_buffer_usage)v;return;default:GGML_ABORT("bad Goose buffer lifecycle write");}}
int64_t fg153_bl_call(int64_t op,int64_t a,int64_t b,int64_t c){auto*buffer=(ggml_backend_buffer_t)(intptr_t)a;switch(op){
case 0:return(intptr_t)malloc((size_t)a);case 1:free((void*)(intptr_t)a);return 0;
case 2:return(intptr_t)ggml_backend_buffer_init(buffer->buft,ggml_backend_multi_buffer_i,(void*)(intptr_t)b,(size_t)c);
case 3:ggml_backend_buffer_free(buffer);return 0;case 4:ggml_backend_buffer_clear(buffer,(uint8_t)b);return 0;case 5:ggml_backend_buffer_set_usage(buffer,(ggml_backend_buffer_usage)b);return 0;
case 6:buffer->iface.free_buffer(buffer);return 0;case 7:delete buffer;return 0;case 8:buffer->iface.clear(buffer,(uint8_t)b);return 0;
case 9:return ggml_backend_buffer_is_multi_buffer(buffer);case 10:buffer->iface.reset(buffer);return 0;case 11:ggml_backend_multi_buffer_set_usage(buffer,(ggml_backend_buffer_usage)b);return 0;
case 12:return ggml_backend_buffer_is_meta(buffer);case 13:ggml_backend_meta_buffer_set_usage(buffer,(ggml_backend_buffer_usage)b);return 0;case 14:return(intptr_t)buffer->iface.get_base(buffer);
case 99:GGML_ABORT("Goose buffer lifecycle invariant failed");default:GGML_ABORT("bad Goose buffer lifecycle primitive");}}
''')
base=A/'source/integration/backend.cpp';s=base.read_text();s=s.replace('#include "../buffers/bridge.h"','#include "../buffers/bridge.h"\n#include "../buffer-lifecycle/bridge.h"',1)
if '#include "../buffer-lifecycle/bridge.h"' not in s:s='#include "../buffer-lifecycle/bridge.h"\n'+s
hooks=[('static void ggml_backend_multi_buffer_free_buffer(ggml_backend_buffer_t buffer) {',1,'(intptr_t)buffer,0','void'),('static void ggml_backend_multi_buffer_clear(ggml_backend_buffer_t buffer, uint8_t value) {',2,'(intptr_t)buffer,value','void'),('void ggml_backend_multi_buffer_set_usage(ggml_backend_buffer_t buffer, enum ggml_backend_buffer_usage usage) {',3,'(intptr_t)buffer,usage','void'),('ggml_backend_buffer_t ggml_backend_multi_buffer_alloc_buffer(ggml_backend_buffer_t * buffers, size_t n_buffers) {',0,'(intptr_t)buffers,n_buffers','pointer'),('void ggml_backend_buffer_free(ggml_backend_buffer_t buffer) {',4,'(intptr_t)buffer,0','void'),('void ggml_backend_buffer_clear(ggml_backend_buffer_t buffer, uint8_t value) {',5,'(intptr_t)buffer,value','void'),('void ggml_backend_buffer_reset(ggml_backend_buffer_t buffer) {',6,'(intptr_t)buffer,0','void'),('void ggml_backend_buffer_set_usage(ggml_backend_buffer_t buffer, enum ggml_backend_buffer_usage usage) {',7,'(intptr_t)buffer,usage','void'),('void * ggml_backend_buffer_get_base(ggml_backend_buffer_t buffer) {',8,'(intptr_t)buffer,0','voidptr')]
for sig,mode,args,ret in hooks:
 assert s.count(sig)==1,sig
 call=f'fg153_buffer_lifecycle({mode},{args})';body=call+';return;' if ret=='void' else 'return ('+('ggml_backend_buffer_t' if ret=='pointer' else 'void*')+')(intptr_t)'+call+';'
 s=s.replace(sig,sig+'\n    if(fg153_buffer_lifecycle_enabled()){'+body+'}\n',1)
s+='\n#include "../buffer-lifecycle/projection.inc"\n';(D/'backend.cpp').write_text(s)
(D/'manifest.json').write_text(json.dumps({'base':str(base),'base_sha256':hashlib.sha256(base.read_bytes()).hexdigest(),'owned_entrypoints':[h[0] for h in hooks],'rollback':'GGML_GOOSE_BUFFERS=0','residual':'typed native POD buffer header construction and provider callbacks; OS malloc/free/delete primitives','model_gate':'PENDING_COMPLETE_BATCH_A'},indent=2)+'\n');print('Prepared combined generic/multi-buffer Goose ownership lifecycle.')
