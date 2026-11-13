extern "fg_w2_get" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg_w2_put" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg_w2_service" fn service(op:i64,a:i64,b:i64)->i64;
extern "fg_w2_set_create" fn set_create()->i64;
extern "fg_w2_set_destroy" fn set_destroy(h:i64);
extern "fg_w2_set_insert" fn set_insert(h:i64,key:i64)->i64;
extern "fg_w2_set_erase" fn set_erase(h:i64,key:i64)->i64;
extern "fg_w2_set_first" fn set_first(h:i64)->i64;
extern "fg_w2_set_last" fn set_last(h:i64)->i64;
extern "fg_w2_set_size" fn set_size(h:i64)->i64;
extern "fg_w2_set_key" fn set_key(h:i64,it:i64)->i64;

extern "fg_kv_get" fn kv_get(h:i64,k:i64,i:i64)->i64;
extern "fg_kv_put" fn kv_put(h:i64,k:i64,i:i64,v:i64);
extern "fg_kv_service" fn kv_service(op:i64,a:i64,b:i64)->i64;
extern "fg_kv_obj" fn kv_obj(kind:i64)->i64;
extern "fg_kv_create" fn kv_create_raw(n_stream:i64,kv_size:i64)->i64;
extern "fg_kv_destroy" fn kv_destroy_raw(h:i64);
extern "fg_kv_slot_push" fn kv_slot_push(s:i64,strm_idx:i64,cell:i64);
extern "fg_kv_slot_len" fn kv_slot_len(s:i64,strm_idx:i64)->i64;
extern "fg_kv_zero_cells" fn kv_zero_cells(kv:i64,strm:i64,sinfo:i64,contiguous:i64);
extern "fg_kv_clear_data" fn kv_clear_data(kv:i64);
extern "fg_kv_other_pos" fn kv_other_pos(kv:i64,seq:i64,is_max:i64)->i64;

fn ensure(ok:bool) {if !ok {let ignored=kv_service(0,0,0);assert(false);}}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0);ensure(a==0||b<=9223372036854775807/a);return a*b;}
fn max_seq()->i64 {let m=get(0,900,0);ensure(m>0);return m;}
fn tok_null()->i64 {return get(0,901,0);}
fn kv_cells(kv:i64,s:i64)->i64 {return kv_get(kv,10+s,0);}
fn kv_head(kv:i64,s:i64)->i64 {return kv_get(kv,100+s,0);}
fn kv_set_head(kv:i64,s:i64,v:i64) {kv_put(kv,100+s,0,v);}
fn kv_stream(kv:i64,seq:i64)->i64 {let n=kv_get(kv,0,0);if n==1 {return 0;}ensure(seq>=0&&seq<n);return seq;}
fn kv_n_stream(kv:i64)->i64 {return kv_get(kv,0,0);}
fn kv_size(kv:i64)->i64 {return get(kv_cells(kv,0),0,0);}

