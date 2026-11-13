// CUDA binary/broadcast policy and arithmetic;foreign effects are storage/ISA/PDL primitives.
extern "fg167_dget" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg167_dmagic" fn magic(p:i64,i:i64,k:i64)->u32;
extern "fg167_umulhi" fn mulhi(a:u32,b:u32)->u32;
extern "fg167_load" fn load(kind:i64,p:u64,off:u64)->f32;
extern "fg167_store" fn store(kind:i64,p:u64,off:u64,value:f32);
extern "fg167_lc" fn lc();
extern "fg167_sync" fn sync();
fn fd(p:i64,n:u32,key:i64)->u32{let hi=mulhi(n,magic(p,key,0));return (((hi as u64)+(n as u64))>>(magic(p,key,1) as u64)) as! u32;}
fn fm(p:i64,n:u32,key:i64)->u32{return n-fd(p,n,key)*magic(p,key,2);}
fn value(op:i64,a:f32,b:f32)->f32{if op==0{return b;}if op==1{return a+b;}if op==2{return a-b;}if op==3{return a*b;}return a/b;}
fn compute(p:i64,op:i64,fuse:i64,ka:i64,kb:i64,kd:i64,i0:u64,i1:u64,i2:u64,i3:u64){let b0=fm(p,i0 as u32,1) as u64;let b1=fm(p,i1 as u32,2) as u64;let b2=fm(p,i2 as u32,3) as u64;let b3=fm(p,i3 as u32,4) as u64;let da=if ka==0{4 as u64}else{2 as u64};let dd=if kd==0{4 as u64}else{2 as u64};let aoff=i0*get(p,5,0)+i1*get(p,5,1)+i2*get(p,5,2)+i3*get(p,5,3);let boff=b0*get(p,6,0)+b1*get(p,6,1)+b2*get(p,6,2)+b3*get(p,6,3);let doff=i0*dd+i1*get(p,4,1)+i2*get(p,4,2)+i3*get(p,4,3);let a=get(p,0,0);var sum:f32=0.0;if a!=0{sum=load(ka,a,aoff);}var j:i64=0;while j<fuse{sum=value(op,sum,load(kb,get(p,0,1+j),boff));j+=1;}store(kd,get(p,0,9),doff,sum);}
fn lane(p:i64,op:i64,fuse:i64,path:i64,ka:i64,kb:i64,kd:i64,x:u64,y:u64,z:u64,step:u64){let n0=get(p,1,0);let n1=get(p,1,1);let n2=get(p,1,2);let n3=get(p,1,3);
 if path==2{let i2=z%n2;let i3=z/n2;if x>=n0{return;}var sum:f32=0.0;sync();var a3=i3;while a3<get(p,2,3){var a2=i2;while a2<get(p,2,2){var a1=y;while a1<get(p,2,1){var a0=x;while a0<get(p,2,0){sum+=load(0,get(p,0,0),a0*get(p,5,0)+a1*get(p,5,1)+a2*get(p,5,2)+a3*get(p,5,3));a0+=n0;}a1+=n1;}a2+=n2;}a3+=n3;}let at=((i3*n2+i2)*n1+y)*n0+x;store(0,get(p,0,9),at*4,sum);return;}
 if path==0{lc();let i2=fd(p,z as u32,0) as u64;let i3=z-i2*n3;if x>=n0||y>=n1||i2>=n2||i3>=n3{return;}sync();var i0=x;while i0<n0{compute(p,op,fuse,ka,kb,kd,i0,y,i2,i3);if n0-i0<=step{break;}i0+=step;}return;}
 let i3=fd(p,x as u32,7) as u64;let after3=x-i3*(magic(p,7,2) as u64);let i2=fd(p,after3 as u32,6) as u64;let after2=after3-i2*(magic(p,6,2) as u64);let i1=fd(p,after2 as u32,5) as u64;let i0=after2-i1*n0;if i0>=n0||i1>=n1||i2>=n2||i3>=n3{return;}sync();compute(p,op,fuse,ka,kb,kd,i0,i1,i2,i3);
}
fn main(){lane(0,0,1,0,0,0,0,0 as u64,0 as u64,0 as u64,1 as u64);print(fd(0,1 as u32,0));}
