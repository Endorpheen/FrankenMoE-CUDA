extern "fg153_gs_const" fn gs_const(code:i64)->i64;
extern "fg153_gs_tensor_get" fn gs_tensor_get(t:i64,k:i64,i:i64)->i64;
extern "fg153_gs_graph_get" fn gs_graph_get(g:i64,k:i64,i:i64)->i64;
extern "fg153_gs_graph_put" fn gs_graph_put(g:i64,k:i64,i:i64,v:i64);
extern "fg153_gs_hash_size" fn gs_hash_size(min_sz:i64)->i64;
extern "fg153_gs_bitset_bytes" fn gs_bitset_bytes(hash_size:i64)->i64;
extern "fg153_gs_create_graph" fn gs_create_graph(ctx:i64,size:i64,grads:i64)->i64;
extern "fg153_gs_view_into" fn gs_view_into(out:i64,src:i64,i0:i64,i1:i64);
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

fn gs_new_graph_custom(ctx:i64,size:i64,grads:i64)->i64 {
    let g=gs_create_graph(ctx,size,grads);
    if g==0 {return 0;}
    gs_graph_put(g,0,0,size);gs_graph_put(g,1,0,0);gs_graph_put(g,2,0,0);
    gs_graph_put(g,6,0,gs_const_l2r());gs_graph_put(g,12,0,0);
    gs_reset_visited(g);
    return g;
}
fn gs_new_graph(ctx:i64)->i64 {return gs_new_graph_custom(ctx,gs_default_graph_size(),0);}

fn gs_graph_view(out:i64,src:i64,i0:i64,i1:i64)->i64 {
    if out==0 {return 0;}
    if src==0 {return 0;}
    gs_ensure(i0>=0);gs_ensure(i1>=i0);
    gs_view_into(out,src,i0,i1);
    gs_ensure(gs_size(out)==0);
    gs_ensure(gs_n_nodes(out)==i1-i0);
    gs_ensure(gs_n_leafs(out)==0);
    gs_ensure(gs_order(out)==gs_order(src));
    gs_ensure(gs_hash_size_field(out)==gs_hash_size_field(src));
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

fn gs_selftest()->i64 {gs_ensure(gs_pad(5,2)==6);gs_ensure(gs_default_graph_size()==2048);gs_ensure(gs_op_none()==0);gs_ensure(gs_hash_full()==-1);return 1;}
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
    gs_ensure(false);return 0;
}
fn main() {print(gs_entry(14,0,0,0,0,0,0));}
