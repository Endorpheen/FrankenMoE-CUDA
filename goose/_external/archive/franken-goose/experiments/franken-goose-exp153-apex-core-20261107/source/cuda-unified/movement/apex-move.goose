// worker-10: Apex CUDA movement/indexing/type-conversion/cache-write block.
//
// Reachable Apex paths only (qwen35moe): cpy/cont/dup, get_rows, set_rows,
// F32/F16 scalar conversion and the Q4_0 KV write/dequant path.  All index
// decomposition, bounds predicates, loops and the Q4_0 block quantizer are
// Goose.  The extern primitives are raw byte load/store, thread/block index,
// fabs/trunc and IEEE half<->float conversion.  No kernel body is hidden in a
// primitive and no shared/global scratch is used.

const T_F32:i64=0;
const T_F16:i64=1;
const T_Q4_0:i64=2;
const T_I32:i64=26;
const T_I64:i64=27;

const P_OP:i64=0;
const P_SRC:i64=1;
const P_DST:i64=2;
const P_IDX:i64=3;
const P_NE00:i64=4;
const P_NE01:i64=5;
const P_NE02:i64=6;
const P_NE03:i64=7;
const P_NB00:i64=8;
const P_NB01:i64=9;
const P_NB02:i64=10;
const P_NB03:i64=11;
const P_NE10:i64=12;
const P_NE11:i64=13;
const P_NE12:i64=14;
const P_NE13:i64=15;
const P_NB10:i64=16;
const P_NB11:i64=17;
const P_NB12:i64=18;
const P_NB13:i64=19;
const P_NB1:i64=20;
const P_NB2:i64=21;
const P_NB3:i64=22;
const P_ST:i64=23;
const P_DT:i64=24;
const P_IT:i64=25;
const P_N:i64=26;
const P_GRID_Y:i64=27;
const P_GRID_Z:i64=28;
const P_AUX0:i64=29;
const P_AUX1:i64=30;
const P_AUX2:i64=31;

// Q4_0 block layout: ggml_half d followed by QK4_0/2 nibble bytes.
const QK4_0:i64=32;
const Q4_0_BYTES:i64=18;

extern "fg153_am_enter_copy_scalar" fn enter_copy_scalar();
extern "fg153_am_enter_copy_contig" fn enter_copy_contig();
extern "fg153_am_enter_copy_blocks" fn enter_copy_blocks();
extern "fg153_am_enter_transpose" fn enter_transpose();
extern "fg153_am_enter_getrows" fn enter_getrows();
extern "fg153_am_enter_setrows" fn enter_setrows();
extern "fg153_am_enter_q4_dequant" fn enter_q4_dequant();

extern "fg153_am_param" fn param(p:i64,i:i64)->i64;
extern "fg153_am_load_u8" fn ldu8(p:i64,o:i64)->i64;
extern "fg153_am_load_u16" fn ldu16(p:i64,o:i64)->i64;
extern "fg153_am_load_i32" fn ldi32(p:i64,o:i64)->i64;
extern "fg153_am_load_i64" fn ldi64(p:i64,o:i64)->i64;
extern "fg153_am_store_u8" fn stu8(p:i64,o:i64,v:i64);
extern "fg153_am_store_u16" fn stu16(p:i64,o:i64,v:i64);
extern "fg153_am_store_i32" fn sti32(p:i64,o:i64,v:i64);
extern "fg153_am_store_f32" fn stf32(p:i64,o:i64,v:f32);
extern "fg153_am_load_f32" fn ldf32(p:i64,o:i64)->f32;
extern "fg153_am_f16_bits" fn f16bits(v:f32)->i64;
extern "fg153_am_f16_value" fn f16val(b:i64)->f32;
extern "fg153_am_fabs" fn fabsv(v:f32)->f32;
extern "fg153_am_trunc_i64" fn truncv(v:f32)->i64;

extern "fg153_am_gid_x" fn gidx()->i64;
extern "fg153_am_bid_x" fn bidx()->i64;
extern "fg153_am_bid_y" fn bidy()->i64;
extern "fg153_am_bid_z" fn bidz()->i64;
extern "fg153_am_tid_x" fn tidx()->i64;
extern "fg153_am_tid_y" fn tidy()->i64;
extern "fg153_am_bdim_x" fn bdimx()->i64;
extern "fg153_am_gdim_y" fn gdimy()->i64;
extern "fg153_am_gdim_z" fn gdimz()->i64;

