#pragma once
#include "fattn.cuh"
// Local component harness only; final host dispatch is a separate Goose module.
void fg175v_test_run(ggml_backend_cuda_context&ctx,ggml_tensor*dst,int ncols,int parallel_blocks,bool original,float*parts,float2*meta,const int*kvmax);
extern "C" int fg175v_test_witness(uint64_t out[2]);
