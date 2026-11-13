// ggml_cuda_op_set destination view in Goose (APX: P3-03 SET).
//
// Native (set.cu:25-42):
//     dst_view.data  = (char *) dst->data + offset;
//     dst_view.ne[]  = src1->ne[];
//     dst_view.nb[0] = ggml_element_size(dst);
//     dst_view.nb[1..3] = nb1..nb3 from op_params;
//
// SET writes src1 into a window of dst: `offset` bytes in, shaped and strided by op_params, not by src1's
// own layout. Two things are easy to get wrong and neither throws:
//
//  1. nb[0] comes from the DESTINATION element size, not from the packed stride of src1. For a same-type
//     operation these agree, which is why the difference survives casual testing and breaks as soon as the
//     window is not densely packed.
//  2. The shape comes from src1 while the strides come from op_params. Taking the shape from dst copies the
//     whole tensor instead of the sub-range, and the row count is what decides how much is overwritten.
//
// `offset` is in BYTES and is added to the base pointer before any stride is applied.

extern "fg245v_fail" fn fail(code:i64);
extern "fg245v_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg245v_st_i64" fn st_i64(base:u64,idx:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// view computes the destination window. out receives
//   [0] data pointer, [1..4] ne, [5..8] nb
// and returns 1.
fn view(dst_data:u64,offset:i64,elem_size:i64,nb1:i64,nb2:i64,nb3:i64,ne0:i64,ne1:i64,ne2:i64,ne3:i64,
        out:u64)->i64{
 ensure(offset>=0 as i64,2600);
 ensure(elem_size>0 as i64,2601);
 ensure(nb1>0 as i64&&nb2>0 as i64&&nb3>0 as i64,2602);
 ensure(ne0>0 as i64&&ne1>0 as i64&&ne2>0 as i64&&ne3>0 as i64,2603);
 st_i64(out,0 as i64,(dst_data as i64)+offset);
 st_i64(out,1 as i64,ne0);
 st_i64(out,2 as i64,ne1);
 st_i64(out,3 as i64,ne2);
 st_i64(out,4 as i64,ne3);
 // nb[0] is the destination element size: SET writes elements, it does not copy a packed blob
 st_i64(out,5 as i64,elem_size);
 st_i64(out,6 as i64,nb1);
 st_i64(out,7 as i64,nb2);
 st_i64(out,8 as i64,nb3);
 1 as i64
}

fn main(){
 view(0 as u64,0 as i64,4 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,0 as u64);
 view(4096 as u64,256 as i64,4 as i64,128 as i64,512 as i64,2048 as i64,32 as i64,4 as i64,2 as i64,1 as i64,24 as u64);
}