fn ensure(ok:bool){if !ok {assert(false);}}
fn emin(a:i64,b:i64)->i64{return if a<b {a}else{b};}
fn emax(a:i64,b:i64)->i64{return if a>b {a}else{b};}
fn eabs(a:i64)->i64{return if a<0 {0-a}else{a};}

fn type_size(k:i64)->i64 {
    if k==T_F32 {return 4;}
    if k==T_F16 {return 2;}
    if k==T_Q4_0 {return Q4_0_BYTES;}
    ensure(false);
    return 0;
}

fn load_val(k:i64,base:i64,off:i64)->f32 {
    if k==T_F32 {return ldf32(base,off);}
    if k==T_F16 {return f16val(ldu16(base,off));}
    ensure(false);
    return (0 as f32);
}
fn store_val(k:i64,base:i64,off:i64,v:f32) {
    if k==T_F32 {stf32(base,off,v);return;}
    if k==T_F16 {stu16(base,off,f16bits(v));return;}
    ensure(false);
}

// ---------------------------------------------------------------------------
// Raw Q4_0 block quantizer, exact cpy-utils.cuh numerical order.
// ---------------------------------------------------------------------------
fn quant_q4_block(src:i64,dst:i64) {
    var amax:f32=(0 as f32);
    var vmax:f32=(0 as f32);
    var j:i64=0;
    while j<QK4_0 {
        let v=ldf32(src,j*4);
        let av=fabsv(v);
        if amax<av {amax=av;vmax=v;}
        j=j+1;
    }
    let d=vmax/(-8 as f32);
    let id=if d!=(0 as f32) {(1 as f32)/d}else{(0 as f32)};
    stu16(dst,0,f16bits(d));
    j=0;
    while j<(QK4_0/2) {
        let x0=ldf32(src,j*4)*id;
        let x1=ldf32(src,(j+(QK4_0/2))*4)*id;
        let xi0=emin(15,truncv(x0+(8.5 as f32)));
        let xi1=emin(15,truncv(x1+(8.5 as f32)));
        stu8(dst,2+j,(xi0&255)|((xi1&255)<<4));
        j=j+1;
    }
}

fn dequant_q4_to_float_into(src:i64,src_block:i64,iqs:i64,dst:i64,base_off:i64,qk:i64) {
    let d=f16val(ldu16(src,src_block*Q4_0_BYTES));
    let q=ldu8(src,src_block*Q4_0_BYTES+2+iqs);
    let v0=((q&15)-8 as i64);
    let v1=((q>>4)-8 as i64);
    stf32(dst,base_off+iqs*4,(v0 as f32)*d);
    stf32(dst,base_off+(iqs+qk/2)*4,(v1 as f32)*d);
}

// ---------------------------------------------------------------------------
// cpy: one scalar 4D element, one contiguous element, one Q4 block.
// ---------------------------------------------------------------------------
fn copy_one(p:i64,i:i64) {
    let st=param(p,P_ST);
    let dt=param(p,P_DT);
    let ne00=param(p,P_NE00);
    let ne01=param(p,P_NE01);
    let ne02=param(p,P_NE02);
    let ne10=param(p,P_NE10);
    let ne11=param(p,P_NE11);
    let ne12=param(p,P_NE12);
    let mat=ne00*ne01*ne02;
    let i03=i/mat;
    var r=i-i03*mat;
    let i02=r/(ne00*ne01);
    r=r-i02*ne00*ne01;
    let i01=r/ne00;
    let i00=r-i01*ne00;
    let so=i00*param(p,P_NB00)+i01*param(p,P_NB01)+i02*param(p,P_NB02)+i03*param(p,P_NB03);
    let matd=ne10*ne11*ne12;
    let j03=i/matd;
    var rd=i-j03*matd;
    let j02=rd/(ne10*ne11);
    rd=rd-j02*ne10*ne11;
    let j01=rd/ne10;
    let j00=rd-j01*ne10;
    let do=j00*param(p,P_NB10)+j01*param(p,P_NB11)+j02*param(p,P_NB12)+j03*param(p,P_NB13);
    if st==T_F16&&dt==T_F16 {stu16(param(p,P_DST),do,ldu16(param(p,P_SRC),so));return;}
    store_val(dt,param(p,P_DST),do,load_val(st,param(p,P_SRC),so));
}

