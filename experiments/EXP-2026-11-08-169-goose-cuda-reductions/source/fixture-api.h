#pragma once
void fg169_native_ggml_cuda_op_sum_rows(ggml_backend_cuda_context&,ggml_tensor*);
void fg169_native_ggml_cuda_op_mean(ggml_backend_cuda_context&,ggml_tensor*);
void fg169_native_ggml_cuda_argmax(ggml_backend_cuda_context&,ggml_tensor*);
void fg169_native_ggml_cuda_op_sum(ggml_backend_cuda_context&,ggml_tensor*);
void fg169_native_sum_rows_f32_cuda(const float*,float*,int,int,cudaStream_t);
void fg169_native_sum_f32_cuda(ggml_cuda_pool&,const float*,float*,int64_t,cudaStream_t);
extern "C" uint64_t fg169_cuda_host_seen();
extern "C" uint64_t fg169_cuda_cub_seen();
extern "C" uint64_t fg169_cuda_live_temp();
extern "C" int fg169_cuda_device_read(uint64_t*);
