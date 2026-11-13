// worker-14 / APX025: canonical graph-optimize concurrent-region producer.
// Ports ggml_backend_cuda_graph_optimize control: fusion alloc deps, fan-out
// discovery, join search, canonical CONC records, interleaving.  Canonical
// maps/vectors replace the native owned producer state.

extern "fg14_s_record_get" fn rec_get(h:i64,k:i64)->i64;
extern "fg14_s_map_new" fn map_new()->i64;
extern "fg14_s_map_put" fn map_put(h:i64,k:i64,v:i64)->i64;
extern "fg14_s_map_get" fn map_get(h:i64,k:i64)->i64;
extern "fg14_s_map_free" fn map_free(h:i64)->i64;
extern "fg14_s_vec_new" fn vec_new(b:i64)->i64;
extern "fg14_s_vec_len" fn vec_len(v:i64)->i64;
extern "fg14_s_vec_data" fn vec_data(v:i64)->i64;
extern "fg14_s_vec_resize" fn vec_resize(v:i64,n:i64)->i64;
extern "fg14_s_vec_free" fn vec_free(v:i64)->i64;
extern "fg14_s_frame_get" fn fget(p:i64,i:i64)->i64;
extern "fg14_s_frame_set" fn fset(p:i64,i:i64,v:i64)->i64;
extern "fg14_s_assert" fn s_assert(c:i64,s:i64)->i64;
extern "fg14_s_check" fn s_check(e:i64,s:i64)->i64;
extern "fg14_s_device_set" fn s_device_set(d:i64)->i64;
extern "fg14_s_ctx_device" fn s_ctx_device(n:i64)->i64;
extern "fg14_s_info_cc" fn s_info_cc(d:i64)->i64;
extern "fg14_s_have_cuda_graph" fn s_have_cuda_graph()->i64;
extern "fg14_s_graph_opt_env" fn s_graph_opt_env()->i64;
extern "fg14_s_fusion_disabled" fn s_fusion_disabled()->i64;
extern "fg14_s_add_dep" fn s_add_dep(pa:i64,a:i64,b:i64)->i64;
extern "fg14_s_g_n" fn g_n(g:i64)->i64;
extern "fg14_s_g_node" fn g_node(g:i64,i:i64)->i64;
extern "fg14_s_g_set_node_at" fn g_set_node(g:i64,i:i64,t:i64)->i64;
extern "fg14_s_g_rotate3" fn g_rotate3(g:i64,a:i64,b:i64,c:i64)->i64;
extern "fg14_s_t_op" fn t_op(t:i64)->i64;
extern "fg14_s_t_src_at" fn t_src(t:i64,i:i64)->i64;
extern "fg14_s_t_view_src_at" fn t_view_src(t:i64)->i64;
extern "fg14_s_t_nrows_at" fn t_nrows(t:i64)->i64;
extern "fg14_s_t_name_attn" fn t_name_attn(t:i64)->i64;
extern "fg14_s_t_depends" fn t_depends(a:i64,b:i64)->i64;
extern "fg14_s_node_is_view_or_noop" fn view_noop(t:i64)->i64;
extern "fg14_res_ctx_attach" fn res_ctx_attach(n:i64,d:i64)->i64;
extern "fg14_res_stream_ctx_reset" fn res_conc_reset(c:i64)->i64;
extern "fg14_res_conc_new" fn conc_new(c:i64,n:i64)->i64;
extern "fg14_res_conc_free" fn conc_free(c:i64)->i64;
extern "fg14_res_conc_set" fn conc_set(c:i64,k:i64,v:i64)->i64;
extern "fg14_res_conc_stream_set" fn conc_stream_set(c:i64,t:i64,s:i64)->i64;
extern "fg14_res_conc_order_push" fn conc_order_push(c:i64,t:i64)->i64;
extern "fg14_res_stream_ctx_insert" fn conc_insert(c:i64,t:i64,e:i64)->i64;
extern "fg14_match_shared" fn match_shared(g:i64,r:i64,s:i64)->i64;

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
 if mode==0 {return optimize(a,b,c);}
 return 0;
}

fn optimize(native:i64,g:i64,params:i64)->i64 {
 let ctx=res_ctx_attach(native,s_ctx_device(native));
 let dev=s_ctx_device(native);
 if s_fusion_disabled()==0 {
  let ignored=s_device_set(dev);
  shared_deps(g,params);
 }
 var enabled=0;
 if s_have_cuda_graph()!=0&&s_graph_opt_env()!=0&&s_info_cc(dev)>=700 {enabled=1;}
 let ignored2=res_conc_reset(ctx);
 if enabled==0 {return 0;}
 let ignored3=s_device_set(dev);
 return producer(ctx,g);
}

fn shared_deps(g:i64,params:i64)->i64 {
 let n=g_n(g);
 var i=0;
 while i+5<n {
  if t_op(g_node(g,i))==30 {
   var j=i+3;
   while j+2<n {
    if t_op(g_node(g,j))==30&&t_op(g_node(g,j+1))==30 {break;}
    if t_op(g_node(g,j))==29&&match_shared(g,i,j)==1 {
     g_rotate3(g,i+3,j,j+3);
     let up=t_src(g_node(g,i+2),1);
     s_add_dep(params,t_src(up,1),g_node(g,i+5));
     s_add_dep(params,t_src(up,2),g_node(g,i+5));
     i+=5;
     break;
    }
    j+=1;
   }
  }
  i+=1;
 }
 return 0;
}