fn sc_n(c:i64)->i64 {return get(c,0,0);}
fn sc_check(c:i64,i:i64) {ensure(c!=0);ensure(i>=0&&i<sc_n(c));}
fn sc_seq_read(c:i64,i:i64,s:i64)->i64 {return get(c,1000+s,i);}
fn sc_seq_write(c:i64,i:i64,s:i64,v:i64) {put(c,1000+s,i,v);}
fn sc_seq_clear(c:i64,i:i64) {let m=max_seq();for s in m {sc_seq_write(c,i,s,0);}}
fn sc_seq_any(c:i64,i:i64)->i64 {let m=max_seq();var a:i64=0;for s in m {if sc_seq_read(c,i,s)!=0 {a=1;}}return a;}
fn sc_seq_total(c:i64,i:i64)->i64 {let m=max_seq();var t:i64=0;for s in m {if sc_seq_read(c,i,s)!=0 {t+=1;}}return t;}
fn sc_cell_clear(c:i64,i:i64) {put(c,10,i,-1);put(c,11,i,0);put(c,12,i,0);put(c,13,i,0);put(c,14,i,tok_null());sc_seq_clear(c,i);}
fn sc_is_empty(c:i64,i:i64)->i64 {sc_check(c,i);if get(c,10,i)==-1 {return 1;}return 0;}
fn sc_pos_get(c:i64,i:i64)->i64 {sc_check(c,i);let v=get(c,10,i);ensure(v!=-1);return v;}
fn sc_get_shift(c:i64,i:i64)->i64 {sc_check(c,i);ensure(get(c,10,i)!=-1);return get(c,11,i);}
fn sc_seq_count(c:i64,i:i64)->i64 {sc_check(c,i);ensure(get(c,10,i)!=-1);return sc_seq_total(c,i);}
fn sc_seq_has(c:i64,i:i64,s:i64)->i64 {sc_check(c,i);if sc_seq_read(c,i,s)!=0 {return 1;}return 0;}
fn sc_seq_get_single(c:i64,i:i64)->i64 {sc_check(c,i);ensure(sc_seq_total(c,i)==1);let m=max_seq();for s in m {if sc_seq_read(c,i,s)!=0 {return s;}}return -1;}
fn sc_pos_in(c:i64,i:i64,p0:i64,p1:i64)->i64 {let v=get(c,10,i);if v>=p0&&v<p1 {return 1;}return 0;}
fn sc_used_count(c:i64)->i64 {return set_size(get(c,20,0));}
fn sc_seq_key(pos:i64,cell:i64)->i64 {ensure(pos>=0&&pos<2147483648);ensure(cell>=0&&cell<4294967296);return mul(pos,4294967296)+cell;}
fn sc_key_pos(k:i64)->i64 {ensure(k>=0);return k/4294967296;}
fn sc_cell_pos_min(c:i64,s:i64)->i64 {let h=get(c,25+s,0);if set_size(h)==0 {return -1;}return sc_key_pos(set_key(h,set_first(h)));}
fn sc_cell_pos_max(c:i64,s:i64)->i64 {let h=get(c,25+s,0);if set_size(h)==0 {return -1;}return sc_key_pos(set_key(h,set_last(h)));}

fn sc_pos_set(c:i64,i:i64,v:i64) {sc_check(c,i);ensure(get(c,10,i)==-1);ensure(sc_seq_any(c,i)==0);put(c,10,i,v);let ignored=set_insert(get(c,20,0),i);}
fn sc_pos_add(c:i64,i:i64,d:i64)->i64 {sc_check(c,i);ensure(get(c,10,i)!=-1);sc_pos_rm_cell(c,i);let nv=get(c,10,i)+d;let ns=get(c,11,i)+d;put(c,10,i,nv);put(c,11,i,ns);put(c,1,0,1);if nv<0 {sc_cell_clear(c,i);let ignored=set_erase(get(c,20,0),i);return 1;}sc_pos_add_cell(c,i);return 0;}
fn sc_pos_div(c:i64,i:i64,d:i64) {sc_check(c,i);ensure(get(c,10,i)!=-1);ensure(d!=0);sc_pos_rm_cell(c,i);let old=get(c,10,i);let nv=old/d;put(c,10,i,nv);put(c,11,i,get(c,11,i)+old-nv);sc_pos_add_cell(c,i);put(c,1,0,1);}
fn sc_seq_add(c:i64,i:i64,s:i64) {sc_check(c,i);ensure(s>=0&&s<max_seq());ensure(get(c,10,i)!=-1);ensure(sc_seq_read(c,i,s)==0);sc_seq_write(c,i,s,1);let ignored=set_insert(get(c,25+s,0),sc_seq_key(get(c,10,i),i));}
fn sc_seq_rm_cell(c:i64,i:i64,s:i64)->i64 {sc_check(c,i);ensure(get(c,10,i)!=-1);ensure(sc_seq_read(c,i,s)!=0);sc_seq_write(c,i,s,0);let ignored=set_erase(get(c,25+s,0),sc_seq_key(get(c,10,i),i));if sc_seq_any(c,i)==0 {sc_cell_clear(c,i);let ignored2=set_erase(get(c,20,0),i);return 1;}return 0;}
fn sc_seq_keep_cell(c:i64,i:i64,s:i64)->i64 {sc_check(c,i);if sc_seq_read(c,i,s)!=0 {sc_pos_rm_cell(c,i);sc_seq_clear(c,i);sc_seq_write(c,i,s,1);sc_pos_add_cell(c,i);return 0;}if sc_seq_any(c,i)!=0 {sc_pos_rm_cell(c,i);sc_cell_clear(c,i);let ignored=set_erase(get(c,20,0),i);return 1;}ensure(get(c,10,i)==-1);return 0;}
fn sc_pos_rm_cell(c:i64,i:i64) {let m=max_seq();let v=get(c,10,i);for s in m {if sc_seq_read(c,i,s)!=0 {let ignored=set_erase(get(c,25+s,0),sc_seq_key(v,i));}}}
fn sc_pos_add_cell(c:i64,i:i64) {let m=max_seq();let v=get(c,10,i);for s in m {if sc_seq_read(c,i,s)!=0 {let ignored=set_insert(get(c,25+s,0),sc_seq_key(v,i));}}}
fn sc_rm(c:i64,i:i64) {sc_check(c,i);ensure(get(c,10,i)!=-1);sc_pos_rm_cell(c,i);sc_cell_clear(c,i);let ignored=set_erase(get(c,20,0),i);}
fn sc_reset(c:i64) {let n=sc_n(c);for i in n {sc_cell_clear(c,i);}put(c,1,0,0);set_destroy(get(c,20,0));put(c,20,0,set_create());let m=max_seq();for s in m {set_destroy(get(c,25+s,0));put(c,25+s,0,set_create());}}
fn sc_ext_set(c:i64,i:i64,x:i64,y:i64,t:i64) {sc_check(c,i);put(c,12,i,x);put(c,13,i,y);put(c,14,i,t);}
fn sc_ext_x(c:i64,i:i64)->i64 {return get(c,12,i);}
fn sc_ext_y(c:i64,i:i64)->i64 {return get(c,13,i);}
fn sc_ext_tok(c:i64,i:i64)->i64 {return get(c,14,i);}