fn copy_contig_one(p:i64,i:i64) {
    let st=param(p,P_ST);
    let dt=param(p,P_DT);
    if st==T_F16&&dt==T_F16 {
        stu16(param(p,P_DST),i*2,ldu16(param(p,P_SRC),i*2));return;
    }
    store_val(dt,param(p,P_DST),i*type_size(dt),load_val(st,param(p,P_SRC),i*type_size(st)));
}

fn copy_blocks_one(p:i64,i:i64) {
    let st=param(p,P_ST);
    let dt=param(p,P_DT);
    let ne00=param(p,P_NE00);
    let ne01=param(p,P_NE01);
    let ne02=param(p,P_NE02);
    let mat=ne00*ne01*ne02;
    let i03=i/mat;
    var r=i-i03*mat;
    let i02=r/(ne00*ne01);
    r=r-i02*ne00*ne01;
    let i01=r/ne00;
    let i00=r-i01*ne00;
    let so=(if st==T_Q4_0 {i00/QK4_0}else{i00})*param(p,P_NB00)+i01*param(p,P_NB01)+i02*param(p,P_NB02)+i03*param(p,P_NB03);
    let ne10=param(p,P_NE10);
    let ne11=param(p,P_NE11);
    let ne12=param(p,P_NE12);
    let matd=ne10*ne11*ne12;
    let j03=i/matd;
    var rd=i-j03*matd;
    let j02=rd/(ne10*ne11);
    rd=rd-j02*ne10*ne11;
    let j01=rd/ne10;
    let j00=rd-j01*ne10;
    let do=(if dt==T_Q4_0 {j00/QK4_0}else{j00})*param(p,P_NB10)+j01*param(p,P_NB11)+j02*param(p,P_NB12)+j03*param(p,P_NB13);
    if st==T_F32&&dt==T_Q4_0 {quant_q4_block(param(p,P_SRC)+so,param(p,P_DST)+do);return;}
    if st==T_Q4_0&&dt==T_F32 {
        var j:i64=0;
        while j<(QK4_0/2) {
            dequant_q4_to_float_into(param(p,P_SRC)+so,0,j,param(p,P_DST),do,QK4_0);
            j=j+1;
        }
        return;
    }
    ensure(false);
}

fn transpose_one_element(p:i64,imat:i64,row:i64,col:i64) {
    let st=param(p,P_ST);
    let dt=param(p,P_DT);
    let n=param(p,P_NE00)*param(p,P_NE01);
    let so=imat*n+col*param(p,P_NE01)+row;
    let do=imat*n+row*param(p,P_NE00)+col;
    if st==T_F16&&dt==T_F16 {
        stu16(param(p,P_DST),do*2,ldu16(param(p,P_SRC),so*2));return;
    }
    store_val(dt,param(p,P_DST),do*type_size(dt),load_val(st,param(p,P_SRC),so*type_size(st)));
}

fn dev_transpose(p:i64) {
    let ne00=param(p,P_NE00);
    let ne01=param(p,P_NE01);
    let n=ne00*ne01;
    let nmat=param(p,P_N)/n;
    let tx=bidy()*32+tidx();
    let ty=bidx()*32+tidy();
    var i:i64=0;
    while i<8 {
        let imat=bidz()*8+i;
        if imat>=nmat {return;}
        var j:i64=0;
        while j<32 {
            let row=ty+j;
            if tx<ne00&&row<ne01&&tx<ne01&&row<ne00 {
                transpose_one_element(p,imat,row,tx);
            }
            j=j+8;
        }
        i=i+1;
    }
}

fn ser_transpose(p:i64) {
    let ne00=param(p,P_NE00);
    let ne01=param(p,P_NE01);
    let n=ne00*ne01;
    let nmat=param(p,P_N)/n;
    let gx=(ne01+31)/32;
    let gy=(ne00+31)/32;
    let gz=(nmat+7)/8;
    var bx:i64=0;
    while bx<gx {
        var by:i64=0;
        while by<gy {
            var bz:i64=0;
            while bz<gz {
                var tiy:i64=0;
                while tiy<8 {
                    var tix:i64=0;
                    while tix<32 {
                        let tx=by*32+tix;
                        let ty=bx*32+tiy;
                        var i:i64=0;
                        while i<8 {
                            let imat=bz*8+i;
                            if imat<nmat {
                                var j:i64=0;
                                while j<32 {
                                    let row=ty+j;
                                    if tx<ne00&&row<ne01&&tx<ne01&&row<ne00 {
                                        transpose_one_element(p,imat,row,tx);
                                    }
                                    j=j+8;
                                }
                            }
                            i=i+1;
                        }
                        tix=tix+1;
                    }
                    tiy=tiy+1;
                }
                bz=bz+1;
            }
            by=by+1;
        }
        bx=bx+1;
    }
}

