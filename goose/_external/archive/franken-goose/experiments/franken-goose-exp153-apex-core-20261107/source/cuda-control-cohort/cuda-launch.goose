extern "fg14_s_graph_destroy" fn destroy_graph(r:i64)->i64;
extern "fg14_s_mul_mat_id_needs_sync" fn mmid_needs_sync(t:i64,cc:i64)->i64;
// worker-14 / APX025: canonical CUDA-graph launch controller.
// Ports ggml_cuda_graph_compute + ggml_cuda_graph_evaluate_and_capture:
// graph-cache warmup/update decisions, concurrent fork/join stream order,
// original-order restore, capture/replay, dispatch and fusion routing.
// Fused/numeric launches remain explicit vendor primitives.

extern "fg14_s_record_get" fn rec_get(h:i64,k:i64)->i64;
extern "fg14_s_record_set" fn rec_set(h:i64,k:i64,v:i64)->i64;
extern "fg14_s_vec_new" fn vec_new(b:i64)->i64;
extern "fg14_s_vec_len" fn vec_len(v:i64)->i64;
extern "fg14_s_vec_data" fn vec_data(v:i64)->i64;
extern "fg14_s_vec_resize" fn vec_resize(v:i64,n:i64)->i64;
extern "fg14_s_vec_free" fn vec_free(v:i64)->i64;
extern "fg14_s_frame_get" fn frame_get(p:i64,i:i64)->i64;
extern "fg14_s_frame_set" fn frame_set(p:i64,i:i64,v:i64)->i64;
extern "fg14_res_ctx_attach" fn res_ctx_attach(n:i64,d:i64)->i64;
extern "fg14_res_graph" fn res_graph(ctx:i64,key:i64)->i64;
extern "fg14_res_graph_node_props" fn res_graph_np(g:i64,cg:i64)->i64;
extern "fg14_res_stream" fn res_stream(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg14_res_stream_ctx_lookup" fn res_conc_lookup(ctx:i64,t:i64)->i64;
extern "fg14_res_stream_ctx_size" fn res_conc_size(ctx:i64)->i64;
extern "fg14_res_stream_ctx_key_at" fn res_conc_key_at(ctx:i64,i:i64)->i64;
extern "fg14_res_stream_ctx_reset" fn res_conc_reset(ctx:i64)->i64;
extern "fg14_res_conc_get" fn res_conc_get(c:i64,k:i64)->i64;
extern "fg14_res_conc_stream_get" fn res_conc_stream(c:i64,t:i64)->i64;
extern "fg14_res_conc_order_len" fn res_conc_order_len(c:i64)->i64;
extern "fg14_res_conc_order_at" fn res_conc_order_at(c:i64,i:i64)->i64;
extern "fg14_res_conc_join_event" fn res_join(c:i64,i:i64)->i64;
extern "fg14_res_conc_valid" fn res_conc_valid(c:i64)->i64;
extern "fg14_s_device_set" fn s_device_set(d:i64)->i64;
extern "fg14_s_check" fn s_check(e:i64,site:i64)->i64;
extern "fg14_s_stream_wait_event" fn s_stream_wait_event(s:i64,e:i64)->i64;
extern "fg14_s_event_record" fn s_event_record(e:i64,s:i64)->i64;
extern "fg14_s_info_cc" fn s_info_cc(d:i64)->i64;
extern "fg14_s_graph_key" fn s_graph_key(g:i64)->i64;
extern "fg14_s_graph_env_disabled" fn s_graph_env_disabled()->i64;
extern "fg14_s_have_cuda_graph" fn s_have_cuda_graph()->i64;
extern "fg14_s_graph_capture_begin" fn s_capture_begin(n:i64)->i64;
extern "fg14_s_graph_capture_end" fn s_capture_end(n:i64,gr:i64)->i64;
extern "fg14_s_graph_update_exec" fn s_graph_update(n:i64,gr:i64)->i64;
extern "fg14_s_graph_instantiate" fn s_graph_instantiate(gr:i64)->i64;
extern "fg14_s_graph_launch" fn s_graph_launch(n:i64,gr:i64)->i64;
extern "fg14_s_g_n" fn s_g_n(g:i64)->i64;
extern "fg14_s_g_node" fn s_g_node(g:i64,i:i64)->i64;
extern "fg14_s_g_node_set" fn s_g_node_set(g:i64,i:i64,t:i64)->i64;
extern "fg14_s_g_find_node" fn s_g_find_node(g:i64,t:i64)->i64;
extern "fg14_s_g_uid" fn s_g_uid(g:i64)->i64;
extern "fg14_s_g_np_len" fn s_np_len(v:i64)->i64;
extern "fg14_s_g_np_same" fn s_np_same(v:i64,g:i64,i:i64)->i64;
extern "fg14_s_g_np_capture" fn s_np_capture(v:i64,g:i64,i:i64)->i64;
extern "fg14_s_t_get" fn s_t_get(t:i64,f:i64,i:i64)->i64;
extern "fg14_s_node_is_view_or_noop" fn s_view_noop(t:i64)->i64;
extern "fg14_s_dispatch_node" fn s_dispatch(n:i64,t:i64)->i64;
extern "fg14_s_log" fn s_log(site:i64,a:i64,b:i64)->i64;
extern "fg14_s_now_us" fn s_now_us()->i64;
extern "fg14_s_ctx_rec" fn s_ctx_rec(n:i64)->i64;
extern "fg14_s_ctx_rec_set" fn s_ctx_rec_set(n:i64,r:i64)->i64;
extern "fg14_s_ctx_device" fn s_ctx_device(n:i64)->i64;
extern "fg14_s_ctx_curr_stream_set" fn s_curr_stream_set(n:i64,v:i64)->i64;
extern "fg14_try_fuse" fn try_fuse(ctx:i64,g:i64,i:i64)->i64;

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
 if mode==0 {return graph_compute(a,b);}
 if mode==1 {return eval(a,b,c,d,e);}
 return -1;
}

