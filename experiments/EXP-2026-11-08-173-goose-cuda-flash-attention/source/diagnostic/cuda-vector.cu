#include <cuda_runtime.h>
static __device__ float*fg175v_trace_ptr;
#include "vector-native.cuh"
#include "vector-primitives.cuh"
#include "vector-functions-generated.cuh"
#include "vector-bridge.cuh"
extern "C" int fg175v_trace_set(float*p){return (int)cudaMemcpyToSymbol(fg175v_trace_ptr,&p,sizeof(p));}
