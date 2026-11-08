const MAX_SRC:i64=10;
const OP_ACC:i64=5;
const OP_ADD:i64=2;
const OP_ADD1:i64=4;
const OP_ADD_ID:i64=3;
const OP_ARGSORT:i64=69;
const OP_CLAMP:i64=50;
const OP_CONCAT:i64=22;
const OP_CONT:i64=35;
const OP_CPY:i64=34;
const OP_CUMSUM:i64=16;
const OP_DIAG:i64=43;
const OP_DIV:i64=8;
const OP_DUP:i64=1;
const OP_FILL:i64=73;
const OP_FLASH_ATTN_EXT:i64=74;
const OP_GATED_DELTA_NET:i64=86;
const OP_GET_ROWS:i64=40;
const OP_GLU:i64=100;
const OP_L2_NORM:i64=28;
const OP_MEAN:i64=17;
const OP_MUL:i64=7;
const OP_MUL_MAT:i64=29;
const OP_MUL_MAT_ID:i64=30;
const OP_NORM:i64=24;
const OP_PAD:i64=64;
const OP_PERMUTE:i64=38;
const OP_REPEAT:i64=20;
const OP_RESHAPE:i64=36;
const OP_RMS_NORM:i64=25;
const OP_ROPE:i64=48;
const OP_SCALE:i64=32;
const OP_SET:i64=33;
const OP_SET_ROWS:i64=42;
const OP_SOFT_MAX:i64=46;
const OP_SOLVE_TRI:i64=85;
const OP_SQR:i64=9;
const OP_SQRT:i64=10;
const OP_SSM_CONV:i64=76;
const OP_SUB:i64=6;
const OP_SUM:i64=14;
const OP_SUM_ROWS:i64=15;
const OP_TOP_K:i64=70;
const OP_TRANSPOSE:i64=39;
const OP_TRI:i64=72;
const OP_UNARY:i64=91;
const OP_VIEW:i64=37;
const ROPE_MROPE:i64=8;
const SORT_DESC:i64=1;
const TYPE_F16:i64=1;
const TYPE_F32:i64=0;
const TYPE_I32:i64=26;
const TYPE_I64:i64=27;
struct Shape { ne:i64[4] }
extern "fg153_get" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg153_entry" fn core(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64;
extern "fg_apex_ops_arg" fn arg(p:i64,i:i64)->i64;
extern "fg_apex_ops_i32" fn read_i32(p:i64,i:i64)->i64;
extern "fg_apex_ops_put" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg_apex_ops_get" fn extra(p:i64,k:i64,i:i64)->i64;
extern "fg_apex_ops_shape" fn shape_query(mode:i64,a:i64,b:i64,m:i64,n:i64)->i64;
extern "fg_apex_ops_alloc" fn alloc(ctx:i64,kind:i64,dims:i64,s:const Shape&,view:i64,off:i64)->i64;
extern "fg_apex_ops_name" fn name(t:i64,kind:i64,a:i64,b:i64);
extern "fg_apex_ops_fail" fn fail();
extern "fg_apex_ops_witness" fn witness(mode:i64);
fn ensure(ok:bool){if !ok {fail();assert(false);}}
fn checked_add(a:i64,b:i64)->i64{ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn signed_add(a:i64,b:i64)->i64{if b>=0{ensure(a<=9223372036854775807-b);}else{ensure(a>=(-9223372036854775807-1)-b);}return a+b;}
fn checked_mul(a:i64,b:i64)->i64{ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn ne(t:i64,i:i64)->i64{return get(t,26,i);}
fn nb(t:i64,i:i64)->i64{return get(t,27,i);}
fn ty(t:i64)->i64{return get(t,20,0);}
fn shape(t:i64)->Shape{var s:Shape=default<Shape>();for i in 4{s.ne[i]=ne(t,i);}return s;}
fn dims(a:i64,b:i64,c:i64,d:i64)->Shape{var s:Shape=default<Shape>();s.ne[0]=a;s.ne[1]=b;s.ne[2]=c;s.ne[3]=d;return s;}
fn count(s:Shape)->i64{var n:i64=1;for i in 4{n=checked_mul(n,s.ne[i]);}return n;}
fn elements(t:i64)->i64{return core(9,t,0,0,0,0,0);}
fn contiguous(t:i64,m:i64)->bool{return shape_query(10,t,t,m,4)!=0;}
fn rows(t:i64)->bool{return ne(t,0)==get(0,31,ty(t))||nb(t,0)==get(0,30,ty(t));}
fn padded(t:i64)->bool{return nb(t,0)==get(0,30,ty(t))&&nb(t,2)==checked_mul(nb(t,1),ne(t,1))&&nb(t,3)==checked_mul(nb(t,2),ne(t,2));}
fn same(a:i64,b:i64)->bool{return shape_query(7,a,b,0,0)!=0;}
fn repeat(a:i64,b:i64)->bool{return shape_query(9,a,b,0,0)!=0;}
fn can_mm(a:i64,b:i64)->bool{return ne(a,0)==ne(b,0)&&ne(a,2)!=0&&ne(a,3)!=0&&ne(b,2)%ne(a,2)==0&&ne(b,3)%ne(a,3)==0;}
fn make(ctx:i64,kind:i64,s:Shape,view:i64,off:i64)->i64{return alloc(ctx,kind,4,s,view,off);}
fn duplicate(ctx:i64,a:i64)->i64{return make(ctx,ty(a),shape(a),0,0);}
fn view(ctx:i64,a:i64)->i64{let t=make(ctx,ty(a),shape(a),a,0);for i in 4{put(t,27,i,nb(a,i));}name(t,0,a,0);return t;}
fn result(ctx:i64,a:i64,inplace:i64)->i64{if inplace!=0{return view(ctx,a);}return duplicate(ctx,a);}
fn wire(t:i64,op:i64,a:i64,b:i64,c:i64)->i64{put(t,50,0,op);put(t,51,0,a);put(t,51,1,b);put(t,51,2,c);return t;}
fn param(t:i64,i:i64,v:i64){put(t,52,i,v);}
fn named(t:i64,kind:i64,a:i64)->i64{name(t,kind,a,0);return t;}
fn unary_result(ctx:i64,a:i64,inplace:i64,op:i64)->i64{return wire(result(ctx,a,inplace),op,a,0,0);}
fn shape_result(ctx:i64,a:i64,s:Shape,kind:i64,op:i64)->i64{return wire(make(ctx,kind,s,0,0),op,a,0,0);}
fn view_shaped(ctx:i64,a:i64,s:Shape,n:i64,off:i64)->i64{let t=alloc(ctx,ty(a),n,s,a,off);name(t,0,a,0);param(t,0,off%4294967296);param(t,1,off/4294967296);return wire(t,OP_VIEW,a,0,0);}
fn positive(bits:i64)->bool{return bits>0&&bits<=2139095040;}
fn entry(mode:i64,p:i64)->i64{
 witness(mode);let ctx=arg(p,0);let a=arg(p,1);let b=arg(p,2);let c=arg(p,3);
 if mode==0{return duplicate(ctx,a);}
 if mode==1{return view(ctx,a);}
 if mode>=2&&mode<=5 {ensure(repeat(b,a));var op:i64=OP_ADD;if mode==3{op=OP_SUB;}if mode==4{op=OP_MUL;}if mode==5{op=OP_DIV;}return wire(result(ctx,a,c),op,a,b,0);}
 if mode==6 {ensure(rows(a));let t=unary_result(ctx,a,c,OP_UNARY);param(t,0,b);return t;}
 if mode>=7&&mode<=9 {var op:i64=OP_NORM;if mode==8{op=OP_RMS_NORM;}if mode==9{op=OP_L2_NORM;}let t=unary_result(ctx,a,c,op);param(t,0,b);return t;}
 if mode==10 {ensure(contiguous(a,1));if b!=0{ensure(contiguous(b,1)&&same(a,b)&&ty(a)==ty(b));}var s=shape(a);if b==0{s.ne[0]=s.ne[0]/2;}let t=wire(make(ctx,ty(a),s,0,0),OP_GLU,a,b,0);param(t,0,c);param(t,1,arg(p,4));return t;}
 if mode==11 {ensure(can_mm(a,b)&&shape_query(5,a,a,0,0)==0);return wire(make(ctx,TYPE_F32,dims(ne(a,1),ne(b,1),ne(b,2),ne(b,3)),0,0),OP_MUL_MAT,a,b,0);}
 if mode==12 {ensure(shape_query(5,a,a,0,0)==0&&ty(c)==TYPE_I32);ensure(ne(a,3)==1&&ne(b,3)==1&&ne(c,2)==1&&ne(c,3)==1);ensure(ne(c,1)==ne(b,2)&&ne(a,0)==ne(b,0)&&ne(b,1)!=0&&ne(c,0)%ne(b,1)==0);return wire(make(ctx,TYPE_F32,dims(ne(a,1),ne(c,0),ne(b,2),1),0,0),OP_MUL_MAT_ID,a,b,c);}
 if mode==13{return shape_result(ctx,a,dims(1,1,1,1),ty(a),OP_SUM);}
 if mode==14{return shape_result(ctx,a,dims(1,ne(a,1),ne(a,2),ne(a,3)),ty(a),OP_SUM_ROWS);}
 if mode==15{return shape_result(ctx,a,dims(1,ne(a,1),ne(a,2),ne(a,3)),TYPE_F32,OP_MEAN);}
 if mode==16{ensure(repeat(a,b));return shape_result(ctx,a,shape(b),ty(a),OP_REPEAT);}
 if mode==17{let s=dims(b,c,arg(p,4),arg(p,5));var empty:bool=false;for i in 4{if ne(a,i)==0{empty=true;}}if !empty{for i in 4{ensure(s.ne[i]%ne(a,i)==0);}}return shape_result(ctx,a,s,ty(a),OP_REPEAT);}
 if mode==18{ensure(c>=0&&c<4&&ty(a)==ty(b));var s=shape(a);for i in 4{if i==c{s.ne[i]=checked_add(ne(a,i),ne(b,i));}else{ensure(ne(a,i)==ne(b,i));}}let t=wire(make(ctx,ty(a),s,0,0),OP_CONCAT,a,b,0);param(t,0,c);return t;}
 if mode==19{ensure(padded(a));let t=unary_result(ctx,a,arg(p,4),OP_SCALE);param(t,0,b);param(t,1,c);return t;}
 if mode==20{let t=unary_result(ctx,a,arg(p,4),OP_CLAMP);param(t,0,b);param(t,1,c);return t;}
 if mode==21{ensure(elements(a)==elements(b));let t=wire(view(ctx,b),OP_CPY,a,b,0);name(t,5,a,b);return t;}
 if mode==22{let t=make(ctx,b,shape(a),0,0);name(t,6,a,0);return wire(t,OP_CPY,a,t,0);}
 if mode==23{return named(unary_result(ctx,a,0,OP_CONT),1,a);}
 if mode==24{let s=dims(b,c,arg(p,4),arg(p,5));ensure(elements(a)==count(s));return named(shape_result(ctx,a,s,ty(a),OP_CONT),1,a);}
 if mode==25{ensure(contiguous(a,0)&&elements(a)==elements(b));return named(wire(make(ctx,ty(a),shape(b),a,0),OP_RESHAPE,a,0,0),2,a);}
 if mode>=26&&mode<=29{var s=dims(b,1,1,1);let n=mode-25;for i in n{s.ne[i]=arg(p,i+2);}ensure(contiguous(a,0)&&elements(a)==count(s));return named(wire(alloc(ctx,ty(a),n,s,a,0),OP_RESHAPE,a,0,0),2,a);}
 if mode>=30&&mode<=33{let n=mode-29;var s=dims(b,1,1,1);for i in n{s.ne[i]=arg(p,i+2);}let offset=arg(p,2*n+1);let t=view_shaped(ctx,a,s,n,offset);if n>1{for i in n-1{put(t,27,i+1,arg(p,n+2+i));}if n==2{put(t,27,2,checked_mul(nb(t,1),s.ne[1]));put(t,27,3,nb(t,2));}if n==3{put(t,27,3,checked_mul(nb(t,2),s.ne[2]));}}return t;}
 if mode==34{var axes=dims(b,c,arg(p,4),arg(p,5));for i in 4{ensure(axes.ne[i]>=0&&axes.ne[i]<4);for j in i{ensure(axes.ne[i]!=axes.ne[j]);}}let t=view(ctx,a);for i in 4{put(t,26,axes.ne[i],ne(a,i));put(t,27,axes.ne[i],nb(a,i));param(t,i,axes.ne[i]);}name(t,3,a,0);return wire(t,OP_PERMUTE,a,0,0);}
 if mode==35{let t=view(ctx,a);put(t,26,0,ne(a,1));put(t,26,1,ne(a,0));put(t,27,0,nb(a,1));put(t,27,1,nb(a,0));name(t,4,a,0);return wire(t,OP_TRANSPOSE,a,0,0);}
 if mode==36{ensure(ne(a,2)==ne(b,1)&&ne(a,3)==ne(b,2)&&ne(b,3)==1&&ty(b)==TYPE_I32);var kind:i64=TYPE_F32;if ty(a)==TYPE_I32{kind=TYPE_I32;}return wire(make(ctx,kind,dims(ne(a,0),ne(b,0),ne(b,1),ne(b,2)),0,0),OP_GET_ROWS,a,b,0);}
 if mode==37{ensure(ne(a,0)==ne(b,0)&&ne(a,2)==ne(b,2)&&ne(a,3)==ne(b,3)&&ne(b,1)==ne(c,0));ensure(ne(c,1)!=0&&ne(c,2)!=0&&ne(b,2)%ne(c,1)==0&&ne(b,3)%ne(c,2)==0&&ne(c,3)==1);ensure((ty(b)==TYPE_F32||ty(b)==TYPE_F16)&&(ty(c)==TYPE_I64||ty(c)==TYPE_I32)&&rows(a)&&rows(b));return wire(view(ctx,a),OP_SET_ROWS,b,c,a);}
 if mode==38{ensure(contiguous(a,0));if b!=0{ensure((ty(b)==TYPE_F16||ty(b)==TYPE_F32)&&contiguous(b,0));ensure(ne(b,0)==ne(a,0)&&ne(b,1)>=ne(a,1)&&ne(b,2)!=0&&ne(b,3)!=0&&ne(a,2)%ne(b,2)==0&&ne(a,3)%ne(b,3)==0);}if positive(arg(p,4)){ensure(b!=0);}let t=wire(result(ctx,a,arg(p,5)),OP_SOFT_MAX,a,b,0);param(t,0,c);param(t,1,arg(p,4));return t;}
 if mode==39{let n=arg(p,4);let sections=arg(p,5);let rope_mode=arg(p,6);ensure((rope_mode & 1)==0&&shape_query(1,b,b,0,0)!=0&&ty(b)==TYPE_I32);let mrope=(rope_mode & ROPE_MROPE)!=0;if mrope{ensure(checked_mul(ne(a,2),4)==ne(b,0));}else{ensure(ne(a,2)==ne(b,0));}if c!=0{ensure(ty(c)==TYPE_F32&&ne(c,0)>=n/2);}let t=wire(result(ctx,a,arg(p,14)),OP_ROPE,a,b,c);param(t,1,n);param(t,2,rope_mode);param(t,4,arg(p,7));for i in 6{param(t,i+5,arg(p,i+8));}if mrope&&sections!=0{for i in 4{param(t,i+11,read_i32(sections,i));}}return t;}
 if mode==40{ensure(ne(a,0)<=2147483647);let t=shape_result(ctx,a,shape(a),TYPE_I32,OP_ARGSORT);param(t,0,b);return t;}
 if mode==41{ensure(ne(a,0)>=b);return shape_result(ctx,a,dims(b,ne(a,1),ne(a,2),ne(a,3)),TYPE_I32,OP_TOP_K);}
 if mode==42{ensure(ne(a,3)==1&&shape_query(2,b,b,0,0)!=0);let nt=ne(a,0)-ne(b,0)+1;ensure(ne(a,0)==ne(b,0)-1+nt&&ne(a,1)==ne(b,1)&&nt>=0);return wire(make(ctx,TYPE_F32,dims(ne(b,1),nt,ne(a,2),1),0,0),OP_SSM_CONV,a,b,0);}
 if mode==43{let g=arg(p,4);let beta=arg(p,5);let state=arg(p,6);let snapshots=arg(p,7);ensure(rows(a)&&rows(b)&&rows(c)&&contiguous(g,0)&&contiguous(beta,0)&&contiguous(state,0));ensure(ty(a)==TYPE_F32&&ty(b)==TYPE_F32&&ty(c)==TYPE_F32&&ty(g)==TYPE_F32&&ty(beta)==TYPE_F32&&ty(state)==TYPE_F32);let sv=ne(c,0);let h=ne(c,1);let nt=ne(c,2);let ns=ne(c,3);ensure((ne(g,0)==1||ne(g,0)==sv)&&ne(beta,0)==1);ensure(ne(state,0)==sv&&ne(state,1)==sv&&ne(state,2)==h&&ne(state,3)==ns&&snapshots>=1);let state_rows=checked_mul(checked_mul(snapshots,sv),ns);let t=wire(make(ctx,TYPE_F32,dims(checked_mul(sv,h),checked_add(checked_mul(nt,ns),state_rows),1,1),0,0),OP_GATED_DELTA_NET,a,b,c);put(t,51,3,g);put(t,51,4,beta);put(t,51,5,state);param(t,0,snapshots);return t;}
 if mode==44{let mask=arg(p,4);ensure(can_mm(b,a)&&ne(a,3)==ne(b,3)&&ne(a,3)==ne(c,3));if mask!=0{ensure(ty(mask)==TYPE_F16&&contiguous(mask,0)&&ne(mask,2)!=0&&ne(mask,3)!=0&&ne(a,2)%ne(mask,2)==0&&ne(a,3)%ne(mask,3)==0);}if positive(arg(p,6)){ensure(mask!=0);}let t=wire(make(ctx,TYPE_F32,dims(ne(c,0),ne(a,2),ne(a,1),ne(a,3)),0,0),OP_FLASH_ATTN_EXT,a,b,c);put(t,51,3,mask);for i in 3{param(t,i,arg(p,i+5));}return t;}
 if mode==45{ensure(ty(a)==TYPE_F32||ty(a)==TYPE_F16);ensure(contiguous(a,0));let t=unary_result(ctx,a,c,OP_FILL);param(t,0,b);return t;}
 if mode==46{ensure(ty(a)==TYPE_F32);return unary_result(ctx,a,0,OP_CUMSUM);}
 if mode==47{ensure(ne(a,1)==1);return shape_result(ctx,a,dims(ne(a,0),ne(a,0),ne(a,2),ne(a,3)),ty(a),OP_DIAG);}
 if mode==48{ensure(ty(a)==TYPE_F32&&contiguous(a,0)&&ne(a,0)==ne(a,1));let t=unary_result(ctx,a,0,OP_TRI);param(t,0,b);return t;}
 if mode==49{ensure(ty(a)==TYPE_F32&&ty(b)==TYPE_F32&&ne(a,0)==ne(a,1)&&ne(a,1)==ne(b,1)&&ne(a,2)==ne(b,2)&&ne(a,3)==ne(b,3));ensure(contiguous(a,0)&&contiguous(b,0)&&c!=0&&arg(p,4)!=0&&arg(p,5)==0);return wire(make(ctx,TYPE_F32,shape(b),0,0),OP_SOLVE_TRI,a,b,0);}
 if mode==50{var s=shape(a);for i in 4{s.ne[i]=signed_add(signed_add(s.ne[i],arg(p,2+i*2)),arg(p,3+i*2));}let t=shape_result(ctx,a,s,ty(a),OP_PAD);for i in 8{param(t,i,arg(p,i+2));}return t;}
 if mode==51{ensure(elements(a)>=elements(b));let t=wire(result(ctx,a,arg(p,7)),OP_SET,a,b,0);ensure(arg(p,6)<1073741824);for i in 5{param(t,i,arg(p,i+3));}return t;}
 if mode==52{ensure(ne(a,0)==ne(b,0)&&ne(a,1)==ne(c,0)&&ne(a,2)==ne(c,1)&&ty(c)==TYPE_I32);return wire(duplicate(ctx,a),OP_ADD_ID,a,b,c);}
 if mode==53{return unary_result(ctx,a,b,OP_DUP);}
 if mode==54{return unary_result(ctx,a,b,OP_SQR);}
 if mode==55{return unary_result(ctx,a,b,OP_SQRT);}
 if mode==56{ensure(ne(a,0)>=b&&ne(a,0)<=2147483647);let sorted=shape_result(ctx,a,shape(a),TYPE_I32,OP_ARGSORT);param(sorted,0,SORT_DESC);let s=dims(b,ne(sorted,1),ne(sorted,2),ne(sorted,3));let t=view_shaped(ctx,sorted,s,4,0);for i in 3{put(t,27,i+1,nb(sorted,i+1));}return t;}
 if mode==57{let op=extra(ctx,0,0);if op==OP_MUL_MAT||op==OP_MUL_MAT_ID{param(ctx,0,a);return 1;}if op==OP_FLASH_ATTN_EXT{param(ctx,3,a);return 1;}return 0;}
 if mode==58{ensure(b>=0&&b<MAX_SRC);let op=extra(ctx,0,0);if (op==OP_MUL_MAT||op==OP_MUL_MAT_ID)&&b==1{param(ctx,2+b,a);return 1;}return 0;}
 if mode==59||mode==60{ensure(extra(ctx,0,0)==OP_MUL_MAT);var index:i64=0;if mode==60{index=1;}param(ctx,index,a);return 0;}
 if mode==61||mode==62{ensure(extra(ctx,0,0)==OP_FLASH_ATTN_EXT);var index:i64=3;if mode==62{ensure(a>=0);index=4;}param(ctx,index,a);return 0;}
 if mode==63||mode==64{var index:i64=2;var op:i64=OP_SOFT_MAX;if mode==64{index=4;op=OP_FLASH_ATTN_EXT;}if a==0{put(ctx,51,index,0);return 0;}ensure(extra(ctx,0,0)==op&&extra(ctx,1,index)==0);let source=extra(ctx,1,0);ensure(ne(source,2)==ne(a,0)&&ty(a)==TYPE_F32);put(ctx,51,index,a);return 0;}
 if mode==65{ensure(padded(a)&&shape_query(0,b,b,0,0)!=0);return wire(result(ctx,a,c),OP_ADD1,a,b,0);}
 if mode==66{ensure(ty(a)==TYPE_F32&&ty(b)==TYPE_F32&&contiguous(a,0)&&elements(b)<=elements(a));let t=wire(result(ctx,a,arg(p,7)),OP_ACC,a,b,0);ensure(arg(p,6)<1073741824);for i in 5{param(t,i,arg(p,i+3));}return t;}
 ensure(false);return 0;
}
fn main(){print(entry(0,0));}
