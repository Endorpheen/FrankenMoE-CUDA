fn micro_0(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};
    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;
        for group in 2 {let col=col0+group*8;
            var ac0:i32[8]=default<i32[8]>();var ac1:i32[8]=default<i32[8]>();var ac2:i32[8]=default<i32[8]>();var ac3:i32[8]=default<i32[8]>();var ac4:i32[8]=default<i32[8]>();var ac5:i32[8]=default<i32[8]>();var ac6:i32[8]=default<i32[8]>();var ac7:i32[8]=default<i32[8]>();var mins:i32[8]=default<i32[8]>();
            for j in 8 {let codes=a.q[panel][j];
                let scales=packed_simd::splat16(a.scales[panel][j] as! i16);
                ac0=packed_simd::add32(ac0,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+0)*nk+slab][j])));
                ac1=packed_simd::add32(ac1,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+1)*nk+slab][j])));
                ac2=packed_simd::add32(ac2,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+2)*nk+slab][j])));
                ac3=packed_simd::add32(ac3,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+3)*nk+slab][j])));
                ac4=packed_simd::add32(ac4,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+4)*nk+slab][j])));
                ac5=packed_simd::add32(ac5,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+5)*nk+slab][j])));
                ac6=packed_simd::add32(ac6,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+6)*nk+slab][j])));
                ac7=packed_simd::add32(ac7,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+7)*nk+slab][j])));
                let bi0=((slab*16+2*j)*bs+col)/8;let bi1=((slab*16+2*j+1)*bs+col)/8;
                mins=packed_simd::add32(mins,packed_simd::mul32(packed_simd::add32(b.sums[bi0],b.sums[bi1]),splat32(a.mins[panel][j])));
            }
            var sums:i32[8]=default<i32[8]>();sums[0]=hsum(ac0);sums[1]=hsum(ac1);sums[2]=hsum(ac2);sums[3]=hsum(ac3);sums[4]=hsum(ac4);sums[5]=hsum(ac5);sums[6]=hsum(ac6);sums[7]=hsum(ac7);
            let result=packed_simd::fma32(-a.dmin[di],mins,scaled(a.d[di],sums));
            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}
        }
    }
}

fn micro_1(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};
    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;
        for group in 2 {let col=col0+group*8;
            var ac0:i32[8]=default<i32[8]>();var ac1:i32[8]=default<i32[8]>();var ac2:i32[8]=default<i32[8]>();var ac3:i32[8]=default<i32[8]>();var ac4:i32[8]=default<i32[8]>();var ac5:i32[8]=default<i32[8]>();var ac6:i32[8]=default<i32[8]>();var ac7:i32[8]=default<i32[8]>();var mins:i32[8]=default<i32[8]>();
            for j in 8 {let codes=a.q[panel][j];
                let s0=a.scales[panel*2+j/4][2*(j%4)];let s1=a.scales[panel*2+j/4][2*(j%4)+1];let scales=pair_scales(s0 as! i16,s1 as! i16);
                let signed=debias(codes,32);
                ac0=packed_simd::add32(ac0,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+0)*nk+slab][j]),signed)));
                ac1=packed_simd::add32(ac1,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+1)*nk+slab][j]),signed)));
                ac2=packed_simd::add32(ac2,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+2)*nk+slab][j]),signed)));
                ac3=packed_simd::add32(ac3,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+3)*nk+slab][j]),signed)));
                ac4=packed_simd::add32(ac4,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+4)*nk+slab][j]),signed)));
                ac5=packed_simd::add32(ac5,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+5)*nk+slab][j]),signed)));
                ac6=packed_simd::add32(ac6,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+6)*nk+slab][j]),signed)));
                ac7=packed_simd::add32(ac7,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+7)*nk+slab][j]),signed)));
            }
            var sums:i32[8]=default<i32[8]>();sums[0]=hsum(ac0);sums[1]=hsum(ac1);sums[2]=hsum(ac2);sums[3]=hsum(ac3);sums[4]=hsum(ac4);sums[5]=hsum(ac5);sums[6]=hsum(ac6);sums[7]=hsum(ac7);
            sums=sub32(sums,splat32(a.mins[panel*2][0]));
            let result=scaled(a.d[di],sums);
            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}
        }
    }
}