fn producer(ctx:i64,g:i64)->i64 {
 let fan=map_new();
 let idx=map_new();
 let fkeys=vec_new(8);
 let rstart=vec_new(8);
 let rend=vec_new(8);
 let branches=vec_new(8);
 if fan==0||idx==0||fkeys==0||rstart==0||rend==0||branches==0 {return -1;}
 let n=g_n(g);
 var i=0;
 while i<n {
  let node=g_node(g,i);
  let ignored=map_put(idx,node,i);
  if view_noop(node)==0&&t_nrows(node)<=1 {
   var s=0;
   while s<10 {
    let src=t_src(node,s);
    if src!=0 {
     var c=map_get(fan,src);
     if c<0 {c=0;vec_push(fkeys,src);}
     map_put(fan,src,c+1);
    }
    s+=1;
   }
  }
  i+=1;
 }
 let nk=vec_len(fkeys);
 var k=0;
 while k<nk {
  let root=vfget(fkeys,k);
  let count=map_get(fan,root);
  if count>=3&&count<=3&&t_name_attn(root)==1 {
   let root_idx=map_get(idx,root);
   if root_idx>=0&&!in_ranges(rstart,rend,root_idx) {
    produce_one(ctx,g,root,root_idx,count,idx,fan,rstart,rend,branches);
   }
  }
  k+=1;
 }
 let ignored1=map_free(fan);
 let ignored2=map_free(idx);
 let ignored3=vec_free(fkeys);
 let ignored4=vec_free(rstart);
 let ignored5=vec_free(rend);
 let nb=vec_len(branches);
 var b=0;
 while b<nb {let ignored6=vec_free(vfget(branches,b));b+=1;}
 let ignored7=vec_free(branches);
 return 0;
}

fn produce_one(ctx:i64,g:i64,root:i64,root_idx:i64,count:i64,idx:i64,fan:i64,rstart:i64,rend:i64,branches:i64)->i64 {
 let nb0=vec_len(branches);
 var b=0;
 while b<count {let ignored=vec_push(branches,vec_new(8));b+=1;}
 let n=g_n(g);
 var i=root_idx+1;
 var bi=0;
 while i<n&&bi<count {
  let node=g_node(g,i);
  if view_noop(node)==0&&t_depends(node,root)==1 {
   vec_push(vfget(branches,nb0+bi),node);
   bi+=1;
  }
  i+=1;
 }
 if bi!=count {return 0;}
 var join=0;
 var join_idx=-1;
 i=root_idx+1;
 while i<n {
  let curr=g_node(g,i);
  var num=0;
  b=0;
  while b<count {if belongs(curr,vfget(branches,nb0+b))==1 {num+=1;} b+=1;}
  if num>=2 {join=curr;join_idx=i;break;}
  var found=0;
  b=0;
  while b<count {
   if belongs(curr,vfget(branches,nb0+b))==1 {
    if !vcontains(vfget(branches,nb0+b),curr) {vec_push(vfget(branches,nb0+b),curr);}
    found=1;
   }
   b+=1;
  }
  if found==0&&view_noop(curr)!=0 {vec_push(vfget(branches,nb0),curr);}
  i+=1;
 }
 if join==0||join_idx<0 {return 0;}
 var total=0;
 b=0;
 while b<count {total+=vec_len(vfget(branches,nb0+b));b+=1;}
 if join_idx-root_idx-1!=total {return 0;}
 let c=conc_new(ctx,count);
 if c==0 {return -1;}
 conc_set(c,5,join);
 b=0;
 while b<count {
  let bv=vfget(branches,nb0+b);
  let m=vec_len(bv);
  var q=0;
  while q<m {conc_stream_set(c,vfget(bv,q),b+1);q+=1;}
  b+=1;
 }
 i=root_idx+1;
 while i<join_idx {conc_order_push(c,g_node(g,i));i+=1;}
 conc_insert(ctx,root,c);
 vec_push(rstart,root_idx);
 vec_push(rend,join_idx);
 // interleave branches into the graph
 var cur=0;
 var ci=root_idx+1;
 while ci<join_idx {
  let bv=vfget(branches,nb0+cur);
  if vec_len(bv)==0 {cur=(cur+1)%count;continue;}
  g_set_node(g,ci,vfget(bv,0));
  ci+=1;
  vpop_front(bv);
  while vec_len(bv)>0&&view_noop(vfget(bv,0))!=0 {
   g_set_node(g,ci,vfget(bv,0));
   ci+=1;
   vpop_front(bv);
  }
  cur=(cur+1)%count;
 }
 return 0;
}

fn in_ranges(a:i64,b:i64,x:i64)->bool {
 let n=vec_len(a);
 var i=0;
 while i<n {
  if x>=vfget(a,i)&&x<=vfget(b,i) {return true;}
  i+=1;
 }
 return false;
}

fn belongs(node:i64,bv:i64)->i64 {
 let n=vec_len(bv);
 var i=0;
 while i<n {
  if t_depends(node,vfget(bv,i))==1 {return 1;}
  i+=1;
 }
 return 0;
}

fn vcontains(v:i64,x:i64)->bool {
 let n=vec_len(v);
 var i=0;
 while i<n {if vfget(v,i)==x {return true;}i+=1;}
 return false;
}

fn vfget(v:i64,i:i64)->i64 {return fget(vec_data(v),i);}
fn vpush(v:i64,x:i64)->i64 {let n=vec_len(v);vec_resize(v,n+1);fset(vec_data(v),n,x);return 0;}
fn vec_push(v:i64,x:i64)->i64 {return vpush(v,x);}
fn vpop_front(v:i64)->i64 {
 let n=vec_len(v);
 var i=1;
 while i<n {fset(vec_data(v),i-1,fget(vec_data(v),i));i+=1;}
 return vec_resize(v,n-1);
}

fn main(){print(entry(0,0,0,0,0,0,0));}
