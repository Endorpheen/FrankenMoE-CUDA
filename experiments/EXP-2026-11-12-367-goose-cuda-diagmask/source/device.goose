// Apex CUDA diag_mask_inf in Goose, from ggml-cuda/diagmask.cu.
//
// Device side: the (row, col) decomposition from the 2-D block, the column
// bound early exit, the linear index row*ncols+col, and the masking rule
// dst = x - (col > n_past + row%rows_per_channel) * FLT_MAX.
//
// Host side: block (1, CUDA_DIAG_MASK_INF_BLOCK_SIZE), grid (nrows, ceil(ncols/
// block), 1) and the rows_per_channel = ne01 wiring.
//
// Every comparison, index and subtraction is Goose. Loads and stores are typed
// primitives, as in the other ported kernels.
extern "fg367d_ld_f32" fn ld_f32(x:u64,o:u64)->f32;
extern "fg367d_st_f32" fn st_f32(x:u64,o:u64,v:f32);
extern "fg367d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// FLT_MAX as the native source spells it.
fn flt_max()->f32{ return 3.4028234663852886e38; }

// One output element. col>=ncols exits before any load, matching the early
// return in the native kernel; the returned flag tells the caller to store.
fn diag_elem(x:u64,dst:u64,ncols:i64,rows_per_channel:i64,n_past:i64,
             row:i64,col:i64)->i64{
 if col>=ncols{ return 0 as i64; }
 let i:i64=row*ncols+col;
 let limit:i64=n_past+(row%rows_per_channel);
 var v:f32=(ld_f32(x,((i*4 as i64) as u64))) as f32;
 if col>limit{
  v=(v-flt_max()) as f32;
 }
 st_f32(dst,((i*4 as i64) as u64),v);
 return 1 as i64;
}

fn mask_rows(x:u64,dst:u64,ncols:i64,rows:i64,rows_per_channel:i64,n_past:i64,
             bx:i64,by:i64,tx:i64,ty:i64,bdx:i64,bdy:i64)->i64{
 ensure(ncols>0 as i64,20);
 ensure(rows>0 as i64,21);
 ensure(rows_per_channel>0 as i64,22);
 let col:i64=bdy*by+ty;
 let row:i64=bdx*bx+tx;
 return diag_elem(x,dst,ncols,rows_per_channel,n_past,row,col);
}

// Host plan: the native launcher's column-block count, with the division made
// explicit so a zero or negative size is refused rather than divided by. Rows
// and block size are passthrough values in the native launcher.
fn plan_grid_y(ncols:i64,block_size:i64)->i64{
 ensure(ncols>0 as i64,10);
 ensure(block_size>0 as i64,12);
 return (ncols+block_size-1 as i64)/block_size;
}

// Retained entry; the parent never executes main.
fn main(){mask_rows(0 as u64,0 as u64,1 as i64,1 as i64,1 as i64,0 as i64,
                   0 as i64,0 as i64,0 as i64,0 as i64,1 as i64,1 as i64);}