fn getrows_row(p:i64,i10:i64,z:i64,start:i64,step:i64) {
    let ne00=param(p,P_NE00);
    let ne12=param(p,P_NE12);
    let i11=z/ne12;
    let i12=z-i11*ne12;
    let idx=param(p,P_IDX);
    let i01=ldi32(idx,i10*param(p,P_NB10)+i11*param(p,P_NB11)+i12*param(p,P_NB12));
    let st=param(p,P_ST);
    let dt=param(p,P_DT);
    let src=param(p,P_SRC);
    let dst=param(p,P_DST);
    let src_row=i01*param(p,P_NB01)+i11*param(p,P_NB02)+i12*param(p,P_NB03);
    let dst_row=i10*param(p,P_NB1)+i11*param(p,P_NB2)+i12*param(p,P_NB3);
    var i00=start;
    while i00<ne00 {
        if st==T_Q4_0 {
            let ib=i00/QK4_0;
            let iqs=(i00-i00/QK4_0*QK4_0)/2;
            let iybs=(i00/QK4_0)*QK4_0;
            let d=f16val(ldu16(src,src_row+ib*Q4_0_BYTES));
            let q=ldu8(src,src_row+ib*Q4_0_BYTES+2+iqs);
            let v0=((q&15)-8 as i64);
            let v1=((q>>4)-8 as i64);
            store_val(dt,dst,dst_row+(iybs+iqs)*type_size(dt),(v0 as f32)*d);
            store_val(dt,dst,dst_row+(iybs+iqs+QK4_0/2)*type_size(dt),(v1 as f32)*d);
        } else {
            store_val(dt,dst,dst_row+i00*type_size(dt),load_val(st,src,src_row+i00*type_size(st)));
        }
        i00=i00+step;
    }
}

fn dev_getrows(p:i64) {
    let ne11=param(p,P_NE11);
    let ne12=param(p,P_NE12);
    let total=ne11*ne12;
    var start=bidy()*bdimx()+tidx();
    if param(p,P_AUX2)!=0 {start=start*2;}
    let step=gdimy()*bdimx();
    var z=bidz();
    while z<total {
        getrows_row(p,bidx(),z,start,step);
        z=z+gdimz();
    }
}

fn ser_getrows(p:i64) {
    let ne11=param(p,P_NE11);
    let ne12=param(p,P_NE12);
    let i10max=param(p,P_AUX0);
    let step=if param(p,P_ST)==T_Q4_0 {2}else{1};
    var i10:i64=0;
    while i10<i10max {
        var z:i64=0;
        while z<ne11*ne12 {
            getrows_row(p,i10,z,0,step);
            z=z+1;
        }
        i10=i10+1;
    }
}

fn setrows_one(p:i64,ii:i64) {
    let dt=param(p,P_DT);
    var i=ii;
    if dt==T_Q4_0 {i=i*QK4_0;}
    let ne00=param(p,P_NE00);
    let ne01=param(p,P_NE01);
    let ne02=param(p,P_NE02);
    let mat=ne00*ne01*ne02;
    let i03=i/mat;
    var r=i-i03*mat;
    let i02=r/(ne00*ne01);
    r=r-i02*ne00*ne01;
    let i01=r/ne00;
    let i00=r-i01*ne00;
    let ne11=param(p,P_NE11);
    let ne12=param(p,P_NE12);
    let i11=i02-ne11*(i02/ne11);
    let i12=i03-ne12*(i03/ne12);
    let i10=i01;
    let idx=param(p,P_IDX);
    let it=param(p,P_IT);
    let iv=if it==T_I32 {ldi32(idx,i10*param(p,P_NB10)+i11*param(p,P_NB11)+i12*param(p,P_NB12))} else {ldi64(idx,i10*param(p,P_NB10)+i11*param(p,P_NB11)+i12*param(p,P_NB12))};
    let st=param(p,P_ST);
    let src=param(p,P_SRC);
    let dst=param(p,P_DST);
    let so=i01*param(p,P_NB01)+i02*param(p,P_NB02)+i03*param(p,P_NB03)+i00*type_size(st);
    var do=iv*param(p,P_NB1)+i02*param(p,P_NB2)+i03*param(p,P_NB3);
    if dt==T_Q4_0 {
        quant_q4_block(src+so,dst+do+(i00/QK4_0)*Q4_0_BYTES);return;
    }
    do=do+i00*type_size(dt);
    if st==T_F16&&dt==T_F16 {stu16(dst,do,ldu16(src,so));return;}
    store_val(dt,dst,do,load_val(st,src,so));
}

