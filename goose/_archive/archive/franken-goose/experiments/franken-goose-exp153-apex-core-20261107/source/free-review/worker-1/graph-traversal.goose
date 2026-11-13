extern "fg153_gt_get" fn gt_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_gt_put" fn gt_put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_gt_hash_find" fn gt_hash_find(g:i64,t:i64)->i64;
extern "fg153_gt_ensure_name" fn gt_ensure_name(t:i64,leaf:i64,idx:i64);

fn ensure(ok:bool) {if !ok {assert(false);}}
fn max_src()->i64 {return 10;}
fn op_none()->i64 {return 0;}
fn flag_param()->i64 {return 4;}
fn flag_compute()->i64 {return 16;}
fn order_l2r()->i64 {return gt_get(0,60,0);}
fn order_r2l()->i64 {return gt_get(0,61,0);}

fn t_op(t:i64)->i64 {return gt_get(t,0,0);}
fn t_flags(t:i64)->i64 {return gt_get(t,1,0);}
fn t_src(t:i64,k:i64)->i64 {return gt_get(t,2,k);}
fn t_has_name(t:i64)->bool {return gt_get(t,4,0)!=0;}

fn g_size(g:i64)->i64 {return gt_get(g,0,0);}
fn g_n_nodes(g:i64)->i64 {return gt_get(g,1,0);}
fn g_set_n_nodes(g:i64,v:i64) {gt_put(g,1,0,v);}
fn g_n_leafs(g:i64)->i64 {return gt_get(g,2,0);}
fn g_set_n_leafs(g:i64,v:i64) {gt_put(g,2,0,v);}
fn g_order(g:i64)->i64 {return gt_get(g,6,0);}
fn g_is_visited(g:i64,pos:i64)->bool {return gt_get(g,7,pos)!=0;}
fn g_mark_visited(g:i64,pos:i64,t:i64) {gt_put(g,8,pos,t);gt_put(g,7,pos,1);gt_put(g,5,pos,0);}
fn g_use_count(g:i64,pos:i64)->i64 {return gt_get(g,5,pos);}
fn g_inc_use(g:i64,pos:i64) {gt_put(g,5,pos,g_use_count(g,pos)+1);}
fn g_hash_size(g:i64)->i64 {return gt_get(g,9,0);}
fn graph_clear_control(g:i64) {
    gt_put(g,2,0,0);gt_put(g,1,0,0);
    var pos:i64=0;while pos<g_hash_size(g) {gt_put(g,7,pos,0);pos+=1;}
}

fn src_index(g:i64,i:i64)->i64 {
    if g_order(g)==order_l2r() {return i;}
    if g_order(g)==order_r2l() {return max_src()-1-i;}
    return i;
}

fn visit_parents(g:i64,node:i64,compute:bool)->i64 {
    if t_op(node)!=op_none()&&compute {gt_put(node,1,0,t_flags(node)|flag_compute());}
    let pos=gt_hash_find(g,node);
    ensure(pos!=gt_get(0,62,0));
    if g_is_visited(g,pos) {
        if compute {
            for i in 10 {
                let src=t_src(node,i);
                if src!=0&&(t_flags(src)&flag_compute())==0 {let ignored=visit_parents(g,src,true);}
            }
        }
        return pos;
    }
    g_mark_visited(g,pos,node);
    for i in 10 {
        let k=src_index(g,i);
        let src=t_src(node,k);
        if src!=0 {let src_pos=visit_parents(g,src,compute);g_inc_use(g,src_pos);}
    }
    if t_op(node)==op_none()&&(t_flags(node)&flag_param())==0 {
        ensure(g_n_leafs(g)<g_size(g));
        if !t_has_name(node) {gt_ensure_name(node,1,g_n_leafs(g));}
        gt_put(g,4,g_n_leafs(g),node);g_set_n_leafs(g,g_n_leafs(g)+1);
    } else {
        ensure(g_n_nodes(g)<g_size(g));
        if !t_has_name(node) {gt_ensure_name(node,0,g_n_nodes(g));}
        gt_put(g,3,g_n_nodes(g),node);g_set_n_nodes(g,g_n_nodes(g)+1);
    }
    return pos;
}

fn visit_parents_branch(g:i64,node:i64,compute:i64)->i64 {
    if compute!=0 {return visit_parents(g,node,true);}
    return visit_parents(g,node,false);
}

fn build_forward_impl(g:i64,tensor:i64,expand:i64,compute:i64)->i64 {
    if expand==0 {graph_clear_control(g);}
    let n_old=g_n_nodes(g);
    let ignored=visit_parents_branch(g,tensor,compute);
    let n_new=g_n_nodes(g)-n_old;
    if n_new>0 {ensure(gt_get(g,3,g_n_nodes(g)-1)==tensor);}
    return n_new;
}

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
    if mode==0 {return build_forward_impl(a,b,1,1);}
    if mode==1 {return build_forward_impl(a,b,1,0);}
    if mode==2 {ensure(d>=0);ensure(0<=c&&c<d);var i:i64=0;while i<d {var compute:i64=0;if i==c {compute=1;}let ignored=build_forward_impl(a,gt_get(b,0,i),1,compute);i+=1;}return gt_get(b,0,c);}
    if mode==11 {return selftest();}
    if mode==3 {return visit_parents_branch(a,b,c);}
    if mode==4 {return g_n_nodes(a);}
    if mode==5 {return g_n_leafs(a);}
    if mode==6 {return gt_get(a,3,b);}
    if mode==7 {return gt_get(a,4,b);}
    if mode==8 {return g_use_count(a,gt_hash_find(a,b));}
    if mode==9 {return t_flags(b);}
    if mode==10 {return if g_is_visited(a,gt_hash_find(a,b)) {1} else {0};}
    ensure(false);return 0;
}

fn selftest()->i64 {ensure(max_src()==10&&op_none()==0&&flag_param()==4&&flag_compute()==16);return 1;}
fn main() {print(entry(11,0,0,0,0,0,0));}