fn sc_clear(kv:i64,data:i64) {let n=kv_n_stream(kv);var s:i64=0;while s<n {sc_reset(kv_cells(kv,s));kv_set_head(kv,s,0);s+=1;}if data!=0 {kv_clear_data(kv);}}

fn sc_seq_rm(kv:i64,seq:i64,p0:i64,p1:i64)->i64 {
 if kv_get(kv,62,0)!=0 {return 1;}
 ensure(seq==-1||(seq>=0&&seq<kv_n_stream_or_max(kv)));
 var a=p0;var b=p1;
 if a<0 {a=0;}
 if b<0 {b=2147483647;}
 if seq>=0 {
   let s=kv_stream(kv,seq);let c=kv_cells(kv,s);let n=sc_n(c);
   var nh=n;var i:i64=0;
   while i<n {if sc_pos_in(c,i,a,b)!=0&&sc_seq_has(c,i,seq)!=0 {if sc_seq_rm_cell(c,i,seq)==1 {if nh==n {nh=i;}}}i+=1;}
   if nh!=n {let h=kv_head(kv,s);if nh<h {kv_set_head(kv,s,nh);}}
 } else {
   var s:i64=0;let ns=kv_n_stream(kv);
   while s<ns {let c=kv_cells(kv,s);let n=sc_n(c);var nh=n;var i:i64=0;while i<n {if sc_pos_in(c,i,a,b)!=0 {sc_rm(c,i);if nh==n {nh=i;}}i+=1;}if nh!=n {let h=kv_head(kv,s);if nh<h {kv_set_head(kv,s,nh);}}s+=1;}
 }
 return 1;
}
fn kv_n_stream_or_max(kv:i64)->i64 {let n=kv_get(kv,0,0);if n>1 {return n;}return max_seq();}

fn sc_seq_cp(kv:i64,src:i64,dst:i64,p0:i64,p1:i64) {
 if kv_get(kv,62,0)!=0 {return;}
 ensure(src>=0&&src<kv_n_stream_or_max(kv));
 ensure(dst>=0&&dst<kv_n_stream_or_max(kv));
 let s0=kv_stream(kv,src);let s1=kv_stream(kv,dst);
 if s0==s1 {
   let c=kv_cells(kv,s0);
   if src==dst {return;}
   var a=p0;var b=p1;
   if a<0 {a=0;}
   if b<0 {b=2147483647;}
   let n=sc_n(c);var i:i64=0;
   while i<n {if sc_pos_in(c,i,a,b)!=0&&sc_seq_has(c,i,src)!=0 {sc_seq_add(c,i,dst);}i+=1;}
   return;
 }
 var full:i64=1;
 let sz=kv_size(kv);
 if p0>0&&p0+1<sz {full=0;}
 if p1>0&&p1+1<sz {full=0;}
 ensure(full!=0);
 let c0=kv_cells(kv,s0);
 let c1=kv_cells(kv,s1);
 let nsc=kv_get(kv,65,0);
 kv_put(kv,65,0,nsc+1);
 kv_put(kv,200+nsc,0,s0);
 kv_put(kv,300+nsc,0,s1);
 sc_reset(c1);
 let n=sc_n(c0);var i:i64=0;
 while i<n {
   if sc_seq_has(c0,i,src)!=0 {
     var p=sc_pos_get(c0,i);
     let sh=sc_get_shift(c0,i);
     let x=sc_ext_x(c0,i);let y=sc_ext_y(c0,i);let tk=sc_ext_tok(c0,i);
     if sh!=0 {p=p-sh;ensure(p>=0);}
     sc_pos_set(c1,i,p);
     sc_seq_add(c1,i,dst);
     if sh!=0 {let ignored=sc_pos_add(c1,i,sh);}
     sc_ext_set(c1,i,x,y,tk);
   }
   i+=1;
 }
 kv_set_head(kv,s1,kv_head(kv,s0));
}