fn dev_setrows(p:i64) {
    let n=param(p,P_N);
    let i=gidx();
    if i>=n {return;}
    setrows_one(p,i);
}

fn ser_setrows(p:i64) {
    let n=param(p,P_N);
    var i:i64=0;
    while i<n {
        setrows_one(p,i);
        i=i+1;
    }
}

fn dev_q4_dequant(p:i64) {
    let vx=param(p,P_SRC);
    let y=param(p,P_DST);
    let nb32=param(p,P_N);
    let blk=bidx();
    let tid=tidx();
    let il=tid/8;
    let ir=tid-tid/8*8;
    let ib=blk*8+ir;
    if ib>=nb32 {return;}
    let d=f16val(ldu16(vx,ib*Q4_0_BYTES));
    var l:i64=0;
    while l<4 {
        let q=ldu8(vx,ib*Q4_0_BYTES+2+4*il+l);
        let v0=((q&15)-8 as i64);
        let v1=((q>>4)-8 as i64);
        stf32(y,(blk*256+32*ir+4*il+l)*4,(v0 as f32)*d);
        stf32(y,(blk*256+32*ir+4*il+16+l)*4,(v1 as f32)*d);
        l=l+1;
    }
}

fn device_copy_scalar(p:i64)->i64 {
    enter_copy_scalar();
    let n=param(p,P_N);
    let i=gidx();
    if i<n {copy_one(p,i);}
    return 0;
}
fn device_copy_contig(p:i64)->i64 {
    enter_copy_contig();
    let n=param(p,P_N);
    let i=gidx();
    if i<n {copy_contig_one(p,i);}
    return 0;
}
fn device_copy_blocks(p:i64)->i64 {
    enter_copy_blocks();
    let n=param(p,P_N);
    let i=gidx()*QK4_0;
    if i<n {copy_blocks_one(p,i);}
    return 0;
}
fn device_transpose(p:i64)->i64 {enter_transpose();dev_transpose(p);return 0;}
fn device_getrows(p:i64)->i64 {enter_getrows();dev_getrows(p);return 0;}
fn device_setrows(p:i64)->i64 {enter_setrows();dev_setrows(p);return 0;}
fn device_q4_dequant(p:i64)->i64 {enter_q4_dequant();dev_q4_dequant(p);return 0;}

fn serial_copy_scalar(p:i64)->i64 {
    enter_copy_scalar();
    let n=param(p,P_N);
    var i:i64=0;
    while i<n {copy_one(p,i);i=i+1;}
    return n;
}
fn serial_copy_contig(p:i64)->i64 {
    enter_copy_contig();
    let n=param(p,P_N);
    var i:i64=0;
    while i<n {copy_contig_one(p,i);i=i+1;}
    return n;
}
fn serial_copy_blocks(p:i64)->i64 {
    enter_copy_blocks();
    let n=param(p,P_N);
    var i:i64=0;
    while i<n {copy_blocks_one(p,i);i=i+QK4_0;}
    return n;
}
fn serial_transpose(p:i64)->i64 {enter_transpose();ser_transpose(p);return 0;}
fn serial_getrows(p:i64)->i64 {enter_getrows();ser_getrows(p);return 0;}
fn serial_setrows(p:i64)->i64 {enter_setrows();ser_setrows(p);return 0;}

fn keep(op:i64,p:i64)->i64 {
    if op==1 {return device_copy_scalar(p);}
    if op==2 {return device_copy_contig(p);}
    if op==3 {return device_copy_blocks(p);}
    if op==4 {return device_transpose(p);}
    if op==5 {return device_getrows(p);}
    if op==6 {return device_setrows(p);}
    if op==7 {return device_q4_dequant(p);}
    if op==8 {return serial_copy_scalar(p);}
    if op==9 {return serial_copy_contig(p);}
    if op==10 {return serial_copy_blocks(p);}
    if op==11 {return serial_transpose(p);}
    if op==12 {return serial_getrows(p);}
    if op==13 {return serial_setrows(p);}
    return 0;
}
fn main() {print(keep(0,0));}
