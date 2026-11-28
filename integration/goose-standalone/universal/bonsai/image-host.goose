type BonsaiCudaInit = c_fnptr(u32) -> i32;
type BonsaiCudaDevice = c_fnptr(u64, i32) -> i32;
type BonsaiCudaContextCreate = c_fnptr(u64, u32, i32) -> i32;
type BonsaiCudaModuleLoad = c_fnptr(u64, u64) -> i32;
type BonsaiCudaLookup = c_fnptr(u64, u64, u64) -> i32;
type BonsaiCudaAttribute = c_fnptr(u64, i32, i32) -> i32;

fn bonsai_pipeline_kernel_names() -> u64 {
    return vocab_text("bonsai_norm\0bonsai_fwht1024\0\0bonsai_residual\0bonsai_decay\0bonsai_conv_step\0goose_gdn_decode\0bonsai_split_q_gate\0goose_rope_decode\0goose_set_q4_rows\0goose_q4_attention_tiles\0bonsai_attention_gate\0bonsai_pq2_row\0bonsai_swiglu\0goose_q4_attention_merge\0goose_gated_delta_net\0goose_rope_phases\0goose_rope_apply\0bonsai_embed_batch\0bonsai_conv_batch\0bonsai_pack_half\0bonsai_pq2_mmq\0bonsai_flash_f32_tile\0bonsai_flash_f32_merge\0bonsai_flash_staged_tile\0bonsai_flash_stage_kv\0bonsai_flash_ordered_merge\0bonsai_flash_compact_tile\0bonsai_flash_compact_merge\0bonsai_flash_compact_ordered_merge\0bonsai_flash_async_tile\0bonsai_pq2_mmq_async\0goose_gdn_mma_v32\0bonsai_pq2_mmq_64x64\0bonsai_decay_reuse4\0bonsai_decay_reuse8\0bonsai_ffn_tail\0bonsai_pq2_mmq_64x64_k64\0bonsai_pq2_mmq_64x128_k64\0bonsai_decode_gqa2\0bonsai_decode_gqa3\0bonsai_pq2_matvec_rows2\0bonsai_flash_tile_group2\0bonsai_flash_tile_group4\0bonsai_mv_pack_q8\0bonsai_pq2_mvq_q8_1\0bonsai_pq2_mvq_q8_2\0bonsai_norm_wide_512_1\0bonsai_norm_wide_128_0\0bonsai_pack_q8_128\0bonsai_pq2_mmq_int8\0bonsai_decay_stage_256_0\0bonsai_decay_warp_pack\0bonsai_flash_kv_lean2\0bonsai_pq2_cached_64x64_k64\0bonsai_pq2_cached_64x128_k64\0bonsai_flash_kv_fused_ordered\0bonsai_pq2_cached_fp16_acc_k32\0bonsai_pq2_half_full_5120\0bonsai_pq2_half_full_17408\0bonsai_pq2_half_full_6144\0bonsai_flash_stage_k_fp8\0bonsai_flash_stage_v_only\0bonsai_flash_fp8_qk\0bonsai_pq2_half_unpack_half2_17408\0bonsai_pq2_mmq_int8_full_n128\0bonsai_flash_stage_v_fp8\0bonsai_flash_fp8_qkpv\0bonsai_pq2_mmq_int8_full_n128_pairload\0bonsai_pack_q8_panel128\0bonsai_pq2_mmq_int8_full_n128_panel\0bonsai_pq2_mmq_int8_full_n128_spread\0bonsai_decode_direct_q4_mma\0bonsai_verify_merge_parallel\0bonsai_decode_direct_q4_mma_half2\0bonsai_ffn_tail_q8_panel\0bonsai_pq2_mmq_int8_down_n128_spread\0bonsai_choice_tiles\0bonsai_choice_final\0").address;
}

fn bonsai_mtp_kernel_names() -> u64 {
    return vocab_text("bonsai_mtp_eh_q4\0bonsai_mtp_mat_q4\0bonsai_pq2_mvq_q8_three_1\0bonsai_pq2_mvq_q8_three_2\0bonsai_conv_verify_prefix\0bonsai_mtp_concat\0bonsai_mtp_q4_four\0bonsai_verify_q4_gqa2\0bonsai_verify_merge_parallel\0bonsai_verify_q4_tail\0bonsai_pq2_mvq_q8_four_1\0bonsai_pq2_mvq_q8_four_2\0bonsai_verify_pair_tail3\0bonsai_conv_verify_prefix_four\0bonsai_pq2_mvq_q8_five_1\0bonsai_pq2_mvq_q8_five_2\0bonsai_verify_five_32\0bonsai_conv_verify_prefix_five\0bonsai_mvq_4_permutation_2\0bonsai_mvq_4_permutation_4\0bonsai_mvq_5_spread_4\0").address;
}

