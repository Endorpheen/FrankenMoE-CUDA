// Apex CUDA out-prod pointer kernel (Goose).
//
// The only owned kernel in out-prod.cu: for every (i2,i3) pair it produces the
// three GEMM pointers that cublasSgemmBatched consumes. Pointer arithmetic is
// done in floats (the native form is `src0_d + expr` on a float pointer), so
// the byte address is base + 4*expr.
extern "fg288d_st_u64" fn st_u64(dst:u64,byte:u64,v:u64);
extern "fg288d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn kernel(src0:u64,src1:u64,dst:u64,pa:u64,pb:u64,pc:u64,
 ne2:i64,ne3:i64,dps2:i64,dps3:i64,
 s02:i64,s03:i64,s12:i64,s13:i64,s2:i64,s3:i64,
 bx:i64,by:i64,bdx:i64,bdy:i64,tx:i64,ty:i64){
 let i2:i64=bx*bdx+tx;
 let i3:i64=by*bdy+ty;
 if i2>=ne2{ return; }
 if i3>=ne3{ return; }
 ensure(dps2>0 as i64&&dps3>0 as i64,30);
 let idx:i64=(i3*ne2+i2)*(8 as i64);
 let ea:i64=(i3/dps3)*s03+(i2/dps2)*s02;
 let eb:i64=i3*s13+i2*s12;
 let ec:i64=i3*s3+i2*s2;
 st_u64(pa,(idx as u64),src0+(ea*(4 as i64)) as u64);
 st_u64(pb,(idx as u64),src1+(eb*(4 as i64)) as u64);
 st_u64(pc,(idx as u64),dst+(ec*(4 as i64)) as u64);
}

fn main(){
 kernel(0,0,0,0,0,0,0 as i64,0 as i64,1 as i64,1 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,16 as i64,16 as i64,0 as i64,0 as i64);
}
