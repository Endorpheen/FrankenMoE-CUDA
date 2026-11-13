// worker-14: Apex CUDA backend control/resources port.
// Goose owns device/context selection, stream/event/pool/scratch lifetime,
// backend tensor/buffer lifecycle, op-support decisions and the serial launch
// orchestration loop.  Numerical kernels stay in workers 9..13; every import
// here is a raw CUDA runtime call or a POD/opaque container projection.
//
// Entry: fg140_goose_cuda_backend(mode, a, b, c, d, e, f)

extern "fg140_s_phys_count" fn s_phys_count()->i64;
extern "fg140_s_prop" fn s_prop(dev:i64,field:i64)->i64;
extern "fg140_s_attr" fn s_attr(dev:i64,attr:i64)->i64;
extern "fg140_s_max_devices" fn s_max_devices()->i64;
extern "fg140_s_build_vmm" fn s_build_vmm()->i64;
extern "fg140_s_vmm_granularity" fn s_vmm_granularity(phys:i64)->i64;
extern "fg140_s_env_int" fn s_env_int(which:i64,dflt:i64)->i64;
extern "fg140_s_env_flag" fn s_env_flag(which:i64)->i64;
extern "fg140_s_env_set" fn s_env_set(which:i64)->i64;
extern "fg140_s_log" fn s_log(site:i64,a:i64,b:i64)->i64;
extern "fg140_s_info_reset" fn s_info_reset()->i64;
extern "fg140_s_info_set_counts" fn s_info_set_counts(dc:i64,pc:i64)->i64;
extern "fg140_s_info_share_inc" fn s_info_share_inc(phys:i64)->i64;
extern "fg140_s_info_share_final" fn s_info_share_final(phys:i64)->i64;
extern "fg140_s_info_set_map" fn s_info_set_map(dev:i64,phys:i64,vi:i64,sh:i64)->i64;
extern "fg140_s_info_get" fn s_info_get(dev:i64,field:i64)->i64;
extern "fg140_s_info_set_prop" fn s_info_set_prop(dev:i64,field:i64,v:i64)->i64;
extern "fg140_s_info_set_split" fn s_info_set_split(dev:i64,num:i64,den:i64)->i64;
extern "fg140_s_info_snapshot_i" fn s_info_snapshot_i(field:i64,dev:i64)->i64;
extern "fg140_s_cuda_set_device" fn s_set_device(phys:i64)->i64;
extern "fg140_s_cuda_get_device" fn s_get_device()->i64;
extern "fg140_s_cuda_check" fn s_check(err:i64,site:i64)->i64;
extern "fg140_s_cuda_malloc" fn s_malloc(size:i64,managed:i64)->i64;
extern "fg140_s_cuda_free" fn s_free(ptr:i64)->i64;
extern "fg140_s_last_error" fn s_last_error()->i64;
extern "fg140_s_clear_error" fn s_clear_error()->i64;
extern "fg140_s_cuda_device_synchronize" fn s_device_sync()->i64;
extern "fg140_s_cuda_mem_get_info" fn s_mem_get_info(phys:i64,frame:i64)->i64;
extern "fg140_s_cuda_can_access_peer" fn s_can_peer(a:i64,b:i64)->i64;
extern "fg140_s_cuda_enable_peer_access" fn s_enable_peer(a:i64,b:i64)->i64;
extern "fg140_s_device_malloc" fn s_device_malloc(size:i64,dev:i64,managed:i64)->i64;
extern "fg140_s_device_malloc_err" fn s_device_malloc_err()->i64;
extern "fg140_s_ctx_device" fn s_ctx_device(ctx:i64)->i64;
extern "fg140_s_ctx_curr_stream" fn s_ctx_curr_stream(ctx:i64)->i64;
extern "fg140_s_ctx_curr_stream_set" fn s_ctx_curr_stream_set(ctx:i64,v:i64)->i64;
extern "fg140_s_ctx_stream" fn s_ctx_stream(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_stream_create" fn s_ctx_stream_create(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_stream_destroy" fn s_ctx_stream_destroy(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_cublas" fn s_ctx_cublas(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_pool" fn s_ctx_pool(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_cublas_create" fn s_ctx_cublas_create(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_cublas_destroy" fn s_ctx_cublas_destroy(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_cublas_ws_size" fn s_ctx_cublas_ws_size(ctx:i64,dev:i64)->i64;
extern "fg140_s_ctx_cublas_ws_size_set" fn s_ctx_cublas_ws_size_set(ctx:i64,dev:i64,v:i64)->i64;
extern "fg140_s_ctx_cublas_ws" fn s_ctx_cublas_ws(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_ctx_cublas_ws_store" fn s_ctx_cublas_ws_store(ctx:i64,dev:i64,slot:i64,p:i64)->i64;
extern "fg140_s_ctx_cublas_set_workspace" fn s_ctx_cublas_set_workspace(ctx:i64,dev:i64,slot:i64,p:i64,size:i64)->i64;
extern "fg140_s_ctx_copy_event" fn s_ctx_copy_event(ctx:i64)->i64;
extern "fg140_s_ctx_copy_event_store" fn s_ctx_copy_event_store(ctx:i64,ev:i64)->i64;
extern "fg140_s_ctx_destroy_begin" fn s_ctx_destroy_begin()->i64;
extern "fg140_s_ctx_destroy_end" fn s_ctx_destroy_end()->i64;
extern "fg140_s_cublas_has_workspace" fn s_cublas_has_ws()->i64;
extern "fg140_s_cuda_stream_create" fn s_cuda_stream_create()->i64;
extern "fg140_s_cuda_stream_destroy" fn s_cuda_stream_destroy(s:i64)->i64;
extern "fg140_s_pool_create" fn s_pool_create(ctx:i64,dev:i64,slot:i64,vmm:i64)->i64;
extern "fg140_s_pool_destroy" fn s_pool_destroy(ctx:i64,dev:i64,slot:i64)->i64;
extern "fg140_s_pool_leg_capacity" fn s_leg_cap()->i64;
extern "fg140_s_pool_leg_get" fn s_leg_get(p:i64,i:i64,f:i64)->i64;
extern "fg140_s_pool_leg_set" fn s_leg_set(p:i64,i:i64,f:i64,v:i64)->i64;
extern "fg140_s_pool_leg_size" fn s_leg_size(p:i64)->i64;
extern "fg140_s_pool_leg_size_set" fn s_leg_size_set(p:i64,v:i64)->i64;
extern "fg140_s_pool_leg_clear" fn s_leg_clear(p:i64)->i64;
extern "fg140_s_pool_leg_invalidate" fn s_leg_invalidate(p:i64)->i64;
extern "fg140_s_pool_leg_alloc_raw" fn s_leg_alloc_raw(p:i64,size:i64)->i64;
extern "fg140_s_pool_leg_alloc_actual" fn s_leg_alloc_actual(p:i64)->i64;
extern "fg140_s_pool_leg_free_raw" fn s_leg_free_raw(p:i64,ptr:i64)->i64;
extern "fg140_s_pool_lookahead" fn s_lookahead(size:i64)->i64;
extern "fg140_s_pool_is_vmm" fn s_pool_is_vmm(p:i64)->i64;
extern "fg140_s_buffer_ctx" fn s_buffer_ctx(b:i64)->i64;
extern "fg140_s_buffer_ctx_field" fn s_buffer_ctx_field(c:i64,f:i64)->i64;
extern "fg140_s_buffer_ctx_free" fn s_buffer_ctx_free(c:i64)->i64;
extern "fg140_s_buffer_size" fn s_buffer_size(b:i64)->i64;
extern "fg140_s_buffer_usage" fn s_buffer_usage(b:i64)->i64;
extern "fg140_s_buffer_buft" fn s_buffer_buft(b:i64)->i64;
extern "fg140_s_buffer_is_cuda" fn s_buffer_is_cuda(b:i64)->i64;
extern "fg140_s_buft_ctx_device" fn s_buft_ctx_device(bt:i64)->i64;
extern "fg140_s_buft_is_cuda" fn s_buft_is_cuda(bt:i64)->i64;
extern "fg140_s_buft_is_cuda_host" fn s_buft_is_cuda_host(bt:i64)->i64;
extern "fg140_s_buft_new_buffer" fn s_buft_new_buffer(bt:i64,dev:i64,ptr:i64,size:i64)->i64;
extern "fg140_s_buft_get_alloc_size" fn s_buft_alloc_size(bt:i64,t:i64)->i64;
extern "fg140_s_buft_name" fn s_buft_name(bt:i64)->i64;
extern "fg140_s_buft_matches_dev" fn s_buft_matches(bt:i64,dev:i64)->i64;
extern "fg140_s_buffer_type" fn s_buffer_type(dev:i64)->i64;
extern "fg140_s_t_get" fn s_t_get(t:i64,f:i64,i:i64)->i64;
extern "fg140_s_t_src_ne" fn s_src_ne(t:i64,i:i64,axis:i64)->i64;
extern "fg140_s_t_src_nb" fn s_src_nb(t:i64,i:i64,axis:i64)->i64;
extern "fg140_s_t_src_contig" fn s_src_contig(t:i64,i:i64,kind:i64)->i64;
extern "fg140_s_t_assert" fn s_t_assert(cond:bool,site:i64)->i64;
extern "fg140_s_t_nbytes" fn s_t_nbytes(t:i64)->i64;
extern "fg140_s_t_view_src" fn s_t_view_src(t:i64)->i64;
extern "fg140_s_t_buffer" fn s_t_buffer(t:i64)->i64;
extern "fg140_s_t_data" fn s_t_data(t:i64)->i64;
extern "fg140_s_t_buft" fn s_t_buft(t:i64)->i64;
extern "fg140_s_t_is_quant" fn s_t_is_quant(t:i64)->i64;
extern "fg140_s_t_element_size" fn s_t_element_size(t:i64)->i64;
extern "fg140_s_t_op_params_i32" fn s_t_op_i32(t:i64,i:i64)->i64;
extern "fg140_s_t_op_params_f32_bits" fn s_t_op_f32_bits(t:i64,i:i64)->i64;
extern "fg140_s_t_nrows" fn s_t_nrows(t:i64)->i64;
extern "fg140_s_ggml_blck_size" fn s_blck_size(ty:i64)->i64;
extern "fg140_s_ggml_type_size" fn s_type_size(ty:i64)->i64;
extern "fg140_s_ggml_row_size" fn s_row_size(ty:i64,n:i64)->i64;
extern "fg140_s_ggml_is_quantized_type" fn s_is_quant_type(ty:i64)->i64;
extern "fg140_s_cuda_memset_async" fn s_memset_async(ptr:i64,val:i64,size:i64,st:i64)->i64;
extern "fg140_s_cuda_memcpy_async" fn s_memcpy_async(dst:i64,src:i64,size:i64,kind:i64,st:i64)->i64;
extern "fg140_s_cuda_memcpy_2d_async" fn s_memcpy_2d(dst:i64,dp:i64,src:i64,sp:i64,w:i64,h:i64,kind:i64,st:i64)->i64;
extern "fg140_s_cuda_memcpy_peer_async" fn s_memcpy_peer_async(dst:i64,dd:i64,src:i64,sd:i64,size:i64,st:i64)->i64;
extern "fg140_s_cuda_stream_sync" fn s_stream_sync(st:i64)->i64;
extern "fg140_s_cuda_stream_begin_capture" fn s_capture_begin(st:i64)->i64;
extern "fg140_s_cuda_stream_end_capture" fn s_capture_end(st:i64,frame:i64)->i64;
extern "fg140_s_cuda_event_create" fn s_event_create()->i64;
extern "fg140_s_cuda_event_destroy" fn s_event_destroy(ev:i64)->i64;
extern "fg140_s_cuda_event_record" fn s_event_record(ev:i64,st:i64)->i64;
extern "fg140_s_cuda_stream_wait_event" fn s_stream_wait_event(st:i64,ev:i64)->i64;
extern "fg140_s_cuda_event_sync" fn s_event_sync(ev:i64)->i64;
extern "fg140_s_stream_per_thread" fn s_stream_per_thread()->i64;
extern "fg140_s_event_wrap_new" fn s_event_wrap_new(dev:i64,ev:i64)->i64;
extern "fg140_s_event_wrap_context" fn s_event_wrap_context(e:i64)->i64;
extern "fg140_s_event_wrap_free" fn s_event_wrap_free(e:i64)->i64;
extern "fg140_s_dev_ctx" fn s_dev_ctx(dev:i64)->i64;
extern "fg140_s_dev_ctx_field" fn s_dev_ctx_field(c:i64,f:i64)->i64;
extern "fg140_s_dev_ctx_set" fn s_dev_ctx_set(c:i64,f:i64,v:i64)->i64;
extern "fg140_s_dev_name" fn s_dev_name(dev:i64)->i64;
extern "fg140_s_dev_desc" fn s_dev_desc(dev:i64)->i64;
extern "fg140_s_dev_type" fn s_dev_type(dev:i64)->i64;
extern "fg140_s_dev_memory" fn s_dev_memory(dev:i64,frame:i64)->i64;
extern "fg140_s_dev_uma_memory" fn s_dev_uma_memory(dev:i64,frame:i64)->i64;
extern "fg140_s_dev_get_memory_uma" fn s_dev_uma(dev:i64)->i64;
extern "fg140_s_props_set" fn s_props_set(props:i64,f:i64,v:i64)->i64;
extern "fg140_s_props_ptr" fn s_props_ptr(props:i64,f:i64)->i64;
extern "fg140_s_backend_new" fn s_backend_new(dev:i64)->i64;
extern "fg140_s_backend_free_ctx" fn s_backend_free_ctx(ctx:i64)->i64;
extern "fg140_s_have_cub" fn s_have_cub()->i64;
extern "fg140_s_have_no_peer_copy" fn s_have_no_peer_copy()->i64;
extern "fg140_s_fattn_alloc_size" fn s_fattn_alloc_size(dev:i64,t:i64)->i64;
extern "fg140_s_int_max" fn s_int_max()->i64;
extern "fg140_s_fattn_supported" fn s_fattn_supported(dev:i64,t:i64)->i64;
extern "fg140_s_lightning_supported" fn s_lightning_supported(dev:i64,t:i64)->i64;
extern "fg140_s_mul_mat_use_fwht" fn s_use_fwht(t:i64)->i64;
extern "fg140_s_backend_ctx" fn s_backend_ctx(b:i64)->i64;
extern "fg140_s_backend_is_cuda" fn s_backend_is_cuda(b:i64)->i64;
extern "fg140_s_backend_delete" fn s_backend_delete(b:i64)->i64;
extern "fg140_s_node_view_or_noop" fn s_view_or_noop(t:i64)->i64;
extern "fg140_s_node_compute_flag" fn s_compute_flag(t:i64)->i64;
extern "fg140_s_graph_n_nodes" fn s_graph_n(g:i64)->i64;
extern "fg140_s_graph_node" fn s_graph_node(g:i64,i:i64)->i64;
extern "fg140_s_try_fuse" fn s_try_fuse(ctx:i64,g:i64,i:i64)->i64;
extern "fg140_s_dispatch_node" fn s_dispatch_node(ctx:i64,t:i64)->i64;
extern "fg140_s_log_op_unsupported" fn s_log_unsupported(t:i64)->i64;
extern "fg140_s_graph_prepare" fn s_graph_prepare(ctx:i64,g:i64)->i64;
extern "fg140_s_graph_capture_begin" fn s_graph_capture_begin(ctx:i64)->i64;
extern "fg140_s_graph_capture_end" fn s_graph_capture_end(ctx:i64,key:i64)->i64;
extern "fg140_s_graph_finish" fn s_graph_finish(ctx:i64,key:i64,use:i64,upd:i64,cap:i64)->i64;
extern "fg140_s_graph_key" fn s_graph_key(g:i64)->i64;
extern "fg140_s_frame_get" fn s_frame_get(frame:i64,i:i64)->i64;
extern "fg140_s_frame_set" fn s_frame_set(frame:i64,i:i64,v:i64)->i64;

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
 if mode==0 {return dev_set(a);}
 if mode==1 {return s_get_device();}
 if mode==2 {return device_malloc(a,b,s_env_flag(0));}
 if mode==3 {return info_init();}
 if mode==4 {return s_info_get(a,0);}
 if mode==5 {return s_info_get(a,11);}
 if mode==10 {return ctx_stream(a,b,c);}
 if mode==11 {return ctx_cublas(a);}
 if mode==12 {return ctx_pool(a,b,c);}
 if mode==13 {return ctx_destroy(a);}
 if mode==20 {return leg_alloc(a,b,c,d);}
 if mode==21 {return leg_free(a,b,c,d);}
 if mode==30 {return buffer_free(a);}
 if mode==31 {return buffer_base(a);}
 if mode==32 {return buffer_init_tensor(a,b);}
 if mode==33 {return buffer_memset(a,b,c,d,e);}
 if mode==34 {return buffer_set(a,b,c,d,e);}
 if mode==35 {return buffer_get(a,b,c,d,e);}
 if mode==36 {return buffer_set2d(a,b);}
 if mode==37 {return buffer_get2d(a,b);}
 if mode==38 {return buffer_cpy(a,b,c);}
 if mode==39 {return buffer_clear(a,b);}
 if mode==40 {return buft_alloc(a,b);}
 if mode==41 {return 128;}
 if mode==42 {return buft_alloc_size(a,b);}
 if mode==43 {return s_buft_name(a);}
 if mode==50 {return backend_free(a);}
 if mode==51 {return backend_set_async(a,b,c,d,e);}
 if mode==52 {return backend_get_async(a,b,c,d,e);}
 if mode==53 {return backend_set2d_async(a,b);}
 if mode==54 {return backend_get2d_async(a,b);}
 if mode==55 {return backend_cpy_async(a,b,c,d);}
 if mode==56 {return backend_sync(a);}
 if mode==60 {return event_record(a,b);}
 if mode==61 {return event_wait(a,b);}
 if mode==70 {return supports_op(a,b);}
 if mode==71 {return supports_buft(a,b);}
 if mode==72 {return offload_op(a,b);}
 if mode==73 {return event_new(a,b);}
 if mode==74 {return event_free(a,b);}
 if mode==75 {return event_sync(a,b);}
 if mode==80 {return dev_memory(a,b);}
 if mode==81 {return s_dev_type(a);}
 if mode==82 {return dev_props(a,b,c);}
 if mode==83 {return backend_init(a);}
 if mode==84 {return s_dev_name(a);}
 if mode==85 {return s_dev_desc(a);}
 if mode==90 {return launch(a,b);}
 s_t_assert(false,100);
 return 0;
}

fn dev_set(dev:i64)->i64 {
 let count=s_info_snapshot_i(0,0);
 s_t_assert(dev>=0&&dev<count,0);
 let phys=s_info_get(dev,0);
 let cur=s_get_device();
 s_check(s_last_error(),1);
 if phys==cur {return 0;}
 return s_check(s_set_device(phys),2);
}

fn device_malloc(size:i64,dev:i64,managed:i64)->i64 {
 let ignored=dev_set(dev);
 return s_device_malloc(size,dev,managed);
}

fn info_init()->i64 {
 let ignored_reset=s_info_reset();
 let phys=s_phys_count();
 if s_last_error()!=0 {s_log(0,0,0);return 0;}
 let ignored_counts=s_info_set_counts(phys,phys);
 var devs=phys;
 if phys==0 {devs=0;}
 let req=s_env_int(0,0);
 if req>0&&phys>0 {devs=req;} else {if req!=0 {s_log(1,req,0);}}
 let cap=s_max_devices();
 if devs>cap {s_log(2,devs,cap);devs=cap;}
 let ignored_counts2=s_info_set_counts(devs,phys);
 var id=0;
 while id<devs {
  let pdev=id%phys;
  let share=s_info_share_inc(pdev);
  let ignored_map=s_info_set_map(id,pdev,share,share+1);
  id+=1;
 }
 id=0;
 while id<devs {
  let pdev=s_info_get(id,0);
  let ignored_fix=s_info_set_map(id,pdev,s_info_get(id,12),s_info_share_final(pdev));
  id+=1;
 }
 var total:i64=0;
 id=0;
 while id<phys {total=total+s_prop(id,0);id+=1;}
 let ignored_log=s_log(3,phys,total/(1024*1024));
 if devs!=phys {let ignored2=s_log(4,devs,phys);}
 total=0;
 id=0;
 while id<devs {
  let pdev=s_info_get(id,0);
  let share=s_info_get(id,11);
  let vram=s_prop(pdev,0)/share;
  let ignored_v=s_info_set_prop(id,8,vram);
  total=total+vram;
  let major=s_prop(pdev,6);
  let minor=s_prop(pdev,7);
  let ignored_cc=s_info_set_prop(id,1,100*major+10*minor);
  let ignored_nsm=s_info_set_prop(id,2,s_prop(pdev,1));
  let ignored_smpb=s_info_set_prop(id,3,s_prop(pdev,2));
  let ignored_smpbo=s_info_set_prop(id,4,s_prop(pdev,3));
  let ignored_integrated=s_info_set_prop(id,5,0);
  let ignored_warp=s_info_set_prop(id,9,s_prop(pdev,4));
  let coop=s_attr(pdev,0);
  let ignored_coop=s_info_set_prop(id,10,if coop!=0 {1} else {0});
  if s_build_vmm()!=0 {
   let vmm=s_attr(pdev,1);
   let ignored_vmm=s_info_set_prop(id,6,if vmm!=0 {1} else {0});
   if vmm!=0 {let ignored_g=s_info_set_prop(id,7,s_vmm_granularity(pdev));}
  } else {let ignored_novmm=s_info_set_prop(id,6,0);}
  id+=1;
 }
 var cum:i64=0;
 id=0;
 while id<devs {
  let vram=s_info_get(id,8);
  let ignored_split=s_info_set_split(id,cum,total);
  cum=cum+vram;
  id+=1;
 }
 if s_env_set(1)!=0 {
  id=0;
  while id<phys {
   let ignored_dev=s_check(s_set_device(id),3);
   var o=0;
   while o<phys {
    if id!=o {
     let can=s_can_peer(id,o);
     s_check(s_last_error(),4);
     if can!=0 {s_check(s_enable_peer(id,o),5);}
    }
    o+=1;
   }
   id+=1;
  }
 }
 return 0;
}

fn ctx_stream(ctx:i64,dev:i64,slot:i64)->i64 {
 let s=s_ctx_stream(ctx,dev,slot);
 if s!=0 {return s;}
 let ignored=dev_set(dev);
 s_check(s_ctx_stream_create(ctx,dev,slot),6);
 return s_ctx_stream(ctx,dev,slot);
}

fn ctx_cublas(ctx:i64)->i64 {
 let dev=s_ctx_device(ctx);
 let slot=s_ctx_curr_stream(ctx);
 let h=s_ctx_cublas(ctx,dev,slot);
 if h!=0 {return h;}
 let ignored=dev_set(dev);
 s_check(s_ctx_cublas_create(ctx,dev,slot),7);
 if s_cublas_has_ws()!=0 {
  var ws=s_ctx_cublas_ws_size(ctx,dev);
  if ws==0 {
   let cc=s_info_get(dev,1);
   ws=if cc>=900 {32*1024*1024} else {4*1024*1024};
   let ignored_ws=s_ctx_cublas_ws_size_set(ctx,dev,ws);
  }
  let ptr=s_malloc(ws,0);
  s_check(s_last_error(),8);
  s_check(s_ctx_cublas_set_workspace(ctx,dev,slot,ptr,ws),9);
  let ignored_store=s_ctx_cublas_ws_store(ctx,dev,slot,ptr);
 }
 return s_ctx_cublas(ctx,dev,slot);
}

fn ctx_pool(ctx:i64,dev:i64,slot:i64)->i64 {
 let p=s_ctx_pool(ctx,dev,slot);
 if p!=0 {return p;}
 let vmm=s_info_get(dev,6);
 s_check(s_pool_create(ctx,dev,slot,vmm),10);
 return s_ctx_pool(ctx,dev,slot);
}

fn ctx_destroy(ctx:i64)->i64 {
 if ctx==0 {return 0;}
 let ignored_begin=s_ctx_destroy_begin();
 let ev=s_ctx_copy_event(ctx);
 if ev!=0 {s_check(s_event_destroy(ev),11);}
 var i=0;
 let maxd=s_max_devices();
 while i<maxd {
  var j=0;
  while j<8 {
   let s=s_ctx_stream(ctx,i,j);
   if s!=0 {s_check(s_ctx_stream_destroy(ctx,i,j),12);}
   let h=s_ctx_cublas(ctx,i,j);
   if h!=0 {s_check(s_ctx_cublas_destroy(ctx,i,j),13);}
   let w=s_ctx_cublas_ws(ctx,i,j);
   if w!=0 {s_check(s_free(w),14);}
   j+=1;
  }
  i+=1;
 }
 let ignored_end=s_ctx_destroy_end();
 // Pool members destruct after the cuBLAS lock is released, matching the
 // original destructor body/member destruction order.
 i=0;
 while i<maxd {
  var j=0;
  while j<8 {
   let p=s_ctx_pool(ctx,i,j);
   if p!=0 {s_check(s_pool_destroy(ctx,i,j),15);}
   j+=1;
  }
  i+=1;
 }
 return 0;
}

fn leg_alloc(ctx:i64,pool:i64,size:i64,out:i64)->i64 {
 let cap=s_leg_cap();
 var best_diff:i64=68719476736;
 var ibest:i64=-1;
 var i=0;
 while i<cap {
  let p=s_leg_get(pool,i,0);
  if p!=0 {
   let b=s_leg_get(pool,i,1);
   if b>=size {
    let diff=b-size;
    if diff<best_diff {
     best_diff=diff;
     ibest=i;
     if diff==0 {
      let ignored_p=s_leg_set(pool,i,0,0);
      let ignored_s=s_leg_set(pool,i,1,0);
      let ignored_o=s_frame_set(out,0,b);
      return p;
     }
    }
   }
  }
  i+=1;
 }
 if ibest>=0 {
  let p=s_leg_get(pool,ibest,0);
  let b=s_leg_get(pool,ibest,1);
  let ignored_p=s_leg_set(pool,ibest,0,0);
  let ignored_s=s_leg_set(pool,ibest,1,0);
  let ignored_o=s_frame_set(out,0,b);
  return p;
 }
 let dev=s_ctx_device(ctx);
 let ignored=dev_set(dev);
 let look=s_lookahead(size);
 let managed=s_env_flag(0);
 var ptr=device_malloc(look,dev,managed);
 var err=s_last_error();
 if err==2 {
  let ignored_clear=s_clear_error();
  s_check(s_device_sync(),16);
  let ignored_cc=s_leg_clear(pool);
  let ignored_inv=s_leg_invalidate(pool);
  ptr=device_malloc(look,dev,managed);
  err=s_last_error();
 }
 s_check(err,17);
 let ignored_sz=s_leg_size_set(pool,s_leg_size(pool)+look);
 let ignored_o2=s_frame_set(out,0,look);
 return ptr;
}

fn leg_free(ctx:i64,pool:i64,ptr:i64,size:i64)->i64 {
 let cap=s_leg_cap();
 var i=0;
 while i<cap {
  if s_leg_get(pool,i,0)==0 {
   let ignored_p=s_leg_set(pool,i,0,ptr);
   let ignored_s=s_leg_set(pool,i,1,size);
   return 0;
  }
  i+=1;
 }
 let dev=s_ctx_device(ctx);
 let ignored=dev_set(dev);
 s_check(s_leg_free_raw(pool,ptr),18);
 let ignored_sz=s_leg_size_set(pool,s_leg_size(pool)-size);
 let ignored_inv=s_leg_invalidate(pool);
 return 0;
}

fn buffer_free(b:i64)->i64 {
 let c=s_buffer_ctx(b);
 if c!=0 {let ignored=s_buffer_ctx_free(c);}
 return 0;
}

fn buffer_base(b:i64)->i64 {
 let c=s_buffer_ctx(b);
 if c==0 {return 0;}
 return s_buffer_ctx_field(c,1);
}

fn buffer_init_tensor(b:i64,t:i64)->i64 {
 let c=s_buffer_ctx(b);
 let vs=s_t_view_src(t);
 if vs!=0 {
  s_t_assert(s_t_buft(vs)==s_buffer_buft(b),20);
  return 0;
 }
 if s_t_is_quant(t)!=0&&s_buffer_usage(b)!=2 {
  let orig=s_t_nbytes(t);
  let padded=s_buft_alloc_size(s_buffer_buft(b),t);
  if padded>orig {
   let ignored=dev_set(s_buffer_ctx_field(c,0));
   let st=s_stream_per_thread();
   s_check(s_memset_async(s_t_data(t)+orig,0,padded-orig,st),21);
   s_check(s_stream_sync(st),22);
  }
 }
 return 0;
}

fn buffer_memset(b:i64,t:i64,val:i64,off:i64,size:i64)->i64 {
 let c=s_buffer_ctx(b);
 let ignored=dev_set(s_buffer_ctx_field(c,0));
 let st=s_stream_per_thread();
 s_check(s_memset_async(s_t_data(t)+off,val,size,st),23);
 s_check(s_stream_sync(st),24);
 return 0;
}

fn buffer_set(b:i64,t:i64,data:i64,off:i64,size:i64)->i64 {
 let c=s_buffer_ctx(b);
 let ignored=dev_set(s_buffer_ctx_field(c,0));
 let st=s_stream_per_thread();
 s_check(s_memcpy_async(s_t_data(t)+off,data,size,1,st),25);
 s_check(s_stream_sync(st),26);
 return 0;
}

fn buffer_get(b:i64,t:i64,data:i64,off:i64,size:i64)->i64 {
 let c=s_buffer_ctx(b);
 let ignored=dev_set(s_buffer_ctx_field(c,0));
 let st=s_stream_per_thread();
 s_check(s_memcpy_async(data,s_t_data(t)+off,size,2,st),27);
 s_check(s_stream_sync(st),28);
 return 0;
}

fn buffer_set2d(b:i64,frame:i64)->i64 {
 let c=s_buffer_ctx(b);
 let ignored=dev_set(s_buffer_ctx_field(c,0));
 let st=s_stream_per_thread();
 let t=s_frame_get(frame,0);
 let data=s_frame_get(frame,1);
 let off=s_frame_get(frame,2);
 let size=s_frame_get(frame,3);
 let n=s_frame_get(frame,4);
 let spt=s_frame_get(frame,5);
 let spd=s_frame_get(frame,6);
 s_check(s_memcpy_2d(s_t_data(t)+off,spt,data,spd,size,n,1,st),29);
 s_check(s_stream_sync(st),30);
 return 0;
}

fn buffer_get2d(b:i64,frame:i64)->i64 {
 let c=s_buffer_ctx(b);
 let ignored=dev_set(s_buffer_ctx_field(c,0));
 let st=s_stream_per_thread();
 let t=s_frame_get(frame,0);
 let data=s_frame_get(frame,1);
 let off=s_frame_get(frame,2);
 let size=s_frame_get(frame,3);
 let n=s_frame_get(frame,4);
 let spt=s_frame_get(frame,5);
 let spd=s_frame_get(frame,6);
 s_check(s_memcpy_2d(data,spd,s_t_data(t)+off,spt,size,n,2,st),31);
 s_check(s_stream_sync(st),32);
 return 0;
}

fn buffer_cpy(b:i64,src:i64,dst:i64)->i64 {
 let sb=s_t_buffer(src);
 if s_buffer_is_cuda(sb)==0 {return 0;}
 let sc=s_buffer_ctx(sb);
 let dc=s_buffer_ctx(s_t_buffer(dst));
 let sd=s_buffer_ctx_field(sc,0);
 let dd=s_buffer_ctx_field(dc,0);
 let sp=s_info_get(sd,0);
 let dp=s_info_get(dd,0);
 let st=s_stream_per_thread();
 if sp==dp {
  s_check(s_memcpy_async(s_t_data(dst),s_t_data(src),s_t_nbytes(src),3,st),33);
 } else {
  if s_have_no_peer_copy()!=0 {return 0;}
  s_check(s_memcpy_peer_async(s_t_data(dst),dp,s_t_data(src),sp,s_t_nbytes(src),st),34);
 }
 s_check(s_stream_sync(st),35);
 return 1;
}

fn buffer_clear(b:i64,val:i64)->i64 {
 let c=s_buffer_ctx(b);
 let ignored=dev_set(s_buffer_ctx_field(c,0));
 let st=s_stream_per_thread();
 s_check(s_memset_async(s_buffer_ctx_field(c,1),val,s_buffer_size(b),st),36);
 s_check(s_stream_sync(st),37);
 return 0;
}

fn buft_alloc(bt:i64,size:i64)->i64 {
 let dev=s_buft_ctx_device(bt);
 let ignored=dev_set(dev);
 let managed=s_env_flag(0);
 let ptr=device_malloc(size,dev,managed);
 let err=s_last_error();
 if err!=0 {
  let ignored_clear=s_clear_error();
  let ignored_log=s_log(5,size/(1024*1024),dev);
  return 0;
 }
 return s_buft_new_buffer(bt,dev,ptr,size);
}

fn buft_alloc_size(bt:i64,t:i64)->i64 {
 let dev=s_buft_ctx_device(bt);
 var size=if s_t_get(t,0,0)==74 {s_fattn_alloc_size(dev,t)} else {s_t_nbytes(t)};
 let ne0=s_t_get(t,5,0);
 if s_is_quant_type(s_t_get(t,3,0))!=0 {
  if ne0%512!=0 {
   s_t_assert(s_t_get(t,6,0)==s_type_size(s_t_get(t,3,0)),21);
   size=size+s_row_size(s_t_get(t,3,0),512-ne0%512);
  }
 }
 return size;
}

fn backend_free(b:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let ignored=ctx_destroy(ctx);
 let ignored2=s_backend_free_ctx(ctx);
 let ignored3=s_backend_delete(b);
 return 0;
}

fn buf_of(t:i64)->i64 {
 let vs=s_t_view_src(t);
 if vs!=0 {return s_t_buffer(vs);}
 return s_t_buffer(t);
}

fn backend_set_async(b:i64,t:i64,data:i64,off:i64,size:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let buf=buf_of(t);
 s_t_assert(s_buffer_buft(buf)==s_buffer_type(s_ctx_device(ctx)),30);
 let st=s_ctx_stream(ctx,s_ctx_device(ctx),s_ctx_curr_stream(ctx));
 s_check(s_memcpy_async(s_t_data(t)+off,data,size,1,st),38);
 return 0;
}

fn backend_get_async(b:i64,t:i64,data:i64,off:i64,size:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let buf=buf_of(t);
 s_t_assert(s_buffer_buft(buf)==s_buffer_type(s_ctx_device(ctx)),31);
 let st=s_ctx_stream(ctx,s_ctx_device(ctx),s_ctx_curr_stream(ctx));
 s_check(s_memcpy_async(data,s_t_data(t)+off,size,2,st),39);
 return 0;
}

fn backend_set2d_async(b:i64,frame:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let t=s_frame_get(frame,0);
 let buf=buf_of(t);
 s_t_assert(s_buffer_buft(buf)==s_buffer_type(s_ctx_device(ctx)),32);
 let st=s_ctx_stream(ctx,s_ctx_device(ctx),s_ctx_curr_stream(ctx));
 let data=s_frame_get(frame,1);
 let off=s_frame_get(frame,2);
 let size=s_frame_get(frame,3);
 let n=s_frame_get(frame,4);
 let spt=s_frame_get(frame,5);
 let spd=s_frame_get(frame,6);
 s_check(s_memcpy_2d(s_t_data(t)+off,spt,data,spd,size,n,1,st),40);
 return 0;
}

fn backend_get2d_async(b:i64,frame:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let t=s_frame_get(frame,0);
 let buf=buf_of(t);
 s_t_assert(s_buffer_buft(buf)==s_buffer_type(s_ctx_device(ctx)),33);
 let st=s_ctx_stream(ctx,s_ctx_device(ctx),s_ctx_curr_stream(ctx));
 let data=s_frame_get(frame,1);
 let off=s_frame_get(frame,2);
 let size=s_frame_get(frame,3);
 let n=s_frame_get(frame,4);
 let spt=s_frame_get(frame,5);
 let spd=s_frame_get(frame,6);
 s_check(s_memcpy_2d(data,spd,s_t_data(t)+off,spt,size,n,2,st),41);
 return 0;
}

fn backend_cpy_async(bs:i64,bd:i64,src:i64,dst:i64)->i64 {
 let bsrc=buf_of(src);
 let bdst=buf_of(dst);
 if s_backend_is_cuda(bs)==0||s_backend_is_cuda(bd)==0 {return 0;}
 if s_buffer_is_cuda(bsrc)==0||s_buffer_is_cuda(bdst)==0 {return 0;}
 let cs=s_backend_ctx(bs);
 let cd=s_backend_ctx(bd);
 let bcs=s_buffer_ctx(bsrc);
 let bcd=s_buffer_ctx(bdst);
 if s_ctx_device(cs)!=s_buffer_ctx_field(bcs,0)||s_ctx_device(cd)!=s_buffer_ctx_field(bcd,0) {return 0;}
 let st=s_ctx_stream(cs,s_ctx_device(cs),s_ctx_curr_stream(cs));
 if bs!=bd {
  let sp=s_info_get(s_ctx_device(cs),0);
  let dp=s_info_get(s_ctx_device(cd),0);
  if sp==dp {
   s_check(s_memcpy_async(s_t_data(dst),s_t_data(src),s_t_nbytes(dst),3,st),42);
  } else {
   if s_have_no_peer_copy()!=0 {return 0;}
   s_check(s_memcpy_peer_async(s_t_data(dst),dp,s_t_data(src),sp,s_t_nbytes(dst),st),43);
  }
  var ev=s_ctx_copy_event(cs);
  if ev==0 {
   let ignored=dev_set(s_ctx_device(cs));
   ev=s_event_create();
   s_check(s_last_error(),44);
   let ignored_store=s_ctx_copy_event_store(cs,ev);
  }
  s_check(s_event_record(ev,st),45);
  let dst_st=s_ctx_stream(cd,s_ctx_device(cd),s_ctx_curr_stream(cd));
  s_check(s_stream_wait_event(dst_st,ev),46);
 } else {
  s_check(s_memcpy_async(s_t_data(dst),s_t_data(src),s_t_nbytes(dst),3,st),47);
 }
 return 1;
}

fn backend_sync(b:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let st=s_ctx_stream(ctx,s_ctx_device(ctx),s_ctx_curr_stream(ctx));
 s_check(s_stream_sync(st),48);
 return 0;
}

fn event_record(b:i64,e:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let st=s_ctx_stream(ctx,s_ctx_device(ctx),s_ctx_curr_stream(ctx));
 s_check(s_event_record(s_event_wrap_context(e),st),49);
 return 0;
}

fn event_wait(b:i64,e:i64)->i64 {
 let ctx=s_backend_ctx(b);
 let st=s_ctx_stream(ctx,s_ctx_device(ctx),s_ctx_curr_stream(ctx));
 if s_backend_is_cuda(b)!=0 {
  s_check(s_stream_wait_event(st,s_event_wrap_context(e)),50);
 } else {s_t_assert(false,40);}
 return 0;
}

fn batch_size(t:i64)->i64 {
 let o=s_t_get(t,0,0);
 if o==40 {return 0;}
 if o==29 {return s_t_get(t,5,1);}
 if o==30||o==48||o==49 {return s_t_get(t,5,2);}
 return s_t_nrows(t);
}

fn offload_op(dev:i64,t:i64)->i64 {
 let c=s_dev_ctx(dev);
 let minb=s_dev_ctx_field(c,1);
 return if batch_size(t)>=minb {1} else {0};
}

fn supports_buft(dev:i64,bt:i64)->i64 {
 if s_buft_is_cuda(bt)!=0&&s_buft_matches(bt,dev)!=0 {return 1;}
 if s_info_get(dev,5)!=0&&s_buft_is_cuda_host(bt)!=0 {return 1;}
 return 0;
}

fn supports_op(dev:i64,t:i64)->i64 {
 var i=0;
 while i<10 {
  if s_t_get(t,27,i)!=0&&s_t_get(t,28,i)!=0 {
   if s_t_get(t,29,i)!=dev {return 0;}
  }
  i+=1;
 }
 let o=s_t_get(t,0,0);
 if o==91 {
  let u=s_t_get(t,1,0);
  if u>21 {return 0;}
  let ty=s_t_get(t,4,0);
  if ty==30&&u==17 {return 0;}
  return if ty==1||ty==0||ty==30 {1} else {0};
 }
 if o==100 {
  let g=s_t_get(t,2,0);
  if g>6 {return 0;}
  let ty=s_t_get(t,4,0);
  if ty==30&&(g==3||g==6) {return 0;}
  if ty==1&&g==3 {return 0;}
  return s_src_contig(t,0,13);
 }
 if o==29||o==30 {
  let aty=s_t_get(t,4,0);
  let bty=s_t_get(t,4,1);
  if s_src_nb(t,0,0)!=s_type_size(aty)||s_src_nb(t,1,0)!=s_type_size(bty) {return 0;}
  if bty==1&&aty!=1&&s_use_fwht(t)==0 {return 0;}
  if o==30&&s_t_op_i32(t,3)==10 {return 0;}
  return mul_mat_type_ok(aty);
 }
 if o==31 {return if s_t_get(t,3,0)==0&&s_t_get(t,4,0)==0&&s_t_get(t,4,1)==0 {1} else {0};}
 if o==40 {
  let ty=s_t_get(t,4,0);
  if ty==1||ty==0||ty==30||ty==26||ty==41||ty==42 {return 1;}
  if ty==2||ty==3||ty==6||ty==7||ty==8 {return 1;}
  if ty==10||ty==11||ty==12||ty==13||ty==14 {return 1;}
  if ty==16||ty==17||ty==22||ty==18||ty==21||ty==19||ty==29||ty==23 {return 1;}
  if ty==20||ty==39 {return if s_src_ne(t,0,0)%256==0 {1} else {0};}
  return 0;
 }
 if o==41 {return if s_t_get(t,3,0)==0&&s_t_get(t,4,0)==0&&s_t_get(t,5,2)==1&&s_t_get(t,5,3)==1 {1} else {0};}
 if o==42 {
  let ty=s_t_get(t,3,0);
  let s0=s_t_get(t,4,0);
  let ok1=(ty==0||ty==1||ty==30||ty==2||ty==3||ty==6||ty==7||ty==8||ty==20)&&s0==0;
  let ok2=ty==1&&s0==1;
  let idx=s_t_get(t,4,1);
  return if (ok1||ok2)&&(idx==27||idx==26) {1} else {0};
 }
 if o==33 {
  let ty=s_t_get(t,3,0);
  return if (ty==0||ty==26)&&ty==s_t_get(t,4,0)&&ty==s_t_get(t,4,1) {1} else {0};
 }
 if o==34 {return cpy_supported(t);}
 if o==1||o==18||o==19 {return 1;}
 if o==20 {let ty=s_t_get(t,4,0);return if ty==0||ty==1 {1} else {0};}
 if o==21 {return if s_t_get(t,3,0)==0&&s_src_ne(t,0,2)*s_src_ne(t,0,3)<=32768 {1} else {0};}
 if o==22 {return concat_supported(t);}
 if o==51 {return if s_t_get(t,4,0)==0&&s_t_get(t,4,1)==0 {1} else {0};}
 if o==55 {
  let ty=s_t_get(t,4,0);
  return if (ty==0||ty==1||ty==30)&&s_t_get(t,3,0)==ty&&s_src_contig(t,0,12)!=0&&s_t_get(t,12,0)!=0 {1} else {0};
 }
 if o==23 {return if s_src_contig(t,0,12)!=0&&s_t_get(t,4,0)==0 {1} else {0};}
 if o==24||o==25||o==28 {return s_src_contig(t,0,15);}
 if o==26 {return s_src_contig(t,0,12);}
 if o==0||o==36||o==37||o==38||o==39||o==3||o==4||o==9||o==10||o==11||o==12||o==13 {return 1;}
 if o==32 {let ty=s_t_get(t,4,0);return if (ty==0||ty==30)&&s_t_get(t,3,0)==ty {1} else {0};}
 if o==50 {return s_src_contig(t,0,15);}
 if o==2||o==6||o==7||o==8 {return bin_supported(t);}
 if o==77 {
  let k=s_t_op_i32(t,0);
  if s_src_ne(t,3,0)==1 {
   let ds=s_src_ne(t,0,0);
   return if (ds==96||ds==128||ds==256)&&s_src_ne(t,0,1)%16==0 {1} else {0};
  }
  if k>1 {return 0;}
  return if s_src_ne(t,0,0)==16&&s_src_ne(t,0,1)==1&&s_src_ne(t,0,2)%128==0&&s_src_ne(t,4,1)==1 {1} else {0};
 }
 if o==76 {return if s_src_ne(t,0,1)%128==0 {1} else {0};}
 if o==35||o==44||o==46 {return 1;}
 if o==47 {return if s_t_op_f32_bits(t,1)==0 {1} else {0};}
 if o==66 {return if s_t_get(t,4,0)==0 {1} else {0};}
 if o==48||o==49 {return if s_src_nb(t,0,0)==s_type_size(s_t_get(t,4,0))&&s_src_contig(t,0,14)!=0 {1} else {0};}
 if o==52||o==54||o==56 {return if s_src_contig(t,0,12)!=0&&s_src_contig(t,1,12)!=0 {1} else {0};}
 if o==57 {
  let a=s_t_get(t,4,0);
  return if (a==1||a==0)&&s_t_get(t,4,1)==0&&s_t_get(t,3,0)==0&&s_src_contig(t,0,12)!=0&&s_src_contig(t,1,12)!=0&&s_t_get(t,12,0)!=0 {1} else {0};
 }
 if o==58 {let a=s_t_get(t,4,0);return if (a==1||a==0)&&s_t_get(t,4,1)==0&&s_t_get(t,3,0)==0 {1} else {0};}
 if o==59||o==60||o==61 {return 1;}
 if o==5 {return if s_src_contig(t,0,12)!=0&&s_src_contig(t,1,12)!=0 {1} else {0};}
 if o==14 {return s_t_get(t,16,0);}
 if o==70 {return if s_src_ne(t,0,0)<=s_int_max() {1} else {0};}
 if o==69 {
  if s_have_cub()!=0 {return if s_src_ne(t,0,0)<=s_int_max() {1} else {0};}
  let ncols_pad=padpow(s_src_ne(t,0,0));
  return if ncols_pad*4<=s_info_get(dev,3) {1} else {0};
 }
 if o==15 {return if s_t_get(t,4,0)==0&&s_t_get(t,3,0)==0&&s_t_get(t,15,0)!=0 {1} else {0};}
 if o==17 {return if s_t_get(t,4,0)==0&&s_t_get(t,3,0)==0&&s_t_get(t,15,0)!=0 {1} else {0};}
 if o==27 {return s_t_get(t,12,0);}
 if o==64 {return 1;}
 if o==63||o==65||o==67||o==68||o==71||o==82||o==83||o==84||o==86 {return 1;}
 if o==88 {return if s_t_get(t,4,0)==0&&s_t_get(t,4,1)==0&&s_t_get(t,4,2)==0&&s_t_get(t,3,0)==0 {1} else {0};}
 if o==89 {return if s_t_get(t,4,0)==0&&s_t_get(t,4,1)==0&&s_t_get(t,3,0)==0 {1} else {0};}
 if o==90 {
  let a=s_t_get(t,4,0);
  let b=s_t_get(t,4,1);
  let cc=s_t_get(t,4,2);
  let d=s_t_get(t,27,3);
  return if a==0&&b==0&&cc==0&&(d==0||s_t_get(t,4,3)==0)&&s_t_get(t,3,0)==0 {1} else {0};
 }
 if o==74 {return s_fattn_supported(dev,t);}
 if o==96||o==97||o==98||o==99||o==73||o==16||o==72||o==43||o==85 {return 1;}
 if o==87 {return s_lightning_supported(dev,t);}
 return 0;
}

fn padpow(n:i64)->i64 {
 var p:i64=1;
 while p<n {p=p*2;}
 return p;
}

fn mul_mat_type_ok(ty:i64)->i64 {
 if ty==0||ty==1||ty==41||ty==42||ty==2||ty==3||ty==6||ty==7||ty==8||ty==39||ty==40 {return 1;}
 if ty==10||ty==11||ty==12||ty==13||ty==14||ty==15||ty==29||ty==19||ty==22||ty==17||ty==16||ty==21||ty==18||ty==20||ty==23||ty==30 {return 1;}
 return 0;
}

fn cpy_supported(t:i64)->i64 {
 let a=s_t_get(t,4,0);
 let b=s_t_get(t,4,1);
 if (a==0||a==30||a==1)&&(b==0||b==30||b==1) {return 1;}
 if a==0&&b==8 {return 1;}
 if a==8&&b==0 {return 1;}
 if a==0&&b==2 {return 1;}
 if a==2&&b==0 {return 1;}
 if a==0&&b==3 {return 1;}
 if a==3&&b==0 {return 1;}
 if a==0&&b==6 {return 1;}
 if a==6&&b==0 {return 1;}
 if a==0&&b==7 {return 1;}
 if a==7&&b==0 {return 1;}
 if a==0&&b==20 {return 1;}
 if a==0&&b==26 {return 1;}
 if a==26&&b==0 {return 1;}
 if a==26&&b==26 {return 1;}
 if a==b&&s_src_contig(t,0,12)!=0&&s_src_contig(t,1,12)!=0 {return 1;}
 return 0;
}

fn concat_supported(t:i64)->i64 {
 let a=s_t_get(t,4,0);
 let b=s_t_get(t,4,1);
 let ty=s_t_get(t,3,0);
 let dim=s_t_op_i32(t,0);
 if a!=b||a!=ty {return 0;}
 if s_is_quant_type(a)!=0 {
  var ok=if dim==3 {1} else {0};
  if dim==3 {
   if s_src_contig(t,0,12)==0||s_src_contig(t,1,12)==0 {ok=0;}
  } else {
   if s_src_contig(t,0,18)==0||s_src_contig(t,1,18)==0 {ok=0;}
  }
  if s_t_get(t,5,0)%s_blck_size(a)!=0||s_t_get(t,5,1)%s_blck_size(a)!=0 {ok=0;}
  return ok;
 }
 if s_blck_size(a)!=1 {return 0;}
 let ts=s_type_size(a);
 return if ts==1||ts==2||ts==4||ts==8 {1} else {0};
}

fn bin_supported(t:i64)->i64 {
 let a=s_t_get(t,4,0);
 let b=s_t_get(t,4,1);
 let ty=s_t_get(t,3,0);
 if a==30||b==30||ty==30 {
  return if a==30&&ty==30&&(b==30||b==0) {1} else {0};
 }
 return if (a==0||a==1)&&(b==0||b==1)&&(ty==0||ty==1) {1} else {0};
}

fn event_new(dev:i64,handle:i64)->i64 {
 let ignored=dev_set(dev);
 let ev=s_event_create();
 s_check(s_last_error(),51);
 return s_event_wrap_new(handle,ev);
}

fn event_free(dev:i64,e:i64)->i64 {
 s_check(s_event_destroy(s_event_wrap_context(e)),52);
 let ignored=s_event_wrap_free(e);
 return 0;
}

fn event_sync(dev:i64,e:i64)->i64 {
 s_check(s_event_sync(s_event_wrap_context(e)),53);
 return 0;
}

fn dev_memory(dev:i64,frame:i64)->i64 {
 let phys=s_info_get(dev,0);
 let ignored=dev_set(dev);
 let err=s_mem_get_info(phys,frame);
 if err!=0 {
  let ignored_clear=s_clear_error();
  let ignored_log=s_log(6,err,dev);
  let ignored_a=s_frame_set(frame,0,0);
  let ignored_b=s_frame_set(frame,1,0);
  return 0;
 }
 if s_dev_uma(dev)!=0 {
  let ok=s_dev_uma_memory(dev,frame);
  if ok==0||s_frame_get(frame,0)<=0 {let ignored2=s_log(7,0,dev);}
 }
 let share=s_info_get(dev,11);
 let ignored_free=s_frame_set(frame,0,s_frame_get(frame,0)/share);
 let ignored_total=s_frame_set(frame,1,s_frame_get(frame,1)/share);
 return 0;
}

fn dev_props(dev:i64,props:i64,frame:i64)->i64 {
 let ignored_a=s_props_set(props,0,s_dev_name(dev));
 let ignored_b=s_props_set(props,1,s_dev_desc(dev));
 let ignored_c=s_props_set(props,2,s_dev_type(dev));
 let c=s_dev_ctx(dev);
 let ignored_d=s_props_set(props,3,s_dev_ctx_field(c,2));
 let ignored_e=s_frame_set(frame,0,0);
 let ignored_f=s_frame_set(frame,1,0);
 let ignored_g=dev_memory(dev,frame);
 let ignored_h=s_props_set(props,4,s_frame_get(frame,0));
 let ignored_i=s_props_set(props,5,s_frame_get(frame,1));
 let ignored_j=s_props_set(props,6,1);
 let ignored_k=s_props_set(props,7,if s_env_flag(1)!=0 {0} else {1});
 let ignored_l=s_props_set(props,8,0);
 let ignored_m=s_props_set(props,9,if s_have_no_peer_copy()!=0 {0} else {1});
 let ignored_n=s_props_set(props,10,if s_dev_type(dev)!=2 {1} else {0});
 return 0;
}

fn backend_init(dev:i64)->i64 {
 let count=s_info_snapshot_i(0,0);
 if dev<0||dev>=count {let ignored=s_log(8,dev,count);return 0;}
 return s_backend_new(dev);
}

fn launch(ctx:i64,g:i64)->i64 {
 let ignored=dev_set(s_ctx_device(ctx));
 let flags=s_graph_prepare(ctx,g);
 let use=flags&1;
 let upd=(flags>>1)&1;
 let key=s_graph_key(g);
 if use!=0&&upd!=0 {s_check(s_graph_capture_begin(ctx),54);}
 let n=s_graph_n(g);
 var i=0;
 while i<n {
  let node=s_graph_node(g,i);
  if s_view_or_noop(node)!=0 {i+=1;continue;}
  if s_compute_flag(node)==0 {i+=1;continue;}
  let skip=s_try_fuse(ctx,g,i);
  if skip!=0 {i=i+skip+1;continue;}
  if s_dispatch_node(ctx,node)==0 {let ignored2=s_log_unsupported(node);s_t_assert(false,60);}
  i+=1;
 }
 s_check(s_graph_finish(ctx,key,use,upd,if use!=0&&upd!=0 {1} else {0}),55);
 return 0;
}

fn main(){print(entry(3,0,0,0,0,0,0));}