fn bonsai_load_image(dispatch: u64, destination: u64, path: u64, image: u64) -> i32 {
    if path != 0 && vocab_byte(path) != 0 {
        return unsafe_c_fnptr<BonsaiCudaModuleLoad>(vocab_word(dispatch + 56))(destination, path);
    }
    return unsafe_c_fnptr<BonsaiCudaModuleLoad>(vocab_word(dispatch + 64))(destination, image);
}

fn bonsai_dynamic_shared(dispatch: u64, model: u64, kernel: u64, bytes: i32) -> i32 {
    return unsafe_c_fnptr<BonsaiCudaAttribute>(vocab_word(dispatch + 128))(
        vocab_word(model + BONSAI_KERNELS + kernel * 8), 8, bytes);
}

fn bonsai_image_flag(name: const u8[:], default_on: bool) -> bool {
    let value = fg_cuda_model_getenv(vocab_text(name).address);
    if value == 0 { return default_on; }
    let expected: u8 = if default_on { 48 } else { 49 };
    let matches = vocab_byte(value) == expected && vocab_byte(value + 1) == 0;
    return if default_on { !matches } else { matches };
}

export fn fg_bonsai_load_images(model: u64, cubin: u64, frozen: u64, mtp_path: u64,
    pipeline_image: u64, frozen_image: u64, draft_image: u64) -> i32 {
    let dispatch = fg_bonsai_cuda_dispatch();
    var device: i32 = 0;
    if bonsai_cuda_status(unsafe_c_fnptr<BonsaiCudaInit>(vocab_word(dispatch))(0), "driver") != 0 ||
        bonsai_cuda_status(unsafe_c_fnptr<BonsaiCudaDevice>(vocab_word(dispatch + 8))(
            vocab_address(unsafe_c_address<c_ptr<const void>>(device)), 0), "device") != 0 ||
        bonsai_cuda_status(unsafe_c_fnptr<BonsaiCudaContextCreate>(vocab_word(dispatch + 16))(model, 0, device), "context") != 0 ||
        bonsai_cuda_status(bonsai_load_image(dispatch, model + 4680, cubin, pipeline_image), "Bonsai CUDA image") != 0 ||
        bonsai_cuda_status(bonsai_load_image(dispatch, model + 4688, frozen, frozen_image), "frozen Goose CUDA image") != 0 { return -1; }
    let mtp = batch_u32(model + SESSION_MTP_ENABLED) != 0;
    if mtp && bonsai_cuda_status(bonsai_load_image(dispatch, model + 8, mtp_path, draft_image), "Bonsai MTP CUDA image") != 0 {
        return -1;
    }
    let lookup = unsafe_c_fnptr<BonsaiCudaLookup>(vocab_word(dispatch + 72));
    let pipeline = vocab_word(model + 4680);
    let frozen_module = vocab_word(model + 4688);
    let mtp_module = vocab_word(model + 8);
    let limit = if mtp { 100 } else { 79 };
    var entry = bonsai_pipeline_kernel_names();
    for index in limit {
        if index == 79 { entry = bonsai_mtp_kernel_names(); }
        let name = if index == 2 { vocab_word(model + 4696) } else { entry };
        let is_frozen = vocab_byte(name) == 103 && vocab_byte(name + 1) == 111 && vocab_byte(name + 2) == 111 &&
            vocab_byte(name + 3) == 115 && vocab_byte(name + 4) == 101 && vocab_byte(name + 5) == 95;
        let module = if index >= 79 { mtp_module } else { if is_frozen { frozen_module } else { pipeline } };
        let result = lookup(model + BONSAI_KERNELS + (index as! u64) * 8, module, name);
        if result != 0 {
            fg_bonsai_cuda_check(result, name);
            return -1;
        }
        entry += (vocab_strlen(entry) as! u64) + 1;
    }
    let pv = bonsai_image_flag("BONSAI_FLASH_FP8_PV\0", false) && bonsai_dynamic_shared(dispatch, model, 67, 41472) == 0;
    batch_set(model + 4816, if pv { 1 } else { 0 });
    let qk = bonsai_image_flag("BONSAI_FLASH_FP8_QK\0", false) && bonsai_dynamic_shared(dispatch, model, 63, 41472) == 0;
    batch_set(model + 4808, if qk { 1 } else { 0 });
    let fused = bonsai_image_flag("BONSAI_FLASH_FUSED_ORDERED\0", true) && bonsai_dynamic_shared(dispatch, model, 56, 50176) == 0;
    batch_set(model + 4792, if fused { 1 } else { 0 });
    if batch_u32(model + 4784) != 0 {
        let panel = bonsai_dynamic_shared(dispatch, model, 53, 50176) == 0;
        batch_set(model + 4784, if panel { 1 } else { 0 });
        if panel { vocab_log("Bonsai Flash: shared K/V query panels\n"); }
    }
    if batch_u32(model + 14688) != 0 &&
        bonsai_cuda_status(bonsai_dynamic_shared(dispatch, model, 32, 38016), "GDN V32 shared memory") != 0 { return -1; }
    return 0;
}
