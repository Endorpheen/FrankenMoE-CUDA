extern "fg_w2_get" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg_w2_put" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg_w2_service" fn service(op:i64,a:i64,b:i64)->i64;
extern "fg_w2_witness" fn witness(mode:i64);
extern "fg_w2_set_create" fn set_create()->i64;
extern "fg_w2_set_destroy" fn set_destroy(h:i64);
extern "fg_w2_set_insert" fn set_insert(h:i64,key:i64)->i64;
extern "fg_w2_set_erase" fn set_erase(h:i64,key:i64)->i64;
extern "fg_w2_set_lower_bound" fn set_lower_bound(h:i64,key:i64)->i64;
extern "fg_w2_set_count" fn set_count(h:i64,key:i64)->i64;
extern "fg_w2_set_first" fn set_first(h:i64)->i64;
extern "fg_w2_set_last" fn set_last(h:i64)->i64;
extern "fg_w2_set_next" fn set_next(h:i64,it:i64)->i64;
extern "fg_w2_set_prev" fn set_prev(h:i64,it:i64)->i64;
extern "fg_w2_set_size" fn set_size(h:i64)->i64;
extern "fg_w2_set_key" fn set_key(h:i64,it:i64)->i64;

fn ensure(ok:bool) {if !ok {let ignored=service(0,0,0);assert(false);}}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0);ensure(a==0||b<=9223372036854775807/a);return a*b;}
fn max_seq()->i64 {let m=get(0,900,0);ensure(m>0);return m;}
fn token_null()->i64 {return get(0,901,0);}
fn n_cells(p:i64)->i64 {ensure(p!=0);return get(p,0,0);}
fn check_cell(p:i64,i:i64) {ensure(p!=0);ensure(i>=0&&i<n_cells(p));}
fn check_seq(s:i64) {ensure(s>=0&&s<max_seq());}
fn used_set(p:i64)->i64 {ensure(p!=0);return get(p,20,0);}
fn seq_pos_set(p:i64,s:i64)->i64 {ensure(p!=0);check_seq(s);return get(p,25,s);}
fn seq_read(p:i64,i:i64,s:i64)->i64 {return get(p,1000+s,i);}
fn seq_write(p:i64,i:i64,s:i64,v:i64) {put(p,1000+s,i,v);}
fn seq_clear(p:i64,i:i64) {let m=max_seq();for s in m {seq_write(p,i,s,0);}}
fn seq_any(p:i64,i:i64)->i64 {let m=max_seq();var any:i64=0;for s in m {if seq_read(p,i,s)!=0 {any=1;}}return any;}
fn seq_total(p:i64,i:i64)->i64 {let m=max_seq();var cnt:i64=0;for s in m {if seq_read(p,i,s)!=0 {cnt+=1;}}return cnt;}
fn cell_clear(p:i64,i:i64) {put(p,10,i,-1);put(p,11,i,0);put(p,12,i,0);put(p,13,i,0);put(p,14,i,token_null());seq_clear(p,i);}
fn cell_copy(dst:i64,di:i64,src:i64,si:i64) {put(dst,10,di,get(src,10,si));put(dst,12,di,get(src,12,si));put(dst,13,di,get(src,13,si));put(dst,14,di,get(src,14,si));let m=max_seq();for s in m {seq_write(dst,di,s,seq_read(src,si,s));}}
fn seq_key(pos:i64,cell:i64)->i64 {ensure(pos>=0&&pos<2147483648);ensure(cell>=0&&cell<4294967296);return mul(pos,4294967296)+cell;}
fn seq_key_pos(k:i64)->i64 {ensure(k>=0);return k/4294967296;}
fn seq_key_cell(k:i64)->i64 {ensure(k>=0);let q=k/4294967296;return k-mul(q,4294967296);}
fn seq_pos_insert(p:i64,s:i64,pos:i64,cell:i64) {let ignored=set_insert(seq_pos_set(p,s),seq_key(pos,cell));}
fn seq_pos_erase(p:i64,s:i64,pos:i64,cell:i64) {let ignored=set_erase(seq_pos_set(p,s),seq_key(pos,cell));}
fn seq_pos_remove_cell(p:i64,i:i64) {let m=max_seq();let v=get(p,10,i);for s in m {if seq_read(p,i,s)!=0 {seq_pos_erase(p,s,v,i);}}}
fn seq_pos_add_cell(p:i64,i:i64) {let m=max_seq();let v=get(p,10,i);for s in m {if seq_read(p,i,s)!=0 {seq_pos_insert(p,s,v,i);}}}
fn used_insert(p:i64,i:i64) {let ignored=set_insert(used_set(p),i);}
fn used_erase(p:i64,i:i64) {let ignored=set_erase(used_set(p),i);}
fn reset(p:i64) {ensure(p!=0);let n=n_cells(p);ensure(n>=0);for i in n {cell_clear(p,i);}put(p,1,0,0);set_destroy(used_set(p));put(p,20,0,set_create());let m=max_seq();for s in m {set_destroy(seq_pos_set(p,s));put(p,25,s,set_create());}}
fn create(n:i64)->i64 {ensure(n>=0);let p=service(1,n,0);ensure(p!=0);put(p,0,0,n);reset(p);return p;}
fn destroy(p:i64) {if p!=0 {set_destroy(used_set(p));let m=max_seq();for s in m {set_destroy(seq_pos_set(p,s));}let ignored=service(2,p,0);}}
fn reset_shift(p:i64) {ensure(p!=0);let n=n_cells(p);for i in n {put(p,11,i,0);}put(p,1,0,0);}
fn has_shift(p:i64)->i64 {ensure(p!=0);return get(p,1,0);}
fn is_empty(p:i64,i:i64)->i64 {check_cell(p,i);let v=get(p,10,i);ensure(v==-1||v>=0);if v==-1 {return 1;}return 0;}
fn used_count(p:i64)->i64 {ensure(p!=0);return set_size(used_set(p));}
fn used_min(p:i64)->i64 {ensure(p!=0);let h=used_set(p);if set_size(h)==0 {return 0;}return set_key(h,set_first(h));}
fn used_max_p1(p:i64)->i64 {ensure(p!=0);let h=used_set(p);if set_size(h)==0 {return 0;}return set_key(h,set_last(h))+1;}
fn pos_get(p:i64,i:i64)->i64 {check_cell(p,i);let v=get(p,10,i);ensure(v!=-1);return v;}
fn get_shift(p:i64,i:i64)->i64 {check_cell(p,i);ensure(get(p,10,i)!=-1);return get(p,11,i);}
fn pos_in(p:i64,i:i64,p0:i64,p1:i64)->i64 {check_cell(p,i);let v=get(p,10,i);if v>=p0&&v<p1 {return 1;}return 0;}
fn pos_set(p:i64,i:i64,v:i64) {check_cell(p,i);ensure(get(p,10,i)==-1);ensure(seq_any(p,i)==0);put(p,10,i,v);used_insert(p,i);}
fn pos_add(p:i64,i:i64,d:i64)->i64 {check_cell(p,i);ensure(get(p,10,i)!=-1);seq_pos_remove_cell(p,i);let nv=get(p,10,i)+d;let ns=get(p,11,i)+d;put(p,10,i,nv);put(p,11,i,ns);put(p,1,0,1);if nv<0 {cell_clear(p,i);used_erase(p,i);return 1;}seq_pos_add_cell(p,i);return 0;}
fn pos_div(p:i64,i:i64,d:i64) {check_cell(p,i);ensure(get(p,10,i)!=-1);ensure(d!=0);seq_pos_remove_cell(p,i);let old=get(p,10,i);let nv=old/d;put(p,10,i,nv);put(p,11,i,get(p,11,i)+old-nv);seq_pos_add_cell(p,i);put(p,1,0,1);}
fn seq_count(p:i64,i:i64)->i64 {check_cell(p,i);ensure(get(p,10,i)!=-1);return seq_total(p,i);}
fn seq_has(p:i64,i:i64,s:i64)->i64 {check_cell(p,i);check_seq(s);if seq_read(p,i,s)!=0 {return 1;}return 0;}
fn seq_add(p:i64,i:i64,s:i64) {check_cell(p,i);check_seq(s);ensure(get(p,10,i)!=-1);ensure(seq_read(p,i,s)==0);seq_write(p,i,s,1);seq_pos_insert(p,s,get(p,10,i),i);}
fn seq_get_single(p:i64,i:i64)->i64 {check_cell(p,i);ensure(seq_total(p,i)==1);let m=max_seq();for s in m {if seq_read(p,i,s)!=0 {return s;}}return -1;}
fn seq_rm(p:i64,i:i64,s:i64)->i64 {check_cell(p,i);check_seq(s);ensure(get(p,10,i)!=-1);ensure(seq_read(p,i,s)!=0);seq_write(p,i,s,0);seq_pos_erase(p,s,get(p,10,i),i);if seq_any(p,i)==0 {cell_clear(p,i);used_erase(p,i);return 1;}return 0;}
fn seq_keep(p:i64,i:i64,s:i64)->i64 {check_cell(p,i);check_seq(s);if seq_read(p,i,s)!=0 {seq_pos_remove_cell(p,i);seq_clear(p,i);seq_write(p,i,s,1);seq_pos_insert(p,s,get(p,10,i),i);return 0;}if seq_any(p,i)!=0 {seq_pos_remove_cell(p,i);cell_clear(p,i);used_erase(p,i);return 1;}ensure(get(p,10,i)==-1);return 0;}
fn seq_pos_min(p:i64,s:i64)->i64 {ensure(p!=0);check_seq(s);let h=seq_pos_set(p,s);if set_size(h)==0 {return -1;}return seq_key_pos(set_key(h,set_first(h)));}
fn seq_pos_max(p:i64,s:i64)->i64 {ensure(p!=0);check_seq(s);let h=seq_pos_set(p,s);if set_size(h)==0 {return -1;}return seq_key_pos(set_key(h,set_last(h)));}
fn seq_pos_count(p:i64,s:i64)->i64 {ensure(p!=0);check_seq(s);return set_size(seq_pos_set(p,s));}
fn seq_pos_tok_le(p:i64,s:i64,pp:i64)->i64 {ensure(p!=0);check_seq(s);if pp<0 {return token_null();}let h=seq_pos_set(p,s);if set_size(h)==0 {return token_null();}var it:i64=0;if pp>=2147483647 {it=0;}else {it=set_lower_bound(h,mul(pp+1,4294967296));}var pred:i64=0;if it==0 {pred=set_last(h);}else {pred=set_prev(h,it);}if pred==0 {return token_null();}let cell=seq_key_cell(set_key(h,pred));return get(p,14,cell);}
fn seq_pos_nth_pos(p:i64,s:i64,k:i64)->i64 {ensure(p!=0);check_seq(s);ensure(k>=0);let h=seq_pos_set(p,s);ensure(k<set_size(h));var it=set_first(h);var j:i64=0;while j<k {it=set_next(h,it);j+=1;}ensure(it!=0);return seq_key_pos(set_key(h,it));}
fn seq_pos_nth_cell(p:i64,s:i64,k:i64)->i64 {ensure(p!=0);check_seq(s);ensure(k>=0);let h=seq_pos_set(p,s);ensure(k<set_size(h));var it=set_first(h);var j:i64=0;while j<k {it=set_next(h,it);j+=1;}ensure(it!=0);return seq_key_cell(set_key(h,it));}
fn ext_get_x(p:i64,i:i64)->i64 {check_cell(p,i);ensure(get(p,10,i)!=-1);return get(p,12,i);}
fn ext_get_y(p:i64,i:i64)->i64 {check_cell(p,i);ensure(get(p,10,i)!=-1);return get(p,13,i);}
fn ext_get_tok(p:i64,i:i64)->i64 {check_cell(p,i);ensure(get(p,10,i)!=-1);return get(p,14,i);}
fn ext_set(p:i64,i:i64,x:i64,y:i64,tok:i64) {check_cell(p,i);put(p,12,i,x);put(p,13,i,y);put(p,14,i,tok);}
fn ext_is_2d_gt(p:i64,i:i64,ox:i64,oy:i64)->i64 {check_cell(p,i);let x=get(p,12,i);let y=get(p,13,i);if y>oy {return 1;}if y==oy&&x>ox {return 1;}return 0;}
fn rm(p:i64,i:i64) {check_cell(p,i);ensure(get(p,10,i)!=-1);seq_pos_remove_cell(p,i);cell_clear(p,i);used_erase(p,i);}
fn cp_range(src:i64,i:i64,n:i64)->i64 {ensure(src!=0);let total=n_cells(src);ensure(i>=0&&n>=0);ensure(i<=total);ensure(n<=total-i);let dst=create(n);for j in n {let si=i+j;ensure(get(src,11,si)==0);cell_copy(dst,j,src,si);}return dst;}
fn cp_list(src:i64,idx:i64,count:i64)->i64 {ensure(src!=0);ensure(idx!=0||count==0);ensure(count>=0);let dst=create(count);for j in count {let si=get(idx,0,j);check_cell(src,si);ensure(get(src,11,si)==0);cell_copy(dst,j,src,si);}return dst;}
fn set_range(dst:i64,i:i64,other:i64) {ensure(dst!=0);ensure(other!=0);let total=n_cells(dst);let m=n_cells(other);ensure(i>=0);ensure(i<=total);ensure(m<=total-i);for j in m {let di=i+j;ensure(get(dst,11,di)==0);let old_pos=get(dst,10,di);let new_pos=get(other,10,j);if old_pos==-1&&new_pos!=-1 {used_insert(dst,di);}if old_pos!=-1&&new_pos==-1 {used_erase(dst,di);}if old_pos!=-1 {seq_pos_remove_cell(dst,di);}cell_copy(dst,di,other,j);if get(dst,10,di)!=-1 {seq_pos_add_cell(dst,di);}}}
fn set_list(dst:i64,idx:i64,other:i64,count:i64) {ensure(dst!=0);ensure(other!=0);ensure(idx!=0||count==0);let m=n_cells(other);ensure(count==m);for j in m {let di=get(idx,0,j);check_cell(dst,di);ensure(get(dst,11,di)==0);let old_pos=get(dst,10,di);let new_pos=get(other,10,j);if old_pos==-1&&new_pos!=-1 {used_insert(dst,di);}if old_pos!=-1&&new_pos==-1 {used_erase(dst,di);}if old_pos!=-1 {seq_pos_remove_cell(dst,di);}cell_copy(dst,di,other,j);if get(dst,10,di)!=-1 {seq_pos_add_cell(dst,di);}}}
fn selftest()->i64 {let p=create(4);pos_set(p,0,3);pos_set(p,1,7);pos_set(p,2,7);seq_add(p,0,0);seq_add(p,1,0);seq_add(p,2,0);ext_set(p,0,0,0,100);ext_set(p,1,0,0,101);ext_set(p,2,0,0,102);ensure(used_count(p)==3);ensure(used_min(p)==0);ensure(used_max_p1(p)==3);ensure(seq_pos_min(p,0)==3);ensure(seq_pos_max(p,0)==7);ensure(seq_pos_count(p,0)==3);ensure(seq_pos_tok_le(p,0,7)==102);ensure(seq_pos_nth_cell(p,0,1)==1);ensure(pos_add(p,0,2)==0);ensure(pos_get(p,0)==5);ensure(get_shift(p,0)==2);ensure(has_shift(p)==1);reset_shift(p);ensure(has_shift(p)==0);let snap=cp_range(p,0,3);ensure(n_cells(snap)==3);ensure(pos_get(snap,0)==5);ensure(used_count(snap)==0);ensure(seq_pos_min(snap,0)==-1);let q=create(3);set_range(q,0,snap);ensure(used_count(q)==3);ensure(seq_pos_min(q,0)==5);ensure(seq_pos_max(q,0)==7);ensure(seq_pos_tok_le(q,0,7)==102);ensure(seq_rm(p,2,0)==1);ensure(seq_pos_count(p,0)==2);ensure(used_count(p)==2);destroy(snap);destroy(q);destroy(p);return 1;}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
 witness(mode);
 if mode==0 {return create(a);}
 if mode==1 {destroy(a);return 0;}
 if mode==2 {reset(a);return 0;}
 if mode==3 {reset_shift(a);return 0;}
 if mode==4 {return n_cells(a);}
 if mode==5 {return is_empty(a,b);}
 if mode==6 {return used_count(a);}
 if mode==7 {return used_min(a);}
 if mode==8 {return used_max_p1(a);}
 if mode==9 {return has_shift(a);}
 if mode==10 {return pos_get(a,b);}
 if mode==11 {return get_shift(a,b);}
 if mode==12 {return pos_in(a,b,c,d);}
 if mode==13 {pos_set(a,b,c);return 0;}
 if mode==14 {return pos_add(a,b,c);}
 if mode==15 {pos_div(a,b,c);return 0;}
 if mode==16 {return seq_count(a,b);}
 if mode==17 {return seq_has(a,b,c);}
 if mode==18 {seq_add(a,b,c);return 0;}
 if mode==19 {return seq_get_single(a,b);}
 if mode==20 {return seq_rm(a,b,c);}
 if mode==21 {return seq_keep(a,b,c);}
 if mode==22 {return seq_pos_min(a,b);}
 if mode==23 {return seq_pos_max(a,b);}
 if mode==24 {return seq_pos_tok_le(a,b,c);}
 if mode==25 {return seq_pos_count(a,b);}
 if mode==26 {return seq_pos_nth_pos(a,b,c);}
 if mode==27 {return seq_pos_nth_cell(a,b,c);}
 if mode==28 {return ext_get_x(a,b);}
 if mode==29 {return ext_get_y(a,b);}
 if mode==30 {return ext_get_tok(a,b);}
 if mode==31 {ext_set(a,b,c,d,e);return 0;}
 if mode==32 {return cp_range(a,b,c);}
 if mode==33 {set_range(a,b,c);return 0;}
 if mode==34 {return cp_list(a,b,c);}
 if mode==35 {set_list(a,b,c,d);return 0;}
 if mode==36 {rm(a,b);return 0;}
 if mode==37 {return ext_is_2d_gt(a,b,c,d);}
 if mode==38 {return selftest();}
 ensure(false);return 0;
}
fn main(){print(entry(38,0,0,0,0,0,0));}