fn sc_seq_keep(kv:i64,seq:i64) {if kv_get(kv,62,0)!=0 {return;}ensure(seq>=0&&seq<kv_n_stream_or_max(kv));let s=kv_stream(kv,seq);let c=kv_cells(kv,s);let n=sc_n(c);var nh=n;var i:i64=0;while i<n {if sc_seq_keep_cell(c,i,seq)==1 {if nh==n {nh=i;}}i+=1;}if nh!=n {let h=kv_head(kv,s);if nh<h {kv_set_head(kv,s,nh);}}}
fn sc_seq_add(kv:i64,seq:i64,p0:i64,p1:i64,shift:i64) {
 if kv_get(kv,62,0)!=0 {return;}
 ensure(seq>=0&&seq<kv_n_stream_or_max(kv));
 let s=kv_stream(kv,seq);let c=kv_cells(kv,s);
 if shift==0 {return;}
 var a=p0;var b=p1;
 if a<0 {a=0;}
 if b<0 {b=2147483647;}
 if a==b {return;}
 let n=sc_n(c);var nh=n;var i:i64=0;
 while i<n {if sc_pos_in(c,i,a,b)!=0&&sc_seq_has(c,i,seq)!=0 {if sc_pos_add(c,i,shift)==1 {if nh==n {nh=i;}}}i+=1;}
 if nh!=n {kv_set_head(kv,s,nh);} else {kv_set_head(kv,s,0);}
}
fn sc_seq_div(kv:i64,seq:i64,p0:i64,p1:i64,d:i64) {
 if kv_get(kv,62,0)!=0 {return;}
 ensure(seq>=0&&seq<kv_n_stream_or_max(kv));
 let s=kv_stream(kv,seq);let c=kv_cells(kv,s);
 if d==1 {return;}
 var a=p0;var b=p1;
 if a<0 {a=0;}
 if b<0 {b=2147483647;}
 if a==b {return;}
 let n=sc_n(c);var i:i64=0;
 while i<n {if sc_pos_in(c,i,a,b)!=0&&sc_seq_has(c,i,seq)!=0 {sc_pos_div(c,i,d);}i+=1;}
}
fn sc_seq_pos_min(kv:i64,seq:i64)->i64 {if kv_get(kv,62,0)!=0 {return kv_other_pos(kv,seq,0);}ensure(seq>=0&&seq<kv_n_stream_or_max(kv));return sc_cell_pos_min(kv_cells(kv,kv_stream(kv,seq)),seq);}
fn sc_seq_pos_max(kv:i64,seq:i64)->i64 {if kv_get(kv,62,0)!=0 {return kv_other_pos(kv,seq,1);}ensure(seq>=0&&seq<kv_n_stream_or_max(kv));return sc_cell_pos_max(kv_cells(kv,kv_stream(kv,seq)),seq);}

fn slot_n_stream(s:i64)->i64 {return kv_get(s,3,0);}
fn slot_strm(s:i64,x:i64)->i64 {return kv_get(s,10+x,0);}
fn slot_size(s:i64)->i64 {return kv_slot_len(s,0);}
fn slot_idx(s:i64,x:i64,j:i64)->i64 {return kv_get(s,100+x,j);}
fn slot_contiguous(s:i64)->i64 {let ns=slot_n_stream(s);if ns==0 {return 1;}if slot_size(s)==0 {return 1;}if ns>1 {return 0;}let h=slot_idx(s,0,0);let n=slot_size(s);var j:i64=0;while j<n {if slot_idx(s,0,j)!=h+j {return 0;}j+=1;}return 1;}