fn micro_2(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};
    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;
        for group in 2 {let col=col0+group*8;
            var ac0:i32[8]=default<i32[8]>();var ac1:i32[8]=default<i32[8]>();var ac2:i32[8]=default<i32[8]>();var ac3:i32[8]=default<i32[8]>();var ac4:i32[8]=default<i32[8]>();var ac5:i32[8]=default<i32[8]>();var ac6:i32[8]=default<i32[8]>();var ac7:i32[8]=default<i32[8]>();var mins:i32[8]=default<i32[8]>();
            for j in 8 {let codes=a.q[panel][j];
                let s0=a.scales[panel*2+j/4][2*(j%4)];let s1=a.scales[panel*2+j/4][2*(j%4)+1];let scales=pair_scales(s0 as! i16,s1 as! i16);
                let signed=debias(codes,4);
                ac0=packed_simd::add32(ac0,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+0)*nk+slab][j]),signed)));
                ac1=packed_simd::add32(ac1,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+1)*nk+slab][j]),signed)));
                ac2=packed_simd::add32(ac2,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+2)*nk+slab][j]),signed)));
                ac3=packed_simd::add32(ac3,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+3)*nk+slab][j]),signed)));
                ac4=packed_simd::add32(ac4,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+4)*nk+slab][j]),signed)));
                ac5=packed_simd::add32(ac5,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+5)*nk+slab][j]),signed)));
                ac6=packed_simd::add32(ac6,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+6)*nk+slab][j]),signed)));
                ac7=packed_simd::add32(ac7,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+7)*nk+slab][j]),signed)));
            }
            var sums:i32[8]=default<i32[8]>();sums[0]=hsum(ac0);sums[1]=hsum(ac1);sums[2]=hsum(ac2);sums[3]=hsum(ac3);sums[4]=hsum(ac4);sums[5]=hsum(ac5);sums[6]=hsum(ac6);sums[7]=hsum(ac7);
            sums=sub32(sums,splat32(a.mins[panel*2][0]));
            let result=scaled(a.d[di],sums);
            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}
        }
    }
}

fn micro_3(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};
    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;
        for group in 2 {let col=col0+group*8;
            var ac0:i32[8]=default<i32[8]>();var ac1:i32[8]=default<i32[8]>();var ac2:i32[8]=default<i32[8]>();var ac3:i32[8]=default<i32[8]>();var ac4:i32[8]=default<i32[8]>();var ac5:i32[8]=default<i32[8]>();var ac6:i32[8]=default<i32[8]>();var ac7:i32[8]=default<i32[8]>();var mins:i32[8]=default<i32[8]>();
            for j in 8 {let codes=a.q[panel][j];
                let s0=a.scales[panel*2+j/4][2*(j%4)];let s1=a.scales[panel*2+j/4][2*(j%4)+1];let scales=pair_scales(s0 as! i16,s1 as! i16);
                ac0=packed_simd::add32(ac0,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+0)*nk+slab][j])));
                ac1=packed_simd::add32(ac1,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+1)*nk+slab][j])));
                ac2=packed_simd::add32(ac2,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+2)*nk+slab][j])));
                ac3=packed_simd::add32(ac3,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+3)*nk+slab][j])));
                ac4=packed_simd::add32(ac4,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+4)*nk+slab][j])));
                ac5=packed_simd::add32(ac5,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+5)*nk+slab][j])));
                ac6=packed_simd::add32(ac6,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+6)*nk+slab][j])));
                ac7=packed_simd::add32(ac7,packed_simd::madd16(scales,packed_simd::maddubs(codes,b.q[(col+7)*nk+slab][j])));
                let bi0=((slab*16+2*j)*bs+col)/8;let bi1=((slab*16+2*j+1)*bs+col)/8;
                mins=packed_simd::add32(mins,packed_simd::add32(packed_simd::mul32(b.sums[bi0],splat32(a.mins[panel*2+j/4][2*(j%4)])),packed_simd::mul32(b.sums[bi1],splat32(a.mins[panel*2+j/4][2*(j%4)+1]))));
            }
            var sums:i32[8]=default<i32[8]>();sums[0]=hsum(ac0);sums[1]=hsum(ac1);sums[2]=hsum(ac2);sums[3]=hsum(ac3);sums[4]=hsum(ac4);sums[5]=hsum(ac5);sums[6]=hsum(ac6);sums[7]=hsum(ac7);
            let result=packed_simd::fma32(-a.dmin[di],mins,scaled(a.d[di],sums));
            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}
        }
    }
}

