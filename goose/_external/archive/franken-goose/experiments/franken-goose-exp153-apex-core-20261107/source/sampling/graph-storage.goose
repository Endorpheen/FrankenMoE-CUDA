extern "fg153_gs_const" fn gs_const(code:i64)->i64;
extern "fg153_gs_tensor_get" fn gs_tensor_get(t:i64,k:i64,i:i64)->i64;
extern "fg153_gs_graph_get" fn gs_graph_get(g:i64,k:i64,i:i64)->i64;
extern "fg153_gs_graph_put" fn gs_graph_put(g:i64,k:i64,i:i64,v:i64);
extern "fg153_gs_hash_prime_count" fn gs_hash_prime_count()->i64;
extern "fg153_gs_hash_prime" fn gs_hash_prime(index:i64)->i64;
extern "fg153_gs_bitset_bytes" fn gs_bitset_bytes(hash_size:i64)->i64;
extern "fg153_gs_arena_new_object" fn gs_arena_new_object(ctx:i64,bytes:i64)->i64;
extern "fg153_gs_arena_graph_ptr" fn gs_arena_graph_ptr(ctx:i64,obj:i64)->i64;
extern "fg153_gs_addr_add" fn gs_addr_add(base:i64,off:i64)->i64;
extern "fg153_gs_raw_graph_set" fn gs_raw_graph_set(base:i64,code:i64,i:i64,v:i64);
extern "fg153_gs_raw_zero" fn gs_raw_zero(addr:i64,bytes:i64);
extern "fg153_gs_hash_insert" fn gs_hash_insert(g:i64,t:i64)->i64;
extern "fg153_gs_hash_find" fn gs_hash_find(g:i64,t:i64)->i64;
extern "fg153_gs_zero_field" fn gs_zero_field(g:i64,field:i64,count:i64);
extern "fg153_gs_tensor_empty" fn gs_tensor_empty(t:i64)->i64;
extern "fg153_gs_tensor_buffer" fn gs_tensor_buffer(t:i64)->i64;
extern "fg153_gs_tensor_data" fn gs_tensor_data(t:i64)->i64;
extern "fg153_gs_zero_data" fn gs_zero_data(t:i64);
extern "fg153_gs_backend_zero" fn gs_backend_zero(t:i64);
extern "fg153_gs_tensor_is_scalar" fn gs_tensor_is_scalar(t:i64)->i64;
extern "fg153_gs_write_f32_one" fn gs_write_f32_one(t:i64);

fn gs_ensure(ok:bool) {if !ok {assert(false);}}
fn gs_const_l2r()->i64 {return gs_const(60);}
fn gs_hash_full()->i64 {return gs_const(62);}
fn gs_op_none()->i64 {return 0;}
fn gs_op_opt_step_adamw()->i64 {return gs_const(70);}
fn gs_type_f32()->i64 {return gs_const(71);}
fn gs_flag_loss()->i64 {return 8;}
fn gs_default_graph_size()->i64 {return 2048;}
fn gs_pad(v:i64,align:i64)->i64 {gs_ensure(align>0);return ((v+align-1)/align)*align;}

// Owned hash-table sizing: binary search over raw prime metadata, then the
// native odd fallback. No search policy lives in the C service.
fn gs_hash_size(min_sz:i64)->i64 {
    var target=min_sz;
    if target<0 {target=0;}
    let n=gs_hash_prime_count();
    var l:i64=0;var r:i64=n;
    while l<r {
        let m=(l+r)/2;
        if gs_hash_prime(m)<target {l=m+1;} else {r=m;}
    }
    if l<n {return gs_hash_prime(l);}
    return target|1;
}

fn gs_size(g:i64)->i64 {return gs_graph_get(g,0,0);}
fn gs_n_nodes(g:i64)->i64 {return gs_graph_get(g,1,0);}
fn gs_n_leafs(g:i64)->i64 {return gs_graph_get(g,2,0);}
fn gs_order(g:i64)->i64 {return gs_graph_get(g,6,0);}
fn gs_hash_size_field(g:i64)->i64 {return gs_graph_get(g,9,0);}
fn gs_node_at(g:i64,i:i64)->i64 {return gs_graph_get(g,3,gs_graph_get(g,14,0)+i);}
fn gs_leaf_at(g:i64,i:i64)->i64 {return gs_graph_get(g,4,i);}
fn gs_use_count(g:i64,pos:i64)->i64 {return gs_graph_get(g,5,pos);}
fn gs_grads_base(g:i64)->i64 {return gs_graph_get(g,15,0);}
fn gs_grad_accs_base(g:i64)->i64 {return gs_graph_get(g,16,0);}

