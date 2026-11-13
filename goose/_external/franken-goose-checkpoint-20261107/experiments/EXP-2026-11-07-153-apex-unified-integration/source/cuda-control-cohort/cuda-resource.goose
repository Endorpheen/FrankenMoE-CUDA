// worker-14 / APX025: canonical CUDA private-resource records.
// Implements the schema in cuda-resource-schema.h over worker-8 canonical
// storage.  No global scratch; every record is caller-owned.  Raw CUDA/cuBLAS
// effects are explicit fg14_s_* vendor primitives.
//
// Entry modes: see goose-entry-resource.c.

extern "fg14_s_record_new" fn s_record_new(kind:i64)->i64;
extern "fg14_s_record_get" fn s_record_get(h:i64,key:i64)->i64;
extern "fg14_s_record_set" fn s_record_set(h:i64,key:i64,v:i64)->i64;
extern "fg14_s_record_free" fn s_record_free(h:i64)->i64;
extern "fg14_s_vec_new" fn s_vec_new(item_bytes:i64)->i64;
extern "fg14_s_vec_len" fn s_vec_len(h:i64)->i64;
extern "fg14_s_vec_data" fn s_vec_data(h:i64)->i64;
extern "fg14_s_vec_resize" fn s_vec_resize(h:i64,count:i64)->i64;
extern "fg14_s_vec_free" fn s_vec_free(h:i64)->i64;
extern "fg14_s_map_new" fn s_map_new()->i64;
extern "fg14_s_map_put" fn s_map_put(h:i64,k:i64,v:i64)->i64;
extern "fg14_s_map_erase" fn s_map_erase(h:i64,k:i64)->i64;
extern "fg14_s_map_free" fn s_map_free(h:i64)->i64;
extern "fg14_s_map_get" fn s_map_get(m:i64,k:i64)->i64;
extern "fg14_s_frame_get" fn s_frame_get(p:i64,i:i64)->i64;
extern "fg14_s_frame_set" fn s_frame_set(p:i64,i:i64,v:i64)->i64;
extern "fg14_s_device_set" fn s_device_set(d:i64)->i64;
extern "fg14_s_check" fn s_check(e:i64,site:i64)->i64;
extern "fg14_s_last_error" fn s_last_error()->i64;
extern "fg14_s_clear_error" fn s_clear_error()->i64;
extern "fg14_s_malloc" fn s_malloc(size:i64)->i64;
extern "fg14_s_free" fn s_free(p:i64)->i64;
extern "fg14_s_stream_create" fn s_stream_create()->i64;
extern "fg14_s_stream_destroy" fn s_stream_destroy(s:i64)->i64;
extern "fg14_s_stream_sync" fn s_stream_sync(s:i64)->i64;
extern "fg14_s_cublas_create" fn s_cublas_create(ctx:i64,dev:i64,slot:i64,stream:i64)->i64;
extern "fg14_s_cublas_destroy" fn s_cublas_destroy(h:i64)->i64;
extern "fg14_s_cublas_set_math" fn s_cublas_set_math(h:i64,mode:i64)->i64;
extern "fg14_s_cublas_set_stream" fn s_cublas_set_stream(h:i64,stream:i64)->i64;
extern "fg14_s_cublas_set_workspace" fn s_cublas_set_workspace(h:i64,p:i64,size:i64)->i64;
extern "fg14_s_event_create" fn s_event_create()->i64;
extern "fg14_s_event_destroy" fn s_event_destroy(e:i64)->i64;
extern "fg14_s_event_record" fn s_event_record(e:i64,s:i64)->i64;
extern "fg14_s_stream_wait_event" fn s_stream_wait_event(s:i64,e:i64)->i64;
extern "fg14_s_info_cc" fn s_info_cc(d:i64)->i64;
extern "fg14_s_info_vmm" fn s_info_vmm(d:i64)->i64;
extern "fg14_s_pool_create" fn s_pool_create(ctx:i64,dev:i64,slot:i64,vmm:i64)->i64;
extern "fg14_s_pool_alloc" fn s_pool_alloc(p:i64,size:i64,out:i64)->i64;
extern "fg14_s_pool_free" fn s_pool_free(p:i64,ptr:i64,size:i64)->i64;
extern "fg14_s_pool_destroy" fn s_pool_destroy(p:i64)->i64;
extern "fg14_s_ctx_device" fn s_ctx_device(ctx:i64)->i64;
extern "fg14_s_ctx_curr_stream" fn s_ctx_curr_stream(ctx:i64)->i64;
extern "fg14_s_ctx_stream" fn s_ctx_stream(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg14_s_ctx_stream_set" fn s_ctx_stream_set(ctx:i64,dev:i64,slot:i64,s:i64)->i64;
extern "fg14_s_ctx_cublas" fn s_ctx_cublas(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg14_s_ctx_cublas_set" fn s_ctx_cublas_set(ctx:i64,dev:i64,slot:i64,h:i64)->i64;
extern "fg14_s_ctx_ws_size" fn s_ctx_ws_size(ctx:i64,dev:i64)->i64;
extern "fg14_s_ctx_ws_size_set" fn s_ctx_ws_size_set(ctx:i64,dev:i64,size:i64)->i64;
extern "fg14_s_ctx_copy_event" fn s_ctx_copy_event(ctx:i64)->i64;
extern "fg14_s_ctx_copy_event_set" fn s_ctx_copy_event_set(ctx:i64,e:i64)->i64;
extern "fg14_s_ctx_rec" fn s_ctx_rec(ctx:i64)->i64;
extern "fg14_s_ctx_rec_set" fn s_ctx_rec_set(ctx:i64,r:i64)->i64;
extern "fg14_s_ctx_pool" fn s_ctx_pool(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg14_s_ctx_pool_set" fn s_ctx_pool_set(ctx:i64,dev:i64,slot:i64,p:i64)->i64;
extern "fg14_s_t_get" fn s_t_get(t:i64,f:i64,i:i64)->i64;
extern "fg14_s_t_nbytes" fn s_t_nbytes(t:i64)->i64;
extern "fg14_s_t_view_src" fn s_t_view_src(t:i64)->i64;
extern "fg14_s_t_src" fn s_t_src(t:i64,i:i64)->i64;
extern "fg14_s_g_n" fn s_g_n(g:i64)->i64;
extern "fg14_s_g_np_capture" fn s_g_np_capture(v:i64,g:i64,i:i64)->i64;
extern "fg14_s_t_data" fn s_t_data(t:i64)->i64;

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
 if mode==0 {return ctx_new(a);}
 if mode==1 {return ctx_attach(a,b);}
 if mode==2 {return ctx_free(a);}
 if mode==3 {return s_record_get(a,b);}
 if mode==4 {return s_record_set(a,b,c);}
 if mode==5 {return stream_get(a,b,c);}
 if mode==6 {return cublas_get(a,b,c);}
 if mode==7 {return pool_get(a,b,c);}
 if mode==8 {return pool_alloc(a,b,c,d,e);}
 if mode==9 {return pool_free(a,b,c,d,e);}
 if mode==10 {return graph_get(a,b);}
 if mode==11 {return s_record_get(a,b);}
 if mode==12 {return s_record_set(a,b,c);}
 if mode==13 {return graph_node_props(a,b);}
 if mode==14 {return graph_invalidate(a);}
 if mode==15 {return conc_new(a,b);}
 if mode==16 {return conc_free(a);}
 if mode==17 {return s_record_get(a,b);}
 if mode==18 {return s_record_set(a,b,c);}
 if mode==19 {return conc_stream_get(a,b);}
 if mode==20 {return conc_stream_set(a,b,c);}
 if mode==21 {return s_vec_len(s_record_get(a,4));}
 if mode==22 {return vec_at(s_record_get(a,4),b);}
 if mode==23 {return vec_push(s_record_get(a,4),b);}
 if mode==24 {return vec_at(s_record_get(a,6),b);}
 if mode==25 {return conc_valid(a);}
 if mode==26 {return stream_ctx_reset(a);}
 if mode==27 {return s_vec_len(s_record_get(a,11));}
 if mode==28 {return vec_at(s_record_get(a,11),b);}
 if mode==29 {return s_map_get(s_record_get(a,10),b);}
 if mode==30 {return stream_ctx_insert(a,b,c);}
 if mode==31 {return any_enabled(a);}
 if mode==32 {return any_instance(a);}
 return -1;
}

fn vec_at(v:i64,i:i64)->i64 {if v==0 {return 0;}let d=s_vec_data(v);if d==0 {return 0;}return load_i64(d,i);}
fn vec_push(v:i64,x:i64)->i64 {
 if v==0 {return -1;}
 let n=s_vec_len(v);
 let r=s_vec_resize(v,n+1);
 if r<0 {return r;}
 let d=s_vec_data(v);
 store_i64(d,n,x);
 return 0;
}
fn load_i64(p:i64,i:i64)->i64 {return s_frame_get(p,i);}
fn store_i64(p:i64,i:i64,v:i64)->i64 {return s_frame_set(p,i,v);}

fn ctx_new(device:i64)->i64 {
 let r=s_record_new(1401);
 if r<=0 {return 0;}
 let ignored1=s_record_set(r,1,device);
 let ignored2=s_record_set(r,2,0);
 let streams=s_map_new();
 let cublas=s_map_new();
 let wss=s_map_new();
 let ws=s_map_new();
 let pools=s_map_new();
 let graphs=s_map_new();
 let conc=s_map_new();
 let ck=s_vec_new(8);
 let gk=s_vec_new(8);
 let sk=s_vec_new(8);
 let bk=s_vec_new(8);
 let wk=s_vec_new(8);
 let pk=s_vec_new(8);
 if streams==0||cublas==0||wss==0||ws==0||pools==0||graphs==0||conc==0||ck==0||gk==0||sk==0||bk==0||wk==0||pk==0 {
  if streams!=0 {s_map_free(streams);}
  if cublas!=0 {s_map_free(cublas);}
  if wss!=0 {s_map_free(wss);}
  if ws!=0 {s_map_free(ws);}
  if pools!=0 {s_map_free(pools);}
  if graphs!=0 {s_map_free(graphs);}
  if conc!=0 {s_map_free(conc);}
  if ck!=0 {s_vec_free(ck);}
  if gk!=0 {s_vec_free(gk);}
  if sk!=0 {s_vec_free(sk);}
  if bk!=0 {s_vec_free(bk);}
  if wk!=0 {s_vec_free(wk);}
  if pk!=0 {s_vec_free(pk);}
  s_record_free(r);
  return 0;
 }
 let ignored3=s_record_set(r,4,streams);
 let ignored4=s_record_set(r,5,cublas);
 let ignored5=s_record_set(r,6,wss);
 let ignored6=s_record_set(r,7,ws);
 let ignored7=s_record_set(r,8,pools);
 let ignored8=s_record_set(r,9,graphs);
 let ignored9=s_record_set(r,10,conc);
 let ignored10=s_record_set(r,11,ck);
 let ignored11=s_record_set(r,13,gk);
 let ignored12=s_record_set(r,14,sk);
 let ignored13=s_record_set(r,15,bk);
 let ignored14=s_record_set(r,16,wk);
 let ignored15=s_record_set(r,17,pk);
 let ignored16=s_record_set(r,12,0);
 return r;
}

fn ctx_attach(native_ctx:i64,device:i64)->i64 {
 if native_ctx==0 {return 0;}
 let existing=s_ctx_rec(native_ctx);
 if existing!=0 {return existing;}
 let r=ctx_new(device);
 if r==0 {return 0;}
 let ignored=s_record_set(r,18,native_ctx);
 s_ctx_rec_set(native_ctx,r);
 return r;
}

fn ctx_free(ctx:i64)->i64 {
 if ctx<=0 {return -1;}
 let native=s_record_get(ctx,18);
 let ck=s_record_get(ctx,11);
 let cmap=s_record_get(ctx,10);
 var i=0;
 while i<s_vec_len(ck) {
  let c=s_map_get(cmap,vec_at(ck,i));
  if c>0 {conc_free(c);}
  i+=1;
 }
 let gk=s_record_get(ctx,13);
 let gmap=s_record_get(ctx,9);
 i=0;
 while i<s_vec_len(gk) {
  let g=s_map_get(gmap,vec_at(gk,i));
  if g>0 {
   let np=s_record_get(g,8);
   if np!=0 {s_vec_free(np);}
   s_record_free(g);
  }
  i+=1;
 }
 let sk=s_record_get(ctx,14); let bk=s_record_get(ctx,15);
 let wk=s_record_get(ctx,16); let pk=s_record_get(ctx,17);
 let sm=s_record_get(ctx,4); let bm=s_record_get(ctx,5);
 let wm=s_record_get(ctx,7); let pm=s_record_get(ctx,8);
 i=0; while i<s_vec_len(sk) {let h=s_map_get(sm,vec_at(sk,i));if h>0 {s_stream_destroy(h);}i+=1;}
 i=0; while i<s_vec_len(bk) {let h=s_map_get(bm,vec_at(bk,i));if h>0 {s_cublas_destroy(h);}i+=1;}
 i=0; while i<s_vec_len(wk) {let h=s_map_get(wm,vec_at(wk,i));if h>0 {s_free(h);}i+=1;}
 i=0; while i<s_vec_len(pk) {let h=s_map_get(pm,vec_at(pk,i));if h>0 {s_pool_destroy(h);}i+=1;}
 // explicit child-container release (worker8 records do not own handles)
 s_map_free(gmap); s_map_free(cmap); s_map_free(sm); s_map_free(bm);
 s_map_free(s_record_get(ctx,6)); s_map_free(wm); s_map_free(pm);
 s_vec_free(gk); s_vec_free(ck); s_vec_free(sk); s_vec_free(bk);
 s_vec_free(wk); s_vec_free(pk);
 if native!=0 {s_ctx_rec_set(native,0);}
 s_record_free(ctx);
 return 0;
}

fn map_key(dev:i64,slot:i64)->i64 {return dev*8+slot;}

fn stream_get(ctx:i64,dev:i64,slot:i64)->i64 {
 let m=s_record_get(ctx,4);
 let k=map_key(dev,slot);
 let s=s_map_get(m,k);
 if s>=0 {return s;}
 let ignored=s_check(s_device_set(dev),10);
 let ns=s_stream_create();
 s_check(s_last_error(),11);
 if ns==0 {return 0;}
 let r=s_map_put(m,k,ns);
 if r<0 {return 0;}
 let keys=s_record_get(ctx,14);
 let ignored2=vec_push(keys,k);
 return ns;
}

fn cublas_get(ctx:i64,dev:i64,slot:i64)->i64 {
 let m=s_record_get(ctx,5);
 let k=map_key(dev,slot);
 let h=s_map_get(m,k);
 if h>=0 {return h;}
 let ignored=s_check(s_device_set(dev),12);
 let stream=stream_get(ctx,dev,slot);
 var nh=s_cublas_create(0,dev,slot,stream);
 if nh==0 {s_check(1,13);}
 if nh!=0 {
  let ignored2=s_cublas_set_math(nh,1);
  let ignored3=s_cublas_set_stream(nh,stream);
  var ws=s_map_get(s_record_get(ctx,6),dev);
  if ws<0 {
   let cc=s_info_cc(dev);
   ws=if cc>=900 {32*1024*1024} else {4*1024*1024};
   let ignored7=s_map_put(s_record_get(ctx,6),dev,ws);
  }
  let wp=s_malloc(ws);
  s_check(s_last_error(),14);
  s_check(s_cublas_set_workspace(nh,wp,ws),15);
  let wsmap=s_record_get(ctx,7);
  let ignored4=s_map_put(wsmap,k,wp);
  let ignored5=vec_push(s_record_get(ctx,16),k);
 }
 let ignored6=s_map_put(m,k,nh);
 let ignored7=vec_push(s_record_get(ctx,15),k);
 return nh;
}

fn pool_get(ctx:i64,dev:i64,slot:i64)->i64 {
 let m=s_record_get(ctx,8);
 let k=map_key(dev,slot);
 let p=s_map_get(m,k);
 if p>=0 {return p;}
 let native=s_record_get(ctx,18);
 let vmm=s_info_vmm(dev);
 let np=s_pool_create(native,dev,slot,vmm);
 if np==0 {s_check(1,16);return 0;}
 let ignored=s_map_put(m,k,np);
 let ignored2=vec_push(s_record_get(ctx,17),k);
 return np;
}

fn pool_alloc(ctx:i64,dev:i64,slot:i64,size:i64,out:i64)->i64 {
 let p=pool_get(ctx,dev,slot);
 let actual=0;
 let ptr=s_pool_alloc(p,size,out);
 s_check(s_last_error(),17);
 return ptr;
}

fn pool_free(ctx:i64,dev:i64,slot:i64,ptr:i64,size:i64)->i64 {
 let p=pool_get(ctx,dev,slot);
 s_pool_free(p,ptr,size);
 s_check(s_last_error(),18);
 return 0;
}

fn graph_get(ctx:i64,key:i64)->i64 {
 let m=s_record_get(ctx,9);
 let g=s_map_get(m,key);
 if g>=0 {return g;}
 let r=s_record_new(1402);
 if r<=0 {return 0;}
 let np=s_vec_new(8*112);
 if np==0 {s_record_free(r);return 0;}
 let ignored1=s_record_set(r,1,0);
 let ignored2=s_record_set(r,2,0);
 let ignored3=s_record_set(r,3,0);
 let ignored4=s_record_set(r,4,0);
 let ignored5=s_record_set(r,5,0);
 let ignored6=s_record_set(r,6,0);
 let ignored7=s_record_set(r,7,0);
 let ignored8=s_record_set(r,8,np);
 let ignored9=s_record_set(r,9,key);
 if s_map_put(m,key,r)<0 {s_vec_free(np);s_record_free(r);return 0;}
 if vec_push(s_record_get(ctx,13),key)<0 {s_map_erase(m,key);s_vec_free(np);s_record_free(r);return 0;}
 return r;
}

fn graph_invalidate(ctx:i64)->i64 {
 let gk=s_record_get(ctx,13);
 var i=0;
 while i<s_vec_len(gk) {
  let g=s_map_get(s_record_get(ctx,9),vec_at(gk,i));
  if g!=0 {let ignored=s_record_set(g,1,0);let ignored2=s_record_set(g,2,0);let ignored3=s_record_set(g,3,0);let ignored4=s_record_set(g,5,0);}
  i+=1;
 }
 return 0;
}

fn graph_node_props(graph:i64,cgraph:i64)->i64 {
 let np=s_record_get(graph,8);
 let n=s_g_n(cgraph);
 let have=s_vec_len(np);
 if have!=n {
  let r=s_vec_resize(np,n);
  if r<0 {return 0;}
  let ignored=s_record_set(graph,3,n);
  var i=0;
  while i<n {s_g_np_capture(np,cgraph,i);i+=1;}
 }
 return np;
}

fn conc_new(ctx:i64,n:i64)->i64 {
 let r=s_record_new(1403);
 if r<=0 {return 0;}
 let fork=s_event_create();
 if fork==0||s_last_error()!=0 {s_record_free(r);s_check(1,19);return 0;}
 let map=s_map_new();
 if map==0 {s_event_destroy(fork);s_record_free(r);return 0;}
 let keys=s_vec_new(8);
 if keys==0 {s_map_free(map);s_event_destroy(fork);s_record_free(r);return 0;}
 let order=s_vec_new(8);
 if order==0 {s_vec_free(keys);s_map_free(map);s_event_destroy(fork);s_record_free(r);return 0;}
 let joins=s_vec_new(8);
 if joins==0 {s_vec_free(order);s_vec_free(keys);s_map_free(map);s_event_destroy(fork);s_record_free(r);return 0;}
 let ignored1=s_record_set(r,1,fork);
 let ignored2=s_record_set(r,2,n);
 let ignored3=s_record_set(r,3,map);
 let ignored4=s_record_set(r,4,order);
 let ignored5=s_record_set(r,5,0);
 let ignored6=s_record_set(r,6,joins);
 let ignored7=s_record_set(r,7,keys);
 var i=0;
 while i<n {
  let e=s_event_create();
  if e==0||s_last_error()!=0 {
   s_check(1,20);
   return 0;
  }
  if vec_push(joins,e)<0 {
   s_event_destroy(e);
   var j=0;
   while j<s_vec_len(joins) {s_event_destroy(vec_at(joins,j));j+=1;}
   s_vec_free(joins);s_vec_free(order);s_vec_free(keys);
   s_map_free(map);s_event_destroy(fork);s_record_free(r);
   return 0;
  }
  i+=1;
 }
 return r;
}

fn conc_free(c:i64)->i64 {
 if c<=0 {return -1;}
 let orders=s_record_get(c,4);
 let joins=s_record_get(c,6);
 let keys=s_record_get(c,7);
 let smap=s_record_get(c,3);
 var i=0;
 while i<s_vec_len(joins) {
  let e=vec_at(joins,i);
  if e!=0 {s_event_destroy(e);}
  i+=1;
 }
 let fork=s_record_get(c,1);
 if fork!=0 {s_event_destroy(fork);}
 if smap!=0 {s_map_free(smap);}
 if orders!=0 {s_vec_free(orders);}
 if joins!=0 {s_vec_free(joins);}
 if keys!=0 {s_vec_free(keys);}
 s_record_free(c);
 return 0;
}

fn conc_stream_get(c:i64,t:i64)->i64 {let v=s_map_get(s_record_get(c,3),t);return v;}
fn conc_stream_set(c:i64,t:i64,s:i64)->i64 {
 let m=s_record_get(c,3);
 let prev=s_map_get(m,t);
 let r=s_map_put(m,t,s);
 if r<0 {return r;}
 if prev<0 {
  if vec_push(s_record_get(c,7),t)<0 {s_map_erase(m,t);return -1;}
 }
 return 0;
}

fn conc_valid(c:i64)->i64 {
 let join=s_record_get(c,5);
 if join==0 {return 0;}
 var jt=s_t_view_src(join);
 if jt==0 {jt=join;}
 let js=s_t_data(jt);
 let je=js+s_t_nbytes(jt);
 let keys=s_record_get(c,7);
 let n=s_vec_len(keys);
 var i=0;
 while i<n {
  let ti=vec_at(keys,i);
  let si=conc_stream_get(c,ti);
  var t1=s_t_view_src(ti);
  if t1==0 {t1=ti;}
  let s1=s_t_data(t1);
  let e1=s1+s_t_nbytes(t1);
  if !(s1<=js&&js<e1)&&!(js<=s1&&s1<je) {
   var j=0;
   while j<n {
    let tj=vec_at(keys,j);
    let sj=conc_stream_get(c,tj);
    var t2=s_t_view_src(tj);
    if t2==0 {t2=tj;}
    let s2=s_t_data(t2);
    let e2=s2+s_t_nbytes(t2);
    if si!=sj&&!(s2<=js&&js<e2)&&!(js<=s2&&s2<je) {
     if !(s2>=e1||e2<=s1) {return 0;}
    }
    j+=1;
   }
   // all sources must either be outside the region or on the same stream
   var k=0;
   while k<10 {
    let src=s_t_src(ti,k);
    if src!=0 {
     let ss=conc_stream_get(c,src);
     if ss>=0&&ss!=si {return 0;}
    }
    k+=1;
   }
  }
  i+=1;
 }
 return 1;
}

fn any_enabled(ctx:i64)->i64 {
 let gk=s_record_get(ctx,13);
 let m=s_record_get(ctx,9);
 var i=0;
 while i<s_vec_len(gk) {
  let g=s_map_get(m,vec_at(gk,i));
  if g!=0&&s_record_get(g,4)==0 {return 1;}
  i+=1;
 }
 return 0;
}

fn any_instance(ctx:i64)->i64 {
 let gk=s_record_get(ctx,13);
 let m=s_record_get(ctx,9);
 var i=0;
 while i<s_vec_len(gk) {
  let g=s_map_get(m,vec_at(gk,i));
  if g!=0&&s_record_get(g,2)!=0 {return 1;}
  i+=1;
 }
 return 0;
}

fn stream_ctx_reset(ctx:i64)->i64 {
 let ck=s_record_get(ctx,11);
 let m=s_record_get(ctx,10);
 var i=0;
 while i<s_vec_len(ck) {
  let t=vec_at(ck,i);
  let c=s_map_get(m,t);
  if c>0 {conc_free(c);}
  s_map_erase(m,t);
  i+=1;
 }
 return s_vec_resize(ck,0);
}

fn stream_ctx_insert(ctx:i64,t:i64,c:i64)->i64 {
 let m=s_record_get(ctx,10);
 let prev=s_map_get(m,t);
 let r=s_map_put(m,t,c);
 if r<0 {return r;}
 if prev<0 {
  if vec_push(s_record_get(ctx,11),t)<0 {s_map_erase(m,t);return -1;}
 }
 return 0;
}

fn main(){print(entry(0,0,0,0,0,0,0));}