fn micro_4(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};
    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;
        for group in 2 {let col=col0+group*8;
            var ac0:i32[8]=default<i32[8]>();var ac1:i32[8]=default<i32[8]>();var ac2:i32[8]=default<i32[8]>();var ac3:i32[8]=default<i32[8]>();var ac4:i32[8]=default<i32[8]>();var ac5:i32[8]=default<i32[8]>();var ac6:i32[8]=default<i32[8]>();var ac7:i32[8]=default<i32[8]>();var mins:i32[8]=default<i32[8]>();
            for j in 8 {let codes=a.q[panel][j];
                let scales=packed_simd::splat16(a.scales[panel][j] as! i16);
                let signed=debias(codes,128);
                ac0=packed_simd::add32(ac0,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+0)*nk+slab][j])));
                ac1=packed_simd::add32(ac1,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+1)*nk+slab][j])));
                ac2=packed_simd::add32(ac2,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+2)*nk+slab][j])));
                ac3=packed_simd::add32(ac3,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+3)*nk+slab][j])));
                ac4=packed_simd::add32(ac4,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+4)*nk+slab][j])));
                ac5=packed_simd::add32(ac5,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+5)*nk+slab][j])));
                ac6=packed_simd::add32(ac6,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+6)*nk+slab][j])));
                ac7=packed_simd::add32(ac7,packed_simd::madd16(scales,signed_pairs(signed,b.q[(col+7)*nk+slab][j])));
            }
            var sums:i32[8]=default<i32[8]>();sums[0]=hsum(ac0);sums[1]=hsum(ac1);sums[2]=hsum(ac2);sums[3]=hsum(ac3);sums[4]=hsum(ac4);sums[5]=hsum(ac5);sums[6]=hsum(ac6);sums[7]=hsum(ac7);
            let result=scaled(a.d[di],sums);
            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}
        }
    }
}

fn micro_5(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};
    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;
        for group in 2 {let col=col0+group*8;
            var ac0:i32[8]=default<i32[8]>();var ac1:i32[8]=default<i32[8]>();var ac2:i32[8]=default<i32[8]>();var ac3:i32[8]=default<i32[8]>();var ac4:i32[8]=default<i32[8]>();var ac5:i32[8]=default<i32[8]>();var ac6:i32[8]=default<i32[8]>();var ac7:i32[8]=default<i32[8]>();var mins:i32[8]=default<i32[8]>();
            for j in 8 {let codes=a.q[panel][j];
                let scales=packed_simd::splat16(a.scales[panel][j] as! i16);
                let signed=debias(codes,128);
                ac0=packed_simd::add32(ac0,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+0)*nk+slab][j]),signed)));
                ac1=packed_simd::add32(ac1,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+1)*nk+slab][j]),signed)));
                ac2=packed_simd::add32(ac2,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+2)*nk+slab][j]),signed)));
                ac3=packed_simd::add32(ac3,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+3)*nk+slab][j]),signed)));
                ac4=packed_simd::add32(ac4,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+4)*nk+slab][j]),signed)));
                ac5=packed_simd::add32(ac5,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+5)*nk+slab][j]),signed)));
                ac6=packed_simd::add32(ac6,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+6)*nk+slab][j]),signed)));
                ac7=packed_simd::add32(ac7,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+7)*nk+slab][j]),signed)));
            }
            var sums:i32[8]=default<i32[8]>();sums[0]=hsum(ac0);sums[1]=hsum(ac1);sums[2]=hsum(ac2);sums[3]=hsum(ac3);sums[4]=hsum(ac4);sums[5]=hsum(ac5);sums[6]=hsum(ac6);sums[7]=hsum(ac7);
            sums=sub32(sums,splat32(a.mins[panel*1][0]));
            let result=scaled(a.d[di],sums);
            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}
        }
    }
}