fn gs_t_op(t:i64)->i64 {return gs_tensor_get(t,0,0);}
fn gs_t_flags(t:i64)->i64 {return gs_tensor_get(t,1,0);}
fn gs_t_type(t:i64)->i64 {return gs_tensor_get(t,2,0);}
fn gs_t_src(t:i64,i:i64)->i64 {return gs_tensor_get(t,3,i);}

fn gs_graph_nbytes(size:i64,grads:i64)->i64 {
    if size<0 {return 0;}
    let hash_size=gs_hash_size(size*2);
    let ptr_size=gs_const(82);let ptr_align=gs_const(83);
    let i32_size=gs_const(84);let i32_align=gs_const(85);
    let bit_align=gs_const(86);let cgraph_size=gs_const(80);
    var p:i64=1024;
    p=gs_pad(p,1)+cgraph_size;
    p=gs_pad(p,ptr_align)+size*ptr_size;
    p=gs_pad(p,ptr_align)+size*ptr_size;
    p=gs_pad(p,i32_align)+hash_size*i32_size;
    p=gs_pad(p,ptr_align)+hash_size*ptr_size;
    if grads!=0 {p=gs_pad(p,ptr_align)+hash_size*ptr_size;p=gs_pad(p,ptr_align)+hash_size*ptr_size;}
    p=gs_pad(p,bit_align)+gs_bitset_bytes(hash_size);
    return p-1024;
}
fn gs_graph_overhead_custom(size:i64,grads:i64)->i64 {return gs_const(88)+gs_pad(gs_graph_nbytes(size,grads),gs_const(89));}
fn gs_graph_overhead()->i64 {return gs_graph_overhead_custom(gs_default_graph_size(),0);}

fn gs_reset_visited(g:i64) {var pos:i64=0;while pos<gs_hash_size_field(g) {gs_graph_put(g,7,pos,0);pos+=1;}}

// Owned arena layout. Mirrors ggml_graph_nbytes placement in Goose and uses
// only raw adapter primitives. No native graph function is called here.
fn gs_create_graph(ctx:i64,size:i64,grads:i64)->i64 {
    if ctx==0||size<=0||size>2147483647 {return 0;}
    let hash_size=gs_hash_size(size*2);
    let bytes=gs_graph_nbytes(size,grads);
    let obj=gs_arena_new_object(ctx,bytes);
    if obj==0 {return 0;}
    let base=gs_arena_graph_ptr(ctx,obj);
    if base==0 {return 0;}
    let ptr_size=gs_const(82);let ptr_align=gs_const(83);
    let i32_size=gs_const(84);let i32_align=gs_const(85);
    let bitset_align=gs_const(86);
    var off=gs_const(80);
    let nodes_off=gs_pad(off,ptr_align);off=nodes_off+size*ptr_size;
    let leafs_off=gs_pad(off,ptr_align);off=leafs_off+size*ptr_size;
    let use_off=gs_pad(off,i32_align);off=use_off+hash_size*i32_size;
    let keys_off=gs_pad(off,ptr_align);off=keys_off+hash_size*ptr_size;
    var grads_off:i64=0;var gradaccs_off:i64=0;
    if grads!=0 {
        grads_off=gs_pad(off,ptr_align);off=grads_off+hash_size*ptr_size;
        gradaccs_off=gs_pad(off,ptr_align);off=gradaccs_off+hash_size*ptr_size;
    }
    let used_off=gs_pad(off,bitset_align);off=used_off+gs_bitset_bytes(hash_size);
    gs_ensure(off==bytes);
    let nodes=gs_addr_add(base,nodes_off);
    let leafs=gs_addr_add(base,leafs_off);
    let use=gs_addr_add(base,use_off);
    let keys=gs_addr_add(base,keys_off);
    var grads_ptr:i64=0;var gradaccs_ptr:i64=0;
    if grads!=0 {grads_ptr=gs_addr_add(base,grads_off);gradaccs_ptr=gs_addr_add(base,gradaccs_off);}
    let used=gs_addr_add(base,used_off);
    gs_raw_graph_set(base,0,0,size);
    gs_raw_graph_set(base,1,0,0);
    gs_raw_graph_set(base,2,0,0);
    gs_raw_graph_set(base,3,0,nodes);
    gs_raw_graph_set(base,4,0,leafs);
    gs_raw_graph_set(base,5,0,use);
    gs_raw_graph_set(base,6,0,gs_const_l2r());
    gs_raw_graph_set(base,9,0,hash_size);
    gs_raw_graph_set(base,10,0,grads_ptr);
    gs_raw_graph_set(base,11,0,gradaccs_ptr);
    gs_raw_graph_set(base,12,0,0);
    gs_raw_graph_set(base,19,0,used);
    gs_raw_graph_set(base,20,0,keys);
    gs_raw_zero(used,gs_bitset_bytes(hash_size));
    if grads!=0 {
        gs_raw_zero(grads_ptr,hash_size*ptr_size);
        gs_raw_zero(gradaccs_ptr,hash_size*ptr_size);
    }
    return base;
}

