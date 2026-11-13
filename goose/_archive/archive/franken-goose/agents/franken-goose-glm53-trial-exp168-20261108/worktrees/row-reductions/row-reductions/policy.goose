// fg169r row-reductions host launch policy: pure host-side arithmetic, no FFI, no plan/packet access.
// Block-size selection shared by sumrows.cu and mean.cu (llama.cpp PR #15132 heuristic):
//   grid is one block per row (block_nums = nrows, zero dynamic shared bytes in the native launch);
//   if (nrows / nsm) < 2 -> 512 threads, to better hide latency on grids that underfill the device;
//   else                 -> 32 threads when ncols < 1024, else 128 threads for better scheduling.
// Only 32/128/512 are ever selected, matching the device module's supported block sizes.
fn rr_row_block_size(ncols:i64,nrows:i64,nsm:i64)->i32{
 if (nrows/nsm)<2{return 512;}
 if ncols<1024{return 32;}
 return 128;
}

// Shared F32 element count for the cross-warp reduction phase (native __shared__ float shared_vals[32];
// rr_block_sum touches only the first bs/32 entries). Parent allocates 32 elements = 128 bytes whenever
// bs > 32; for bs == 32 the cross-warp phase is absent and no shared storage is required.
fn rr_row_shared_floats(bs:i32)->i32{
 if bs>32{return 32;}
 return 0;
}

// Compile-time reachability stub with dummy scalars; never executed on the host.
fn main(){
 rr_row_block_size(512,1,148);
 rr_row_block_size(2048,296,148);
 rr_row_shared_floats(32);
 rr_row_shared_floats(512);
}