fn graph_compute(native:i64,cgraph:i64)->i64 {
 let ctx=ctx_of(native);
 if ctx==0 {return -1;}
 let dev=rec_get(ctx,1);
 let ignored=s_check(s_device_set(dev),30);
 let key=s_graph_key(cgraph);
 let grec=res_graph(ctx,key);
 if grec==0 {return -1;}
 let ignored3=rec_set(grec,7,s_now_us());
 let ignored2=sweep(ctx);
 var use=enabled(grec,dev);
 var upd:i64=0;
 if use!=0&&compatible(cgraph,dev)==0 {use=0;}
 if use!=0 {
  let changed=update_required(grec,cgraph);
  if changed<0 {return -1;}
  if rec_get(grec,5)==0 {
   if changed==0 {let ignored=rec_set(grec,5,1);upd=1;} else {use=0;}
  } else {
   if changed!=0 {let ignored=rec_set(grec,5,0);use=0;}
   else {upd=if rec_get(grec,2)==0 {1} else {0};}
  }
 }
 if use!=0&&upd!=0 {s_check(s_capture_begin(native),31);}
 return eval(ctx,cgraph,use,upd,key);
}

fn ctx_of(native:i64)->i64 {
 let r=s_ctx_rec(native);
 if r!=0 {return r;}
 let nr=res_ctx_attach(native,s_ctx_device(native));
 s_ctx_rec_set(native,nr);
 return nr;
}

fn enabled(g:i64,dev:i64)->i64 {
 if s_have_cuda_graph()==0 {return 0;}
 if rec_get(g,1)==0 {
  if s_info_cc(dev)<700 {let ignored=rec_set(g,4,1);}
 }
 if rec_get(g,4)!=0 {return 0;}
 if s_graph_env_disabled()!=0 {return 0;}
 return 1;
}

fn compatible(g:i64,dev:i64)->i64 {
 var i:i64=0;let n=s_g_n(g);let cc=s_info_cc(dev);
 while i<n {
  let node=s_g_node(g,i);
  if s_view_noop(node)==0&&s_t_get(node,0,0)==30&&mmid_needs_sync(node,cc)!=0 {return 0;}
  i+=1;
 }
 return 1;
}

fn update_required(g:i64,cgraph:i64)->i64 {
 let uid=s_g_uid(cgraph);let cached=rec_get(g,6);let n=s_g_n(cgraph);
 let old_np=rec_get(g,8);let old_n=s_np_len(old_np);
 if uid!=0&&uid==cached&&old_n==n {return 0;}
 let ignored=rec_set(g,6,uid);let np=res_graph_np(g,cgraph);
 if np==0 {return -1;}
 var res=if old_n!=n {1} else {0};var i:i64=0;
 while i<n {
  if res!=0||s_np_same(np,cgraph,i)==0 {res=1;s_np_capture(np,cgraph,i);}
  i+=1;
 }
 return res;
}

fn sweep(ctx:i64)->i64 {
 let now=s_now_us();
 let last=rec_get(ctx,12);
 if now-last<5000000 {return 0;}
 let ignored=rec_set(ctx,12,now);
 let gk=rec_get(ctx,13);
 let n=vec_len(gk);
 var i=0;
 while i<n {
  let key=frame_get(vec_data(gk),i);
  let g=res_graph(ctx,key);
  let lu=rec_get(g,7);
  if now-lu>=10000000 {
   destroy_graph(g);
   let ignored2=rec_set(g,1,0);
   let ignored3=rec_set(g,2,0);
   let ignored4=rec_set(g,6,0);
   let ignored5=rec_set(g,5,0);
  }
  i+=1;
 }
 return 0;
}