fn micro_6(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};
    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;
        for group in 2 {let col=col0+group*8;
            var ac0:i32[8]=default<i32[8]>();var ac1:i32[8]=default<i32[8]>();var ac2:i32[8]=default<i32[8]>();var ac3:i32[8]=default<i32[8]>();var ac4:i32[8]=default<i32[8]>();var ac5:i32[8]=default<i32[8]>();var ac6:i32[8]=default<i32[8]>();var ac7:i32[8]=default<i32[8]>();var mins:i32[8]=default<i32[8]>();
            for j in 8 {let codes=a.q[panel][j];
                let s0=a.scales[panel*2+j/4][2*(j%4)];let s1=a.scales[panel*2+j/4][2*(j%4)+1];let scales=pair_scales(s0 as! i16,s1 as! i16);
                let signed=debias(codes,128);
                ac0=packed_simd::add32(ac0,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+0)*nk+slab][j]),signed)));
                ac1=packed_simd::add32(ac1,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+1)*nk+slab][j]),signed)));
                ac2=packed_simd::add32(ac2,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+2)*nk+slab][j]),signed)));
                ac3=packed_simd::add32(ac3,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+3)*nk+slab][j]),signed)));
                ac4=packed_simd::add32(ac4,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+4)*nk+slab][j]),signed)));
                ac5=packed_simd::add32(ac5,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+5)*nk+slab][j]),signed)));
                ac6=packed_simd::add32(ac6,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+6)*nk+slab][j]),signed)));
                ac7=packed_simd::add32(ac7,packed_simd::madd16(scales,packed_simd::maddubs(as_unsigned(b.q[(col+7)*nk+slab][j]),signed)));
            }
            var sums:i32[8]=default<i32[8]>();sums[0]=hsum(ac0);sums[1]=hsum(ac1);sums[2]=hsum(ac2);sums[3]=hsum(ac3);sums[4]=hsum(ac4);sums[5]=hsum(ac5);sums[6]=hsum(ac6);sums[7]=hsum(ac7);
            sums=sub32(sums,splat32(a.mins[panel*2][0]));
            let result=scaled(a.d[di],sums);
            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}
        }
    }
}

fn micro(p:const Plan&,w:Workspace&) {
    assert(p.nk>=1&&p.nk<=16&&p.slab>=0&&p.slab<p.nk&&p.row0>=0&&p.col0>=0&&p.out0>=0&&p.row0+16<=256/p.nk&&p.col0+16<=256/p.nk&&p.out0+16<=256&&p.col0%16==0);
    if p.nk>1 {assert(p.row0==0&&p.col0==0);}
    if p.kind==0||p.kind==1 {micro_0(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==2 {micro_1(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==3 {micro_2(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==4 {micro_3(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==5 {micro_4(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==6||p.kind==9||p.kind==10||p.kind==11 {micro_5(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else {assert(p.kind==7||p.kind==8||p.kind==12);micro_6(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    witness(1,p.kind,1);
}

fn gather_y(p:const Plan&,y:const Q8[:],routes:const Route[:],w:Workspace&) {
    let f=tile_traits(p.kind);let b:ActivationTile& = &w.b;let bs=if p.nk==1 {256} else {16};
    assert(p.count>=0&&p.count<=256&&p.nk>=1&&p.nk<=16&&round16(p.count)*p.nk<=256&&p.route_first>=0&&p.slots>0&&p.y_stride>0&&p.start>=0);
    if p.nk>1 {assert(p.count<=16);}
    if p.dense==0 {assert(p.route_first+p.count<=routes.len);}
    for slab in p.nk {for row in round16(p.count) {
        let panel=row*p.nk+slab;let di=slab*16+row;
        if row<p.count {
            var source:i64=0;
            if p.dense!=0 {source=(p.input_first+row)*p.y_stride+p.start+slab;}
            else {let route=routes[p.route_first+row];assert(route.slot>=0&&route.token>=0);source=((route.slot as i64)%p.slots+(route.token as i64)*p.slots)*p.y_stride+p.start+slab;}let x=&y[source];b.d[di]=x.d;
            for s in 16 {let index=(slab*16+s)*bs+row;b.sums[index/8][index%8]=x.sums[s] as i32;}
            for j in 8 {b.q[panel][j]=if f.act!=0&&p.defer_bias==0 {biased(x.q[j])} else {x.q[j]};}
        } else {
            b.d[di]=(0 as f32);for s in 16 {let index=(slab*16+s)*bs+row;b.sums[index/8][index%8]=0;}
            for j in 8 {b.q[panel][j]=if f.act!=0&&p.defer_bias==0 {biased(default<i8[32]>())} else {default<i8[32]>()};}
        }
    }}
    witness(2,p.kind,p.count*p.nk);
}