fn gs_new_graph_custom(ctx:i64,size:i64,grads:i64)->i64 {
    let g=gs_create_graph(ctx,size,grads);
    if g==0 {return 0;}
    return g;
}
fn gs_new_graph(ctx:i64)->i64 {return gs_new_graph_custom(ctx,gs_default_graph_size(),0);}

// View policy in Goose: bounds and node offset are computed here; the adapter
// only stores POD fields into caller-owned storage.
fn gs_graph_view(out:i64,src:i64,i0:i64,i1:i64)->i64 {
    if out==0 {return 0;}
    if src==0 {return 0;}
    gs_ensure(i0>=0);gs_ensure(i1>=i0);
    let ptr_size=gs_const(82);
    let nodes_base=gs_graph_get(src,17,0);
    let use_base=gs_graph_get(src,21,0);
    let keys_base=gs_graph_get(src,22,0);
    let used_base=gs_graph_get(src,23,0);
    let hash_size=gs_hash_size_field(src);
    let order=gs_order(src);
    let nodes=gs_addr_add(nodes_base,i0*ptr_size);
    gs_raw_graph_set(out,0,0,0);
    gs_raw_graph_set(out,1,0,i1-i0);
    gs_raw_graph_set(out,2,0,0);
    gs_raw_graph_set(out,3,0,nodes);
    gs_raw_graph_set(out,4,0,0);
    gs_raw_graph_set(out,5,0,use_base);
    gs_raw_graph_set(out,6,0,order);
    gs_raw_graph_set(out,9,0,hash_size);
    gs_raw_graph_set(out,10,0,0);
    gs_raw_graph_set(out,11,0,0);
    gs_raw_graph_set(out,12,0,0);
    gs_raw_graph_set(out,19,0,used_base);
    gs_raw_graph_set(out,20,0,keys_base);
    gs_ensure(gs_size(out)==0);
    gs_ensure(gs_n_nodes(out)==i1-i0);
    gs_ensure(gs_n_leafs(out)==0);
    gs_ensure(gs_order(out)==order);
    gs_ensure(gs_hash_size_field(out)==hash_size);
    gs_ensure(gs_graph_get(out,18,0)==0);
    gs_ensure(gs_grads_base(out)==0);
    gs_ensure(gs_grad_accs_base(out)==0);
    return out;
}

fn gs_graph_cpy(src:i64,dst:i64) {
    gs_ensure(gs_size(dst)>=gs_n_leafs(src));gs_ensure(gs_size(dst)>=gs_n_nodes(src));
    gs_ensure(gs_hash_size_field(dst)>=gs_hash_size_field(src));
    gs_graph_put(dst,2,0,gs_n_leafs(src));gs_graph_put(dst,1,0,gs_n_nodes(src));gs_graph_put(dst,6,0,gs_order(src));
    var i:i64=0;while i<gs_n_leafs(src) {gs_graph_put(dst,4,i,gs_leaf_at(src,i));i+=1;}
    i=0;while i<gs_n_nodes(src) {gs_graph_put(dst,3,i,gs_node_at(src,i));i+=1;}
    var pos:i64=0;while pos<gs_hash_size_field(src) {
        if gs_graph_get(src,7,pos)!=0 {let key=gs_graph_get(src,8,pos);let new_pos=gs_hash_insert(dst,key);gs_graph_put(dst,5,new_pos,gs_use_count(src,pos));}
        pos+=1;
    }
    if gs_grads_base(dst)!=0 {gs_zero_field(dst,10,gs_hash_size_field(dst));gs_zero_field(dst,11,gs_hash_size_field(dst));}
    if gs_grads_base(src)!=0 {
        gs_ensure(gs_grads_base(dst)!=0);gs_ensure(gs_grad_accs_base(dst)!=0);
        var j:i64=0;while j<gs_n_nodes(src) {
            let node=gs_node_at(src,j);
            let i_src=gs_hash_find(src,node);let i_dst=gs_hash_find(dst,node);
            gs_ensure(i_src!=gs_hash_full());gs_ensure(gs_graph_get(src,7,i_src)!=0);
            gs_ensure(i_dst!=gs_hash_full());gs_ensure(gs_graph_get(dst,7,i_dst)!=0);
            gs_graph_put(dst,10,i_dst,gs_graph_get(src,10,i_src));gs_graph_put(dst,11,i_dst,gs_graph_get(src,11,i_src));
            j+=1;
        }
    }
}

