// Graph construction owns dimensions,validation,parameter packing and edges.
extern "fg158_arg" fn arg(p:i64,i:i64)->i64;
extern "fg158_ne" fn ne(t:i64,i:i64)->i64;
extern "fg158_prop" fn prop(t:i64,k:i64)->i64;
extern "fg158_src" fn src(t:i64,i:i64)->i64;
extern "fg158_param" fn param(t:i64,i:i64)->i64;
extern "fg158_set_op" fn set_op(t:i64,op:i64);
extern "fg158_set_src" fn set_src(t:i64,i:i64,v:i64);
extern "fg158_set_param" fn set_param(t:i64,i:i64,v:i64);
extern "fg158_new" fn fresh(c:i64,ty:i64,n0:i64,n1:i64,n2:i64,n3:i64)->i64;
extern "fg158_dup" fn dup(c:i64,a:i64)->i64;
extern "fg158_view" fn view(c:i64,a:i64)->i64;
extern "fg158_pred" fn pred(which:i64,a:i64,b:i64)->i64;
extern "fg158_i32" fn load_i32(p:i64,i:i64)->i64;
extern "fg158_name" fn name(t:i64,a:i64,b:i64,style:i64);
extern "fg158_float" fn float_bits(x:i64)->f32;
extern "fg158_fail" fn fail(code:i64);
fn op_acc()->i64{return 5;}
fn op_add()->i64{return 2;}
fn op_argmax()->i64{return 18;}
fn op_argsort()->i64{return 69;}
fn op_concat()->i64{return 22;}
fn op_cont()->i64{return 35;}
fn op_count_equal()->i64{return 19;}
fn op_cpy()->i64{return 34;}
fn op_cumsum()->i64{return 16;}
fn op_diag()->i64{return 43;}
fn op_flash_attn_ext()->i64{return 74;}
fn op_gated_delta_net()->i64{return 86;}
fn op_get_rows()->i64{return 40;}
fn op_glu()->i64{return 100;}
fn op_mean()->i64{return 17;}
fn op_mul_mat()->i64{return 29;}
fn op_mul_mat_id()->i64{return 30;}
fn op_repeat()->i64{return 20;}
fn op_repeat_back()->i64{return 21;}
fn op_rope()->i64{return 48;}
fn op_set_rows()->i64{return 42;}
fn op_soft_max()->i64{return 46;}
fn op_ssm_conv()->i64{return 76;}
fn op_sum_rows()->i64{return 15;}
fn op_top_k()->i64{return 70;}
fn op_tri()->i64{return 72;}
fn op_unary()->i64{return 91;}
fn type_bf16()->i64{return 30;}
fn type_f16()->i64{return 1;}
fn type_f32()->i64{return 0;}
fn type_i32()->i64{return 26;}
fn type_i64()->i64{return 27;}
fn unary_count()->i64{return 22;}
fn max_src()->i64{return 10;}
fn rope_mrope()->i64{return 8;}
fn ensure(v:bool,code:i64){if !v{fail(code);}}
fn plus(a:i64,b:i64)->i64{ensure(a>=0&&b>=0&&a<=9223372036854775807-b,1);return a+b;}
fn times(a:i64,b:i64)->i64{ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a),2);return a*b;}
fn divides(a:i64,b:i64)->bool{return b>0&&a%b==0;}
fn shaped(c:i64,ty:i64,a:i64)->i64{return fresh(c,ty,ne(a,0),ne(a,1),ne(a,2),ne(a,3));}
fn result(c:i64,a:i64,inplace:i64)->i64{if inplace!=0{return view(c,a);}return dup(c,a);}
fn finish(r:i64,op:i64,a:i64,b:i64,c:i64)->i64{set_op(r,op);set_src(r,0,a);set_src(r,1,b);set_src(r,2,c);return r;}
fn contiguous(t:i64){ensure(pred(0,t,0)!=0,3);}
fn rows(t:i64){ensure(pred(1,t,0)!=0,4);}
fn same(a:i64,b:i64){ensure(pred(3,a,b)!=0,5);}
fn repeat(a:i64,b:i64){ensure(pred(4,a,b)!=0,6);}
fn common(p:i64)->i64{
 let mode=arg(p,0);let c=arg(p,1);let a=arg(p,2);let b=arg(p,3);let d=arg(p,4);let op=arg(p,5);let inplace=arg(p,6);
 if mode==0||mode==1||mode==2||mode==3||mode==4||mode==5||mode==6||mode==7||mode==8||mode==9{
  if mode==1{repeat(b,a);}
  if mode==2{ensure(pred(5,b,0)!=0&&pred(2,a,0)!=0,7);}
  if mode==3{rows(a);ensure(op>=0&&op<unary_count(),8);}
  if mode==4{ensure(pred(2,a,0)!=0,9);}
  if mode==5{contiguous(a);ensure(prop(a,0)==type_f32()||prop(a,0)==type_f16(),10);}
  if mode==6{ensure(ne(a,0)==ne(b,0)&&ne(a,1)==ne(d,0)&&ne(a,2)==ne(d,1)&&prop(d,0)==type_i32(),11);}
  if mode==7{ensure(pred(6,b,a)!=0,12);ensure(prop(a,1)!=0||prop(a,0)==type_f16()||prop(a,0)==type_bf16(),13);}
  var r:i64=0;if mode==7{r=shaped(c,arg(p,7),a);}else{r=result(c,a,inplace);}
  var actual=op;if mode==3{actual=op_unary();}if mode==7{actual=op_add();}
  finish(r,actual,a,b,d);
  if mode==3{set_param(r,0,op);}
  if mode==4||mode==8||mode==5{set_param(r,0,arg(p,7));}
  if mode==4||mode==9{set_param(r,1,arg(p,8));}
  if mode==9{set_param(r,0,arg(p,7));}
  return r;
 }
 if mode==10{
  ensure(pred(7,a,0)!=0,14);
  if b!=0{ensure(pred(7,b,0)!=0,15);same(a,b);ensure(prop(a,0)==prop(b,0),16);}
  let r=fresh(c,prop(a,0),if b!=0{ne(a,0)}else{ne(a,0)/2},ne(a,1),ne(a,2),ne(a,3));set_param(r,0,op);set_param(r,1,inplace);return finish(r,op_glu(),a,b,0);
 }
 if mode==11||mode==12||mode==13{
  if mode==11{ensure(pred(8,a,b)!=0,17);}if mode==13{ensure(pred(9,a,b)!=0,18);}ensure(pred(10,a,0)==0,19);
  if mode==12{ensure(prop(d,0)==type_i32()&&ne(a,3)==1&&ne(b,3)==1&&ne(d,2)==1&&ne(d,3)==1&&ne(d,1)==ne(b,2)&&ne(a,0)==ne(b,0)&&divides(ne(d,0),ne(b,1)),20);}
  let r=fresh(c,type_f32(),if mode==13{ne(a,0)}else{ne(a,1)},if mode==12{ne(d,0)}else{if mode==13{ne(b,0)}else{ne(b,1)}},ne(b,2),if mode==12{1}else{ne(b,3)});return finish(r,op,a,b,d);
 }
 if mode==14{
  var n0:i64=1;var n1:i64=1;var n2:i64=1;var n3:i64=1;var ty=prop(a,0);
  if op==op_sum_rows()||op==op_mean(){n1=ne(a,1);n2=ne(a,2);n3=ne(a,3);}
  if op==op_mean(){ty=type_f32();}
  if op==op_argmax(){ensure(pred(11,a,0)!=0&&ne(a,0)<=2147483647,21);n0=ne(a,1);ty=type_i32();}
  if op==op_count_equal(){same(a,b);ty=type_i64();}
  if op==op_cumsum(){ensure(prop(a,0)==type_f32(),22);return finish(dup(c,a),op,a,0,0);}
  return finish(fresh(c,ty,n0,n1,n2,n3),op,a,b,0);
 }
 if mode==15{if op==op_repeat_back(){repeat(b,a);}else{repeat(a,b);}return finish(shaped(c,prop(a,0),b),op,a,0,0);}
 if mode==16{
  let n0=arg(p,7);let n1=arg(p,8);let n2=arg(p,9);let n3=arg(p,10);ensure(n0>=0&&n1>=0&&n2>=0&&n3>=0,23);
  ensure(pred(12,a,0)!=0||(divides(n0,ne(a,0))&&divides(n1,ne(a,1))&&divides(n2,ne(a,2))&&divides(n3,ne(a,3))),24);return finish(fresh(c,prop(a,0),n0,n1,n2,n3),op_repeat(),a,0,0);
 }
 if mode==17{
  ensure(op>=0&&op<4&&prop(a,0)==prop(b,0),25);var i:i64=0;while i<4{if i!=op{ensure(ne(a,i)==ne(b,i),26);}i+=1;}
  let r=fresh(c,prop(a,0),if op==0{plus(ne(a,0),ne(b,0))}else{ne(a,0)},if op==1{plus(ne(a,1),ne(b,1))}else{ne(a,1)},if op==2{plus(ne(a,2),ne(b,2))}else{ne(a,2)},if op==3{plus(ne(a,3),ne(b,3))}else{ne(a,3)});set_param(r,0,op);return finish(r,op_concat(),a,b,0);
 }
 if mode==18{
  ensure(ne(a,2)==ne(b,1)&&ne(a,3)==ne(b,2)&&ne(b,3)==1&&prop(b,0)==type_i32(),27);
  return finish(fresh(c,if prop(a,0)==type_i32(){type_i32()}else{type_f32()},ne(a,0),ne(b,0),ne(b,1),ne(b,2)),op_get_rows(),a,b,0);
 }
 if mode==19{
  ensure(ne(a,0)==ne(b,0)&&ne(a,2)==ne(b,2)&&ne(a,3)==ne(b,3)&&ne(b,1)==ne(d,0)&&divides(ne(b,2),ne(d,1))&&divides(ne(b,3),ne(d,2))&&ne(d,3)==1,28);
  ensure((prop(b,0)==type_f32()||prop(b,0)==type_f16())&&(prop(d,0)==type_i32()||prop(d,0)==type_i64()),29);rows(a);rows(b);return finish(view(c,a),op_set_rows(),b,d,a);
 }
 if mode==20{
  contiguous(a);if b!=0{ensure(prop(b,0)==type_f16()||prop(b,0)==type_f32(),30);contiguous(b);ensure(ne(b,0)==ne(a,0)&&ne(b,1)>=ne(a,1)&&divides(ne(a,2),ne(b,2))&&divides(ne(a,3),ne(b,3)),31);}
  ensure(!(float_bits(arg(p,8))>0.0)||b!=0,32);let r=result(c,a,inplace);set_param(r,0,arg(p,7));set_param(r,1,arg(p,8));return finish(r,op_soft_max(),a,b,0);
 }
 if mode==21{ensure(pred(13,a,0)==pred(13,b,0),33);let r=view(c,b);name(r,a,b,if prop(b,3)!=0{0}else{1});return finish(r,op_cpy(),a,b,0);}
 if mode==22{let r=shaped(c,op,a);name(r,a,0,1);return finish(r,op_cpy(),a,r,0);}
 if mode==23||mode==24{
  var r:i64=0;if mode==23{r=dup(c,a);}else{let n0=arg(p,7);let n1=arg(p,8);let n2=arg(p,9);let n3=arg(p,10);ensure(pred(13,a,0)==times(times(times(n0,n1),n2),n3),34);r=fresh(c,prop(a,0),n0,n1,n2,n3);}name(r,a,0,2);return finish(r,op_cont(),a,0,0);
 }
 if mode==25{
  ensure((op&1)==0&&pred(14,b,0)!=0&&prop(b,0)==type_i32(),35);let mult=if (op&rope_mrope())!=0{4}else{1};ensure(times(ne(a,2),mult)==ne(b,0),36);let nd=arg(p,7);if d!=0{ensure(prop(d,0)==type_f32()&&ne(d,0)>=nd/2,37);}
  let r=result(c,a,inplace);set_param(r,0,0);set_param(r,1,nd);set_param(r,2,op);set_param(r,3,0);set_param(r,4,arg(p,8));var j:i64=0;while j<6{set_param(r,5+j,arg(p,9+j));j+=1;}
  let sections=arg(p,15);j=0;while j<4{set_param(r,11+j,if mult==4&&sections!=0{load_i32(sections,j)}else{0});j+=1;}set_param(r,15,0);return finish(r,op_rope(),a,b,d);
 }
 if mode==26{
  ensure(pred(8,b,a)!=0&&ne(a,3)==ne(b,3)&&ne(a,3)==ne(d,3),38);let mask=arg(p,7);
  if mask!=0{ensure(prop(mask,0)==type_f16(),39);contiguous(mask);ensure(divides(ne(a,2),ne(mask,2))&&divides(ne(a,3),ne(mask,3)),40);}
  ensure(!(float_bits(arg(p,9))>0.0)||mask!=0,41);let r=fresh(c,type_f32(),ne(d,0),ne(a,2),ne(a,1),ne(a,3));set_param(r,0,arg(p,8));set_param(r,1,arg(p,9));set_param(r,2,arg(p,10));finish(r,op_flash_attn_ext(),a,b,d);set_src(r,3,mask);return r;
 }
 if mode==27{
  ensure(ne(a,3)==1&&pred(11,b,0)!=0&&ne(a,1)==ne(b,1),42);let tokens=ne(a,0)-ne(b,0)+1;ensure(tokens>=0,43);return finish(fresh(c,type_f32(),ne(b,1),tokens,ne(a,2),1),op_ssm_conv(),a,b,0);
 }
 if mode==28{
  let gate=arg(p,7);let beta=arg(p,8);let state=arg(p,9);let count=arg(p,10);rows(a);rows(b);rows(d);contiguous(gate);contiguous(beta);contiguous(state);
  ensure(prop(a,0)==type_f32()&&prop(b,0)==type_f32()&&prop(d,0)==type_f32()&&prop(gate,0)==type_f32()&&prop(beta,0)==type_f32()&&prop(state,0)==type_f32(),44);
  let sv=ne(d,0);let heads=ne(d,1);let tokens=ne(d,2);let seqs=ne(d,3);ensure((ne(gate,0)==1||ne(gate,0)==sv)&&ne(beta,0)==1&&ne(state,0)==sv&&ne(state,1)==sv&&ne(state,2)==heads&&ne(state,3)==seqs&&count>=1&&count<=2147483647,45);
  let sr=times(times(count,sv),seqs);let r=fresh(c,type_f32(),times(sv,heads),plus(times(tokens,seqs),sr),1,1);set_param(r,0,count);finish(r,op_gated_delta_net(),a,b,d);set_src(r,3,gate);set_src(r,4,beta);set_src(r,5,state);return r;
 }
 if mode==29{ensure(ne(a,0)<=2147483647,46);let r=shaped(c,type_i32(),a);set_param(r,0,op);return finish(r,op_argsort(),a,0,0);}
 if mode==30{ensure(op>=0&&ne(a,0)>=op,47);return finish(fresh(c,type_i32(),op,ne(a,1),ne(a,2),ne(a,3)),op_top_k(),a,0,0);}
 if mode==31{ensure(prop(a,0)==type_f32()&&ne(a,0)==ne(a,1),48);contiguous(a);let r=dup(c,a);set_param(r,0,op);return finish(r,op_tri(),a,0,0);}
 if mode==32{
  ensure(pred(13,a,0)>=pred(13,b,0),49);ensure(arg(p,10)>=0&&arg(p,10)<1073741824,50);
  if op==op_acc(){contiguous(a);ensure(prop(a,0)==type_f32()&&prop(b,0)==type_f32(),51);}
  let r=result(c,a,inplace);var j:i64=0;while j<4{set_param(r,j,arg(p,7+j));j+=1;}set_param(r,4,inplace);return finish(r,op,a,b,0);
 }
 if mode==33{ensure(ne(a,1)==1,52);return finish(fresh(c,prop(a,0),ne(a,0),ne(a,0),ne(a,2),ne(a,3)),op_diag(),a,0,0);}
 if mode==34{let r=result(c,a,inplace);set_param(r,0,op);return finish(r,arg(p,7),a,0,0);}
 if mode==35{
  // Precision/hint and sink mutation preserve exact parameter slots and NULL semantics.
  if op==0||op==1{ensure(prop(a,2)==op_mul_mat(),53);set_param(a,op,arg(p,7));return 0;}
  if op==2{let nodeop=prop(a,2);if nodeop==op_mul_mat()||nodeop==op_mul_mat_id(){set_param(a,0,arg(p,7));return 1;}if nodeop==op_flash_attn_ext(){set_param(a,3,arg(p,7));return 1;}return 0;}
  if op==3{let idx=arg(p,8);ensure(idx>=0&&idx<max_src(),54);let nodeop=prop(a,2);if (nodeop==op_mul_mat()||nodeop==op_mul_mat_id())&&idx==1{set_param(a,3,arg(p,7));return 1;}return 0;}
  if op==4||op==5||op==6{ensure(prop(a,2)==op_flash_attn_ext(),55);if op==5{return param(a,3);}if op==6{ensure(arg(p,7)>=0,56);}set_param(a,if op==4{3}else{4},arg(p,7));return 0;}
  if op==7||op==8{let slot=if op==7{2}else{4};if b==0{set_src(a,slot,0);return 0;}ensure(prop(a,2)==if op==7{op_soft_max()}else{op_flash_attn_ext()},57);ensure(src(a,slot)==0&&ne(src(a,0),2)==ne(b,0)&&prop(b,0)==type_f32(),58);set_src(a,slot,b);return 0;}
 }
 fail(99);return 0;
}
fn main(){print(common(0));}