fn eval(ctx:i64,cgraph:i64,use:i64,upd:i64,key:i64)->i64 {
 let native=rec_get(ctx,18);
 let dev=rec_get(ctx,1);
 var conc_active=0;
 var conc=0;
 var should=0;
 if use==0||upd!=0 {
  let nconc=res_conc_size(ctx);
  if nconc>0 {
   should=1;
   var i=0;
   while i<nconc {
    let c=res_conc_lookup(ctx,res_conc_key_at(ctx,i));
    if c<0||res_conc_valid(c)==0 {should=0;}
    i+=1;
   }
  }
  if should!=0 {
   restore_order(ctx,cgraph);
  } else {
   let ignored=res_conc_reset(ctx);
  }
  var prev=0;
  var i=0;
  let n=s_g_n(cgraph);
  while i<n {
   let node=s_g_node(cgraph,i);
   if conc_active!=0 {
    if node==res_conc_get(conc,5) {
     s_curr_stream_set(native,0);
     let nstr=res_conc_get(conc,2);
     var s=1;
     while s<=nstr {
      let st=res_stream(ctx,dev,s);
      let ev=res_join(conc,s-1);
      s_check(s_event_record(ev,st),32);
      s_check(s_stream_wait_event(res_stream(ctx,dev,0),ev),33);
      s+=1;
     }
     conc_active=0;
     conc=0;
    } else {
     let sn=res_conc_stream(conc,node);
     s_curr_stream_set(native,sn);
    }
   } else {
    if i-prev>1 {
     let prev_node=s_g_node(cgraph,i-1);
     let c=res_conc_lookup(ctx,prev_node);
     if c>=0 {conc=c;conc_active=1;fork(ctx,dev,conc);}
     if conc_active!=0 {
      let sn=res_conc_stream(conc,node);
      s_curr_stream_set(native,sn);
     }
    }
   }
   prev=i;
   if s_view_noop(node)!=0 {i+=1;continue;}
   if (s_t_get(node,2,0)&16)==0 {i+=1;continue;}
   let skip=try_fuse(ctx,cgraph,i);
   if skip!=0 {i=i+skip+1;continue;}
   if s_dispatch(native,node)==0 {s_log(99,node,0);return -2;}
   if conc_active==0 {
    let c=res_conc_lookup(ctx,node);
    if c>=0 {conc=c;conc_active=1;fork(ctx,dev,conc);}
   }
   i+=1;
  }
 }
 let grec=res_graph(ctx,key);
 if use!=0&&upd!=0 {s_check(s_capture_end(native,grec),34);}
 if use!=0 {
  if rec_get(grec,2)==0 {s_check(s_graph_instantiate(grec),35);}
  if upd!=0 {s_check(s_graph_update(native,grec),36);}
  s_check(s_graph_launch(native,grec),37);
 }
 return 0;
}

fn fork(ctx:i64,dev:i64,c:i64)->i64 {
 let main=res_stream(ctx,dev,0);
 let fev=res_conc_get(c,1);
 s_check(s_event_record(fev,main),38);
 let n=res_conc_get(c,2);
 var s=1;
 while s<=n {
  s_check(s_stream_wait_event(res_stream(ctx,dev,s),fev),39);
  s+=1;
 }
 return 0;
}

fn restore_order(ctx:i64,g:i64)->i64 {
 let nconc=res_conc_size(ctx);
 var ei=0;
 while ei<nconc {
  let c=res_conc_lookup(ctx,res_conc_key_at(ctx,ei));
  if c>=0 {
   let n=res_conc_order_len(c);
   if n>0 {
    let pos=vec_new(8);
    let ignored=vec_resize(pos,n);
    var k=0;
    var ok=1;
    while k<n {
     let idx=s_g_find_node(g,res_conc_order_at(c,k));
     if idx<0 {ok=0;} else {frame_set(vec_data(pos),k,idx);}
     k+=1;
    }
    if ok!=0 {
     var x=0;
     while x<n {
      var y=x+1;
      while y<n {
       let a=frame_get(vec_data(pos),x);
       let b=frame_get(vec_data(pos),y);
       if b<a {frame_set(vec_data(pos),x,b);frame_set(vec_data(pos),y,a);}
       y+=1;
      }
      x+=1;
     }
     var contiguous=1;
     var x2=1;
     while x2<n {
      let a=frame_get(vec_data(pos),x2-1);
      let b=frame_get(vec_data(pos),x2);
      if b!=a+1 {contiguous=0;}
      x2+=1;
     }
     if contiguous!=0 {
      let start=frame_get(vec_data(pos),0);
      var k2=0;
      while k2<n {s_g_node_set(g,start+k2,res_conc_order_at(c,k2));k2+=1;}
     }
    }
    let ignored2=vec_free(pos);
   }
  }
  ei+=1;
 }
 return 0;
}

fn main(){print(entry(0,0,0,0,0,0,0));}