fn gs_new_graph_dup(ctx:i64,src:i64,force_grads:i64)->i64 {
    var need_grads:i64=0;if gs_grads_base(src)!=0||force_grads!=0 {need_grads=1;}
    let result=gs_new_graph_custom(ctx,gs_size(src),need_grads);
    if result==0 {return 0;}
    gs_graph_cpy(src,result);return result;
}

fn gs_graph_get_grad(g:i64,node:i64)->i64 {
    if gs_grads_base(g)==0 {return 0;}
    let pos=gs_hash_find(g,node);if pos==gs_hash_full() {return 0;}
    if gs_graph_get(g,7,pos)==0 {return 0;}return gs_graph_get(g,10,pos);
}
fn gs_graph_get_grad_acc(g:i64,node:i64)->i64 {
    if gs_grad_accs_base(g)==0 {return 0;}
    let pos=gs_hash_find(g,node);if pos==gs_hash_full() {return 0;}
    if gs_graph_get(g,7,pos)==0 {return 0;}return gs_graph_get(g,11,pos);
}

fn gs_set_zero(t:i64) {
    if gs_tensor_empty(t)==0 {
        if gs_tensor_buffer(t)!=0 {gs_backend_zero(t);}else {gs_ensure(gs_tensor_data(t)!=0);gs_zero_data(t);}
    }
}

fn gs_graph_reset(g:i64) {
    if g!=0 {
        gs_ensure(gs_grads_base(g)!=0);
        var i:i64=0;while i<gs_n_nodes(g) {
            let node=gs_node_at(g,i);let grad_acc=gs_graph_get_grad_acc(g,node);
            if gs_t_op(node)==gs_op_opt_step_adamw() {gs_set_zero(gs_t_src(node,2));gs_set_zero(gs_t_src(node,3));}
            if grad_acc!=0 {
                if (gs_t_flags(node)&gs_flag_loss())!=0 {
                    gs_ensure(gs_t_type(grad_acc)==gs_type_f32());gs_ensure(gs_tensor_is_scalar(grad_acc)!=0);gs_write_f32_one(grad_acc);
                }else {gs_set_zero(grad_acc);}
            }
            i+=1;
        }
    }
}

fn gs_graph_size(g:i64)->i64 {return gs_size(g);}
fn gs_graph_n_nodes(g:i64)->i64 {return gs_n_nodes(g);}
fn gs_graph_node(g:i64,i:i64)->i64 {
    if i<0 {gs_ensure(gs_n_nodes(g)+i>=0);return gs_node_at(g,gs_n_nodes(g)+i);}
    gs_ensure(i<gs_n_nodes(g));return gs_node_at(g,i);
}
fn gs_graph_nodes(g:i64)->i64 {return gs_graph_get(g,17,0);}
fn gs_graph_clear_metadata(g:i64) {gs_graph_put(g,2,0,0);gs_graph_put(g,1,0,0);gs_reset_visited(g);}

fn gs_selftest()->i64 {gs_ensure(gs_pad(5,2)==6);gs_ensure(gs_default_graph_size()==2048);gs_ensure(gs_op_none()==0);gs_ensure(gs_hash_full()==-1);gs_ensure(gs_hash_size(8)==11);gs_ensure(gs_hash_size(32)==37);gs_ensure(gs_hash_size(4096)==4099);return 1;}
fn gs_entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
    if mode==0 {return gs_new_graph_custom(a,b,c);}
    if mode==1 {return gs_new_graph(a);}
    if mode==2 {return gs_graph_overhead_custom(a,b);}
    if mode==3 {return gs_graph_overhead();}
    if mode==4 {return gs_graph_view(a,b,c,d);}
    if mode==5 {gs_graph_cpy(a,b);return 0;}
    if mode==6 {return gs_new_graph_dup(a,b,c);}
    if mode==7 {gs_graph_reset(a);return 0;}
    if mode==8 {return gs_graph_size(a);}
    if mode==9 {return gs_graph_n_nodes(a);}
    if mode==10 {return gs_graph_node(a,b);}
    if mode==11 {return gs_graph_nodes(a);}
    if mode==12 {return gs_graph_get_grad(a,b);}
    if mode==13 {return gs_graph_get_grad_acc(a,b);}
    if mode==14 {return gs_selftest();}
    if mode==15 {gs_graph_clear_metadata(a);return 0;}
    if mode==20 {return gs_graph_nbytes(a,b);}
    if mode==21 {return gs_graph_overhead_custom(a,b);}
    if mode==22 {return gs_graph_overhead();}
    if mode==23 {return gs_new_graph(a);}
    gs_ensure(false);return 0;
}
fn main() {print(gs_entry(14,0,0,0,0,0,0));}
