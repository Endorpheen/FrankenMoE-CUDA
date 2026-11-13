#pragma once
void fg167_native_ggml_cuda_op_repeat(ggml_backend_cuda_context&,ggml_tensor*);
void fg167_native_ggml_cuda_op_add(ggml_backend_cuda_context&,ggml_tensor*);
void fg167_native_ggml_cuda_op_sub(ggml_backend_cuda_context&,ggml_tensor*);
void fg167_native_ggml_cuda_op_mul(ggml_backend_cuda_context&,ggml_tensor*);
void fg167_native_ggml_cuda_op_div(ggml_backend_cuda_context&,ggml_tensor*);
void fg167_native_ggml_cuda_op_repeat_back(ggml_backend_cuda_context&,ggml_tensor*);
void fg167_native_ggml_cuda_op_fused_add(ggml_backend_cuda_context&,ggml_tensor*,int);
void fg167_native_ggml_cuda_op_fused_mul(ggml_backend_cuda_context&,ggml_tensor*,int);
static void invoke(int request,bool native,ggml_backend_cuda_context&ctx,ggml_tensor*d,int count){switch(request){
case 0:if(native)fg167_native_ggml_cuda_op_repeat(ctx,d);else ggml_cuda_op_repeat(ctx,d);break;
case 1:if(native)fg167_native_ggml_cuda_op_add(ctx,d);else ggml_cuda_op_add(ctx,d);break;
case 2:if(native)fg167_native_ggml_cuda_op_sub(ctx,d);else ggml_cuda_op_sub(ctx,d);break;
case 3:if(native)fg167_native_ggml_cuda_op_mul(ctx,d);else ggml_cuda_op_mul(ctx,d);break;
case 4:if(native)fg167_native_ggml_cuda_op_div(ctx,d);else ggml_cuda_op_div(ctx,d);break;
case 5:if(native)fg167_native_ggml_cuda_op_repeat_back(ctx,d);else ggml_cuda_op_repeat_back(ctx,d);break;
case 6:if(native)fg167_native_ggml_cuda_op_fused_add(ctx,d,count);else ggml_cuda_op_fused_add(ctx,d,count);break;
case 7:if(native)fg167_native_ggml_cuda_op_fused_mul(ctx,d,count);else ggml_cuda_op_fused_mul(ctx,d,count);break;
default:std::abort();}}
