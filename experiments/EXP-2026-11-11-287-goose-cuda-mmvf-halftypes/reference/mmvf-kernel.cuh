// Verbatim copy of mul_mat_vec_f from the read-only source tree, because it is
// defined inside mmvf.cu (not a header). Source:
// /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/mmvf.cu
// sha256 64d6aae3f3cc42d2508b0ceb67ac62d79dbc699cf2f5b36a87022c9652c9979d
// Only the kernel name changed (mul_mat_vec_f -> mul_mat_vec_f_ref); the body is
// unedited. Lines 8-385 of the source.
template <typename T, typename type_acc, int ncols_dst, int block_size, bool has_fusion = false, bool is_multi_token_id = false>
static __global__ void mul_mat_vec_f_ref(
        const T * x_ptr, const float * y_ptr, const int32_t * ids_ptr, const ggml_cuda_mm_fusion_args_device fusion, float * dst_ptr,
        const int ncols2, const uint3 nchannels_y, const int stride_row, const int stride_col_y2, const int stride_col_dst,
        const uint3 channel_ratio, const int stride_channel_x, const int stride_channel_y, const int stride_channel_dst,
        const uint3 sample_ratio, const int stride_sample_x, const int stride_sample_y, const int stride_sample_dst,
        const int ids_stride) {
    const T       * GGML_CUDA_RESTRICT x   = x_ptr;
    const float   * GGML_CUDA_RESTRICT y   = y_ptr;
    const int32_t * GGML_CUDA_RESTRICT ids = ids_ptr;
    float         * GGML_CUDA_RESTRICT dst = dst_ptr;
    const int row         = blockIdx.x;
    // for MUL_MAT_ID - blockIdx.y = n_expert_used, blockIdx.z = ncols_dst (tokens)
    const int channel_dst = blockIdx.y;
    const int tid         = threadIdx.x;

    int token_idx;
    int channel_x;
    int channel_y;
    int sample_dst;

    ggml_cuda_pdl_sync();
    if constexpr (is_multi_token_id) {
        // Multi-token MUL_MAT_ID path, adding these in the normal path causes a perf regression for n_tokens=1 case
        token_idx  = blockIdx.z;
        channel_x  = ids[channel_dst + token_idx * ids_stride];
        channel_y  = fastmodulo(channel_dst, nchannels_y);
        sample_dst = 0;
    } else {
        token_idx  = ids ? blockIdx.z                                          : 0;
        channel_x  = ids ? ids[blockIdx.y + token_idx * ids_stride]            : fastdiv((uint32_t) channel_dst, channel_ratio);
        channel_y  = ids ? fastmodulo(blockIdx.y, nchannels_y)                 : channel_dst;
        sample_dst = ids ? 0                                                   : blockIdx.z;
    }

    const int sample_x    = fastdiv((uint32_t) sample_dst, sample_ratio);
    const int sample_y    = sample_dst;

    constexpr int warp_size   = ggml_cuda_get_physical_warp_size();

    x   += int64_t(sample_x)  *stride_sample_x   + channel_x  *stride_channel_x   + row*stride_row;
    y   += int64_t(sample_y)  *stride_sample_y   + channel_y  *stride_channel_y;
    dst += int64_t(sample_dst)*stride_sample_dst + channel_dst*stride_channel_dst;
    if constexpr (is_multi_token_id) {
        y   += token_idx*stride_col_y2*2;
        dst += token_idx*stride_col_dst;
    }

    bool use_gate = false;
    bool use_bias = false;
    bool use_gate_bias = false;
    ggml_glu_op glu_op = ggml_glu_op::GGML_GLU_OP_SWIGLU;
    float glu_limit = 0.0f;
    const T * gate_x = nullptr;
    const float * x_bias = nullptr;
    const float * gate_bias = nullptr;

    if constexpr (has_fusion) {
        use_gate = fusion.gate != nullptr;
        use_bias = fusion.x_bias != nullptr;
        use_gate_bias = fusion.gate_bias != nullptr;
        glu_op = fusion.glu_op;
        glu_limit = fusion.glu_limit;

        if (use_gate) {
            gate_x = static_cast<const T *>(fusion.gate);
        }
        if (use_bias) {
            x_bias = static_cast<const float *>(fusion.x_bias);
        }
        if (use_gate_bias) {
            gate_bias = static_cast<const float *>(fusion.gate_bias);
            use_gate_bias = use_gate;
        } else {
            use_gate_bias = false;
        }
    }

    if (use_gate) {
        gate_x += int64_t(sample_x)  *stride_sample_x   + channel_x  *stride_channel_x   + row*stride_row;
    }

    if constexpr (has_fusion) {
        const int channel_bias = ids ? channel_x : channel_dst;
        if (use_bias) {
            x_bias += int64_t(sample_dst)*stride_sample_dst + channel_bias*stride_channel_dst;
        }
        if (use_gate_bias) {
            gate_bias += int64_t(sample_dst)*stride_sample_dst + channel_bias*stride_channel_dst;
        }
    }

    const float2 * y2 = (const float2 *) y;

    extern __shared__ char data_mmv[];
    float * buf_iw = (float *) data_mmv;
    [[maybe_unused]] float * buf_iw_gate = nullptr;
    if constexpr (has_fusion) {
        buf_iw_gate = (float *) (data_mmv + warp_size*sizeof(float));
    }

    if (block_size > warp_size) {
        if (tid < warp_size) {
            buf_iw[tid] = 0.0f;
            if constexpr (has_fusion) {
                if (use_gate) {
                    buf_iw_gate[tid] = 0.0f;
                }
            }
        }
        __syncthreads();
    }

    float sumf[ncols_dst] = {0.0f};
    float sumf_gate[ncols_dst];
    if constexpr (has_fusion) {
#pragma unroll
        for (int j = 0; j < ncols_dst; ++j) {
            sumf_gate[j] = 0.0f;
        }
    }

    if constexpr (std::is_same_v<T, float>) {
        const float2 * x2 = (const float2 *) x;
        [[maybe_unused]] const float2 * gate_x2 = nullptr;
        if constexpr (has_fusion) {
            if (use_gate) {
                gate_x2 = (const float2 *) gate_x;
            }
        }

        for (int col2 = tid; col2 < ncols2; col2 += block_size) {
            const float2 tmpx = x2[col2];
            float2 tmpx_gate = make_float2(0.0f, 0.0f);
            if constexpr (has_fusion) {
                if (use_gate) {
                    tmpx_gate = gate_x2[col2];
                }
            }

#pragma unroll
            for (int j = 0; j < ncols_dst; ++j) {
                const float2 tmpy = y2[j*stride_col_y2 + col2];
                ggml_cuda_mad(sumf[j], tmpx.x, tmpy.x);
                ggml_cuda_mad(sumf[j], tmpx.y, tmpy.y);

                if constexpr (has_fusion) {
                    if (use_gate) {
                        ggml_cuda_mad(sumf_gate[j], tmpx_gate.x, tmpy.x);
                        ggml_cuda_mad(sumf_gate[j], tmpx_gate.y, tmpy.y);
                    }
                }
            }
        }
    } else if constexpr (std::is_same_v<T, half>) {
        const half2 * x2 = (const half2 *) x;
        [[maybe_unused]] const half2 * gate_x2 = nullptr;
        if constexpr (has_fusion) {
            if (use_gate) {
                gate_x2 = (const half2 *) gate_x;
            }
        }

        if (std::is_same_v<type_acc, float>) {
            for (int col2 = tid; col2 < ncols2; col2 += block_size) {
                const float2 tmpx = __half22float2(x2[col2]);
                float2 tmpx_gate = make_float2(0.0f, 0.0f);
                if constexpr (has_fusion) {
                    if (use_gate) {
                        tmpx_gate = __half22float2(gate_x2[col2]);
                    }
                }
#pragma unroll
                for (int j = 0; j < ncols_dst; ++j) {
                    const float2 tmpy = y2[j*stride_col_y2 + col2];
                    ggml_cuda_mad(sumf[j], tmpx.x, tmpy.x);
                    ggml_cuda_mad(sumf[j], tmpx.y, tmpy.y);

                    if constexpr (has_fusion) {
                        if (use_gate) {
                            ggml_cuda_mad(sumf_gate[j], tmpx_gate.x, tmpy.x);
                            ggml_cuda_mad(sumf_gate[j], tmpx_gate.y, tmpy.y);
                        }
                    }
                }
            }
        } else {
#ifdef FP16_AVAILABLE
            half2 sumh2[ncols_dst] = {{0.0f, 0.0f}};
            half2 sumh2_gate[ncols_dst] = {{0.0f, 0.0f}};

            for (int col2 = tid; col2 < ncols2; col2 += block_size) {
                const half2 tmpx = x2[col2];
                half2 tmpx_gate = make_half2(0.0f, 0.0f);
                if constexpr (has_fusion) {
                    if (use_gate) {
                        tmpx_gate = gate_x2[col2];
                    }
                }
#pragma unroll
                for (int j = 0; j < ncols_dst; ++j) {
                    const float2 tmpy = y2[j*stride_col_y2 + col2];
                    sumh2[j] += tmpx * make_half2(tmpy.x, tmpy.y);

                    if constexpr (has_fusion) {
                        if (use_gate) {
                            sumh2_gate[j] += tmpx_gate * make_half2(tmpy.x, tmpy.y);
                        }
                    }
                }
            }

#pragma unroll
            for (int j = 0; j < ncols_dst; ++j) {
                sumf[j] = __low2float(sumh2[j]) + __high2float(sumh2[j]);
            }

            if constexpr (has_fusion) {
                if (use_gate) {
#pragma unroll
                    for (int j = 0; j < ncols_dst; ++j) {
                        sumf_gate[j] = __low2float(sumh2_gate[j]) + __high2float(sumh2_gate[j]);
                    }
                }
            }
#else
            NO_DEVICE_CODE;
#endif // FP16_AVAILABLE
        }
    } else if constexpr (std::is_same_v<T, nv_bfloat16>) {
//TODO: add support for ggml_cuda_mad for hip_bfloat162
#if defined(GGML_USE_HIP)
        const int * x2 = (const int *) x;
        const int * gate_x2 = nullptr;
        if constexpr (has_fusion) {
            if (use_gate) {
                gate_x2 = (const int *) gate_x;
            }
        }
        for (int col2 = tid; col2 < ncols2; col2 += block_size) {
            const int tmpx = x2[col2];
            int tmpx_gate = 0;
            if constexpr (has_fusion) {
                if (use_gate) {
                    tmpx_gate = gate_x2[col2];
                }
            }
#pragma unroll
            for (int j = 0; j < ncols_dst; ++j) {
                const float2 tmpy = y2[j*stride_col_y2 + col2];
                const float tmpx0 = ggml_cuda_cast<float>(reinterpret_cast<const nv_bfloat16 *>(&tmpx)[0]);
                const float tmpx1 = ggml_cuda_cast<float>(reinterpret_cast<const nv_bfloat16 *>(&tmpx)[1]);
                ggml_cuda_mad(sumf[j], tmpx0, tmpy.x);
                ggml_cuda_mad(sumf[j], tmpx1, tmpy.y);

                if constexpr (has_fusion) {
                    if (use_gate) {
                        const float tmpx0_gate = ggml_cuda_cast<float>(reinterpret_cast<const nv_bfloat16 *>(&tmpx_gate)[0]);
                        const float tmpx1_gate = ggml_cuda_cast<float>(reinterpret_cast<const nv_bfloat16 *>(&tmpx_gate)[1]);
                        ggml_cuda_mad(sumf_gate[j], tmpx0_gate, tmpy.x);
                        ggml_cuda_mad(sumf_gate[j], tmpx1_gate, tmpy.y);
                    }
                }
            }
        }
#else
        const nv_bfloat162 * x2 = (const nv_bfloat162 *) x;
        [[maybe_unused]] const nv_bfloat162 * gate_x2 = nullptr;
        if constexpr (has_fusion) {
            if (use_gate) {
                gate_x2 = (const nv_bfloat162 *) gate_x;
            }
        }
        for (int col2 = tid; col2 < ncols2; col2 += block_size) {
            const nv_bfloat162 tmpx = x2[col2];
            [[maybe_unused]] nv_bfloat162 tmpx_gate;
            if constexpr (has_fusion) {
                if (use_gate) {
                    tmpx_gate = gate_x2[col2];
                }
            }
#pragma unroll
            for (int j = 0; j < ncols_dst; ++j) {
                const float2 tmpy = y2[j*stride_col_y2 + col2];
                ggml_cuda_mad(sumf[j], tmpx.x, tmpy.x);
                ggml_cuda_mad(sumf[j], tmpx.y, tmpy.y);

                if constexpr (has_fusion) {
                    if (use_gate) {
                        ggml_cuda_mad(sumf_gate[j], tmpx_gate.x, tmpy.x);
                        ggml_cuda_mad(sumf_gate[j], tmpx_gate.y, tmpy.y);
                    }
                }
            }
        }
#endif
    } else {
        static_assert(std::is_same_v<T, void>, "unsupported type");
    }

    ggml_cuda_pdl_lc();
#pragma unroll
    for (int j = 0; j < ncols_dst; ++j) {
        sumf[j] = warp_reduce_sum<warp_size>(sumf[j]);

        if constexpr (has_fusion) {
            if (use_gate) {
                sumf_gate[j] = warp_reduce_sum<warp_size>(sumf_gate[j]);
            }
        }

        if (block_size > warp_size) {
            buf_iw[tid/warp_size] = sumf[j];
            if constexpr (has_fusion) {
                if (use_gate) {
                    buf_iw_gate[tid/warp_size] = sumf_gate[j];
                }
            }
            __syncthreads();
            if (tid < warp_size) {
                sumf[j] = buf_iw[tid];
                sumf[j] = warp_reduce_sum<warp_size>(sumf[j]);
                if constexpr (has_fusion) {
                    if (use_gate) {
                        sumf_gate[j] = buf_iw_gate[tid];
                        sumf_gate[j] = warp_reduce_sum<warp_size>(sumf_gate[j]);
                    }
                }
            }

            if (j < ncols_dst) {
                __syncthreads();
            }
        }
    }

    if (tid >= ncols_dst) {
        return;
    }

    float value = sumf[tid];

    if constexpr (has_fusion) {
        if (use_bias) {
            value += x_bias[tid*stride_col_dst + row];
        }

        if (use_gate) {
            float gate_value = sumf_gate[tid];
            if (use_gate_bias) {
                gate_value += gate_bias[tid*stride_col_dst + row];
            }
            switch (glu_op) {
                case GGML_GLU_OP_SWIGLU:
                    value *= ggml_cuda_op_silu_single(gate_value);
                    break;
                case GGML_GLU_OP_GEGLU:
                    value *= ggml_cuda_op_gelu_single(gate_value);
                    break;
                case GGML_GLU_OP_SWIGLU_OAI: {
                    value = ggml_cuda_op_swiglu_oai_single(gate_value, value);
                    break;
                }
                case GGML_GLU_OP_SWIGLU_CLAMP:
                    value = ggml_cuda_op_swiglu_clamp_single(gate_value, value, glu_limit);
                    break;
                default:
                    break;
            }
        }
    }

    dst[tid*stride_col_dst + row] = value;

    if constexpr (!has_fusion) {
        GGML_UNUSED_VARS(use_gate, use_bias, use_gate_bias, glu_op, glu_limit, gate_x, x_bias, gate_bias, sumf_gate);
    }
}
