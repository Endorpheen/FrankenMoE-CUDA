struct ShapeVec { ne:i64[4], nb:i64[4], type_size:i64, block_size:i64 }
fn shape_is_empty(a:const ShapeVec&)->bool { for i in 4 { if a.ne[i]==0 {return true;} } return false; }
fn shape_is_scalar(a:const ShapeVec&)->bool { return a.ne[0]==1&&a.ne[1]==1&&a.ne[2]==1&&a.ne[3]==1; }
fn shape_is_vector(a:const ShapeVec&)->bool { return a.ne[1]==1&&a.ne[2]==1&&a.ne[3]==1; }
fn shape_is_matrix(a:const ShapeVec&)->bool { return a.ne[2]==1&&a.ne[3]==1; }
fn shape_is_3d(a:const ShapeVec&)->bool { return a.ne[3]==1; }
fn shape_n_dims(a:const ShapeVec&)->i64 { if a.ne[3]>1 {return 4;} if a.ne[2]>1 {return 3;} if a.ne[1]>1 {return 2;} return 1; }
fn shape_is_transposed(a:const ShapeVec&)->bool { return a.nb[0]>a.nb[1]; }
fn shape_is_permuted(a:const ShapeVec&)->bool { return a.nb[0]>a.nb[1]||a.nb[1]>a.nb[2]||a.nb[2]>a.nb[3]; }
fn shape_same_shape(a:const ShapeVec&, b:const ShapeVec&)->bool { return a.ne[0]==b.ne[0]&&a.ne[1]==b.ne[1]&&a.ne[2]==b.ne[2]&&a.ne[3]==b.ne[3]; }
fn shape_same_stride(a:const ShapeVec&, b:const ShapeVec&)->bool { return a.nb[0]==b.nb[0]&&a.nb[1]==b.nb[1]&&a.nb[2]==b.nb[2]&&a.nb[3]==b.nb[3]; }
fn shape_can_repeat(a:const ShapeVec&, b:const ShapeVec&)->bool { if shape_is_empty(a) {return shape_is_empty(b);} return b.ne[0]%a.ne[0]==0&&b.ne[1]%a.ne[1]==0&&b.ne[2]%a.ne[2]==0&&b.ne[3]%a.ne[3]==0; }
fn shape_is_contiguous_m_n(a:const ShapeVec&, m:i64, n:i64)->bool {
  if a.block_size<=0||a.type_size<0 {return false;}
  if m<0||m>4||n<0||n>4 {return false;}
  var next_nb:i64=a.type_size;
  if a.ne[0]!=a.block_size&&a.nb[0]!=next_nb {return false;}
  if a.ne[0]<0 {return false;}let first_rows=a.ne[0]/a.block_size;if next_nb!=0&&first_rows>9223372036854775807/next_nb {return false;}next_nb=next_nb*first_rows;
  var i:i64=1;
  while i<n {
    if i>m {
      if a.ne[i]!=1&&a.nb[i]!=next_nb {return false;}
      if a.ne[i]<0||(next_nb!=0&&a.ne[i]>9223372036854775807/next_nb) {return false;}next_nb=next_nb*a.ne[i];
    } else {
      if a.ne[i]<0||a.nb[i]<0||(a.ne[i]!=0&&a.nb[i]>9223372036854775807/a.ne[i]) {return false;}next_nb=a.ne[i]*a.nb[i];
    }
    i+=1;
  }
  return true;
}
fn entry(mode:i64, a:const ShapeVec&, b:const ShapeVec&, m:i64, n:i64)->i64 {
  if mode==0 { if shape_is_scalar(a) {return 1;} return 0; }
  if mode==1 { if shape_is_vector(a) {return 1;} return 0; }
  if mode==2 { if shape_is_matrix(a) {return 1;} return 0; }
  if mode==3 { if shape_is_3d(a) {return 1;} return 0; }
  if mode==4 { return shape_n_dims(a); }
  if mode==5 { if shape_is_transposed(a) {return 1;} return 0; }
  if mode==6 { if shape_is_permuted(a) {return 1;} return 0; }
  if mode==7 { if shape_same_shape(a,b) {return 1;} return 0; }
  if mode==8 { if shape_same_stride(a,b) {return 1;} return 0; }
  if mode==9 { if shape_can_repeat(a,b) {return 1;} return 0; }
  if mode==10 { if shape_is_contiguous_m_n(a,m,n) {return 1;} return 0; }
  return -1;
}
fn main() { let a:ShapeVec=default<ShapeVec>(); let b:ShapeVec=default<ShapeVec>(); let ok=entry(0,a,b,0,4); print(ok); }