fn sc_state_clear(kv:i64,seq:i64) {
 if seq==-1 {sc_clear(kv,1);return;}
 ensure(seq>=0&&seq<kv_n_stream_or_max(kv));
 let strm=kv_stream(kv,seq);
 let c=kv_cells(kv,strm);
 let sinfo=kv_obj(3);
 kv_put(sinfo,0,0,strm);kv_put(sinfo,1,0,strm);kv_put(sinfo,3,0,1);kv_put(sinfo,10,0,strm);
 let n=sc_n(c);var i:i64=0;
 while i<n {if sc_seq_has(c,i,seq)!=0&&sc_seq_count(c,i)==1 {kv_slot_push(sinfo,0,i);}i+=1;}
 sc_state_clear_at(kv,seq,strm,sinfo);
 kv_destroy_raw(sinfo);
}
fn sc_state_clear_at(kv:i64,seq:i64,strm:i64,sinfo:i64) {
 if seq==-1 {sc_clear(kv,1);return;}
 let ignored=sc_seq_rm(kv,seq,-1,-1);
 if slot_size(sinfo)==0 {return;}
 let contig=slot_contiguous(sinfo);
 kv_zero_cells(kv,strm,sinfo,contig);
}

fn sc_kv_create(n_stream:i64,size:i64)->i64 {let kv=kv_create_raw(n_stream,size);var s:i64=0;while s<n_stream {let c=kv_cells(kv,s);put(c,0,0,size);sc_reset(c);kv_set_head(kv,s,0);s+=1;}return kv;}
fn sc_kv_destroy(kv:i64) {if kv!=0 {let m=max_seq();let n=kv_n_stream(kv);var s:i64=0;while s<n {let c=kv_cells(kv,s);set_destroy(get(c,20,0));for q in m {set_destroy(get(c,25+q,0));}let ignored=service(2,c,0);s+=1;}kv_destroy_raw(kv);}}

fn selftest()->i64 {
 let kv=sc_kv_create(1,4);
 let c=kv_cells(kv,0);
 sc_pos_set(c,0,1);sc_pos_set(c,1,2);
 sc_seq_add(c,0,1);sc_seq_add(c,1,1);
 ensure(sc_used_count(c)==2);
 let ignored=sc_seq_rm(kv,1,-1,-1);
 ensure(sc_used_count(c)==0);
 sc_pos_set(c,2,5);sc_seq_add(c,2,1);sc_seq_add(c,2,2);
 sc_seq_keep(kv,2);
 ensure(sc_seq_total(c,2)==1);
 sc_seq_add(kv,1,-1,-1,3);
 ensure(sc_pos_get(c,2)==8);
 ensure(sc_seq_pos_min(kv,1)==8);
 sc_seq_div(kv,1,-1,-1,2);
 ensure(sc_pos_get(c,2)==4);
 sc_seq_cp(kv,1,0,-1,-1);
 ensure(sc_seq_has(c,2,0)==1);
 ensure(sc_seq_count(c,2)==3);
 sc_state_clear(kv,1);
 ensure(sc_seq_has(c,2,1)==0);
 ensure(sc_seq_count(c,2)==2);
 sc_clear(kv,0);
 ensure(sc_used_count(c)==0);
 sc_kv_destroy(kv);
 return 1;
}

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 kv_service(100,mode,0);
 if mode==0 {return sc_kv_create(a,b);}
 if mode==1 {sc_kv_destroy(a);return 0;}
 if mode==2 {sc_clear(a,b);return 0;}
 if mode==3 {return sc_seq_rm(a,b,c,d);}
 if mode==4 {sc_seq_cp(a,b,c,d,e);return 0;}
 if mode==5 {sc_seq_keep(a,b);return 0;}
 if mode==6 {sc_seq_add(a,b,c,d,e);return 0;}
 if mode==7 {sc_seq_div(a,b,c,d,e);return 0;}
 if mode==8 {return sc_seq_pos_min(a,b);}
 if mode==9 {return sc_seq_pos_max(a,b);}
 if mode==10 {sc_state_clear(a,b);return 0;}
 if mode==11 {return selftest();}
 ensure(false);return 0;
}
fn main(){print(entry(11,0,0,0,0,0));}
