// Full-file mechanical migration draft. Kept out of all build targets.
// Function order is retained; simple conditions, loops and steps use Goose syntax.
// Pending language lowering: native layouts, raw pointers, atomics, TLS,
// platform branches, initializers and preprocessing. CUDA call sites select
// either the generated driver imports or the graph-capture dispatcher.
// This file is not a replacement for the production driver yet.

const SIZE_MAX: u64 = 0xffffffffffffffff;
const INT_MAX: i32 = 2147483647;
const INT64_MAX: i64 = 9223372036854775807;
const UINT32_MAX: u32 = 0xffffffff;
const UINT64_MAX: u64 = 0xffffffffffffffff;

const CUDA_HOST_CONTEXT_LOCK: i32 = 0;
const CUDA_HOST_GRAPH_LOCK: i32 = 1;
var cuda_local_fg_cuda_expert_stage_enabled_small_enabled_3445: i32 = -1;
var cuda_local_submit_expert_stage_range_reported_3670: bool;
var cuda_local_i2_lfu_phase_enabled_3799: i32 = -1;
var cuda_local_i2_packed_q8_enabled_4709: i32 = -1;
var cuda_local_i2_packed_q8_expand_reported_4763: bool;
var cuda_local_i2_packed_q8_extra_reported_4768: u32;
var cuda_local_i2_packed_q8_wide_reported_4773: bool;
var cuda_local_i2_packed_q8_reported_4778: bool;
var cuda_local_i2_matrix_enabled_enabled_4788: i32 = -1;
var cuda_local_i2_launch_matrix_reported_4803: u32;
var cuda_local_i2_launch_matrix_reported_verify_4841: bool;
var cuda_local_launch_matrix_dense_mmvq_logged_4888: bool;
var cuda_local_i2_warm_phase_enabled_5272: i32 = -1;
var cuda_local_fg_cuda_inkling_expert_phase_enabled_5376: i32 = -1;
var cuda_local_fg_cuda_inkling_expert_phase_trace_enabled_5378: i32 = -1;
var cuda_local_fg_cuda_routed_graph_defer_prior_wave_5763: i32 = -1;
var cuda_local_fg_cuda_routed_graph_fused_tail_logged_5935: bool;
var cuda_local_fg_cuda_routed_graph_reported_5973: bool;
var cuda_local_fg_cuda_routed_graph_reported_6001: bool;
var cuda_local_fg_cuda_routed_graph_reported_6056: bool;
var cuda_local_fg_cuda_routed_graph_reported_6097: bool;
var cuda_local_launch_flash_vec_paired_logged_6295: bool;
var cuda_local_launch_flash_vec_quad_logged_6301: bool;
var cuda_local_launch_flash_vec_key_bound_logged_6308: bool;
var cuda_local_fg_cuda_vector_rms_decode_logged_6757: bool;

var cuda_saved_inkling_arena_depthmix_module: u64;
var cuda_saved_inkling_arena_depthmix_prepare: u64;
var cuda_saved_inkling_arena_depthmix_score: u64;
var cuda_saved_inkling_arena_depthmix_mix: u64;
var cuda_saved_inkling_prelude_use_module: u64;
var cuda_saved_inkling_prelude_use_prepare: u64;
var cuda_saved_inkling_loop_save_module: u64;
var cuda_saved_inkling_loop_save_prepare: u64;
var cuda_saved_inkling_loop_materialize_module: u64;
var cuda_saved_inkling_loop_materialize_score: u64;
var cuda_saved_inkling_loop_materialize_mix: u64;
var cuda_saved_launch_grouped_iq4_mmq_module: u64;
var cuda_saved_launch_grouped_iq4_mmq_pack: u64;
var cuda_saved_launch_grouped_iq4_mmq_multiply: u64[6];
var cuda_saved_i2_packed_q8_module: u64;
var cuda_saved_i2_packed_q8_pack: u64;
var cuda_saved_i2_packed_q8_multiply: u64;
var cuda_saved_i2_packed_q8_narrow4: u64;
var cuda_saved_i2_packed_q8_wide2: u64;
var cuda_saved_i2_packed_q8_wide4: u64;
var cuda_saved_i2_packed_q8_expand: u64;
var cuda_saved_i2_packed_q8_fp32w2: u64;
var cuda_saved_i2_packed_q8_fp32w4: u64;
var cuda_saved_i2_launch_matrix_module: u64;
var cuda_saved_i2_launch_matrix_kernels: u64[4];
var cuda_saved_giga_dense_matrix_module: u64;
var cuda_saved_giga_dense_matrix_kernel: u64;
var cuda_saved_launch_matrix_zero_job: device_buffer;

extern fn fg_cuda_host_once_lfu();
extern fn fg_cuda_host_once_prior();
extern fn fg_cuda_host_once_buckets();
extern fn fg_cuda_host_lock(which: i32);
extern fn fg_cuda_host_unlock(which: i32);
extern fn fg_cuda_host_ensure_initialized();
extern fn fg_cuda_host_register_cleanup();

extern fn fg_cuda_host_view_i64(address: u64, length: i64) -> i64[:];
extern fn fg_cuda_host_view_u64(address: u64, length: i64) -> u64[:];
extern fn fg_cuda_host_view_u8(address: u64, length: i64) -> u8[:];

extern "fg_cuda_driver_slice_f32" fn cuda_slice_address(value: const f32[:]) -> u64;
extern "fg_cuda_driver_slice_i32" fn cuda_slice_address(value: const i32[:]) -> u64;
extern "fg_cuda_driver_slice_i64" fn cuda_slice_address(value: const i64[:]) -> u64;
extern "fg_cuda_driver_slice_u32" fn cuda_slice_address(value: const u32[:]) -> u64;
extern "fg_cuda_driver_slice_u64" fn cuda_slice_address(value: const u64[:]) -> u64;
extern "fg_cuda_driver_slice_u8" fn cuda_slice_address(value: const u8[:]) -> u64;

extern fn fg_cuda_host_view_i32(address: u64, length: i64) -> i32[:];
extern fn fg_cuda_host_view_f32(address: u64, length: i64) -> f32[:];
extern fn fg_cuda_host_malloc(bytes: u64) -> u64;
extern fn fg_cuda_host_calloc(count: u64, bytes: u64) -> u64;
extern fn fg_cuda_host_realloc(address: u64, bytes: u64) -> u64;
extern fn fg_cuda_host_free(address: u64);
extern fn fg_cuda_host_memcpy(dst: u64, src: u64, bytes: u64) -> u64;
extern fn fg_cuda_host_memmove(dst: u64, src: u64, bytes: u64) -> u64;
extern fn fg_cuda_host_memset(dst: u64, value: i32, bytes: u64) -> u64;

extern fn fg_cuda_host_monotonic_ns() -> u64;
extern fn fg_cuda_host_env_u32(address: u64, default_value: u32) -> u32;
extern fn fg_cuda_host_store_release_u32(address: u64, value: u32);

extern fn fg_cuda_host_page_size() -> i64;
extern fn fg_cuda_host_physical_pages(page: i64) -> i64;
extern fn fg_cuda_host_verify_mailbox() -> bool;
extern fn fg_cuda_host_load_acquire_u32(address: u64) -> u32;
extern fn fg_cuda_host_cpu_pause();

extern fn fg_cuda_host_positive_env(address: u64) -> i64;
extern fn fg_cuda_host_env_atoi(address: u64, default_value: i32) -> i32;
extern fn fg_cuda_host_getenv(name: u64) -> u64;
extern fn fg_cuda_host_option(name: u64) -> bool;
extern fn fg_cuda_host_env_equal(value: u64, expected: u64) -> bool;

fn cuda_env(name: const u8[:]) -> u64 {
    if name.len >= 96 { return 0; }
    var key: u8[96];
    for i in name.len { key[i] = name[i]; }
    key[name.len] = 0;
    return fg_cuda_host_getenv(fg_cuda_driver_bytes_address(key));
}

fn cuda_option(name: const u8[:]) -> bool {
    if name.len >= 96 { return false; }
    var key: u8[96];
    for i in name.len { key[i] = name[i]; }
    key[name.len] = 0;
    return fg_cuda_host_option(fg_cuda_driver_bytes_address(key));
}

fn cuda_env_equal(value: u64, expected: const u8[:]) -> bool {
    if value == 0 || expected.len >= 96 { return false; }
    var key: u8[96];
    for i in expected.len { key[i] = expected[i]; }
    key[expected.len] = 0;
    return fg_cuda_host_env_equal(value, fg_cuda_driver_bytes_address(key));
}

// CUDA reads the argument bytes from their original typed locations.
extern "fg_cuda_driver_arg_u8" fn cuda_kernel_argument_address(value: const u8&) -> u64;
extern "fg_cuda_driver_arg_i8" fn cuda_kernel_argument_address(value: const i8&) -> u64;
extern "fg_cuda_driver_arg_u16" fn cuda_kernel_argument_address(value: const u16&) -> u64;
extern "fg_cuda_driver_arg_i16" fn cuda_kernel_argument_address(value: const i16&) -> u64;
extern "fg_cuda_driver_arg_u32" fn cuda_kernel_argument_address(value: const u32&) -> u64;
extern "fg_cuda_driver_arg_i32" fn cuda_kernel_argument_address(value: const i32&) -> u64;
extern "fg_cuda_driver_arg_u64" fn cuda_kernel_argument_address(value: const u64&) -> u64;
extern "fg_cuda_driver_arg_i64" fn cuda_kernel_argument_address(value: const i64&) -> u64;
extern "fg_cuda_driver_arg_f32" fn cuda_kernel_argument_address(value: const f32&) -> u64;
extern "fg_cuda_driver_arg_f64" fn cuda_kernel_argument_address(value: const f64&) -> u64;
extern "fg_cuda_driver_arg_bool" fn cuda_kernel_argument_address(value: const bool&) -> u64;

fn cuda_lookup_kernel(output: u64, module: u64, name: const u8[:]) -> i32 {
    if name.len >= 96 { return 1; }
    var symbol: u8[96];
    for index in name.len { symbol[index] = name[index]; }
    symbol[name.len] = 0;
    return fg_cuda_driver_function_get_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(output), unsafe_c_ptr<c_ptr<void>>(module), unsafe_c_ptr<c_ptr<const i8>>(unsafe_c_address<c_ptr<const void>>(symbol)));
}

extern fn fg_cuda_host_diagnostic_108(arg0: i64, arg1: i64) -> i32;
extern fn fg_cuda_host_diagnostic_109(arg0: i64, arg1: i64) -> i32;
extern fn fg_cuda_host_diagnostic_110(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_111(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_112(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_113(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_114() -> i32;
extern fn fg_cuda_host_diagnostic_0(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_1() -> i32;
extern fn fg_cuda_host_diagnostic_2() -> i32;
extern fn fg_cuda_host_diagnostic_3() -> i32;
extern fn fg_cuda_host_diagnostic_4() -> i32;
extern fn fg_cuda_host_diagnostic_5() -> i32;
extern fn fg_cuda_host_diagnostic_6() -> i32;
extern fn fg_cuda_host_diagnostic_7() -> i32;
extern fn fg_cuda_host_diagnostic_8(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_9(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_10(arg0: u64, arg1: u64, arg2: u64) -> i32;
extern fn fg_cuda_host_diagnostic_11(arg0: u64, arg1: u64) -> i32;
extern fn fg_cuda_host_diagnostic_12(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_13(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_14(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_15(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_16(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_17(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_18(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_19(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_20(arg0: u64, arg1: u64) -> i32;
extern fn fg_cuda_host_diagnostic_21(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_22(arg0: u64, arg1: u64, arg2: u64) -> i32;
extern fn fg_cuda_host_diagnostic_23(arg0: u64, arg1: u64) -> i32;
extern fn fg_cuda_host_diagnostic_24(arg0: i32, arg1: u64, arg2: u64, arg3: u32, arg4: u32) -> i32;
extern fn fg_cuda_host_diagnostic_25(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_26(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_27(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_28() -> i32;
extern fn fg_cuda_host_diagnostic_29(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_30() -> i32;
extern fn fg_cuda_host_diagnostic_31() -> i32;
extern fn fg_cuda_host_diagnostic_32(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_33(arg0: u64, arg1: u64, arg2: u32) -> i32;
extern fn fg_cuda_host_diagnostic_34() -> i32;
extern fn fg_cuda_host_diagnostic_35() -> i32;
extern fn fg_cuda_host_diagnostic_36(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_37() -> i32;
extern fn fg_cuda_host_diagnostic_38(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_39(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_40(arg0: i64, arg1: i64) -> i32;
extern fn fg_cuda_host_diagnostic_41(arg0: u64, arg1: u64, arg2: u64, arg3: u64, arg4: u64) -> i32;
extern fn fg_cuda_host_diagnostic_42(arg0: u64, arg1: u64) -> i32;
extern fn fg_cuda_host_diagnostic_43(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_44(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_45() -> i32;
extern fn fg_cuda_host_diagnostic_46(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_47(arg0: i64) -> i32;
extern fn fg_cuda_host_diagnostic_48(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_49(arg0: i32, arg1: i32, arg2: i32, arg3: u64, arg4: u64) -> i32;
extern fn fg_cuda_host_diagnostic_50() -> i32;
extern fn fg_cuda_host_diagnostic_51(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_52(arg0: i32, arg1: u64) -> i32;
extern fn fg_cuda_host_diagnostic_53(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_54(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_55() -> i32;
extern fn fg_cuda_host_diagnostic_56(arg0: i32, arg1: i32, arg2: i32, arg3: i32) -> i32;
extern fn fg_cuda_host_diagnostic_57() -> i32;
extern fn fg_cuda_host_diagnostic_58() -> i32;
extern fn fg_cuda_host_diagnostic_59(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_60() -> i32;
extern fn fg_cuda_host_diagnostic_61(arg0: i32, arg1: i32) -> i32;
extern fn fg_cuda_host_diagnostic_62() -> i32;
extern fn fg_cuda_host_diagnostic_63(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_64(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_65(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_66() -> i32;
extern fn fg_cuda_host_diagnostic_67(arg0: u32) -> i32;
extern fn fg_cuda_host_diagnostic_68() -> i32;
extern fn fg_cuda_host_diagnostic_69(arg0: u64, arg1: u64, arg2: u64, arg3: u64, arg4: u64, arg5: u64, arg6: u64, arg7: u64) -> i32;
extern fn fg_cuda_host_diagnostic_70() -> i32;
extern fn fg_cuda_host_diagnostic_71(arg0: u64, arg1: u64, arg2: u64) -> i32;
extern fn fg_cuda_host_diagnostic_72(arg0: u64, arg1: u64) -> i32;
extern fn fg_cuda_host_diagnostic_73() -> i32;
extern fn fg_cuda_host_diagnostic_74(arg0: u32, arg1: u32) -> i32;
extern fn fg_cuda_host_diagnostic_75(arg0: u32, arg1: u32) -> i32;
extern fn fg_cuda_host_diagnostic_76(arg0: u32) -> i32;
extern fn fg_cuda_host_diagnostic_77() -> i32;
extern fn fg_cuda_host_diagnostic_78() -> i32;
extern fn fg_cuda_host_diagnostic_79() -> i32;
extern fn fg_cuda_host_diagnostic_80() -> i32;
extern fn fg_cuda_host_diagnostic_81(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_82() -> i32;
extern fn fg_cuda_host_diagnostic_83(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_84() -> i32;
extern fn fg_cuda_host_diagnostic_85(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_86(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_87() -> i32;
extern fn fg_cuda_host_diagnostic_88() -> i32;
extern fn fg_cuda_host_diagnostic_89() -> i32;
extern fn fg_cuda_host_diagnostic_90(arg0: i32, arg1: i32) -> i32;
extern fn fg_cuda_host_diagnostic_91(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_92() -> i32;
extern fn fg_cuda_host_diagnostic_93(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_94() -> i32;
extern fn fg_cuda_host_diagnostic_95(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_96(arg0: i64) -> i32;
extern fn fg_cuda_host_diagnostic_97() -> i32;
extern fn fg_cuda_host_diagnostic_98(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_99(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_100(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_101(arg0: i64, arg1: i32) -> i32;
extern fn fg_cuda_host_diagnostic_102() -> i32;
extern fn fg_cuda_host_diagnostic_103(arg0: i64, arg1: i32) -> i32;
extern fn fg_cuda_host_diagnostic_104() -> i32;
extern fn fg_cuda_host_diagnostic_105(arg0: i32) -> i32;
extern fn fg_cuda_host_diagnostic_106(arg0: u64) -> i32;
extern fn fg_cuda_host_diagnostic_107(arg0: i32) -> i32;

extern "fg_cuda_driver_arg_kernel_node_params" fn cuda_kernel_argument_address(value: const kernel_node_params&) -> u64;
extern "fg_cuda_driver_arg_memory_properties" fn cuda_kernel_argument_address(value: const memory_properties&) -> u64;
extern "fg_cuda_driver_arg_memory_access" fn cuda_kernel_argument_address(value: const memory_access&) -> u64;
extern "fg_cuda_driver_arg_memcpy_batch_attributes" fn cuda_kernel_argument_address(value: const memcpy_batch_attributes&) -> u64;
extern "fg_cuda_driver_arg_graph_update_result" fn cuda_kernel_argument_address(value: const graph_update_result&) -> u64;

extern fn fg_cuda_image_goose_cuda_image() -> u64;
extern fn fg_cuda_image_goose_cuda_mmq_image() -> u64;
extern fn fg_cuda_image_flash_interior_image() -> u64;
extern fn fg_cuda_image_rms_onevec_image() -> u64;
extern fn fg_cuda_image_fwht_fixed_image() -> u64;
extern fn fg_cuda_image_moe_epilogue_image() -> u64;
extern fn fg_cuda_image_attention_key_bound_image() -> u64;
extern fn fg_cuda_image_attention_first_row_image() -> u64;
extern fn fg_cuda_image_attention_unroll_image() -> u64;
extern fn fg_cuda_image_fused_up_gate_image() -> u64;
extern fn fg_cuda_image_fused_shared_image() -> u64;
extern fn fg_cuda_image_compact_route_image() -> u64;
extern fn fg_cuda_image_route_mailbox_image() -> u64;
extern fn fg_cuda_image_router_softmax_image() -> u64;
extern fn fg_cuda_image_i2_lazy_prelude_image() -> u64;
extern fn fg_cuda_image_giga_dense_image() -> u64;
extern fn fg_cuda_image_pred_image() -> u64;
extern fn fg_cuda_image_giga_vector_image() -> u64;

// Shared CPU and graph operations from the universal Goose unit.
extern fn fg143_activation_download_all(ranges: i64[:], base: u64) -> i32;
extern fn fg143_activation_output_plan(ranges: const i64[:], capacity: i64, bytes: i64, used: i64, limit: i64, reuse: bool, selective: bool, plan: i64[:]);
extern fn fg143_activation_pending(ranges: const i64[:], host: u64, bytes: i64) -> bool;
extern fn fg143_activation_prepare_records(ranges: i64[:], large: i64[:], host: u64, bytes: i64, step: i64, overwrite: bool, counters: u64[:]) -> bool;
extern fn fg143_activation_publish_record(ranges: i64[:], count: i64, host: u64, bytes: i64, offset: i64, deadline: i64, reuse: bool) -> i64;
extern fn fg143_activation_sync(ranges: i64[:], base: u64, address: u64, bytes: i64, device: bool, invalidate: bool) -> i32;
extern fn fg143_expert_balance_apex(groups: i64[:], bytes: i64, layer_words: i64) -> bool;
extern fn fg143_expert_down_split(jobs: const u8[:], tensor: u64, plane_bytes: u64, experts: u64) -> i64;
extern fn fg143_expert_pageable_run(jobs: const u8[:], capacity: u64, total: u64[:]) -> i64;
extern fn fg143_expert_pinned_batch(jobs: const u8[:]) -> bool;
extern fn fg143_expert_prefill_plan(groups: const i64[:], bank_bytes: u64, planes: u64, budget_mib: u64, plan: u64[:]) -> bool;
extern fn fg143_expert_source_pinned(spans: const i64[:], source: u64, bytes: u64) -> bool;
extern fn fg143_flash_stream_workers(queries: i64, groups: i64, keys: i64, sms: i64) -> i64;
extern fn fg143_flash_vector_parts(keys: i64) -> i64;
extern fn fg143_mmq_dense_window(tiles: i64, window: i64, group: i64) -> i64;
extern fn fg143_mmq_linear_jobs(jobs: const i64[:], stride: i64, bytes: i64) -> bool;
extern fn fg143_mmq_stream_workers(rows: i64, tiles: i64, extent: i64, sms: i64) -> i64;
extern fn fg143_mmq_tiles(jobs: const i64[:], words: i64, tile_width: i64, tiles: i64[:]) -> i64;
extern fn fg143_mmq_wide_first(tiles: i64[:]) -> i64;
extern fn fg143_mmq_window(extent: i64, tiles: i64, tile_width: i64, budget: i64) -> i64;
extern fn fg143_pinned_packet_slot(used: i64, capacity: i64, bytes: i64) -> i64;
extern fn fg143_routed_layer_matches(layers: const i64[:], plan: const i64[:]) -> bool;
extern fn fg143_vmm_capacity() -> i64;
extern fn fg143_vmm_prepare(bytes: u64, mapped: u64, limit: u64, granularity: u64, failed: bool, has_address: bool, plan: i64[:]) -> bool;


extern fn dw_cuda_dense_offset(ranges: i64[:], address: u64, bytes: i64) -> i64;
extern fn qr_state_offset(ranges: i64[:], address: u64, bytes: i64, kind: i64) -> i64;
extern fn qr_state_index(ranges: i64[:], address: u64, bytes: i64, kind: i64) -> i64;
extern fn fg143_activation_lookup(ranges: i64[:], host: u64, bytes: i64) -> i64;
extern fn fg143_activation_overlap(ranges: i64[:], host: u64, bytes: i64, first: i64) -> i64;
extern fn fg143_activation_discard_overwritten(ranges: i64[:], host: u64, bytes: i64) -> u64;
extern fn fg143_activation_expire(ranges: i64[:], step: i64) -> u64;
extern fn fg143_activation_slot(ranges: i64[:], capacity: i64, bytes: i64) -> i64;
extern fn fg143_activation_record_slot(ranges: i64[:]) -> i64;
extern fn fg143_activation_device_overlap(ranges: i64[:], offset: i64, bytes: i64, first: i64) -> i64;
extern fn fg143_activation_eviction_slot(ranges: i64[:], capacity: i64, bytes: i64) -> i64;
extern fn fg143_activation_last_input(ranges: i64[:], host: u64, bytes: i64, step: i64, window: i64, output_bytes: i64) -> i64;
extern fn fg_standalone_option_enabled(name: c_ptr<const i8>) -> bool;
extern fn fg143_cpu_prefill() -> u8;
extern fn ec_reset(groups: i64[:], layers: i64[:], words32: u64, words64: i64[:]);
extern fn ec_prepare(groups: i64[:], layers: i64[:], words32: u64, words64: i64[:], tensor: u64, bytes: i64, heads: i64[:], result: i64[:], stage_all: u8) -> i64;
extern fn inkling_ec_prepare(groups: i64[:], layers: i64[:], words32: u64, words64: i64[:], tensor: u64, bytes: i64, heads: i64[:], result: i64[:], stage_all: u8) -> i64;
extern fn ec_prepare_compact(groups: i64[:], layers: i64[:], words32: u64, words64: i64[:], tensor: u64, bytes: i64, ids: i64[:], result: i64[:], stage_all: u8) -> i64;
extern fn exp1744_init(argc: i32, argv: c_ptr<c_ptr<i8>>);
extern fn exp1744_prepare(groups: i64[:], layers: i64[:], words32: u64, words64: i64[:], tensor: u64, bytes: i64, ids: i64[:], result: i64[:], stage_all: u8, compact: u8, policy: i64) -> i64;
extern fn ec_manage_pins(spans: i64[:], acquire: i64) -> i64;
extern fn fg143_storage_ram_available() -> u64;

type Context = u64;
type Module = u64;
type Function = u64;
type Stream = u64;
type Event = u64;
type Graph = u64;
type GraphExec = u64;
type GraphNode = u64;
type DevicePtr = u64;
type AllocationHandle = u64;
// CUDA_KERNEL_NODE_PARAMS_v2 layout for the optional Graph Driver API.
struct kernel_node_params {
    function: u64 = 0,
    grid_x: u32 = 0, grid_y: u32 = 0, grid_z: u32 = 0,
    block_x: u32 = 0, block_y: u32 = 0, block_z: u32 = 0, shared_bytes: u32 = 0,
    padding: u32 = 0,
    params: u64 = 0, extra: u64 = 0,
    kernel: u64 = 0,
    context: u64 = 0,
}
// Expected CUDA_KERNEL_NODE_PARAMS_v2 size: 72 bytes on x86-64.

// Native CUDA descriptor field order is shared by Linux and Windows x86-64.
struct memory_location { kind: i32, device: i32 }
struct memcpy_batch_attributes {
    source_access_order: i32,
    source_location: memory_location, destination_location: memory_location,
    flags: u32,
}
struct memory_properties {
    kind: i32,
    handle_types: i32,
    location: memory_location,
    platform_metadata: u64,
    flags: u64,
}
struct memory_access { location: memory_location, flags: i32 }

struct vmm_mapping {
    previous: u64,
    handle: u64,
    offset: u64, bytes: u64,
    mapped: bool,
}

const MIB_BYTES: i64 = 1024 * 1024;
const DEFAULT_ACTIVATION_BYTES: i64 = 64 * MIB_BYTES;
const WIDE_ACTIVATION_BYTES: i64 = 384 * MIB_BYTES;
const EXPERT_HOST_SLOTS: i64 = 3;
const EXPERT_HOST_SLOT_BYTES: i64 = 4 * MIB_BYTES;
// Release events kept for expert waves, so a transfer may run while an earlier
// layer still computes instead of waiting for the immediately preceding one.
const EXPERT_SLOT_EVENTS: i64 = 4;
const MMVQ_GRAPH_SLOTS: i64 = 1024;
const MMVQ_GRAPH_KEY_WORDS: i64 = 16;

const MMVQ_GRAPH_WORDS: i64 = MMVQ_GRAPH_KEY_WORDS + 1;
const DECODE_GRAPH_REGIONS: i64 = 256;
const REPLAY_OPS: i64 = 128;
const REPLAY_ARGS: i64 = 32;
const REPLAY_BYTES: i64 = 64 * 1024;
const replay_kernel: i32 = 0;
const replay_h2d: i32 = 1;
const replay_d2d: i32 = 2;
const replay_wait: i32 = 3;
const replay_wait_planes: i32 = 4;

fn replay_is_wait(kind: i32) -> bool {
    return kind == replay_wait || kind == replay_wait_planes;
}

struct replay_layout {
    function: u64,
    count: u64, bytes: u64,
    offset: u64[REPLAY_ARGS], size: u64[REPLAY_ARGS],
}

struct ReplayKernelOperation {
    layout: i64 = -1,
    shape: u32[7],
    start: u64,
}

struct ReplayCopyOperation {
    dst: u64,
    src: u64,
    bytes: u64,
}

// The two explicit payloads replace the native union.
const REPLAY_RECORD_BYTES: u64 = 80;
struct replay_operation {
    kind: i32,
    kernel: ReplayKernelOperation,
    copy: ReplayCopyOperation,
}

struct replay_region {
    graph: u64,
    exec: u64,
    nodes: u64[REPLAY_OPS],
    count: u64,
    ops: replay_operation[:],
    args: u8[:],
}

extern fn fg_cuda_host_view_replay_ops(address: u64, length: i64) -> replay_operation[:];
extern "fg_cuda_driver_replay_slice" fn replay_ops_address(ops: const replay_operation[:]) -> u64;

struct graph_update_result {
    result: i32,
    node: u64, from: u64,
}

struct device_buffer {
    address: u64,
    bytes: u64,
    mappings: u64,
    reservation_bytes: u64,
    vmm_failed: bool,
    alignment_padding: u8[7],
}

struct expert_cache {
    weights: device_buffer,
    bank_bytes: u64,
    prefill_offset: u64,
    prefill_bytes: u64,
    prefill_workspace: u64,
    prefill_release: u64,
    prefill_ready: u64,
    prefill_full_buffer: u64,
    prefill_skipped_capacity: u64,
    prefill_next_tensor: u64,
    prefill_next_fills: u64,
    prefill_next_bytes: i64,
    prefill_next_count: i64,
    prefill_next_active: u64[64],
    prefill_prefetch_used: u64,
    prefill_prefetch_discarded: u64,
    prefill_projections: u64,
    prefill_hits: u64,
    prefill_upload_bytes: u64,
    groups: i64[:],
    layers: i64[:],
    work: i64[:],
    state: u64,
    hits: u64,
    fills: u64,
    evictions: u64,
    uploaded_bytes: u64,
    prefill_next_slot: u32,
    prefill_double: bool,
    prefill_compact: bool,
    prefill_wait_decode: bool,
    decode_seen: bool,
    one_touch: bool,
    alignment_padding: u8[7],
}

struct expert_upload_job {
    destination: u64,
    source: u64,
    bytes: u64,
    pageable: bool,
    alignment_padding: u8[7],
}

// Native expert upload records use a 32-byte stride, with fields at 0, 8, 16 and 24.

extern fn fg_cuda_host_view_expert_jobs(address: u64, length: i64) -> expert_upload_job[:];
extern "fg_cuda_driver_expert_slice" fn expert_jobs_address(jobs: const expert_upload_job[:]) -> u64;

fn expert_job_bytes(jobs: const expert_upload_job[:], count: u64) -> u8[:] {
    return fg_cuda_host_view_u8(expert_jobs_address(jobs), (count * 32) as! i64);
}

fn expert_jobs_window(jobs: const expert_upload_job[:], first: u64, length: u64) -> expert_upload_job[:] {
    return fg_cuda_host_view_expert_jobs(expert_jobs_address(jobs) + 32 * first, length as! i64);
}

struct expert_host_ring {
    storage: u64,
    ready: u64[EXPERT_HOST_SLOTS],
    index: u64,
    used: u64,
    busy: bool[EXPERT_HOST_SLOTS],
    enabled: bool,
    alignment_padding: u8[4],
}

struct expert_copy_task {
    jobs: expert_upload_job[:],
    count: u64,
    destination: u64,
}

struct CudaExpertCopyWorker {
    task: expert_copy_task,
    running: bool,
    stop: bool,
}

var expert_copy_worker: CudaExpertCopyWorker;

extern fn fg_cuda_host_worker_lock();
extern fn fg_cuda_host_worker_unlock();
extern fn fg_cuda_host_worker_wait_ready();
extern fn fg_cuda_host_worker_wait_done();
extern fn fg_cuda_host_worker_signal_ready();
extern fn fg_cuda_host_worker_signal_done();
extern fn fg_cuda_host_worker_start() -> bool;
extern fn fg_cuda_host_worker_join();

fn copy_expert_payloads(task: expert_copy_task) {
    var destination = task.destination;
    for i in (task.count as! i64) {
        let job = task.jobs[i];
        fg_cuda_host_memcpy(destination, job.source, job.bytes);
        destination += job.bytes;
    }
}

export fn fg_cuda_host_copy_worker_callback() {
    fg_cuda_host_worker_lock();
    while !expert_copy_worker.stop {
        if expert_copy_worker.task.count == 0 {
            fg_cuda_host_worker_wait_ready();
            continue;
        }
        let task: expert_copy_task = expert_copy_worker.task;
        fg_cuda_host_worker_unlock();
        copy_expert_payloads(task);
        fg_cuda_host_worker_lock();
        expert_copy_worker.task.count = 0;
        fg_cuda_host_worker_signal_done();
    }
    fg_cuda_host_worker_unlock();
}

struct CudaState_cuda {
    library: u64,
    context: u64,
    module: u64,
    kernel: u64,
    attention_tiles: u64,
    attention_merge: u64,
    gdn: u64,
    gdn_decode: u64,
    q4_rows: u64,
    vector: u64,
    vector_flat: u64,
    vector_flat4: u64,
    rms_reference: u64,
    rms_reference_one: u64,
    rms_reference_one_warp4: u64,
    mmq_pack: u64,
    mmq_pack_shared: u64,
    mmq_pack_glu: u64,
    mmq_matrix: u64,
    mmq_q4_matrix: u64,
    mmq_q6_matrix: u64,
    mmq_pack_k_major: u64,
    mmq_q6_dense_half_k: u64,
    mmq_q6_dense_pipeline: u64,
    mmq_q4_dense_shared: u64,
    mmq_q4_routed_shared: u64,
    mmq_q4_dense_x64: u64,
    mmq_q4_routed_x64: u64,
    mmq_q6_dense_shared: u64,
    mmq_q6_routed_shared: u64,
    mmq_q6_dense_x64: u64,
    mmq_q6_routed_x64: u64,
    mmq_q4_dense_x128: u64,
    mmq_q6_dense_x128: u64,
    mmq_q4_dense_m64: u64,
    mmq_q4_dense_m128: u64,
    mmq_q4_dense_large: u64,
    mmq_q4_prepare: u64,
    mmq_q4_dense_prepared: u64,
    mmq_q4_dense_pipeline: u64,
    mmq_q4_routed_x128: u64,
    mmq_q6_routed_x128: u64,
    mmq_q6_routed_pipeline: u64,
    mmq_q4_routed_m128: u64,
    mmq_q4_routed_down_m128: u64,
    mmvq_pack: u64,
    mmvq: u64,
    mmvq_short: u64,
    mmvq_dense_pack: u64,
    mmvq_dense_row: u64,
    dense_mma: u64,
    dense_mma_reference: u64,
    gdn_mma: u64,
    gdn_mma_v32: u64,
    flash_mma: u64,
    flash_stage: u64,
    flash_vec: u64,
    flash_vec_merge: u64,
    flash_vec_pack: u64,
    flash_vec_packed: u64,
    flash_vec_paired: u64,
    flash_vec_quad: u64,
    flash_vec_quad_key_bound: u64,
    mmvq_up_gate_glu: u64,
    mmvq_shared_up_gate_glu: u64,
    moe_add_shared_residual: u64,
    rms_decode_2048: u64,
    route_compact8: u64,
    route_softmax_fixed: u64,
    route_weights: device_buffer,
    route_mailbox_kernel: u64,
    route_mailbox_host: u64,
    route_mailbox_device: u64,
    mmq_stream: u64,
    mmq_stream_fixup: u64,
    flash_stream: u64,
    flash_fixup_uniform: u64,
    flash_fixup_general: u64,
    rope_phases: u64,
    rope_apply: u64,
    ssm_conv: u64,
    ssm_conv_decode: u64,
    ssm_conv_long: u64,
    ssm_conv_direct: u64,
    ssm_conv_save_tail: u64,
    rope_decode: u64,
    route_reduce: u64,
    route_sort: u64,
    route_topk: u64,
    route_gather: u64,
    route_norm: u64,
    blit: u64,
    blit_transpose: u64,
    fwht: u64,
    fwht_q4: u64,
    moe_weighted: u64,
    fwht_fixed: u64[2],
    fwht_q4_fixed: u64[2],
    input: device_buffer,
    output: device_buffer,
    weight: device_buffer,
    jobs: device_buffer,
    mmq_packed: device_buffer,
    mmq_tiles: device_buffer,
    mmq_partials: device_buffer,
    attention_value: device_buffer,
    attention_scratch: device_buffer,
    attention_config: device_buffer,
    gdn_beta: device_buffer,
    activations: device_buffer,
    activation_capacity: u64,
    packet_host: u64,
    inkling_prefill_readback: u64,
    dense: device_buffer,
    dense_ranges: i64[:],
    state: device_buffer,
    state_ranges: i64[:],
    state_views: device_buffer[:],
    state_logical_bytes: u64,
    experts: expert_cache,
    expert_stream: u64,
    expert_slots_ready: u64,
    expert_uploads_ready: u64,
    expert_planes_ready: u64,
    expert_slots_ring: u64[EXPERT_SLOT_EVENTS],
    expert_upload_jobs: expert_upload_job[:],
    expert_upload_count: u64,
    expert_upload_capacity: u64,
    expert_upload_split: u64,
    expert_host: expert_host_ring,
    expert_pins: i64[:],
    expert_pinned_bytes: i64,
    moe_prefill_epilogue: u64,
    vmm_granularity: u64,
    vmm_capacity: u64,
    vmm_properties: memory_properties,
    graph_stream: u64,
    flash_graph: u64,
    flash_graph_exec: u64,
    flash_graph_nodes: u64[3],
    flash_graph_node_count: u64,
    mmvq_graph_cache: u64[:],
    decode_regions: u64[DECODE_GRAPH_REGIONS],
    graph_update: u64,
    copy_device_async: u64,
    routed_graph_scratch: device_buffer,
    init: u64,
    device_get: u64,
    device_attribute: u64,
    create: u64,
    destroy: u64,
    push: u64,
    pop: u64,
    module_load: u64,
    function_get: u64,
    function_set_attribute: u64,
    alloc: u64,
    memory_info: u64,
    release: u64,
    vmm_get_granularity: u64,
    vmm_reserve: u64,
    vmm_free: u64,
    vmm_create: u64,
    vmm_map: u64,
    vmm_access: u64,
    vmm_unmap: u64,
    vmm_release: u64,
    stream_create: u64,
    stream_destroy: u64,
    stream_wait_event: u64,
    event_create: u64,
    event_destroy: u64,
    event_record: u64,
    event_sync: u64,
    graph_begin: u64,
    graph_end: u64,
    graph_roots: u64,
    graph_dependents: u64,
    graph_node_type: u64,
    graph_instantiate: u64,
    graph_set_kernel: u64,
    graph_launch: u64,
    graph_destroy: u64,
    graph_exec_destroy: u64,
    function_param: u64,
    function_name: u64,
    host_register: u64,
    host_unregister: u64,
    upload_batch: u64,
    host_alloc: u64,
    host_free: u64,
    upload: u64,
    upload_async: u64,
    download: u64,
    copy_device: u64,
    zero: u64,
    launch: u64,
    sync: u64,
    mmq_q4_large_rows: u32,
    mmq_q4_large_cols: u32,
    route_mailbox_sequence: u32,
    expert_slots_index: u32,
    expert_slots_records: u32,
    status: i32,
    multiprocessors: i32,
    adapt_activation_capacity: bool,
    direct_wide_vectors: bool,
    stage_matrix_input: bool,
    defer_matrix_waits: bool,
    async_packets: bool,
    expert_upload_queued: bool,
    expert_decode_overlap: bool,
    expert_prefill_staging: bool,
    expert_current_staging: bool,
    expert_split_planes: bool,
    expert_planes_split: bool,
    expert_async_copies: bool,
    mmq_window: bool,
    mmq_shared: bool,
    moe_topk: bool,
    activation_reference: bool,
    rms_postscale: bool,
    mmq_align_window: bool,
    mmq_dense_partial: bool,
    mmq_shared_reuse: bool,
    mmq_dense_reuse: bool,
    mmvq_dense_tail: bool,
    scratch_vmm: bool,
    lazy_kv: bool,
    flash_graph_enabled: bool,
    flash_graph_pack: bool,
    flash_stream_all: bool,
    flash_tail_mma: bool,
    mmvq_graph_enabled: bool,
    routed_graph_enabled: bool,
    full_ffn_enabled: bool,
    decode_graph_enabled: bool,
    decode_reuse: bool,
    decode_early_ffn: bool,
    compact_moe_ids: bool,
}
var cuda: CudaState_cuda;

const ACTIVATION_WORDS: i64 = 5;
const ACTIVATION_RECORDS: i64 = 256;
const GDN_MMA_SHARED_BYTES: i64 = 93312;
const GDN_V32_SHARED_BYTES: i64 = 38016;
const Q4_PIPE_SHARED_BYTES: i64 = 2 * 30720;
const Q6_HALF_K_SHARED_BYTES: i64 = 53760;
const Q6_PIPE_SHARED_BYTES: i64 = Q6_HALF_K_SHARED_BYTES + 18432;
const Q6_ROUTED_SHARED_BYTES: i64 = Q6_PIPE_SHARED_BYTES + 128 * 4;
struct CudaState_activations {
    target: u64,
    target_bytes: i64,
    target_deadline: i64,
    step: i64,
    count: i64,
    used: i64,
    ranges: i64[ACTIVATION_RECORDS * ACTIVATION_WORDS],
    large_output: i64[ACTIVATION_WORDS],
    hits: u64,
    skipped_download_bytes: u64,
    expired_bytes: u64,
    shared_pack_reuses: u64,
    dense_pack_reuses: u64,
    flat_vectors: u64,
    flat4_vectors: u64,
    q4_prepared: u64,
    q4_pipelined: u64,
    q6_pipelined: u64,
    q6_routed_pipelined: u64,
    expert_parallel_batches: u64,
    packet_used: u64,
    active: bool,
    unavailable: bool,
    defer_waits: bool,
    pending: bool,
    retire: bool,
    reuse: bool,
    selective_eviction: bool,
    alignment_padding: u8[1],
}

struct CudaState_decode_capture {
    region: u64,
    kernels: u64,
    replays: u64,
    updates: u64,
    maximum: u64,
    reused: u64,
    changed_nodes: u64,
    scope: bool,
    capturing: bool,
    buffering: bool,
    alignment_padding: u8[5],
}
// Thread-local storage must be supplied by the host ABI.

var replay_layouts: replay_layout[128];
var replay_layout_count: u64;
var replay_regions: replay_region[DECODE_GRAPH_REGIONS];
// The activation graph lock serializes buffered decode regions.
struct CudaReplayBuffer {
    count: u64,
    used: u64,
    ops: replay_operation[REPLAY_OPS],
    args: u8[REPLAY_BYTES],
}
var replay_buffer: CudaReplayBuffer;

// Host boundary: return the zero-initialized state for the calling thread.
// The address remains stable until thread exit.
struct CudaThreadState {
    matrix_input: u64,
    matrix_output: u64,
    i2_expert_trace: CudaState_i2_expert_trace,
    i2_lfu: CudaState_i2_lfu,
    i2_prior: CudaState_i2_prior,
    i2_warm: CudaState_i2_warm,
    cuda_local_set_q4_rows_logged_3124: bool,
    cuda_local_i2_lfu_phase_prefill_trained_3805: bool,
    cuda_local_launch_matrix_reported_5008: u32,
    cuda_local_fg_cuda_inkling_expert_phase_begin_bytes_5395: u64,
    cuda_local_fg_cuda_inkling_expert_phase_begin_fills_5395: u64,
    cuda_local_fg_cuda_inkling_expert_phase_measuring_5396: bool,
    cuda_local_fg_cuda_routed_graph_deferred_5772: u32,
    cuda_local_fg_cuda_routed_graph_consumed_6104: u64,
    cuda_local_fg_cuda_routing_launches_7003: u64,
    cuda_local_fg_cuda_fwht_logged_7128: bool,
    cuda_local_fg_cuda_moe_epilogue_fused_7184: u64,
    mailbox_verified: u32, mailbox_forced: u32, mailbox_reads: u32, mailbox_fallbacks: u32,
    activations: CudaState_activations,
    decode_capture: CudaState_decode_capture,
    matrix_glu: CudaState_matrix_glu,
    shared_activation_pack: CudaState_shared_activation_pack,
    route_compact_pending: CudaState_route_compact_pending,
    published_state: CudaState_published_state,
    i2_arena_probe: CudaState_i2_arena_probe,
    i2_depthmix_input: CudaState_i2_depthmix_input,
    i2_device_loop: CudaState_i2_device_loop,
    matrix_reusable_host: u64,
    matrix_glu_active: bool,
    matrix_input_bytes: i64,
    cuda_matrices: u64,
    cuda_routed_groups: u64,
    cuda_routed_replays: u64,
    cuda_full_ffn_groups: u64,
    cuda_full_ffn_replays: u64,
    cuda_matrix_launches: u64,
    cuda_cached_projections: u64,
    cuda_mmq_projections: u64,
    cuda_mmq_stream_windows: u64,
    cuda_mmq_fixups: u64,
    cuda_dense_mma: u64,
    cuda_f32_matrices: u64,
    cuda_matrix_kind: i32,
    cuda_attentions: u64,
    cuda_causal_attentions: u64,
    cuda_gdns: u64,
    cuda_gdn_mmas: u64,
    cuda_flash_mmas: u64,
    cuda_resident_kv: u64,
    cuda_resident_gdn: u64,
    cuda_q4_writes: u64,
    cuda_vectors: u64,
    cuda_fwhts: u64,
    cuda_moe_weighted_tiles: u64,
    cuda_ropes: u64,
    cuda_convs: u64,
    cuda_routes: u64[5],
    cuda_blits: u64[3],
    i2_residual_host: u64,
    i2_residual_device: u64,
    i2_attention_scratch: u64,
    inkling_moe_resident_host: u64,
    inkling_moe_output_slot: u64,
    inkling_moe_current: u64,
    inkling_conv_weights: u64,
    inkling_conv_history: u64,
    inkling_prelude_scales: u64,
    cuda_inkling_quants: bool,
    inkling_decode_overlap: bool,
    inkling_overlap_current_uploads: u64,
    inkling_overlap_groups: u64,
    inkling_overlap_resident: u64,
    inkling_overlap_missing: u64,
    giga_small_expert_wave: bool,
    i2_exact_verify: bool,
}

fn cuda_zero_CudaState_decode_capture() -> CudaState_decode_capture {
    var value: CudaState_decode_capture = default<CudaState_decode_capture>();
    return value;
}

fn cuda_zero_CudaState_i2_depthmix_input() -> CudaState_i2_depthmix_input {
    var value: CudaState_i2_depthmix_input = default<CudaState_i2_depthmix_input>();
    return value;
}

fn cuda_zero_CudaState_i2_device_loop() -> CudaState_i2_device_loop {
    var value: CudaState_i2_device_loop = default<CudaState_i2_device_loop>();
    return value;
}

fn cuda_zero_CudaState_i2_prior() -> CudaState_i2_prior {
    var value: CudaState_i2_prior = default<CudaState_i2_prior>();
    return value;
}

var cuda_thread_layout: CudaThreadState;
extern fn fg_cuda_host_thread_storage(layout: const CudaThreadState&) -> CudaThreadState[:];

fn cuda_thread_state() -> CudaThreadState[:] {
    return fg_cuda_host_thread_storage(cuda_thread_layout);
}

extern fn fg_cuda_has_kernel_arg_counts() -> bool;
extern fn fg_cuda_kernel_arg_count(name: u64) -> i64;

fn replay_layout_for(function: u64) -> i64 {
    if !fg_cuda_has_kernel_arg_counts() { return -1; }
    for i in (replay_layout_count as! i64) {
        if replay_layouts[i].function == function { return i; }
    }
    if replay_layout_count >= 128 { return -1; }
    var name: u64[1] = [0];
    if fg_cuda_driver_function_name_typed(unsafe_c_ptr<c_ptr<c_ptr<const i8>>>(unsafe_c_address<c_ptr<const void>>(name)), unsafe_c_ptr<c_ptr<void>>(function)) != 0 {
        return -1;
    }
    var args: i64;
    if function == cuda.route_softmax_fixed { args = 3; }
    else if function == cuda.route_compact8 { args = 4; }
    else if function == cuda.route_mailbox_kernel { args = 6; }
    else if function == cuda.mmvq_up_gate_glu { args = 7; }
    else if function == cuda.mmvq_shared_up_gate_glu { args = 6; }
    else { args = fg_cuda_kernel_arg_count(name[0]); }
    if args < 0 || args > REPLAY_ARGS { return -1; }

    var layout: replay_layout = default<replay_layout>();
    layout.function = function;
    layout.count = args as! u64;
    for i in args {
        var offset: u64[1] = [0];
        var bytes: u64[1] = [0];
        let status = fg_cuda_driver_function_param_typed(unsafe_c_ptr<c_ptr<void>>(function), i as! u64,
            unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(offset)), unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(bytes)));
        if status != 0 || offset[0] > (REPLAY_BYTES as! u64) || bytes[0] > (REPLAY_BYTES as! u64) - offset[0] {
            return -1;
        }
        layout.offset[i] = offset[0];
        layout.size[i] = bytes[0];
        let end = offset[0] + bytes[0];
        if end > layout.bytes { layout.bytes = end; }
    }
    let index = replay_layout_count;
    replay_layouts[index] = layout;
    replay_layout_count += 1;
    return index as! i64;
}

fn replay_kernel_params(op: const replay_operation&, args: u64[:]) -> kernel_node_params {
    let layout = replay_layouts[op.kernel.layout];
    let storage = fg_cuda_driver_bytes_address(replay_buffer.args);
    for i in (layout.count as! i64) {
        args[i] = storage + op.kernel.start + layout.offset[i];
    }
    let shape = op.kernel.shape;
    return kernel_node_params { function: layout.function, grid_x: shape[0], grid_y: shape[1],
        grid_z: shape[2], block_x: shape[3], block_y: shape[4], block_z: shape[5], shared_bytes: shape[6],
        params: if layout.count != 0 { fg_cuda_driver_words_address(args) } else { 0 },
        extra: 0, kernel: 0, context: 0 };
}

fn pack_replay_commands(commands: u64[:], parameters: u64[:]) {
    let storage = fg_cuda_driver_bytes_address(replay_buffer.args);
    for i in (replay_buffer.count as! i64) {
        let op = replay_buffer.ops[i];
        let at = i * REPLAY_COMMAND_WORDS;
        commands[at] = op.kind as! u64;
        if op.kind == replay_kernel {
            let layout = replay_layouts[op.kernel.layout];
            commands[at + 1] = layout.function;
            for field in 7 { commands[at + field + 2] = op.kernel.shape[field] as! u64; }
            let offset = i * REPLAY_ARGS;
            for arg in (layout.count as! i64) {
                parameters[offset + arg] = storage + op.kernel.start + layout.offset[arg];
            }
            commands[at + 9] = if layout.count == 0 { 0 } else {
                fg_cuda_replay_node_address(unsafe_c_address<c_ptr<void>>(parameters[offset]))
            };
        } else if op.kind == replay_h2d || op.kind == replay_d2d {
            commands[at + 1] = op.copy.dst;
            commands[at + 2] = op.copy.src;
            commands[at + 3] = op.copy.bytes;
        }
    }
}

fn record_buffered_decode() -> i32 {
    var thread_storage = cuda_thread_state();
    var commands: u64[REPLAY_OPS * REPLAY_COMMAND_WORDS];
    var parameters: u64[REPLAY_OPS * REPLAY_ARGS];
    var buffering: i64[1] = [1];
    pack_replay_commands(commands, parameters);
    let active = fg_cuda_host_view_u64(fg_cuda_driver_words_address(commands),
        (replay_buffer.count as! i64) * REPLAY_COMMAND_WORDS);
    let status = fg_cuda_replay_record(active, cuda.graph_stream,
        cuda.expert_uploads_ready, cuda.expert_planes_ready, buffering);
    thread_storage[0].decode_capture.buffering = buffering[0] != 0;
    return status;
}

fn release_replay_region(region: replay_region&) {
    if region.exec != 0 { fg_cuda_driver_graph_exec_destroy(region.exec); }
    if region.graph != 0 { fg_cuda_driver_graph_destroy(region.graph); }
    fg_cuda_host_free(replay_ops_address(region.ops));
    fg_cuda_host_free(cuda_slice_address(region.args));
    region.exec = 0;
    region.graph = 0;
    region.count = 0;
    region.ops = fg_cuda_host_view_replay_ops(0, 0);
    region.args = fg_cuda_host_view_u8(0, 0);
    for i in REPLAY_OPS { region.nodes[i] = 0; }
}

fn clear_replay_regions() {
    if !cuda.decode_reuse { return; }
    for i in 0 .. DECODE_GRAPH_REGIONS { release_replay_region(replay_regions[i]); }
}

fn update_replay_region(region: replay_region&) -> bool {
    var thread_storage = cuda_thread_state();
    if region.exec == 0 || region.count != replay_buffer.count { return false; }
    block {
        var i: u64 = 0;
        while i < region.count {
            let old = region.ops[i];
            let op = replay_buffer.ops[i];
            if old.kind != op.kind { return false; }
            if op.kind == replay_kernel {
                if old.kernel.layout != op.kernel.layout || old.kernel.start != op.kernel.start { return false; }
            } else if op.kind != replay_wait && op.kind != replay_wait_planes &&
                (old.copy.dst != op.copy.dst || old.copy.src != op.copy.src || old.copy.bytes != op.copy.bytes) {
                return false;
            }
            i += 1;
        }
    };
    for i in (region.count as! i64) {
        let old = region.ops[i];
        let op = replay_buffer.ops[i];
        if op.kind != replay_kernel { continue; }
        let start: u64 = op.kernel.start;
        let bytes = replay_layouts[op.kernel.layout].bytes;
        var same = old.kernel.shape == op.kernel.shape;
        if same {
            for offset in (start as! i64) .. ((start + bytes) as! i64) {
                if region.args[offset] != replay_buffer.args[offset] { same = false; break; }
            }
        }
        if same { continue; }
        var args: u64[32];
        let p: kernel_node_params = replay_kernel_params(op, args);
        if fg_cuda_driver_graph_set_kernel_typed(unsafe_c_ptr<c_ptr<void>>(region.exec), unsafe_c_ptr<c_ptr<void>>(region.nodes[i]), unsafe_c_address<c_ptr<const void>>(p)) != 0 { return false; }
        for offset in (start as! i64) .. ((start + bytes) as! i64) {
            region.args[offset] = replay_buffer.args[offset];
        }
        region.ops[i] = op;
        thread_storage[0].decode_capture.changed_nodes += 1;
    }
    return true;
}

fn store_replay_region(region: replay_region&, graph: u64, exec: u64) -> bool {
    // Only reuse captures that match the recorded linear stream command by command.
    var next: u64 = 0;
    var count: u64 = 0;
    if fg_cuda_driver_graph_roots_typed(unsafe_c_ptr<c_ptr<void>>(graph), null, unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(count))) != 0 || count != 1 ||
        fg_cuda_driver_graph_roots_typed(unsafe_c_ptr<c_ptr<void>>(graph), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(next)), unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(count))) != 0 { return false; }
    block {
        var i: u64 = 0;
        while i < replay_buffer.count {
            let kind: i32 = replay_buffer.ops[i].kind;
            let expected: i32 = if kind == replay_kernel { 0 }
                else if replay_is_wait(kind) { 6 } else { 1 };
            var node_type: i32 = 0;
            if fg_cuda_driver_graph_node_type_typed(unsafe_c_ptr<c_ptr<void>>(next), unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(node_type))) != 0 || node_type != expected { return false; }
            region.nodes[i] = next;
            count = 0;
            if fg_cuda_driver_graph_dependents_typed(unsafe_c_ptr<c_ptr<void>>(next), null, unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(count))) != 0 ||
                count != (if i + 1 < replay_buffer.count { 1 } else { 0 }) { return false; }
            if count != 0 && fg_cuda_driver_graph_dependents_typed(unsafe_c_ptr<c_ptr<void>>(next), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(next)), unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(count))) != 0 { return false; }
            i += 1;
        }
    };
    let ops_address = fg_cuda_host_malloc(replay_buffer.count * REPLAY_RECORD_BYTES);
    let args_address = fg_cuda_host_malloc(if replay_buffer.used != 0 { replay_buffer.used } else { 1 });
    if ops_address == 0 || args_address == 0 {
        fg_cuda_host_free(ops_address);
        fg_cuda_host_free(args_address);
        return false;
    }
    let ops = fg_cuda_host_view_replay_ops(ops_address, replay_buffer.count as! i64);
    let args = fg_cuda_host_view_u8(args_address, replay_buffer.used as! i64);
    for i in ops.len { ops[i] = replay_buffer.ops[i]; }
    for i in args.len { args[i] = replay_buffer.args[i]; }
    region.graph = graph;
    region.exec = exec;
    region.count = replay_buffer.count;
    region.ops = ops;
    region.args = args;
    return true;
}

fn submit_buffered_decode(index: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    let cached_index = index < (DECODE_GRAPH_REGIONS as! u64);
    if cached_index {
        var region .= replay_regions[index];
        if update_replay_region(region) {
            thread_storage[0].decode_capture.reused += 1;
            return fg_cuda_driver_graph_launch(region.exec, cuda.graph_stream);
        }
        release_replay_region(region);
    }
    var graph: u64 = 0;
    var exec: u64 = 0;
    var error: i32 = record_buffered_decode();
    if !thread_storage[0].decode_capture.buffering {
        let ended: i32 = fg_cuda_driver_graph_end_typed(unsafe_c_ptr<c_ptr<void>>(cuda.graph_stream), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(graph)));
        if error == 0 { error = ended; }
    }
    if error == 0 { error = fg_cuda_driver_graph_instantiate_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(exec)), unsafe_c_ptr<c_ptr<void>>(graph), 0); }
    if error == 0 { error = fg_cuda_driver_graph_launch(exec, cuda.graph_stream); }
    let cached: bool = error == 0 && cached_index && store_replay_region(replay_regions[index], graph, exec);
    if !cached {
        if exec != 0 { fg_cuda_driver_graph_exec_destroy(exec); }
        if graph != 0 { fg_cuda_driver_graph_destroy(graph); }
    }
    return error;
}

fn decode_capture_submit() -> i32 {
    var thread_storage = cuda_thread_state();
    var state .= thread_storage[0].decode_capture;
    if !state.capturing { return 0; }
    state.capturing = false;
    if state.buffering {
        let index = state.region;
        state.region += 1;
        let error = submit_buffered_decode(index);
        state.buffering = false;
        if error == 0 {
            state.replays += 1;
            if state.kernels > state.maximum { state.maximum = state.kernels; }
        }
        state.kernels = 0;
        return error;
    }

    let cache = state.region < (DECODE_GRAPH_REGIONS as! u64);
    let index = state.region;
    var captured: u64[2];
    if cache { captured[0] = cuda.decode_regions[index]; }

    let error = fg_cuda_replay_finish_capture(cuda.graph_stream, cache, captured);
    if cache && captured[1] != 2 {
        state.region += 1;
        cuda.decode_regions[index] = captured[0];
    }
    if captured[1] == 1 { state.updates += 1; }
    if captured[1] == 2 { return error; }
    if error == 0 {
        state.replays += 1;
        if state.kernels > state.maximum { state.maximum = state.kernels; }
    }
    state.kernels = 0;
    return error;
}

fn decode_capture_discard() -> i32 {
    var thread_storage = cuda_thread_state();
    thread_storage[0].decode_capture.capturing = false;
    thread_storage[0].decode_capture.kernels = 0;
    var graph: u64 = 0;
    let ended: i32 = if thread_storage[0].decode_capture.buffering { 0 } else { fg_cuda_driver_graph_end_typed(unsafe_c_ptr<c_ptr<void>>(cuda.graph_stream), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(graph))) };
    thread_storage[0].decode_capture.buffering = false;
    let destroyed: i32 = if graph != 0 { fg_cuda_driver_graph_destroy(graph) } else { 0 };
    let drained: i32 = fg_cuda_driver_sync();
    return if ended != 0 { ended } else if destroyed != 0 { destroyed } else { drained };
}

fn decode_capture_begin() -> i32 {
    var thread_storage = cuda_thread_state();
    if thread_storage[0].decode_capture.capturing || !thread_storage[0].decode_capture.scope { return 0; }
    const blocking_stream: i64 = 0;
    const thread_local_capture: i64 = 1;
    var error: i32 = if (cuda.graph_stream != 0) { 0 } else { fg_cuda_driver_stream_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.graph_stream)), (blocking_stream as! u32)) };
    if error == 0 && cuda.decode_reuse {
        replay_buffer.count = 0;
        replay_buffer.used = 0;
        thread_storage[0].decode_capture.buffering = true;
    } else if error == 0 {
        error = fg_cuda_driver_graph_begin(cuda.graph_stream, (thread_local_capture as! i32));
    }
    if error == 0 { thread_storage[0].decode_capture.capturing = true; }
    return error;
}

extern fn fg_cuda_driver_load_argument_address(address: u64) -> u64;
extern "fg143_copy_bytes" fn cuda_draft_copy_bytes(dst: u64, src: u64, count: i64);

fn buffer_decode_kernel(layout_index: i64, shape: u32[7], params: u64) -> bool {
    if layout_index < 0 || layout_index >= replay_layouts.len { return false; }
    let layout = replay_layouts[layout_index];
    let start = (replay_buffer.used + 15) & ~(15 as! u64);
    if replay_buffer.count >= (REPLAY_OPS as! u64) || start > (REPLAY_BYTES as! u64) ||
        layout.bytes > (REPLAY_BYTES as! u64) - start || (layout.count != 0 && params == 0) { return false; }

    var source: u64[32];
    for i in (layout.count as! i64) {
        source[i] = fg_cuda_driver_load_argument_address(params + (8 * i) as! u64);
        if source[i] == 0 { return false; }
    }
    for i in (replay_buffer.used as! i64) .. ((start + layout.bytes) as! i64) {
        replay_buffer.args[i] = 0;
    }
    let storage = fg_cuda_driver_bytes_address(replay_buffer.args);
    for i in (layout.count as! i64) {
        cuda_draft_copy_bytes(storage + start + layout.offset[i], source[i], layout.size[i] as! i64);
    }
    var op .= replay_buffer.ops[replay_buffer.count];
    op.kind = replay_kernel;
    op.kernel.layout = layout_index;
    op.kernel.start = start;
    op.kernel.shape = shape;
    replay_buffer.count += 1;
    replay_buffer.used = start + layout.bytes;
    return true;
}

// Diagnostic/candidate (GOOSE_DECODE_SPLIT=N): after a decode FFN submission, submit the
// following region every N buffered kernels so the GPU starts the next layer while the
// host still records the rest. Kernels and their order are unchanged.
var decode_split_every: i32 = -1;
var decode_split_armed: i32;
var decode_split_count: i32 = -1;
// Forward declaration: decode_capture_submit
fn decode_launch(kernel: u64, gx: u32, gy: u32, gz: u32, bx: u32, by: u32, bz: u32, shared: u32, stream: u64, params: u64, extra: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    if decode_split_every < 0 {
        decode_split_every = fg_cuda_host_env_atoi(cuda_env("GOOSE_DECODE_SPLIT"), 0);
    }
    let error: i32 = if stream != 0 { decode_capture_submit() } else { decode_capture_begin() };
    if error != 0 { return error; }
    var launch_stream = stream;
    if thread_storage[0].decode_capture.capturing {
        launch_stream = cuda.graph_stream;
        thread_storage[0].decode_capture.kernels += 1;
    }
    if thread_storage[0].decode_capture.buffering {
        let shape: u32[7] = [gx, gy, gz, bx, by, bz, shared];
        if extra == 0 && buffer_decode_kernel(replay_layout_for(kernel), shape, params) {
            if decode_split_armed != 0 && decode_split_every > 0 && thread_storage[0].decode_capture.kernels >= (decode_split_every as! u64) {
                decode_split_armed = if decode_split_armed > 1 { decode_split_armed - 1 } else { 0 };
                return decode_capture_submit();
            }
            return 0;
        }
        let recorded: i32 = record_buffered_decode();
        if recorded != 0 { return recorded; }
    }
    return fg_cuda_driver_launch(kernel, gx, gy, gz, bx, by, bz, shared, launch_stream, params, extra);
}

fn append_replay_copy(kind: i32, dst: u64, src: u64, bytes: u64) {
    var op: replay_operation;
    op.kind = kind;
    op.copy = ReplayCopyOperation { dst: dst, src: src, bytes: bytes };
    replay_buffer.ops[replay_buffer.count] = op;
    replay_buffer.count += 1;
}

fn append_replay_wait(kind: i32) {
    var op: replay_operation;
    op.kind = kind;
    replay_buffer.ops[replay_buffer.count] = op;
    replay_buffer.count += 1;
}

fn decode_upload_async(dst: u64, src: c_ptr<const void>, bytes: u64, stream: u64) -> i32 {
    var tls = cuda_thread_state();
    let error = if stream != 0 { decode_capture_submit() } else { decode_capture_begin() };
    if error != 0 { return error; }
    let active_stream = if tls[0].decode_capture.capturing { cuda.graph_stream } else { stream };
    if tls[0].decode_capture.buffering {
        if replay_buffer.count < (REPLAY_OPS as! u64) {
            append_replay_copy(replay_h2d, dst, fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(src)), bytes);
            return 0;
        }
        let recorded = record_buffered_decode();
        if recorded != 0 { return recorded; }
    }
    return fg_cuda_driver_upload_async_typed(dst, src, bytes, unsafe_c_ptr<c_ptr<void>>(active_stream));
}

fn snapshot_packet(data: c_ptr<const void>, bytes: u64) -> c_ptr<const void> {
    var tls = cuda_thread_state();
    var activations .= tls[0].activations;
    let capacity = MIB_BYTES as! u64;
    if cuda.packet_host == 0 || data == null || bytes == 0 || bytes > capacity { return null; }
    let checked: i64 = (if bytes > 4096 { 4096 } else { bytes }) as! i64;
    let index = fg143_pinned_packet_slot(activations.packet_used as! i64, MIB_BYTES, checked);
    if index < 0 || bytes > capacity - (index as! u64) { return null; }
    let host = cuda.packet_host + (index as! u64);
    fg_cuda_host_memcpy(host, fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(data)), bytes);
    activations.packet_used = (index as! u64) + bytes;
    activations.pending = true;
    return unsafe_c_ptr<c_ptr<const void>>(host);
}

fn decode_upload(dst: u64, src: c_ptr<const void>, bytes: u64) -> i32 {
    var tls = cuda_thread_state();
    if tls[0].decode_capture.scope && cuda_capture_async_hook && bytes == 8192 {
        // Capture owns the pinned copy even if the caller releases src.
        let host = snapshot_packet(src, bytes);
        if host != null { return decode_upload_async(dst, host, bytes, 0); }
    }
    let error = decode_capture_submit();
    return if error == 0 { fg_cuda_driver_upload_typed(dst, src, bytes) } else { error };
}

fn decode_copy_device(dst: u64, src: u64, bytes: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    if thread_storage[0].decode_capture.scope && (cuda.copy_device_async != 0) {
        var error: i32 = decode_capture_begin();
        if error == 0 && thread_storage[0].decode_capture.buffering {
            if replay_buffer.count < (REPLAY_OPS as! u64) {
                append_replay_copy(replay_d2d, dst, src, bytes);
                return 0;
            }
            error = record_buffered_decode();
        }
        return if error == 0 { fg_cuda_driver_copy_device_async(dst, src, (bytes as! u64), cuda.graph_stream) } else { error };
    }
    let error: i32 = decode_capture_submit();
    return if error == 0 { fg_cuda_driver_copy_device(dst, src, (bytes as! u64)) } else { error };
}

fn decode_wait_for_expert_planes() -> i32 {
    var thread_storage = cuda_thread_state();
    const wait_external: i64 = 1;
    if thread_storage[0].decode_capture.buffering {
        if replay_buffer.count < (REPLAY_OPS as! u64) {
            append_replay_wait(replay_wait_planes);
            return 0;
        }
        let recorded: i32 = record_buffered_decode();
        if recorded != 0 { return recorded; }
    }
    return fg_cuda_driver_stream_wait_event(cuda.graph_stream, cuda.expert_planes_ready, (wait_external as! u32));
}

fn decode_wait_for_experts() -> i32 {
    var thread_storage = cuda_thread_state();
    const wait_external: i64 = 1;
    if thread_storage[0].decode_capture.buffering {
        if replay_buffer.count < (REPLAY_OPS as! u64) {
            append_replay_wait(replay_wait);
            return 0;
        }
        let recorded: i32 = record_buffered_decode();
        if recorded != 0 { return recorded; }
    }
    return fg_cuda_driver_stream_wait_event(cuda.graph_stream, cuda.expert_uploads_ready, (wait_external as! u32));
}

// Host accesses and allocation changes are submission barriers.
fn decode_download(dst: u64, src: u64, bytes: u64) -> i32 {
    let status = decode_capture_submit();
    if status != 0 { return status; }
    return fg_cuda_driver_download(dst, src, (bytes as! u64));
}

fn decode_zero(dst: u64, value: u8, bytes: u64) -> i32 {
    let status = decode_capture_submit();
    if status != 0 { return status; }
    return fg_cuda_driver_zero(dst, value, (bytes as! u64));
}

fn decode_alloc(dst: u64, bytes: u64) -> i32 {
    let status = decode_capture_submit();
    if status != 0 { return status; }
    return fg_cuda_driver_alloc(dst, (bytes as! u64));
}

fn decode_sync() -> i32 {
    let status = decode_capture_submit();
    if status != 0 { return status; }
    return fg_cuda_driver_sync();
}

fn decode_memory_info(free_bytes: u64, total_bytes: u64) -> i32 {
    let status = decode_capture_submit();
    if status != 0 { return status; }
    return fg_cuda_driver_memory_info(free_bytes, total_bytes);
}


fn decode_release(dst: u64) -> i32 {
    let error: i32 = decode_capture_submit();
    if error != 0 { return error; }
    clear_replay_regions();
    return fg_cuda_driver_release(dst);
}

var cuda_capture_async_hook: bool = false;

fn cuda_capture_launch(kernel: u64, gx: u32, gy: u32, gz: u32, bx: u32, by: u32, bz: u32, shared: u32, stream: u64, params: u64, extra: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_launch(kernel, gx, gy, gz, bx, by, bz, shared, stream, params, extra); }
    return fg_cuda_driver_launch(kernel, gx, gy, gz, bx, by, bz, shared, stream, params, extra);
}

fn cuda_capture_upload(dst: u64, src: c_ptr<const void>, bytes: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_upload(dst, src, bytes); }
    return fg_cuda_driver_upload_typed(dst, src, bytes);
}

fn cuda_capture_upload_async(dst: u64, src: c_ptr<const void>, bytes: u64, stream: u64) -> i32 {
    if cuda.decode_graph_enabled && cuda_capture_async_hook { return decode_upload_async(dst, src, bytes, stream); }
    return fg_cuda_driver_upload_async_typed(dst, src, bytes, unsafe_c_ptr<c_ptr<void>>(stream));
}

fn cuda_capture_download(dst: u64, src: u64, bytes: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_download(dst, src, bytes); }
    return fg_cuda_driver_download(dst, src, (bytes as! u64));
}

fn cuda_capture_copy_device(dst: u64, src: u64, bytes: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_copy_device(dst, src, bytes); }
    return fg_cuda_driver_copy_device(dst, src, (bytes as! u64));
}

fn cuda_capture_zero(dst: u64, value: u8, bytes: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_zero(dst, value, bytes); }
    return fg_cuda_driver_zero(dst, value, (bytes as! u64));
}

fn cuda_capture_alloc(dst: u64, bytes: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_alloc(dst, bytes); }
    return fg_cuda_driver_alloc(dst, (bytes as! u64));
}

fn cuda_capture_release(dst: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_release(dst); }
    return fg_cuda_driver_release(dst);
}

fn cuda_capture_sync() -> i32 {
    if cuda.decode_graph_enabled { return decode_sync(); }
    return fg_cuda_driver_sync();
}

fn cuda_capture_memory_info(free_bytes: u64, total_bytes: u64) -> i32 {
    if cuda.decode_graph_enabled { return decode_memory_info(free_bytes, total_bytes); }
    return fg_cuda_driver_memory_info(free_bytes, total_bytes);
}

fn finish_device_state_write() -> i32 {
    var thread_storage = cuda_thread_state();
    if !thread_storage[0].decode_capture.capturing { return cuda_capture_sync(); }
    thread_storage[0].activations.pending = true;
    return 0;
}

fn install_decode_capture() {
    cuda.graph_update = fg_cuda_driver_graph_update_bind(cuda.library);
    if (cuda.graph_update == 0) { return; }
    cuda.copy_device_async = fg_cuda_driver_copy_device_async_bind(cuda.library);
    cuda_capture_async_hook = cuda.upload_async != 0;
    cuda.decode_graph_enabled = true;
    var compact_ids: u64 = cuda_env("GOOSE_CUDA_COMPACT_MOE_IDS");
    cuda.compact_moe_ids = !cuda_env_equal(compact_ids, "0");
    var early_ffn: u64 = cuda_env("GOOSE_CUDA_DECODE_EARLY_FFN");
    cuda.decode_early_ffn = !cuda_env_equal(early_ffn, "0");
    if fg_cuda_has_kernel_arg_counts() {
        var reuse: u64 = cuda_env("GOOSE_CUDA_DECODE_REUSE");
        if cuda_env_equal(reuse, "1") {
            cuda.function_param = fg_cuda_driver_function_param_bind(cuda.library);
            cuda.function_name = fg_cuda_driver_function_name_bind(cuda.library);
            cuda.graph_node_type = fg_cuda_driver_graph_node_type_bind(cuda.library);
            cuda.decode_reuse = cuda.function_param != 0 && cuda.function_name != 0 && cuda.graph_node_type != 0;
        }
    }
    var overlap: u64 = cuda_env("GOOSE_CUDA_DECODE_EXPERT_OVERLAP");
    if (cuda.expert_stream != 0) && cuda_env_equal(overlap, "1") &&
        fg_cuda_driver_event_record(cuda.expert_uploads_ready, cuda.expert_stream) == 0 {
        cuda.expert_decode_overlap = true;
    }
}

export fn fg_cuda_decode_graph_enabled() -> u8 {
    return if cuda.decode_graph_enabled { 1 } else { 0 };
}

struct CudaState_matrix_glu {
    target: u64,
    bytes: i64,
    packed: bool,
}
struct CudaState_shared_activation_pack {
    host: u64,
    input: u64, packed: u64,
    bytes: i64, step: i64, rows: i64, stride: i64,
    extent: i32, layout: i32,
    dense: bool,
}
struct CudaState_route_compact_pending {
    scores_host: u64, ids_host: u64,
    scores_device: u64,
    mailbox_sequence: u32,
}
struct CudaState_published_state {
    source: u64, destination: u64,
    bytes: i64,
}

fn free_expert_metadata(cache: expert_cache&) {
    fg_cuda_host_free(cuda_slice_address(cache.groups));
    fg_cuda_host_free(cuda_slice_address(cache.layers));
    fg_cuda_host_free(cuda_slice_address(cache.work));
    fg_cuda_host_free(cache.state);
}

extern fn fg_cuda_host_view_vmm_mapping(address: u64, length: i64) -> vmm_mapping[:];

fn release_vmm(buffer: device_buffer&) -> i32 {
    let sync_error: i32 = if buffer.mappings != 0 { cuda_capture_sync() } else { 0 };
    if sync_error != 0 { return sync_error; }
    clear_replay_regions();
    while buffer.mappings != 0 {
        let segment_address = buffer.mappings;
        var mapping = fg_cuda_host_view_vmm_mapping(segment_address, 1);
        var segment .= mapping[0];
        if segment.mapped {
            let error: i32 = fg_cuda_driver_vmm_unmap(buffer.address + segment.offset, segment.bytes);
            if error != 0 { return error; }
            segment.mapped = false;
            buffer.bytes = segment.offset;
        }
        let error: i32 = fg_cuda_driver_vmm_release(segment.handle);
        if error != 0 { return error; }
        buffer.mappings = segment.previous;
        fg_cuda_host_free(segment_address);
    }
    let error: i32 = fg_cuda_driver_vmm_free(buffer.address, buffer.reservation_bytes);
    if error == 0 {
        if cuda_env("GOOSE_GRAPH_LOG") != 0 { fg_cuda_host_diagnostic_0(((buffer.address as! u64) as! u64)); }
        buffer.address = 0;
        buffer.bytes = 0;
        buffer.mappings = 0;
        buffer.reservation_bytes = 0;
        buffer.vmm_failed = false;
    }
    return error;
}

fn init_vmm(device: i32, default_lazy: bool) {
    const vmm_attribute: i64 = 102;
    const allocation_pinned: i64 = 1;
    const location_device: i64 = 1;
    const minimum_granularity: i64 = 0;
    var setting: u64 = cuda_env("GOOSE_CUDA_VMM");
    let scratch: bool = cuda_env_equal(setting, "1");
    let lazy: bool = default_lazy || cuda_option("GOOSE_KV_LAZY_VMM");
    var available: i32 = 0;
    if (!scratch && !lazy) ||
        fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(available)), (vmm_attribute as! i32), device) != 0 || available == 0 { return; }
    cuda.vmm_get_granularity = fg_cuda_driver_vmm_get_granularity_bind(cuda.library);
    if cuda.vmm_get_granularity == 0 { return; }
    cuda.vmm_reserve = fg_cuda_driver_vmm_reserve_bind(cuda.library);
    if cuda.vmm_reserve == 0 { return; }
    cuda.vmm_free = fg_cuda_driver_vmm_free_bind(cuda.library);
    if cuda.vmm_free == 0 { return; }
    cuda.vmm_create = fg_cuda_driver_vmm_create_bind(cuda.library);
    if cuda.vmm_create == 0 { return; }
    cuda.vmm_map = fg_cuda_driver_vmm_map_bind(cuda.library);
    if cuda.vmm_map == 0 { return; }
    cuda.vmm_access = fg_cuda_driver_vmm_access_bind(cuda.library);
    if cuda.vmm_access == 0 { return; }
    cuda.vmm_unmap = fg_cuda_driver_vmm_unmap_bind(cuda.library);
    if cuda.vmm_unmap == 0 { return; }
    cuda.vmm_release = fg_cuda_driver_vmm_release_bind(cuda.library);
    if cuda.vmm_release == 0 { return; }
    cuda.vmm_properties = memory_properties {
        kind: allocation_pinned as! i32, handle_types: 0,
        location: memory_location { kind: location_device as! i32, device: device },
        platform_metadata: 0, flags: 0,
    };
    var granularity: u64 = 0;
    if fg_cuda_driver_vmm_get_granularity_typed(unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(granularity)), unsafe_c_address<c_ptr<const void>>(cuda.vmm_properties), minimum_granularity as! i32) == 0 &&
        granularity > 0 {
        cuda.vmm_granularity = granularity;
        cuda.scratch_vmm = scratch;
        cuda.lazy_kv = lazy;
    }
}

fn init_cuda_graphs() {
    var flash: u64 = cuda_env("GOOSE_CUDA_GRAPH");
    var mmvq: u64 = cuda_env("GOOSE_CUDA_MMVQ_GRAPH");
    var pack: u64 = cuda_env("GOOSE_CUDA_GRAPH_PACK");
    var routed: u64 = cuda_env("GOOSE_CUDA_ROUTED_GRAPH");
    var full_ffn: u64 = cuda_env("GOOSE_CUDA_FULL_FFN_GRAPH");
    let flash_requested: bool = cuda.flash_vec != 0 && cuda_env_equal(flash, "1");
    let mmvq_requested: bool = cuda.mmvq != 0 && cuda_env_equal(mmvq, "1");
    let routed_requested: bool = cuda.mmvq != 0 && cuda_env_equal(routed, "1");
    if !flash_requested && !mmvq_requested && !routed_requested { return; }
    cuda.stream_create = fg_cuda_driver_stream_create_bind(cuda.library);
    if cuda.stream_create == 0 { return; }
    cuda.stream_destroy = fg_cuda_driver_stream_destroy_bind(cuda.library);
    if cuda.stream_destroy == 0 { return; }
    cuda.graph_begin = fg_cuda_driver_graph_begin_bind(cuda.library);
    if cuda.graph_begin == 0 { return; }
    cuda.graph_end = fg_cuda_driver_graph_end_bind(cuda.library);
    if cuda.graph_end == 0 { return; }
    cuda.graph_roots = fg_cuda_driver_graph_roots_bind(cuda.library);
    if cuda.graph_roots == 0 { return; }
    cuda.graph_dependents = fg_cuda_driver_graph_dependents_bind(cuda.library);
    if cuda.graph_dependents == 0 { return; }
    cuda.graph_instantiate = fg_cuda_driver_graph_instantiate_bind(cuda.library);
    if cuda.graph_instantiate == 0 { return; }
    cuda.graph_set_kernel = fg_cuda_driver_graph_set_kernel_bind(cuda.library);
    if cuda.graph_set_kernel == 0 { return; }
    cuda.graph_launch = fg_cuda_driver_graph_launch_bind(cuda.library);
    if cuda.graph_launch == 0 { return; }
    cuda.graph_destroy = fg_cuda_driver_graph_destroy_bind(cuda.library);
    if cuda.graph_destroy == 0 { return; }
    cuda.graph_exec_destroy = fg_cuda_driver_graph_exec_destroy_bind(cuda.library);
    if cuda.graph_exec_destroy == 0 { return; }
    cuda.flash_graph_enabled = flash_requested;
    cuda.flash_graph_pack = flash_requested && cuda_env_equal(pack, "1");
    if mmvq_requested || routed_requested {
        let cache_address = fg_cuda_host_calloc((MMVQ_GRAPH_SLOTS * MMVQ_GRAPH_WORDS) as! u64, 8);
        if cache_address != 0 {
            cuda.mmvq_graph_cache = fg_cuda_host_view_u64(cache_address, MMVQ_GRAPH_SLOTS * MMVQ_GRAPH_WORDS);
        }
        cuda.mmvq_graph_enabled = mmvq_requested;
        cuda.routed_graph_enabled = routed_requested;
        cuda.full_ffn_enabled = routed_requested && cuda_env_equal(full_ffn, "1");
        var decode: u64 = cuda_env("GOOSE_CUDA_DECODE_GRAPH");
        if cuda.full_ffn_enabled && cuda_env_equal(decode, "1") { install_decode_capture(); }
    }
}

fn release_mmvq_graphs() {
    if cuda.mmvq_graph_cache.len == 0 { return; }
    for i in MMVQ_GRAPH_SLOTS {
        let executable = cuda.mmvq_graph_cache[i * MMVQ_GRAPH_WORDS + MMVQ_GRAPH_KEY_WORDS];
        if executable != 0 { fg_cuda_driver_graph_exec_destroy(executable); }
    }
    fg_cuda_host_free(cuda_slice_address(cuda.mmvq_graph_cache));
    cuda.mmvq_graph_cache = fg_cuda_host_view_u64(0, 0);
}

fn release_flash_graph() {
    if (cuda.flash_graph_exec != 0) { fg_cuda_driver_graph_exec_destroy(cuda.flash_graph_exec); }
    if (cuda.flash_graph != 0) { fg_cuda_driver_graph_destroy(cuda.flash_graph); }
    cuda.flash_graph_exec = 0;
    cuda.flash_graph = 0;
    cuda.flash_graph_node_count = 0;
}

fn release_expert_stream() {
    var ring .= cuda.expert_host;
    if expert_copy_worker.running {
        fg_cuda_host_worker_lock();
        expert_copy_worker.stop = true;
        fg_cuda_host_worker_signal_ready();
        fg_cuda_host_worker_unlock();
        fg_cuda_host_worker_join();
        expert_copy_worker.running = false;
    }
    for i in 0 .. EXPERT_HOST_SLOTS {
        if ring.ready[i] != 0 { fg_cuda_driver_event_destroy(ring.ready[i]); }
    }
    if ring.storage != 0 { fg_cuda_driver_host_free(ring.storage); }
    ring.storage = 0;
    ring.index = 0;
    ring.used = 0;
    ring.enabled = false;
    for i in EXPERT_HOST_SLOTS {
        ring.ready[i] = 0;
        ring.busy[i] = false;
    }
    if (cuda.expert_stream != 0) { fg_cuda_driver_stream_destroy(cuda.expert_stream); }
    if (cuda.expert_uploads_ready != 0) { fg_cuda_driver_event_destroy(cuda.expert_uploads_ready); }
    if (cuda.expert_planes_ready != 0) { fg_cuda_driver_event_destroy(cuda.expert_planes_ready); }
    if (cuda.expert_slots_ready != 0) { fg_cuda_driver_event_destroy(cuda.expert_slots_ready); }
    for i in 0 .. EXPERT_SLOT_EVENTS
        { if cuda.expert_slots_ring[i] != 0 { fg_cuda_driver_event_destroy(cuda.expert_slots_ring[i]); } }
    cuda.expert_stream = 0;
    cuda.expert_uploads_ready = 0;
    cuda.expert_planes_ready = 0;
    cuda.expert_slots_ready = 0;
    for i in 0 .. EXPERT_SLOT_EVENTS { cuda.expert_slots_ring[i] = 0; }
    cuda.expert_slots_index = 0;
    cuda.expert_slots_records = 0;
}

// A rejected experiment must leave the accepted profile unchanged. These
// settings are enabled only when the variable is explicitly set to 1.
fn expert_option_explicit(name: const u8[:]) -> bool {
    return cuda_env_equal(cuda_env(name), "1");
}

fn init_expert_stream() {    const nonblocking_stream: i64 = 1; const disable_timing: i64 = 2;
    if !cuda_option("GOOSE_CUDA_EXPERT_OVERLAP") { return; }
    cuda.upload_async = fg_cuda_driver_upload_async_bind(cuda.library);
    if cuda.upload_async == 0 { return; }
    cuda.stream_create = fg_cuda_driver_stream_create_bind(cuda.library);
    if cuda.stream_create == 0 { return; }
    cuda.stream_destroy = fg_cuda_driver_stream_destroy_bind(cuda.library);
    if cuda.stream_destroy == 0 { return; }
    cuda.stream_wait_event = fg_cuda_driver_stream_wait_event_bind(cuda.library);
    if cuda.stream_wait_event == 0 { return; }
    cuda.event_create = fg_cuda_driver_event_create_bind(cuda.library);
    if cuda.event_create == 0 { return; }
    cuda.event_destroy = fg_cuda_driver_event_destroy_bind(cuda.library);
    if cuda.event_destroy == 0 { return; }
    cuda.event_record = fg_cuda_driver_event_record_bind(cuda.library);
    if cuda.event_record == 0 { return; }
    var disable_batch: u64 = cuda_env("GOOSE_CUDA_DISABLE_EXPERT_BATCH");
    if !cuda_env_equal(disable_batch, "1")
        { cuda.upload_batch = fg_cuda_driver_upload_batch_bind(cuda.library); }
    var error: i32 = fg_cuda_driver_stream_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.expert_stream)), (nonblocking_stream as! u32));
    block {
        var i: u64 = 0;
        while i < (EXPERT_SLOT_EVENTS as! u64) && error == 0 {
            error = fg_cuda_driver_event_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.expert_slots_ring[i])), (disable_timing as! u32));
            i += 1;
        }
    };
    if error == 0 { error = fg_cuda_driver_event_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.expert_uploads_ready)), (disable_timing as! u32)); }
    if error == 0 { error = fg_cuda_driver_event_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.expert_planes_ready)), (disable_timing as! u32)); }
    if error == 0 { cuda.expert_split_planes = expert_option_explicit("GOOSE_CUDA_EXPERT_SPLIT_PLANES"); }
    if error == 0 { cuda.expert_async_copies = expert_option_explicit("GOOSE_CUDA_EXPERT_ASYNC_COPIES"); }
    if error != 0 { release_expert_stream(); }
    if error == 0 && cuda_option("GOOSE_CUDA_EXPERT_STAGE_PAGEABLE") &&
        (cuda.host_alloc != 0) && (cuda.host_free != 0) {
        cuda.event_sync = fg_cuda_driver_event_sync_bind(cuda.library);
        cuda.expert_host.enabled = cuda.event_sync != 0;
        cuda.expert_prefill_staging = cuda_option("GOOSE_CUDA_EXPERT_STAGE_CURRENT");
        if cuda.expert_host.enabled && cuda_option("GOOSE_CUDA_EXPERT_PARALLEL_COPY") {
            expert_copy_worker.stop = false;
            expert_copy_worker.running = fg_cuda_host_worker_start();
        }
    }
}

export fn fg_cuda_host_cleanup_callback() {
    if (cuda.context != 0) {
        if fg_cuda_driver_push(cuda.context) == 0 {
            if cuda.decode_reuse { cuda_capture_sync(); }
            clear_replay_regions();
            if (cuda.expert_stream != 0) {
                cuda_capture_sync();
                release_expert_stream();
            }
            if cuda.expert_pins.len > 0 { ec_manage_pins(cuda.expert_pins, 0); }
            if (cuda.inkling_prefill_readback != 0) { fg_cuda_driver_host_free(cuda.inkling_prefill_readback); }
            if (cuda.packet_host != 0) {
                cuda_capture_sync();
                fg_cuda_driver_host_free(cuda.packet_host);
            }
            if (cuda.route_mailbox_host != 0) {
                cuda_capture_sync();
                fg_cuda_driver_host_free(cuda.route_mailbox_host);
            }
            release_flash_graph();
            release_mmvq_graphs();
            if cuda.experts.prefill_ready != 0 { fg_cuda_driver_event_destroy(cuda.experts.prefill_ready); }
            if cuda.experts.prefill_release != 0 { fg_cuda_driver_event_destroy(cuda.experts.prefill_release); }
            for i in 0 .. DECODE_GRAPH_REGIONS {
                if (cuda.decode_regions[i] != 0) { fg_cuda_driver_graph_exec_destroy(cuda.decode_regions[i]); }
            }
            if (cuda.graph_stream != 0) { fg_cuda_driver_stream_destroy(cuda.graph_stream); }
            if (cuda.attention_scratch.reservation_bytes != 0) { release_vmm(&cuda.attention_scratch); }
            if (cuda.mmq_packed.reservation_bytes != 0) { release_vmm(&cuda.mmq_packed); }
            block {
                var i: i64 = 0;
                while i < cuda.state_views.len {
                    if (cuda.state_views[i].reservation_bytes != 0) { release_vmm(&cuda.state_views[i]); }
                    i += 1;
                }
            };
            var previous: u64 = 0;
            fg_cuda_driver_pop_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(previous)));
        }
        fg_cuda_driver_destroy(cuda.context);
    }
    fg_cuda_host_free(cuda_slice_address(cuda.dense_ranges));
    fg_cuda_host_free(cuda_slice_address(cuda.state_ranges));
    fg_cuda_host_free(state_views_address(cuda.state_views));
    fg_cuda_host_free(cuda_slice_address(cuda.expert_pins));
    fg_cuda_host_free(expert_jobs_address(cuda.expert_upload_jobs));
    free_expert_metadata(cuda.experts);
    if (cuda.library != 0) { fg_cuda_driver_close(cuda.library); }
}

fn init_route_mailbox() -> i32 {
    let host_device = fg_cuda_driver_host_device_pointer_bind(cuda.library);
    if host_device == 0 || (cuda.host_alloc == 0) || (cuda.host_free == 0) { return -1; }
    var module: u64 = 0;
    var error: i32 = fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_route_mailbox_image()));
    if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_mailbox_kernel), module, "goose_route_compact8"); }
    var host: c_ptr<void> = null;
    if error == 0 { error = fg_cuda_driver_host_alloc_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(host)), 64, 2); }
    if error == 0 { error = fg_cuda_driver_host_device_pointer_typed(unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(cuda.route_mailbox_device)), unsafe_c_ptr<c_ptr<void>>(host), 0); }
    if error != 0 {
        if host != null { fg_cuda_driver_host_free_typed(unsafe_c_ptr<c_ptr<void>>(host)); }
        return error;
    }
    cuda.route_mailbox_host = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(host));
    fg_cuda_host_memset(cuda.route_mailbox_host, 0, 64);
    return 0;
}

fn load_cuda() {
    if fg_cuda_image_goose_cuda_image() == 0 { return; }
    cuda.library = fg_cuda_driver_open();
    if (cuda.library == 0) { return; }
    cuda.init = fg_cuda_driver_init_bind(cuda.library);
    if cuda.init == 0 { return; }
    cuda.device_get = fg_cuda_driver_device_get_bind(cuda.library);
    if cuda.device_get == 0 { return; }
    cuda.device_attribute = fg_cuda_driver_device_attribute_bind(cuda.library);
    if cuda.device_attribute == 0 { return; }
    cuda.create = fg_cuda_driver_create_bind(cuda.library);
    if cuda.create == 0 { return; }
    cuda.destroy = fg_cuda_driver_destroy_bind(cuda.library);
    if cuda.destroy == 0 { return; }
    cuda.push = fg_cuda_driver_push_bind(cuda.library);
    if cuda.push == 0 { return; }
    cuda.pop = fg_cuda_driver_pop_bind(cuda.library);
    if cuda.pop == 0 { return; }
    cuda.module_load = fg_cuda_driver_module_load_bind(cuda.library);
    if cuda.module_load == 0 { return; }
    cuda.function_get = fg_cuda_driver_function_get_bind(cuda.library);
    if cuda.function_get == 0 { return; }
    cuda.function_set_attribute = fg_cuda_driver_function_set_attribute_bind(cuda.library);
    if cuda.function_set_attribute == 0 { return; }
    cuda.alloc = fg_cuda_driver_alloc_bind(cuda.library);
    if cuda.alloc == 0 { return; }
    cuda.memory_info = fg_cuda_driver_memory_info_bind(cuda.library);
    if cuda.memory_info == 0 { return; }
    cuda.release = fg_cuda_driver_release_bind(cuda.library);
    if cuda.release == 0 { return; }
    cuda.upload = fg_cuda_driver_upload_bind(cuda.library);
    if cuda.upload == 0 { return; }
    cuda.download = fg_cuda_driver_download_bind(cuda.library);
    if cuda.download == 0 { return; }
    cuda.copy_device = fg_cuda_driver_copy_device_bind(cuda.library);
    if cuda.copy_device == 0 { return; }
    cuda.zero = fg_cuda_driver_zero_bind(cuda.library);
    if cuda.zero == 0 { return; }
    cuda.launch = fg_cuda_driver_launch_bind(cuda.library);
    if cuda.launch == 0 { return; }
    cuda.sync = fg_cuda_driver_sync_bind(cuda.library);
    if cuda.sync == 0 { return; }
    cuda.host_register = fg_cuda_driver_host_register_bind(cuda.library);
    cuda.host_unregister = fg_cuda_driver_host_unregister_bind(cuda.library);
    var no_pinned: u64 = cuda_env("GGML_CUDA_NO_PINNED");
    if cuda_option("GOOSE_CUDA_ASYNC_PACKETS") &&
        ((no_pinned == 0) || cuda_env_equal(no_pinned, "0")) {
        cuda.host_alloc = fg_cuda_driver_host_alloc_bind(cuda.library);
        cuda.host_free = fg_cuda_driver_host_free_bind(cuda.library);
        cuda.upload_async = fg_cuda_driver_upload_async_bind(cuda.library);
        cuda.async_packets = cuda.host_alloc != 0 && cuda.host_free != 0 && cuda.upload_async != 0;
    }
    var device: i32 = 0;
    if fg_cuda_driver_init(0) != 0 || fg_cuda_driver_device_get_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(device)), 0) != 0 { return; }
    cuda.status = -1;
    var arena_setting: u64 = cuda_env("GOOSE_CUDA_ACTIVATION_MIB");
    cuda.activation_capacity = DEFAULT_ACTIVATION_BYTES as! u64;
    cuda.adapt_activation_capacity = arena_setting == 0;
    cuda.direct_wide_vectors = cuda_option("GOOSE_CUDA_WIDE_VECTOR");
    cuda.stage_matrix_input = cuda_option("GOOSE_CUDA_STAGE_MATRIX_INPUT");
    cuda.defer_matrix_waits = cuda_option("GOOSE_CUDA_DEFER_MATRIX_WAITS");
    if (arena_setting != 0) {
        let value = fg_cuda_host_positive_env(arena_setting);
        if value < 64 || value > INT64_MAX / MIB_BYTES {
            fg_cuda_host_diagnostic_1();
            return;
        }
        cuda.activation_capacity = (value as! u64) * (MIB_BYTES as! u64);
    }
    cuda.mmq_window = cuda_option("GOOSE_MMQ_WINDOW");
    cuda.mmq_align_window = cuda_option("GOOSE_MMQ_ALIGN_WINDOW");
    cuda.mmq_shared_reuse = cuda_option("GOOSE_MMQ_SHARED_REUSE");
    cuda.mmq_dense_reuse = cuda_option("GOOSE_MMQ_DENSE_REUSE");
    cuda.mmq_dense_partial = cuda_option("GOOSE_MMQ_DENSE_PARTIAL");
    cuda.mmq_shared = cuda_option("GOOSE_MMQ_SHARED");
    cuda.moe_topk = cuda_option("GOOSE_MOE_TOPK");
    var setting: u64 = cuda_env("GOOSE_MMQ");
    var stream_setting: u64 = cuda_env("GOOSE_MMQ_STREAM_K");
    let stream_k: bool = cuda_env_equal(stream_setting, "1");
    let mmq: bool = stream_k || (cuda_env_equal(setting, "1"));
    let mmvq_requested: bool = cuda_option("GOOSE_MMVQ");
    var mmvq_tail: u64 = cuda_env("GOOSE_MMVQ_DENSE_TAIL");
    cuda.mmvq_dense_tail = cuda_env_equal(mmvq_tail, "1");
    var dense_setting: u64 = cuda_env("GOOSE_DENSE_MMA");
    let dense_mma: bool = cuda_env_equal(dense_setting, "1");
    var dense_reference_setting: u64 = cuda_env("GOOSE_DENSE_MMA_REFERENCE");
    let dense_reference: bool = dense_mma && cuda_env_equal(dense_reference_setting, "1");
    var gdn_setting: u64 = cuda_env("GOOSE_GDN_MMA");
    let gdn_mma: bool = cuda_env_equal(gdn_setting, "1");
    var flash_setting: u64 = cuda_env("GOOSE_FLASH_MMA");
    let flash_mma: bool = cuda_env_equal(flash_setting, "1");
    let vec_pack: bool = cuda_option("GOOSE_FLASH_VEC_PREPACK");
    var flash_stream_setting: u64 = cuda_env("GOOSE_FLASH_STREAM_K");
    cuda.flash_stream_all = flash_mma && cuda_env_equal(flash_stream_setting, "1");
    var flash_tail_setting: u64 = cuda_env("GOOSE_FLASH_TAIL_MMA");
    cuda.flash_tail_mma = flash_mma && cuda_env_equal(flash_tail_setting, "1");
    let flash_stream: bool = cuda.flash_stream_all || cuda.flash_tail_mma;
    let flash_vec_requested: bool = cuda_option("GOOSE_FLASH_VEC");
    var rms_setting: u64 = cuda_env("GOOSE_RMS_REFERENCE");
    let rms_reference: bool = cuda_env_equal(rms_setting, "1");
    var q6_setting: u64 = cuda_env("GOOSE_MMQ_Q6_REFERENCE");
    let q6_reference: bool = mmq && cuda_env_equal(q6_setting, "1");
    let q6_shared: bool = cuda_option("GOOSE_MMQ_Q6_SHARED");
    let q6_x64: bool = cuda_option("GOOSE_MMQ_Q6_X64");
    var q4_setting: u64 = cuda_env("GOOSE_MMQ_Q4_REFERENCE");
    let q4_reference: bool = mmq && cuda_env_equal(q4_setting, "1");
    let q4_shared: bool = cuda_option("GOOSE_MMQ_Q4_WIDE_SHARED");
    let q4_routed_shared: bool = cuda_option("GOOSE_MMQ_Q4_ROUTED_SHARED");
    let q4_x64: bool = cuda_option("GOOSE_MMQ_Q4_X64");
    let dense_x128: bool = cuda_option("GOOSE_MMQ_DENSE_X128");
    let routed_x128: bool = cuda_option("GOOSE_MMQ_ROUTED_X128");
    var ssm_setting: u64 = cuda_env("GOOSE_SSM_REFERENCE");
    let ssm_reference: bool = cuda_env_equal(ssm_setting, "1");
    var activation_setting: u64 = cuda_env("GOOSE_CUDA_ACTIVATION_REFERENCE");
    cuda.activation_reference = cuda_env_equal(activation_setting, "1");
    let tensor_cores: bool = mmq || dense_mma || gdn_mma || flash_mma;
    const compute_capability_major: i64 = 75;
    const compute_capability_minor: i64 = 76;
    var major: i32 = 0;
    var minor: i32 = 0;
    let sm80_image: bool = (tensor_cores || mmvq_requested || flash_vec_requested) && fg_cuda_image_goose_cuda_mmq_image() != 0 &&
        fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(major)), (compute_capability_major as! i32), device) == 0 && major >= 8;
    if tensor_cores && !sm80_image {
        fg_cuda_host_diagnostic_2();
        return;
    }
    let mmvq: bool = mmvq_requested && sm80_image;
    let flash_vec: bool = flash_vec_requested && sm80_image;
    let mma_image: bool = tensor_cores || mmvq || flash_vec;
    let reference_arch: bool = (q4_reference || q6_reference || dense_reference) && major == 8 &&
        fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(minor)), (compute_capability_minor as! i32), device) == 0 && minor == 9;
    if stream_k || flash_stream || (reference_arch && (q4_reference || q6_reference)) {
        const multiprocessor_count: i64 = 16;
        if fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(cuda.multiprocessors)), (multiprocessor_count as! i32), device) != 0 || cuda.multiprocessors <= 0 { return; }
    }
    var error: i32 = fg_cuda_driver_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.context)), 0, device);
    if error == 0 {
        init_vmm(device, false);
        init_expert_stream();
        error = fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.module)), unsafe_c_ptr<c_ptr<const void>>(if mma_image { fg_cuda_image_goose_cuda_mmq_image() } else { fg_cuda_image_goose_cuda_image() }));
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.kernel), cuda.module, "goose_quantized_matmul"); }
        if error == 0 && mmq { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_pack), cuda.module, "goose_mmq_pack_q8"); }
        if error == 0 && mmq { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_pack_shared), cuda.module, "goose_mmq_pack_shared"); }
        if error == 0 && mmq && cuda_option("GOOSE_MMQ_SWIGLU_PACK")
            { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_pack_glu), cuda.module, "goose_mmq_pack_swiglu"); }
        if error == 0 && mmq { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_matrix), cuda.module, "goose_mmq_matmul"); }
        if error == 0 && mmvq { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmvq_pack), cuda.module, "goose_mmvq_quantize"); }
        if error == 0 && mmvq { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmvq), cuda.module, "goose_mmvq_matvec"); }
        if error == 0 && mmvq
            { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmvq_short), cuda.module, "goose_mmvq_short_rows"); }
        if error == 0 && mmvq {
            cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmvq_dense_pack), cuda.module, "goose_mmvq_dense_quantize");
            cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmvq_dense_row), cuda.module, "goose_mmvq_dense_matvec");
        }
        if error == 0 && reference_arch && q4_reference {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_matrix), cuda.module, "goose_mmq_q4_reference");
            if error == 0 && q4_shared
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_shared), cuda.module, "goose_mmq_q4_dense_shared"); }
            if error == 0 && q4_routed_shared
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_routed_shared), cuda.module, "goose_mmq_q4_routed_shared"); }
            if error == 0 && q4_x64 && q4_shared
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_x64), cuda.module, "goose_mmq_q4_dense_x64"); }
            if error == 0 && q4_x64 && q4_shared && dense_x128
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_x128), cuda.module, "goose_mmq_q4_dense_x128"); }
            var q4_m64: u64 = cuda_env("GOOSE_MMQ_Q4_M64");
            if error == 0 && (cuda.mmq_q4_dense_x128 != 0) && cuda_env_equal(q4_m64, "1")
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_m64), cuda.module, "goose_mmq_q4_dense_m64"); }
            var q4_m128: u64 = cuda_env("GOOSE_MMQ_Q4_M128");
            if error == 0 && (cuda.mmq_q4_dense_m64 != 0) && cuda_env_equal(q4_m128, "1")
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_m128), cuda.module, "goose_mmq_q4_dense_m128"); }
            var q4_large: u64 = cuda_env("GOOSE_MMQ_Q4_LARGE_TILE");
            if error == 0 && (cuda.mmq_q4_dense_m128 != 0) && (q4_large != 0) {
                let more_columns: bool = cuda_env_equal(q4_large, "128x256");
                if more_columns || cuda_env_equal(q4_large, "256x128") {
                    let name: const u8[:] = if more_columns { "goose_mmq_q4_dense_m128n256" } else { "goose_mmq_q4_dense_m256n128" };
                    error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_large), cuda.module, name);
                    cuda.mmq_q4_large_rows = if more_columns { 128 } else { 256 };
                    cuda.mmq_q4_large_cols = if more_columns { 256 } else { 128 };
                    if error == 0 && more_columns && cuda_option("GOOSE_MMQ_Q4_PREPARE") {
                        error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_prepare), cuda.module, "goose_mmq_q4_prepare");
                        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_prepared), cuda.module,
                            "goose_mmq_q4_dense_m128n256_prepared"); }
                        if error == 0 && cuda_option("GOOSE_MMQ_Q4_PIPELINE") {
                            const max_dynamic_shared_size: i64 = 8;
                            var setup: i32 = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_dense_pipeline), cuda.module,
                                "goose_mmq_q4_dense_m128n256_pipeline");
                            if setup == 0 { setup = fg_cuda_driver_function_set_attribute(cuda.mmq_q4_dense_pipeline,
                                (max_dynamic_shared_size as! i32), (Q4_PIPE_SHARED_BYTES as! i32)); }
                            if setup != 0 { cuda.mmq_q4_dense_pipeline = 0; }
                        }
                    }
                }
            }
            if error == 0 && q4_x64 && q4_routed_shared
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_routed_x64), cuda.module, "goose_mmq_q4_routed_x64"); }
            if error == 0 && (cuda.mmq_q4_routed_x64 != 0) && routed_x128
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_routed_x128), cuda.module, "goose_mmq_q4_routed_x128"); }
            if error == 0 && (cuda.mmq_q4_routed_x128 != 0) && cuda_option("GOOSE_MMQ_Q4_ROUTED_M128")
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_routed_m128), cuda.module, "goose_mmq_q4_routed_m128"); }
            if error == 0 && cuda_option("GOOSE_MMQ_Q4_ROUTED_DOWN_M128")
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q4_routed_down_m128), cuda.module, "goose_mmq_q4_routed_down_m128"); }
        }
        if error == 0 && reference_arch && q6_reference {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_matrix), cuda.module, "goose_mmq_q6_reference");
            if error == 0 && q6_shared
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_dense_shared), cuda.module, "goose_mmq_q6_dense_shared"); }
            if error == 0 && q6_shared
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_routed_shared), cuda.module, "goose_mmq_q6_routed_shared"); }
            if error == 0 && q6_shared && q6_x64
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_dense_x64), cuda.module, "goose_mmq_q6_dense_x64"); }
            if error == 0 && q6_shared && q6_x64 && dense_x128
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_dense_x128), cuda.module, "goose_mmq_q6_dense_x128"); }
            if error == 0 && (cuda.mmq_q6_dense_x128 != 0) && cuda_option("GOOSE_MMQ_Q6_HALF_K") {
                const max_dynamic_shared_size: i64 = 8;
                error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_dense_half_k), cuda.module, "goose_mmq_q6_dense_m128_half_k");
                if error == 0 { error = fg_cuda_driver_function_set_attribute(cuda.mmq_q6_dense_half_k,
                    (max_dynamic_shared_size as! i32), (Q6_HALF_K_SHARED_BYTES as! i32)); }
                if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_pack_k_major), cuda.module, "goose_mmq_pack_k_major"); }
                if error == 0 && cuda_option("GOOSE_MMQ_Q6_PIPELINE") {
                    var setup: i32 = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_dense_pipeline), cuda.module,
                        "goose_mmq_q6_dense_m128_pipeline");
                    if setup == 0 { setup = fg_cuda_driver_function_set_attribute(cuda.mmq_q6_dense_pipeline,
                        (max_dynamic_shared_size as! i32), (Q6_PIPE_SHARED_BYTES as! i32)); }
                    if setup != 0 { cuda.mmq_q6_dense_pipeline = 0; }
                }
            }
            if error == 0 && q6_shared && q6_x64
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_routed_x64), cuda.module, "goose_mmq_q6_routed_x64"); }
            if error == 0 && (cuda.mmq_q6_routed_x64 != 0) && routed_x128
                { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_routed_x128), cuda.module, "goose_mmq_q6_routed_x128"); }
            if error == 0 && (cuda.mmq_q6_routed_x128 != 0) && cuda_option("GOOSE_MMQ_Q6_ROUTED_PIPELINE") {
                const max_dynamic_shared_size: i64 = 8;
                var setup: i32 = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_q6_routed_pipeline), cuda.module,
                    "goose_mmq_q6_routed_m128_pipeline");
                if setup == 0 { setup = fg_cuda_driver_function_set_attribute(cuda.mmq_q6_routed_pipeline,
                    (max_dynamic_shared_size as! i32), (Q6_ROUTED_SHARED_BYTES as! i32)); }
                if setup != 0 { cuda.mmq_q6_routed_pipeline = 0; }
            }
        }
        if error == 0 && stream_k { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_stream), cuda.module, "goose_mmq_stream"); }
        if error == 0 && stream_k { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmq_stream_fixup), cuda.module, "goose_mmq_stream_fixup"); }
        if error == 0 && dense_mma { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.dense_mma), cuda.module, "goose_dense_mma"); }
        if error == 0 && reference_arch && dense_reference {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.dense_mma_reference), cuda.module, "goose_dense_mma_reference");
        }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.attention_tiles), cuda.module, "goose_q4_attention_tiles"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.attention_merge), cuda.module, "goose_q4_attention_merge"); }
        var flash_interior: u64 = cuda_env("GOOSE_FLASH_INTERIOR");
        if error == 0 && flash_mma && major == 8 &&
            (!cuda_env_equal(flash_interior, "0")) &&
            fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(minor)), (compute_capability_minor as! i32), device) == 0 && minor == 9 {
            var image: u64 = 0;
            var kernel: u64 = 0;
            if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(image)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_flash_interior_image())) == 0 &&
                cuda_lookup_kernel(cuda_kernel_argument_address(kernel), image, "goose_flash_causal_mma") == 0 {
                cuda.flash_mma = kernel;
                fg_cuda_host_diagnostic_3();
            }
        }
        if error == 0 && flash_mma && (cuda.flash_mma == 0) {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_mma), cuda.module, "goose_flash_causal_mma");
        }
        if error == 0 && flash_mma { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_stage), cuda.module, "goose_flash_q4_stage"); }
        if error == 0 && flash_stream { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_stream), cuda.module, "goose_flash_stream_mma"); }
        if error == 0 && flash_stream { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_fixup_uniform), cuda.module, "goose_flash_fixup_uniform"); }
        if error == 0 && flash_stream { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_fixup_general), cuda.module, "goose_flash_fixup_general"); }
        if error == 0 && flash_vec { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_vec), cuda.module, "goose_flash_vec_q4"); }
        if error == 0 && flash_vec { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_vec_merge), cuda.module, "goose_flash_vec_combine"); }
        if error == 0 && flash_vec && vec_pack
            { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_vec_pack), cuda.module, "goose_flash_vec_pack_query"); }
        if error == 0 && flash_vec && vec_pack {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_vec_packed), cuda.module, "goose_flash_vec_q4_packed");
            if error == 0
                { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_vec_paired), cuda.module, "goose_flash_vec_q4_packed_pair"); }
            if error == 0
                { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_vec_quad), cuda.module, "goose_flash_vec_q4_packed_quad"); }
        }
        if error == 0 && mmvq && major == 8 &&
            fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(minor)), (compute_capability_minor as! i32), device) == 0 && minor == 9 {
            var disabled: u64 = cuda_env("GOOSE_CUDA_DISABLE_FUSED_UP_GATE");
            if !cuda_env_equal(disabled, "1") {
                var candidate: u64 = 0;
                if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(candidate)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_fused_up_gate_image())) == 0
                    { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmvq_up_gate_glu), candidate, "goose_mmvq_up_gate_glu"); }
            }
            var disable_shared: u64 = cuda_env("GOOSE_CUDA_DISABLE_FUSED_SHARED");
            if !cuda_env_equal(disable_shared, "1") {
                var candidate: u64 = 0;
                if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(candidate)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_fused_shared_image())) == 0
                    { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.mmvq_shared_up_gate_glu), candidate, "goose_mmvq_shared_up_gate_glu"); }
            }
        }
        if error == 0 && (cuda.flash_vec_quad != 0) && major == 8 &&
            fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(minor)), (compute_capability_minor as! i32), device) == 0 && minor == 9 {
            var disable_key_bound: u64 = cuda_env("GOOSE_CUDA_DISABLE_ATTENTION_KEY_BOUND");
            if !cuda_env_equal(disable_key_bound, "1") {
                var candidate: u64 = 0;
                var disable_row: u64 = cuda_env("GOOSE_CUDA_DISABLE_ATTENTION_FIRST_ROW");
                let first_row: bool = !cuda_env_equal(disable_row, "1");
                var disable_unroll: u64 = cuda_env("GOOSE_CUDA_DISABLE_ATTENTION_UNROLL");
                let unroll: bool = first_row && (!cuda_env_equal(disable_unroll, "1"));
                let image: u64 = if unroll { fg_cuda_image_attention_unroll_image() } else { if first_row { fg_cuda_image_attention_first_row_image() } else { fg_cuda_image_attention_key_bound_image() } };
                if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(candidate)), unsafe_c_ptr<c_ptr<const void>>(image)) == 0 &&
                    cuda_lookup_kernel(cuda_kernel_argument_address(cuda.flash_vec_quad_key_bound), candidate,
                                      "goose_flash_vec_q4_packed_quad") == 0 {
                    fg_cuda_host_diagnostic_4();
                    if first_row { fg_cuda_host_diagnostic_5(); }
                    if unroll { fg_cuda_host_diagnostic_6(); }
                }
            }
        }
        if error == 0 { init_cuda_graphs(); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.gdn), cuda.module, "goose_gated_delta_net"); }
        if error == 0 && cuda_option("GOOSE_GDN_DECODE") {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.gdn_decode), cuda.module, "goose_gdn_decode");
        }
        if error == 0 && gdn_mma {
            const max_dynamic_shared_size: i64 = 8;
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.gdn_mma), cuda.module, "goose_gdn_mma");
            if error == 0 { error = fg_cuda_driver_function_set_attribute(cuda.gdn_mma, (max_dynamic_shared_size as! i32), (GDN_MMA_SHARED_BYTES as! i32)); }
            if error == 0 && cuda_option("GOOSE_GDN_MMA_V32") {
                var setup: i32 = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.gdn_mma_v32), cuda.module, "goose_gdn_mma_v32");
                if setup == 0 { setup = fg_cuda_driver_function_set_attribute(cuda.gdn_mma_v32, (max_dynamic_shared_size as! i32), (GDN_V32_SHARED_BYTES as! i32)); }
                if setup != 0 { cuda.gdn_mma_v32 = 0; }
            }
        }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.q4_rows), cuda.module, "goose_set_q4_rows"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.vector), cuda.module, "goose_vector_ops"); }
        if error == 0
            { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.moe_add_shared_residual), cuda.module, "goose_moe_add_shared_residual"); }
        if error == 0 && cuda_option("GOOSE_CUDA_VECTOR_FLAT")
            { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.vector_flat), cuda.module, "goose_vector_flat"); }
        if error == 0 && (cuda.vector_flat != 0) && cuda_option("GOOSE_CUDA_VECTOR_FLAT4")
            { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.vector_flat4), cuda.module, "goose_vector_flat4"); }
        if error == 0 && rms_reference { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.rms_reference), cuda.module, "goose_rms_reference"); }
        var rms_onevec: u64 = cuda_env("GOOSE_RMS_BATCHED");
        if error == 0 && rms_reference && major == 8 &&
            (!cuda_env_equal(rms_onevec, "0")) &&
            fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(minor)), (compute_capability_minor as! i32), device) == 0 && minor == 9 {
            var image: u64 = 0;
            var kernel: u64 = 0;
            if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(image)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_rms_onevec_image())) == 0 &&
                cuda_lookup_kernel(cuda_kernel_argument_address(kernel), image, "goose_rms_reference_one") == 0 {
                cuda.rms_reference_one = kernel;
                cuda_lookup_kernel(cuda_kernel_argument_address(cuda.rms_reference_one_warp4), image, "goose_rms_reference_one_warp4");
                fg_cuda_host_diagnostic_7();
            }
        }
        if error == 0 && rms_reference
            { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.rms_decode_2048), cuda.module, "goose_rms_decode_2048"); }
        cuda.rms_postscale = cuda.rms_reference != 0 && cuda_option("GOOSE_CUDA_RMS_POSTSCALE");
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.rope_phases), cuda.module, "goose_rope_phases"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.rope_apply), cuda.module, "goose_rope_apply"); }
        if error == 0 && cuda_option("GOOSE_ROPE_DECODE") {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.rope_decode), cuda.module, "goose_rope_decode");
        }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.ssm_conv), cuda.module, "goose_ssm_conv"); }
        if error == 0 && ssm_reference {
            error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.ssm_conv_long), cuda.module, "goose_ssm_conv_long");
            var decode_setting: u64 = cuda_env("GOOSE_SSM_DECODE_REFERENCE");
            if error == 0 && cuda_env_equal(decode_setting, "1") {
                error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.ssm_conv_decode), cuda.module, "goose_ssm_conv_decode");
            }
        }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_reduce), cuda.module, "goose_route_reduce"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_sort), cuda.module, "goose_route_sort"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_topk), cuda.module, "goose_route_topk"); }
        if error == 0 && mmvq && major == 8 && minor == 9 {
            var disable: u64 = cuda_env("GOOSE_CUDA_DISABLE_ROUTE_COMPACT8");
            var routing: u64 = 0;
            if (!cuda_env_equal(disable, "1")) &&
                fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(routing)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_compact_route_image())) == 0
                { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_compact8), routing, "goose_route_compact8"); }
            var mailbox: u64 = cuda_env("GOOSE_ROUTE_MAILBOX");
            if (cuda.route_compact8 != 0) && (!cuda_env_equal(mailbox, "0")) {
                let setup: i32 = init_route_mailbox();
                if setup != 0 { cuda.route_mailbox_kernel = 0; }
                fg_cuda_host_diagnostic_8((setup as! i32));
            }
        }
        if error == 0 && mmvq && major == 8 && minor == 9 {
            var disable: u64 = cuda_env("GOOSE_CUDA_DISABLE_ROUTER_UNROLL");
            var module: u64 = 0;
            if (!cuda_env_equal(disable, "1")) &&
                fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_router_softmax_image())) == 0
                { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_softmax_fixed), module, "goose_router_softmax_fixed"); }
        }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_gather), cuda.module, "goose_route_gather"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.route_norm), cuda.module, "goose_route_norm"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.blit), cuda.module, "goose_tensor_blit"); }
        if error == 0 && cuda_option("GOOSE_CUDA_DIRECT_SSM") {
            cuda_lookup_kernel(cuda_kernel_argument_address(cuda.ssm_conv_direct), cuda.module, "goose_ssm_conv_direct");
            cuda_lookup_kernel(cuda_kernel_argument_address(cuda.ssm_conv_save_tail), cuda.module, "goose_ssm_conv_save_tail");
        }
        if error == 0 && cuda_option("GOOSE_CUDA_TILED_BLIT")
            { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.blit_transpose), cuda.module, "goose_tensor_transpose_concat"); }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.fwht), cuda.module, "goose_kv_fwht"); }
        if error == 0 && cuda_option("GOOSE_CUDA_FWHT_Q4")
            { cuda_lookup_kernel(cuda_kernel_argument_address(cuda.fwht_q4), cuda.module, "goose_fwht_q4"); }
        var fwht_fixed: u64 = cuda_env("GOOSE_FWHT_FIXED");
        if error == 0 && major == 8 && (!cuda_env_equal(fwht_fixed, "0")) &&
            fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(minor)), (compute_capability_minor as! i32), device) == 0 && minor == 9 {
            var image: u64 = 0;
            if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(image)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_fwht_fixed_image())) == 0 {
                cuda_lookup_kernel(cuda_kernel_argument_address(cuda.fwht_fixed[0]), image, "goose_kv_fwht_64");
                cuda_lookup_kernel(cuda_kernel_argument_address(cuda.fwht_fixed[1]), image, "goose_kv_fwht_256");
                if (cuda.fwht_q4 != 0) {
                    cuda_lookup_kernel(cuda_kernel_argument_address(cuda.fwht_q4_fixed[0]), image, "goose_fwht_q4_64");
                    cuda_lookup_kernel(cuda_kernel_argument_address(cuda.fwht_q4_fixed[1]), image, "goose_fwht_q4_256");
                }
            }
        }
        if error == 0 { error = cuda_lookup_kernel(cuda_kernel_argument_address(cuda.moe_weighted), cuda.module, "goose_moe_weighted"); }
        var moe_epilogue: u64 = cuda_env("GOOSE_MOE_PREFILL_EPILOGUE");
        if error == 0 && major == 8 && (!cuda_env_equal(moe_epilogue, "0")) &&
            fg_cuda_driver_device_attribute_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(minor)), (compute_capability_minor as! i32), device) == 0 && minor == 9 {
            var image: u64 = 0;
            var kernel: u64 = 0;
            if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(image)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_moe_epilogue_image())) == 0 &&
                cuda_lookup_kernel(cuda_kernel_argument_address(kernel), image, "goose_moe_shared_residual_prefill") == 0
                { cuda.moe_prefill_epilogue = kernel; }
        }

        var current: u64 = 0;
        var pop_error: i32 = fg_cuda_driver_pop_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(current)));
        if error == 0 { error = pop_error; }
    }
    if error != 0 {
        fg_cuda_host_diagnostic_9((error as! i32));
        return;
    }
    cuda.status = 1;
}

export fn fg_cuda_host_init_callback() {
    load_cuda();
    if (cuda.library != 0) { fg_cuda_host_register_cleanup(); }
}

fn release_buffer(buffer: device_buffer&) -> i32 {
    var thread_storage = cuda_thread_state();
    if buffer .== &cuda.mmq_packed { thread_storage[0].shared_activation_pack.host = 0; }
    if (buffer.address == 0) { return 0; }
    if (buffer.reservation_bytes != 0) { return release_vmm(buffer); }
    let error: i32 = cuda_capture_release(buffer.address);
    if error == 0 {
        buffer.address = 0;
        buffer.bytes = 0;
        buffer.mappings = 0;
        buffer.reservation_bytes = 0;
        buffer.vmm_failed = false;
    }
    return error;
}

fn reserve_vmm(buffer: device_buffer&, bytes: u64, limit: u64, clear: bool) -> i32 {
    const out_of_memory: i64 = 2;
    const access_read_write: i64 = 3;
    var plan: i64[3];
    let prepared: bool = fg143_vmm_prepare(bytes, buffer.bytes, limit, cuda.vmm_granularity,
        buffer.vmm_failed, buffer.address != 0, fg_cuda_host_view_i64(cuda_slice_address(plan), (3 as! i64)));
    if plan[0] != 0 { cuda.vmm_capacity = (plan[0] as! u64); }
    if !prepared { return out_of_memory as! i32; }
    let growth: u64 = (plan[1] as! u64);
    let reservation: u64 = (plan[2] as! u64);
    let submitted: i32 = decode_capture_submit();
    if submitted != 0 { return submitted; }
    let segment_address = fg_cuda_host_malloc(40);
    if segment_address == 0 { return out_of_memory as! i32; }
    var mapping = fg_cuda_host_view_vmm_mapping(segment_address, 1);
    var segment .= mapping[0];
    segment.previous = buffer.mappings;
    segment.handle = 0;
    segment.offset = buffer.bytes;
    segment.bytes = growth;
    segment.mapped = false;
    var error: i32 = 0;
    if (buffer.address == 0) {
        var base: u64 = 0;
        error = fg_cuda_driver_vmm_reserve_typed(unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(base)), reservation, cuda.vmm_granularity, 0, 0);
        if error == 0 {
            buffer.address = base;
            buffer.reservation_bytes = reservation;
        }
    }
    if error == 0 { error = fg_cuda_driver_vmm_create_typed(unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(segment.handle)), segment.bytes, unsafe_c_address<c_ptr<const void>>(cuda.vmm_properties), 0); }
    if error != 0 {
        fg_cuda_host_free(segment_address);
        return error;
    }
    buffer.mappings = segment_address;
    let address: u64 = buffer.address + segment.offset;
    error = fg_cuda_driver_vmm_map(address, segment.bytes, 0, segment.handle, 0);
    if error == 0 {
        segment.mapped = true;
        let access = memory_access { location: cuda.vmm_properties.location, flags: access_read_write as! i32 };
        error = fg_cuda_driver_vmm_access_typed(address, segment.bytes, unsafe_c_address<c_ptr<const void>>(access), 1);
    }
    if error == 0 && clear { error = cuda_capture_zero(address, 0, segment.bytes); }
    if error == 0 {
        buffer.bytes += segment.bytes;
        if cuda_env("GOOSE_GRAPH_LOG") != 0 {
            fg_cuda_host_diagnostic_10(((buffer.address as! u64) as! u64), (segment.offset as! u64), (segment.bytes as! u64));
        }
    } else if clear {
        // The graph must not consume an address whose mapping or zeroing failed.
        buffer.vmm_failed = true;
    }
    return error;
}

fn reserve(buffer: device_buffer&, bytes: u64) -> i32 {
    if bytes <= buffer.bytes { return 0; }
    if cuda.scratch_vmm && (buffer .== &cuda.attention_scratch || buffer .== &cuda.mmq_packed) &&
        (buffer.address == 0 || buffer.reservation_bytes != 0) {
        let error: i32 = reserve_vmm(buffer, bytes, (fg143_vmm_capacity() as! u64), false);
        if error == 0 { return 0; }
    }
    var error: i32 = release_buffer(buffer);
    if error != 0 { return error; }
    var address: u64 = 0;
    let address_out = cuda_kernel_argument_address(address);
    error = cuda_capture_alloc(address_out, bytes);
    // A completed MMQ projection has no live users of its packing buffers.
    // Reclaim them under pressure before admitting the next operator.
    if error == 2 && buffer .!= &cuda.mmq_packed && buffer .!= &cuda.mmq_tiles &&
        (cuda.mmq_packed.address != 0 || cuda.mmq_tiles.address != 0) {
        error = release_buffer(&cuda.mmq_packed);
        if error == 0 { error = release_buffer(&cuda.mmq_tiles); }
        if error == 0 { error = cuda_capture_alloc(address_out, bytes); }
    }
    if error == 0 {
        buffer.address = address;
        buffer.bytes = bytes;
    }
    if error == 2 {
        var available: u64 = 0;
        var total: u64 = 0;
        if cuda_capture_memory_info(cuda_kernel_argument_address(available), cuda_kernel_argument_address(total)) == 0 {
            fg_cuda_host_diagnostic_11((bytes as! u64), (available as! u64));
        }
    }
    return error;
}

fn finish_context() -> i32 {
    var context: u64 = 0;
    let status: i32 = fg_cuda_driver_pop_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(context)));
    fg_cuda_host_unlock(CUDA_HOST_CONTEXT_LOCK);
    return status;
}

fn push_context() -> i32 {
    fg_cuda_host_lock(CUDA_HOST_CONTEXT_LOCK);
    var status: i32 = fg_cuda_driver_push(cuda.context);
    if status != 0 {
        fg_cuda_host_unlock(CUDA_HOST_CONTEXT_LOCK);
        fg_cuda_host_diagnostic_12((status as! i32));
        return -1;
    }
    return 1;
}

fn start_context() -> i32 {
    var setting: u64 = cuda_env("GOOSE_CUDA");
    if cuda_env_equal(setting, "0") { return 0; }
    fg_cuda_host_ensure_initialized();
    return if cuda.status == 1 { push_context() } else { cuda.status };
}

fn activation_ranges() -> i64[:] {
    var thread_storage = cuda_thread_state();
    return fg_cuda_host_view_i64(cuda_slice_address(thread_storage[0].activations.ranges), (thread_storage[0].activations.count * ACTIVATION_WORDS as! i64));
}

fn activation_large_range() -> i64[:] {
    var thread_storage = cuda_thread_state();
    return fg_cuda_host_view_i64(cuda_slice_address(thread_storage[0].activations.large_output), (ACTIVATION_WORDS as! i64));
}

fn activation_download_record(range: i64[:], base: u64) -> i32 {
    if range[1] == 0 || range[3] != 0 { return 0; }
    let error: i32 = cuda_capture_download((range[0] as! u64),
        base + (range[2] as! u64), (range[1] as! u64));
    if error == 0 { range[3] = 1; }
    return error;
}

export fn fg_cuda_activation_copy_to_host(host: u64, source: u64, bytes: i64) -> i32 {
    return cuda_capture_download((host as! u64), source, (bytes as! u64));
}

fn activation_download_all() -> i32 {
    var thread_storage = cuda_thread_state();
    return if thread_storage[0].activations.count != 0 { fg143_activation_download_all(activation_ranges(), cuda.activations.address) } else { 0 };
}

fn activation_evict_device_range(offset: i64, bytes: i64) -> i32 {
    var thread_storage = cuda_thread_state();
    return if thread_storage[0].activations.count != 0 { fg143_activation_sync(activation_ranges(), cuda.activations.address,
        (offset as! u64), bytes, true, true) } else { 0 };
}

fn activation_sync_range(host: u64, bytes: i64, invalidate: bool) -> i32 {
    var thread_storage = cuda_thread_state();
    if thread_storage[0].activations.large_output[1] > 0 {
        let error: i32 = fg143_activation_sync(activation_large_range(), cuda.output.address,
            host, bytes, false, invalidate);
        if error != 0 { return error; }
    }
    return if thread_storage[0].activations.count != 0 { fg143_activation_sync(activation_ranges(), cuda.activations.address,
        host, bytes, false, invalidate) } else { 0 };
}

export fn fg_cuda_activation_graph_begin(planned: bool) -> i32 {
    var thread_storage = cuda_thread_state();
    thread_storage[0].route_compact_pending.ids_host = 0;
    if thread_storage[0].activations.active { return -1; }
    var setting: u64 = cuda_env("GOOSE_CUDA");
    if cuda_env_equal(setting, "0") { return 0; }
    fg_cuda_host_lock(CUDA_HOST_GRAPH_LOCK);
    thread_storage[0].activations.active = true;
    thread_storage[0].activations.step = -1;
    thread_storage[0].shared_activation_pack.host = 0;
    thread_storage[0].activations.shared_pack_reuses = 0;
    thread_storage[0].activations.dense_pack_reuses = 0;
    thread_storage[0].activations.flat_vectors = 0;
    thread_storage[0].activations.flat4_vectors = 0;
    thread_storage[0].activations.q4_prepared = 0;
    thread_storage[0].activations.q4_pipelined = 0;
    thread_storage[0].activations.q6_pipelined = 0;
    thread_storage[0].activations.q6_routed_pipelined = 0;
    thread_storage[0].activations.expert_parallel_batches = 0;
    thread_storage[0].activations.unavailable = false;
    thread_storage[0].activations.defer_waits = planned && cuda_option("GOOSE_CUDA_DEFER_WAITS");
    thread_storage[0].activations.retire = cuda_option("GOOSE_ACTIVATION_LIFETIMES");
    thread_storage[0].activations.reuse = cuda_option("GOOSE_CUDA_ARENA_REUSE");
    thread_storage[0].activations.selective_eviction = cuda_option("GOOSE_CUDA_SELECTIVE_EVICTION");
    thread_storage[0].activations.pending = false;
    thread_storage[0].activations.hits = 0;
    thread_storage[0].activations.skipped_download_bytes = 0;
    thread_storage[0].activations.expired_bytes = 0;
    thread_storage[0].activations.packet_used = 0;
    thread_storage[0].decode_capture = cuda_zero_CudaState_decode_capture();
    return 0;
}

export fn fg_cuda_activation_read_address(host: u64, bytes: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    let pending: bool = (thread_storage[0].activations.large_output[1] > 0 && thread_storage[0].activations.large_output[3] == 0 &&
        fg143_activation_pending(activation_large_range(), host, bytes)) ||
        (thread_storage[0].activations.count > 0 && fg143_activation_pending(activation_ranges(), host, bytes));
    if !pending { return 0; }
    if push_context() != 1 { return -1; }
    let error: i32 = activation_sync_range(host, bytes, false);
    return if finish_context() == 0 && error == 0 { 0 } else { -1 };
}

export fn fg_cuda_activation_read(data: u8[:]) -> i64 {
    return fg_cuda_activation_read_address(((cuda_slice_address(data)) as! u64), data.len);
}

export fn fg_cuda_activation_read_f32(data: f32[:]) -> i64 {
    if data.len < 0 || data.len > INT64_MAX / (4 as! i64) { return -1; }
    return fg_cuda_activation_read_address(((cuda_slice_address(data)) as! u64), data.len * (4 as! i64));
}

fn activation_prepare_target(host: u64, bytes: i64, fresh: bool, overwrite: bool, step: i64, deadline: i64) -> bool {
    var thread_storage = cuda_thread_state();
    thread_storage[0].activations.target = if fresh { host } else { 0 };
    thread_storage[0].activations.target_bytes = bytes;
    thread_storage[0].activations.target_deadline = deadline;
    thread_storage[0].activations.step = if thread_storage[0].activations.retire { step } else { -1 };
    var counters: u64[2];
    let overlap: bool = fg143_activation_prepare_records(activation_ranges(), activation_large_range(),
        host, bytes, thread_storage[0].activations.step, overwrite, fg_cuda_host_view_u64(cuda_slice_address(counters), (2 as! i64)));
    thread_storage[0].activations.expired_bytes += counters[0];
    thread_storage[0].activations.skipped_download_bytes += counters[1];
    return overlap;
}

export fn fg_cuda_activation_prepare(host: u64, bytes: i64, fresh: bool, overwrite: bool, step: i64, deadline: i64) -> i32 {
    var thread_storage = cuda_thread_state();
    if !thread_storage[0].activations.active || !activation_prepare_target(host, bytes, fresh, overwrite, step, deadline) { return 0; }
    if push_context() != 1 { return -1; }
    let error: i32 = activation_sync_range(host, bytes, true);
    return if finish_context() == 0 && error == 0 { 0 } else { -1 };
}

export fn fg_cuda_activation_graph_end(success: bool, all: bool, host: u64, bytes: i64, hits: u64, skipped_download_bytes: u64, expired_bytes: u64) -> i32 {
    var expired_bytes_out = fg_cuda_host_view_u64(expired_bytes, 1);
    var skipped_download_bytes_out = fg_cuda_host_view_u64(skipped_download_bytes, 1);
    var hits_out = fg_cuda_host_view_u64(hits, 1);
    var thread_storage = cuda_thread_state();
    hits_out[0] = thread_storage[0].activations.hits;
    thread_storage[0].activations.hits = 0;
    skipped_download_bytes_out[0] = thread_storage[0].activations.skipped_download_bytes;
    thread_storage[0].activations.skipped_download_bytes = 0;
    expired_bytes_out[0] = thread_storage[0].activations.expired_bytes;
    thread_storage[0].activations.expired_bytes = 0;
    if !thread_storage[0].activations.active { return 0; }
    if thread_storage[0].activations.shared_pack_reuses != 0 && (cuda_env("GOOSE_MMQ_SHARED_REUSE_REPORT") != 0) {
        fg_cuda_host_diagnostic_13((thread_storage[0].activations.shared_pack_reuses as! u64));
    }
    if thread_storage[0].activations.dense_pack_reuses != 0 && (cuda_env("GOOSE_MMQ_SHARED_REUSE_REPORT") != 0) {
        fg_cuda_host_diagnostic_14((thread_storage[0].activations.dense_pack_reuses as! u64));
    }
    if thread_storage[0].activations.q4_prepared != 0 && (cuda_env("GOOSE_MMQ_SHARED_REUSE_REPORT") != 0) {
        fg_cuda_host_diagnostic_15((thread_storage[0].activations.q4_prepared as! u64));
    }
    if thread_storage[0].activations.q4_pipelined != 0 && (cuda_env("GOOSE_MMQ_SHARED_REUSE_REPORT") != 0) {
        fg_cuda_host_diagnostic_16((thread_storage[0].activations.q4_pipelined as! u64));
    }
    if thread_storage[0].activations.q6_pipelined != 0 && (cuda_env("GOOSE_MMQ_SHARED_REUSE_REPORT") != 0) {
        fg_cuda_host_diagnostic_17((thread_storage[0].activations.q6_pipelined as! u64));
    }
    if thread_storage[0].activations.q6_routed_pipelined != 0 && (cuda_env("GOOSE_MMQ_SHARED_REUSE_REPORT") != 0) {
        fg_cuda_host_diagnostic_18((thread_storage[0].activations.q6_routed_pipelined as! u64));
    }
    if thread_storage[0].activations.expert_parallel_batches != 0 && (cuda_env("GOOSE_MMQ_SHARED_REUSE_REPORT") != 0) {
        fg_cuda_host_diagnostic_19((thread_storage[0].activations.expert_parallel_batches as! u64));
    }
    if thread_storage[0].activations.flat_vectors != 0 && (cuda_env("GOOSE_CUDA_VECTOR_FLAT_REPORT") != 0) {
        fg_cuda_host_diagnostic_20((thread_storage[0].activations.flat_vectors as! u64), (thread_storage[0].activations.flat4_vectors as! u64));
    }
    var error: i32 = 0;
    thread_storage[0].decode_capture.scope = false;
    if thread_storage[0].activations.pending || thread_storage[0].decode_capture.capturing {
        if push_context() != 1 { error = -1; }
        else {
            error = if !success && thread_storage[0].decode_capture.capturing { decode_capture_discard() } else { cuda_capture_sync() };
            if finish_context() != 0 { error = -1; }
            if error != 0 { fg_cuda_host_diagnostic_21((error as! i32)); }
        }
        if error != 0 && thread_storage[0].activations.packet_used != 0 { cuda.async_packets = false; }
    }
    if error == 0 && success && (thread_storage[0].activations.count > 0 || thread_storage[0].activations.large_output[1] > 0) {
        if all {
            if push_context() != 1 { error = -1; }
            else {
                error = activation_download_all();
                if error == 0 { error = activation_download_record(thread_storage[0].activations.large_output, cuda.output.address); }
                if finish_context() != 0 { error = -1; }
            }
        } else {
            error = (fg_cuda_activation_read_address(host, bytes) as! i32);
        }
    }
    thread_storage[0].activations.count = 0;
    thread_storage[0].activations.used = 0;
    thread_storage[0].activations.large_output[1] = 0;
    thread_storage[0].activations.target = 0;
    thread_storage[0].activations.pending = false;
    thread_storage[0].activations.active = false;
    fg_cuda_host_unlock(CUDA_HOST_GRAPH_LOCK);
    if thread_storage[0].decode_capture.replays != 0 && (cuda_env("GOOSE_GRAPH_LOG") != 0) {
        fg_cuda_host_diagnostic_22((thread_storage[0].decode_capture.replays as! u64), (thread_storage[0].decode_capture.updates as! u64), (thread_storage[0].decode_capture.maximum as! u64));
        if cuda.decode_reuse
            { fg_cuda_host_diagnostic_23((thread_storage[0].decode_capture.reused as! u64), (thread_storage[0].decode_capture.changed_nodes as! u64)); }
    }
    return error;
}

struct CudaState_i2_arena_probe {
    active: bool,
    peak_end: u64,
    spills: u32, flushes: u32,
}

export fn inkling_arena_window(active: bool, begin: i32) {
    var thread_storage = cuda_thread_state();
    if !active && thread_storage[0].i2_arena_probe.active {
        fg_cuda_host_diagnostic_24((begin as! i32), (cuda.activations.bytes as! u64), (thread_storage[0].i2_arena_probe.peak_end as! u64), (thread_storage[0].i2_arena_probe.spills as! u32), (thread_storage[0].i2_arena_probe.flushes as! u32));
    }
    thread_storage[0].i2_arena_probe.active = active;
    thread_storage[0].i2_arena_probe.peak_end = 0;
    thread_storage[0].i2_arena_probe.spills = 0;
    thread_storage[0].i2_arena_probe.flushes = 0;
}

fn activation_usable_bytes() -> u64 {
    var thread_storage = cuda_thread_state();
    let reserved: u64 = 217 * (MIB_BYTES as! u64);
    let capacity: u64 = cuda.activations.bytes;
    return if thread_storage[0].i2_arena_probe.active && capacity > reserved { capacity - reserved } else { capacity };
}


struct CudaState_i2_depthmix_input {
    host: u64,
    device: u64,
    bytes: u64,
    reads: u32,
}


export fn inkling_arena_residual_open(host: u64) {
    inkling_arena_residual_open_typed(unsafe_c_ptr<c_ptr<f32>>(host));
}

fn inkling_arena_residual_open_typed(host: c_ptr<f32>) {
    var thread_storage = cuda_thread_state();
    thread_storage[0].i2_residual_host = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(host));
    thread_storage[0].i2_residual_device = 0;
    thread_storage[0].i2_attention_scratch = 0;
}

export fn inkling_arena_depthmix_reads() -> u32 {
    var thread_storage = cuda_thread_state();
    return thread_storage[0].i2_depthmix_input.reads;
}

export fn inkling_arena_depthmix_close() -> u32 {
    var thread_storage = cuda_thread_state();
    let reads: u32 = thread_storage[0].i2_depthmix_input.reads;
    thread_storage[0].i2_depthmix_input = cuda_zero_CudaState_i2_depthmix_input();
    inkling_arena_residual_open(0);
    return reads;
}


export fn inkling_prelude_clear() {
    var thread_storage = cuda_thread_state();
    thread_storage[0].inkling_prelude_scales = 0;
}

export fn inkling_moe_retain_output(host: u64, device: u64) {
    inkling_moe_retain_output_typed(unsafe_c_ptr<c_ptr<f32>>(host), device);
}

fn inkling_moe_retain_output_typed(host: c_ptr<f32>, device: u64) {
    var thread_storage = cuda_thread_state();
    thread_storage[0].inkling_moe_resident_host = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(host));
    thread_storage[0].inkling_moe_output_slot = device;
}

export fn inkling_arena_depthmix(host_input: u64, host_next: u64, host_query: u64, host_output: u64, rows: i32, save_history: bool, full_outputs: u64) -> bool {
    return inkling_arena_depthmix_typed(unsafe_c_ptr<c_ptr<const f32>>(host_input), unsafe_c_ptr<c_ptr<const f32>>(host_next), unsafe_c_ptr<c_ptr<const f32>>(host_query), unsafe_c_ptr<c_ptr<f32>>(host_output), rows, save_history, full_outputs);
}

fn inkling_arena_depthmix_typed(host_input: c_ptr<const f32>, host_next: c_ptr<const f32>,
        host_query: c_ptr<const f32>, host_output: c_ptr<f32>, rows: i32,
        save_history: bool, full_outputs: u64) -> bool {
    var tls = cuda_thread_state();
    var state .= tls[0];
    const batch: u64 = 4096;
    const width: u64 = 2304;
    const plane_bytes: u64 = batch * width * 4;
    if rows < 1 || rows > 4 || !state.i2_arena_probe.active || start_context() != 1 { return false; }

    let split = activation_usable_bytes();
    let raw = cuda.activations.address + split;
    let next = raw + 4 * plane_bytes;
    var output = next + plane_bytes;
    let table = output + plane_bytes;
    let scales = table + 32;
    let scores = scales + batch * 4;
    let query = scores + 5 * batch * 4;
    let pre_attention = host_next == null;
    let queries: i32 = if full_outputs != 0 { 3 } else { 1 };
    if full_outputs != 0 { output = full_outputs; }
    let boundary = pre_attention && save_history && !(state.inkling_prelude_scales != 0 && rows == 1);
    let normalized = if boundary { raw + (rows as! u64) * plane_bytes }
        else if pre_attention { next } else { state.i2_attention_scratch };
    let saved_current = state.inkling_moe_current;
    let residual_input = if host_input != null { normalized + plane_bytes } else { saved_current };
    let current = if pre_attention && host_input == null { saved_current } else { next };
    let required = 6 * plane_bytes + 32 + (6 * batch + width) * 4;
    let next_host_address = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(host_next));
    var status: i32 = if (host_input != null || saved_current != 0) && (!boundary || rows < 4) &&
        cuda.activations.bytes - split >= required && normalized != 0 &&
        (pre_attention || (state.i2_residual_host == next_host_address && state.i2_residual_device == next)) {
        0
    } else { -1 };

    var module .= cuda_saved_inkling_arena_depthmix_module;
    var prepare .= cuda_saved_inkling_arena_depthmix_prepare;
    var score .= cuda_saved_inkling_arena_depthmix_score;
    var mix .= cuda_saved_inkling_arena_depthmix_mix;
    if status == 0 && module == 0 {
        status = fg_cuda_driver_module_load_typed(
            unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)),
            unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_i2_lazy_prelude_image()));
    }
    if status == 0 && prepare == 0 {
        status = cuda_lookup_kernel(cuda_kernel_argument_address(prepare), module, "i2_lazy_prepare");
    }
    if status == 0 && score == 0 {
        status = cuda_lookup_kernel(cuda_kernel_argument_address(score), module, "i2_plane_score");
    }
    if status == 0 && mix == 0 {
        status = cuda_lookup_kernel(cuda_kernel_argument_address(mix), module, "i2_plane_mix");
    }
    var planes: u64[4];
    for i in rows { planes[i] = raw + (i as! u64) * plane_bytes; }
    let add: i32 = if !pre_attention && !save_history { 1 } else { 0 };
    let lazy_index: i32 = if state.inkling_prelude_scales != 0 && rows > 1 { 1 } else { -1 };
    let history_scales = state.inkling_prelude_scales;
    let upload = if pre_attention { next } else if save_history { planes[rows - 1] } else { residual_input };
    if status == 0 && host_input != null {
        status = cuda_capture_upload(upload, unsafe_c_ptr<c_ptr<const void>>(host_input), plane_bytes);
    }
    if status == 0 {
        status = cuda_capture_upload(table, unsafe_c_address<c_ptr<const void>>(planes), (rows as! u64) * 8);
    }

    let prep_args = [
        cuda_kernel_argument_address(current), cuda_kernel_argument_address(residual_input), cuda_kernel_argument_address(next),
        cuda_kernel_argument_address(normalized), cuda_kernel_argument_address(scales), cuda_kernel_argument_address(add),
    ];
    let score_args = [
        cuda_kernel_argument_address(table), cuda_kernel_argument_address(normalized), cuda_kernel_argument_address(query),
        cuda_kernel_argument_address(scores), cuda_kernel_argument_address(rows), cuda_kernel_argument_address(lazy_index),
        cuda_kernel_argument_address(history_scales),
    ];
    let mix_args = [
        cuda_kernel_argument_address(table), cuda_kernel_argument_address(normalized), cuda_kernel_argument_address(scores),
        cuda_kernel_argument_address(output), cuda_kernel_argument_address(rows), cuda_kernel_argument_address(lazy_index),
        cuda_kernel_argument_address(history_scales),
    ];
    if status == 0 {
        status = cuda_capture_launch(prepare, batch as! u32, 1, 1, 128, 1, 1, 0, 0, cuda_slice_address(prep_args), 0);
    }
    let query_host_address = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(host_query));
    for q in queries {
        if status != 0 { break; }
        output = (if full_outputs != 0 { full_outputs } else { next + plane_bytes }) + (q as! u64) * plane_bytes;
        let source = unsafe_c_ptr<c_ptr<const void>>(query_host_address + (q as! u64) * width * 4);
        status = cuda_capture_upload(query, source, width * 4);
        if status == 0 {
            status = cuda_capture_launch(score, (rows + 1) as! u32, batch as! u32, 1, 128, 1, 1, 0, 0,
                cuda_slice_address(score_args), 0);
        }
        if status == 0 {
            status = cuda_capture_launch(mix, (width / 128) as! u32, batch as! u32, 1, 128, 1, 1, 0, 0,
                cuda_slice_address(mix_args), 0);
        }
    }
    if status == 0 { status = cuda_capture_sync(); }
    if status == 0 {
        state.i2_depthmix_input.host = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(host_output));
        state.i2_depthmix_input.device = if full_outputs != 0 { full_outputs } else { output };
        state.i2_depthmix_input.bytes = (queries as! u64) * plane_bytes;
        state.i2_depthmix_input.reads = 0;
    }
    return finish_context() == 0 && status == 0;
}

export fn inkling_arena_boundary_read(output: u64, index: i32, last_only: bool) -> bool {
    return inkling_arena_boundary_read_typed(unsafe_c_ptr<c_ptr<f32>>(output), index, last_only);
}

fn inkling_arena_boundary_read_typed(output: c_ptr<f32>, index: i32, last_only: bool) -> bool {
    var tls = cuda_thread_state();
    const row_bytes: u64 = 2304 * 4;
    const rows: u64 = 4096;
    const plane_bytes: u64 = rows * row_bytes;
    if !tls[0].i2_arena_probe.active || start_context() != 1 { return false; }

    let split = activation_usable_bytes();
    var status: i32 = -1;
    if index >= 0 && index < 4 && cuda.activations.bytes >= split &&
        cuda.activations.bytes - split >= ((index + 1) as! u64) * plane_bytes {
        let plane = cuda.activations.address + split + (index as! u64) * plane_bytes;
        let offset = if last_only { (rows - 1) * row_bytes } else { 0 };
        let bytes = if last_only { row_bytes } else { plane_bytes };
        status = cuda_capture_download(fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(output)),
            plane + offset, bytes);
    }
    return finish_context() == 0 && status == 0;
}

const i2_loop_batch: i64 = 4096;
const i2_loop_width: i64 = 2304;
const i2_loop_plane: i64 = i2_loop_batch * i2_loop_width * 4;

export fn inkling_prelude_use(capture: bool) -> bool {
    var tls = cuda_thread_state();
    var state .= tls[0];
    if !state.i2_arena_probe.active || start_context() != 1 { return false; }
    let split = activation_usable_bytes();
    let raw = cuda.activations.address + split;
    let plane = i2_loop_plane as! u64;
    let prelude = raw + plane;
    let next = raw + 4 * plane;
    let normalized: u64 = 0;
    let scales = raw + 6 * plane + 131072;
    let required = 6 * plane + 131072 + (i2_loop_batch as! u64) * 4;
    var status: i32 = if cuda.activations.bytes >= split && cuda.activations.bytes - split >= required { 0 } else { -1 };

    if status == 0 && capture {
        var module .= cuda_saved_inkling_prelude_use_module;
        var prepare .= cuda_saved_inkling_prelude_use_prepare;
        if state.inkling_moe_current == 0 { status = -1; }
        if status == 0 && module == 0 {
            status = fg_cuda_driver_module_load_typed(
                unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)),
                unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_i2_lazy_prelude_image()));
        }
        if status == 0 && prepare == 0 {
            status = cuda_lookup_kernel(cuda_kernel_argument_address(prepare), module, "i2_lazy_prepare");
        }
        if status == 0 { status = cuda_capture_copy_device(prelude, state.inkling_moe_current, plane); }
        if status == 0 {
            let add: i32 = 0;
            let args = [
                cuda_kernel_argument_address(prelude), cuda_kernel_argument_address(prelude),
                cuda_kernel_argument_address(next), cuda_kernel_argument_address(normalized),
                cuda_kernel_argument_address(scales), cuda_kernel_argument_address(add),
            ];
            status = cuda_capture_launch(prepare, i2_loop_batch as! u32, 1, 1, 128, 1, 1, 0, 0,
                cuda_slice_address(args), 0);
        }
        if status == 0 { status = cuda_capture_sync(); }
        if status == 0 { state.inkling_prelude_scales = scales; }
    }
    if status == 0 && state.inkling_prelude_scales == 0 { status = -1; }
    if status == 0 { state.inkling_moe_current = prelude; }
    return finish_context() == 0 && status == 0;
}

export fn inkling_prefill_final_rows(current: u64, prelude: u64) -> bool {
    return inkling_prefill_final_rows_typed(unsafe_c_ptr<c_ptr<f32>>(current), unsafe_c_ptr<c_ptr<f32>>(prelude));
}

fn inkling_prefill_final_rows_typed(current: c_ptr<f32>, prelude: c_ptr<f32>) -> bool {
    var tls = cuda_thread_state();
    if !tls[0].i2_arena_probe.active || start_context() != 1 { return false; }
    let row_bytes = (i2_loop_width * 4) as! u64;
    let row = (i2_loop_batch - 1) as! u64;
    var status: i32 = 0;
    if current != null {
        let device = tls[0].inkling_moe_current;
        status = if device == 0 { -1 } else {
            cuda_capture_download(fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(current)),
                device + row * row_bytes, row_bytes)
        };
    }
    if status == 0 && prelude != null {
        let device = tls[0].inkling_prelude_scales;
        let destination = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(prelude));
        let plane = cuda.activations.address + activation_usable_bytes() + (i2_loop_plane as! u64);
        var scale: f32 = 0;
        status = if device == 0 { -1 } else {
            cuda_capture_download(cuda_kernel_argument_address(scale), device + row * 4, 4)
        };
        if status == 0 { status = cuda_capture_download(destination, plane + row * row_bytes, row_bytes); }
        if status == 0 {
            var data = fg_cuda_host_view_f32(destination, i2_loop_width);
            for i in data.len { data[i] *= scale; }
        }
    }
    return finish_context() == 0 && status == 0;
}

struct CudaState_i2_device_loop {
    storage: u64,
    queries: u64,
    host: u64[4],
    selected: i32,
    generated: u32,
}

export fn inkling_loop_save(storage: u64, previous: u64) -> bool {
    return inkling_loop_save_typed(storage, unsafe_c_ptr<c_ptr<const f32>>(previous));
}

fn inkling_loop_save_typed(storage: u64, previous: c_ptr<const f32>) -> bool {
    var tls = cuda_thread_state();
    var state .= tls[0];
    if storage == 0 || (previous == null && state.inkling_moe_current == 0) ||
        !state.i2_arena_probe.active || start_context() != 1 { return false; }
    let plane = i2_loop_plane as! u64;
    let split = activation_usable_bytes();
    let raw = cuda.activations.address + split;
    let source = raw + 2 * plane;
    let current = storage + plane;
    let prepared_row = storage + 2 * plane;
    let scale = raw + 6 * plane;
    let required = 6 * plane + (i2_loop_batch as! u64) * 4;
    var status: i32 = if cuda.activations.bytes >= split && cuda.activations.bytes - split >= required { 0 } else { -1 };

    var module .= cuda_saved_inkling_loop_save_module;
    var prepare .= cuda_saved_inkling_loop_save_prepare;
    if status == 0 && module == 0 {
        status = fg_cuda_driver_module_load_typed(
            unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)),
            unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_i2_lazy_prelude_image()));
    }
    if status == 0 && prepare == 0 {
        status = cuda_lookup_kernel(cuda_kernel_argument_address(prepare), module, "i2_lazy_prepare");
    }
    if status == 0 { status = cuda_capture_copy_device(storage, source, plane); }
    if status == 0 && previous != null {
        status = cuda_capture_upload(current, unsafe_c_ptr<c_ptr<const void>>(previous), plane);
    }
    if status == 0 {
        let previous_device = if previous != null { current } else { state.inkling_moe_current };
        let add: i32 = 0;
        let args = [
            cuda_kernel_argument_address(previous_device), cuda_kernel_argument_address(previous_device),
            cuda_kernel_argument_address(prepared_row), cuda_kernel_argument_address(current),
            cuda_kernel_argument_address(scale), cuda_kernel_argument_address(add),
        ];
        status = cuda_capture_launch(prepare, i2_loop_batch as! u32, 1, 1, 128, 1, 1, 0, 0,
            cuda_slice_address(args), 0);
    }
    if status == 0 { status = cuda_capture_sync(); }
    let popped = finish_context();
    if status != 0 || popped != 0 { return false; }
    state.i2_device_loop.storage = storage;
    return true;
}

fn inkling_host_address(pointer: c_ptr<const f32>) -> u64 {
    return fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(pointer));
}

export fn inkling_loop_bind(queries: u64, q: u64, k: u64, v: u64, ffn: u64) {
    inkling_loop_bind_typed(unsafe_c_ptr<c_ptr<const f32>>(queries), unsafe_c_ptr<c_ptr<const f32>>(q), unsafe_c_ptr<c_ptr<const f32>>(k), unsafe_c_ptr<c_ptr<const f32>>(v), unsafe_c_ptr<c_ptr<const f32>>(ffn));
}

fn inkling_loop_bind_typed(queries: c_ptr<const f32>, q: c_ptr<const f32>, k: c_ptr<const f32>,
        v: c_ptr<const f32>, ffn: c_ptr<const f32>) {
    var tls = cuda_thread_state();
    var state .= tls[0].i2_device_loop;
    state.queries = inkling_host_address(queries);
    state.host = [inkling_host_address(q), inkling_host_address(k), inkling_host_address(v), inkling_host_address(ffn)];
    state.selected = -1;
    state.generated = 0;
}

export fn inkling_loop_generated() -> u32 {
    let tls = cuda_thread_state();
    return tls[0].i2_device_loop.generated;
}

export fn inkling_loop_release() {
    var tls = cuda_thread_state();
    tls[0].i2_device_loop = cuda_zero_CudaState_i2_device_loop();
}

fn inkling_loop_materialize(host: u64, bytes: i64, device: u64&) -> i32 {
    var tls = cuda_thread_state();
    var state .= tls[0].i2_device_loop;
    if !tls[0].activations.active || state.storage == 0 || state.queries == 0 ||
        bytes != i2_loop_plane || host == 0 { return 0; }
    var index: i32 = 0;
    while index < 4 && state.host[index] != host { index += 1; }
    if index == 4 { return 0; }

    let storage = state.storage;
    let plane = i2_loop_plane as! u64;
    device = storage + 2 * plane;
    if state.selected == index { return 1; }
    let current = storage + plane;
    let table = cuda.activations.address + activation_usable_bytes() + 6 * plane;
    let scores = table + 8;
    let query = scores + 2 * (i2_loop_batch as! u64) * 4;

    var module .= cuda_saved_inkling_loop_materialize_module;
    var score .= cuda_saved_inkling_loop_materialize_score;
    var mix .= cuda_saved_inkling_loop_materialize_mix;
    var status: i32 = 0;
    if module == 0 {
        status = fg_cuda_driver_module_load_typed(
            unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)),
            unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_i2_lazy_prelude_image()));
    }
    if status == 0 && score == 0 {
        status = cuda_lookup_kernel(cuda_kernel_argument_address(score), module, "i2_plane_score");
    }
    if status == 0 && mix == 0 {
        status = cuda_lookup_kernel(cuda_kernel_argument_address(mix), module, "i2_plane_mix");
    }
    if status == 0 { status = cuda_capture_upload(table, unsafe_c_address<c_ptr<const void>>(storage), 8); }
    if status == 0 {
        let row_bytes = (i2_loop_width * 4) as! u64;
        let source = unsafe_c_ptr<c_ptr<const void>>(state.queries + (index as! u64) * row_bytes);
        status = cuda_capture_upload(query, source, row_bytes);
    }
    if status != 0 { return -1; }

    let scales: u64 = 0;
    let rows: i32 = 1;
    let lazy: i32 = -1;
    let score_args = [
        cuda_kernel_argument_address(table), cuda_kernel_argument_address(current),
        cuda_kernel_argument_address(query), cuda_kernel_argument_address(scores),
        cuda_kernel_argument_address(rows), cuda_kernel_argument_address(lazy), cuda_kernel_argument_address(scales),
    ];
    let mix_args = [
        cuda_kernel_argument_address(table), cuda_kernel_argument_address(current),
        cuda_kernel_argument_address(scores), cuda_kernel_argument_address(device),
        cuda_kernel_argument_address(rows), cuda_kernel_argument_address(lazy), cuda_kernel_argument_address(scales),
    ];
    status = cuda_capture_launch(score, 2, i2_loop_batch as! u32, 1, 128, 1, 1, 0, 0,
        cuda_slice_address(score_args), 0);
    if status == 0 {
        status = cuda_capture_launch(mix, (i2_loop_width / 128) as! u32, i2_loop_batch as! u32, 1,
            128, 1, 1, 0, 0, cuda_slice_address(mix_args), 0);
    }
    if status != 0 { return -1; }
    tls[0].activations.pending = true;
    state.selected = index;
    state.generated |= (1 as! u32) << (index as! u32);
    return 1;
}

fn activation_reserve_output(bytes: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    if thread_storage[0].i2_arena_probe.active { thread_storage[0].i2_arena_probe.spills += 1; }
    if thread_storage[0].activations.active && thread_storage[0].activations.defer_waits { thread_storage[0].activations.pending = true; }
    let error: i32 = activation_download_record(thread_storage[0].activations.large_output, cuda.output.address);
    if error != 0 { return error; }
    thread_storage[0].activations.large_output[1] = 0;
    return reserve(&cuda.output, bytes);
}

var giga_profile_selected: bool;

export fn fg_cuda_gigachat4_select_profile() {
    giga_profile_selected = true;
}

fn activation_growth_capacity() -> u64 {
    if !giga_profile_selected { return WIDE_ACTIVATION_BYTES as! u64; }
    var setting: u64 = cuda_env("GOOSE_GIGA_WIDE_ACTIVATION");
    if cuda_env_equal(setting, "0") { return WIDE_ACTIVATION_BYTES as! u64; }
    let giga_bytes: u64 = 768 * (MIB_BYTES as! u64);
    let giga_large_bytes: u64 = 1024 * (MIB_BYTES as! u64);
    let headroom: u64 = 2048 * (MIB_BYTES as! u64);
    var available: u64 = 0;
    var total: u64 = 0;
    if (cuda.memory_info != 0) && cuda_capture_memory_info(cuda_kernel_argument_address(available), cuda_kernel_argument_address(total)) == 0 {
        if available >= giga_large_bytes + headroom { return giga_large_bytes; }
        if available >= giga_bytes + headroom { return giga_bytes; }
    }
    return WIDE_ACTIVATION_BYTES as! u64;
}

fn activation_widen() -> i32 {
    var thread_storage = cuda_thread_state();
    cuda.adapt_activation_capacity = false;
    var larger: device_buffer = default<device_buffer>();
    var capacity: u64 = activation_growth_capacity();
    var error: i32 = reserve(&larger, capacity);
    while error == 2 && capacity > (WIDE_ACTIVATION_BYTES as! u64) {
        capacity = if capacity > 768 * (MIB_BYTES as! u64) { 768 * (MIB_BYTES as! u64) } else { WIDE_ACTIVATION_BYTES as! u64 };
        error = reserve(&larger, capacity);
    }
    if error == 2 { return 0; }
    if error != 0 { return error; }
    error = activation_download_all();
    if error == 0 { error = release_buffer(&cuda.activations); }
    if error != 0 {
        release_buffer(&larger);
        return error;
    }
    thread_storage[0].activations.count = 0;
    thread_storage[0].activations.used = 0;
    cuda.activations = larger;
    cuda.activation_capacity = capacity;
    if cuda_env("GOOSE_GIGA_ARENA_REPORT") != 0 && giga_profile_selected
        { fg_cuda_host_diagnostic_25(capacity / (MIB_BYTES as! u64)); }
    return 0;
}

fn stage_last_matrix_input(input: const u8[:], window: i64, bytes: i64, staged: u64&) -> i32 {
    var thread_storage = cuda_thread_state();
    let address: u64 = ((cuda_slice_address(input)) as! u64);
    let index: i64 = fg143_activation_last_input(activation_ranges(), address, input.len,
        thread_storage[0].activations.step, window, bytes);
    if index < 0 { return 0; }
    let range = ACTIVATION_WORDS * index;
    var error: i32 = reserve(&cuda.input, (input.len as! u64));
    if error == 2 { return 0; }
    if error == 0 { error = cuda_capture_copy_device(cuda.input.address,
        cuda.activations.address + (thread_storage[0].activations.ranges[range + 2] as! u64), (input.len as! u64)); }
    if error != 0 { return error; }
    thread_storage[0].activations.ranges[range + 1] = 0;
    staged = cuda.input.address;
    thread_storage[0].activations.hits += 1;
    return 0;
}

// Called with the CUDA context bound. Reserve output before resolving input
// addresses so a flush never invalidates a pointer passed to a launch.
fn activation_output_for_input(host: u64, bytes: i64, device: u64&, input: const u8[:], staged: u64[:]) -> i32 {
    var thread_storage = cuda_thread_state();
    // All kernels and synchronous copies use the same legacy default stream.
    // Check its completion at graph exit even if a later operation fails.
    if thread_storage[0].activations.active && thread_storage[0].activations.defer_waits { thread_storage[0].activations.pending = true; }
    let eligible: bool = thread_storage[0].activations.active && !thread_storage[0].activations.unavailable && host != 0 &&
        host == thread_storage[0].activations.target && bytes > 0 && bytes <= thread_storage[0].activations.target_bytes;
    if eligible && cuda.adapt_activation_capacity &&
        bytes > DEFAULT_ACTIVATION_BYTES && bytes <= WIDE_ACTIVATION_BYTES {
        let error: i32 = activation_widen();
        if error != 0 { return error; }
    }
    if eligible && (bytes as! u64) <= cuda.activation_capacity {
        if (cuda.activations.address == 0) {
            let error: i32 = reserve(&cuda.activations, cuda.activation_capacity);
            if error == 2 && cuda.activation_capacity > (DEFAULT_ACTIVATION_BYTES as! u64) {
                fg_cuda_host_diagnostic_26(cuda.activation_capacity / (MIB_BYTES as! u64));
                return error;
            }
            if error == 2 { thread_storage[0].activations.unavailable = true; } // CUDA_ERROR_OUT_OF_MEMORY
            else if error != 0 { return error; }
        }
        if (cuda.activations.address != 0) && (bytes as! u64) <= activation_usable_bytes() {
            let capacity: i64 = (activation_usable_bytes() as! i64);
            var plan: i64[3];
            fg143_activation_output_plan(activation_ranges(), capacity, bytes, thread_storage[0].activations.used,
                ACTIVATION_RECORDS, thread_storage[0].activations.reuse, thread_storage[0].activations.selective_eviction, fg_cuda_host_view_i64(cuda_slice_address(plan), (3 as! i64)));
            var pos: i64 = plan[0];
            if pos == -1 { return -1; }
            if plan[1] != 0 {
                if staged.len != 0 {
                    let error = stage_last_matrix_input(input, pos, bytes, staged[0]);
                    if error != 0 { return error; }
                }
                let error: i32 = activation_evict_device_range(pos, bytes);
                if error != 0 { return error; }
            }
            if plan[2] != 0 {
                if thread_storage[0].i2_arena_probe.active { thread_storage[0].i2_arena_probe.flushes += 1; }
                let error: i32 = activation_download_all();
                if error != 0 { return error; }
                thread_storage[0].activations.count = 0;
                thread_storage[0].activations.used = 0;
                pos = 0;
            }
            if pos >= 0 {
                let end: u64 = (pos as! u64) + (bytes as! u64);
                if thread_storage[0].i2_arena_probe.active && end > thread_storage[0].i2_arena_probe.peak_end { thread_storage[0].i2_arena_probe.peak_end = end; }
                device = cuda.activations.address + (pos as! u64);
                return 0;
            }
        }
    }
    let error: i32 = activation_reserve_output((bytes as! u64));
    if error == 0 { device = cuda.output.address; }
    return error;
}

fn activation_output(host: u64, bytes: i64, device: u64&) -> i32 {
    return activation_output_for_input(host, bytes, device, fg_cuda_host_view_u8(0, 0), fg_cuda_host_view_u64(0, 0));
}

fn activation_publish(host: u64, bytes: i64, device: u64, complete: bool) -> i32 {
    var thread_storage = cuda_thread_state();
    let large: bool = device == cuda.output.address;
    // The fallback allocation is already sized for this result. Let readers
    // use it directly until the next writer needs the allocation.
    let keep_large: bool = large && bytes > DEFAULT_ACTIVATION_BYTES &&
        host == thread_storage[0].activations.target && bytes == thread_storage[0].activations.target_bytes;
    if !thread_storage[0].activations.active || (large && !keep_large) {
        return cuda_capture_download((host as! u64), device, (bytes as! u64));
    }
    let error: i32 = if complete || thread_storage[0].activations.defer_waits { 0 } else { cuda_capture_sync() };
    if error != 0 { return error; }
    let offset: i64 = if large { 0 } else { ((device - cuda.activations.address) as! i64) };
    let deadline: i64 = if host == thread_storage[0].activations.target { thread_storage[0].activations.target_deadline } else { -1 };
    let records = if large { activation_large_range() } else { fg_cuda_host_view_i64(cuda_slice_address(thread_storage[0].activations.ranges), (ACTIVATION_RECORDS * ACTIVATION_WORDS as! i64)) };
    let slot: i64 = fg143_activation_publish_record(records, if large { 0 } else { thread_storage[0].activations.count },
        host, bytes, offset, deadline, !large && thread_storage[0].activations.reuse);
    if slot < 0 { return -1; }
    if !large {
        if slot == thread_storage[0].activations.count { thread_storage[0].activations.count += 1; }
        if !thread_storage[0].activations.reuse { thread_storage[0].activations.used = offset + bytes; }
    }
    return 0;
}

fn activation_device(host: u64, bytes: i64) -> u64 {
    var thread_storage = cuda_thread_state();
    if thread_storage[0].activations.active && thread_storage[0].i2_residual_device != 0 && bytes > 0 {
        let size: u64 = 36 * (MIB_BYTES as! u64);
        if (bytes as! u64) <= size && host >= thread_storage[0].i2_residual_host &&
            host - thread_storage[0].i2_residual_host <= size - (bytes as! u64)
            { return thread_storage[0].i2_residual_device + (host - thread_storage[0].i2_residual_host); }
    }
    if thread_storage[0].activations.active && thread_storage[0].i2_depthmix_input.device != 0 && bytes > 0 &&
        (bytes as! u64) <= thread_storage[0].i2_depthmix_input.bytes && host >= thread_storage[0].i2_depthmix_input.host {
        let offset: u64 = host - thread_storage[0].i2_depthmix_input.host;
        if offset <= thread_storage[0].i2_depthmix_input.bytes - (bytes as! u64)
            { return thread_storage[0].i2_depthmix_input.device + offset; }
    }
    if thread_storage[0].activations.large_output[1] > 0 {
        let offset: i64 = fg143_activation_lookup(activation_large_range(), host, bytes);
        if offset >= 0 { return cuda.output.address + (offset as! u64); }
    }
    if thread_storage[0].activations.count == 0 { return 0; }
    let offset: i64 = fg143_activation_lookup(activation_ranges(), host, bytes);
    return if offset >= 0 { cuda.activations.address + (offset as! u64) } else { 0 };
}

export fn fg_cuda_wide_vector_ready(x: u64, y: u64, output: u64, bytes: i64) -> bool {
    var thread_storage = cuda_thread_state();
    if !cuda.direct_wide_vectors || !thread_storage[0].activations.active || !thread_storage[0].activations.reuse ||
        thread_storage[0].activations.unavailable || cuda.activations.address == 0 || output != thread_storage[0].activations.target ||
        bytes <= 0 || bytes > thread_storage[0].activations.target_bytes || (bytes as! u64) > activation_usable_bytes() ||
        activation_device(x, bytes) == 0 || activation_device(y, bytes) == 0 { return false; }
    return fg143_activation_slot(activation_ranges(), (activation_usable_bytes() as! i64), bytes) >= 0 &&
        fg143_activation_record_slot(activation_ranges()) < ACTIVATION_RECORDS;
}

const JOBS_RING: i64 = 4;
struct CudaJobsSlot {
    host: u64,
    bytes: u64,
    done: u64,
    busy: bool,
}
var jobs_ring: CudaJobsSlot[JOBS_RING];
var jobs_ring_next: u32;
var jobs_ring_state: i32;

fn upload_jobs_async(device: u64, data: c_ptr<const void>, bytes: u64) -> i32 {
    if cuda_env_equal(cuda_env("GOOSE_JOBS_ASYNC"), "0") || jobs_ring_state == 2 { return -2; }
    if jobs_ring_state == 0 {
        if cuda.host_alloc == 0 || cuda.upload_async == 0 || cuda.event_create == 0 ||
            cuda.event_record == 0 || cuda.event_sync == 0 {
            jobs_ring_state = 2;
            return -2;
        }
        for i in JOBS_RING {
            let output = unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(jobs_ring[i].done));
            if fg_cuda_driver_event_create_typed(output, 0) != 0 {
                jobs_ring_state = 2;
                return -2;
            }
        }
        jobs_ring_state = 1;
    }
    var slot .= jobs_ring[jobs_ring_next];
    if slot.busy {
        if fg_cuda_driver_event_sync(slot.done) != 0 { return -1; }
        slot.busy = false;
    }
    if slot.bytes < bytes {
        if slot.host != 0 && cuda.host_free != 0 { fg_cuda_driver_host_free(slot.host); }
        slot.host = 0;
        slot.bytes = 0;
        let mib = MIB_BYTES as! u64;
        if bytes > UINT64_MAX - (mib - 1) { return -1; }
        let capacity = ((bytes + mib - 1) / mib) * mib;
        var host: c_ptr<void> = null;
        if fg_cuda_driver_host_alloc_typed(unsafe_c_address<c_ptr<c_ptr<void>>>(host), capacity, 0) != 0 ||
            host == null { return -1; }
        slot.host = fg_cuda_replay_node_address(host);
        slot.bytes = capacity;
    }
    fg_cuda_host_memcpy(slot.host, fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(data)), bytes);
    if cuda_capture_upload_async(device, unsafe_c_ptr<c_ptr<const void>>(slot.host), bytes, 0) != 0 { return -1; }
    if fg_cuda_driver_event_record(slot.done, 0) != 0 { return -1; }
    slot.busy = true;
    jobs_ring_next = (jobs_ring_next + 1) % (JOBS_RING as! u32);
    return 0;
}

fn upload_packet(device: u64, data: c_ptr<const void>, bytes: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    if !thread_storage[0].activations.active || !thread_storage[0].activations.defer_waits || !cuda.async_packets || bytes > 4096 {
        return cuda_capture_upload(device, data, bytes);
    }
    if (cuda.packet_host == 0) {
        var host: c_ptr<void> = null;
        if fg_cuda_driver_host_alloc_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(host)), (MIB_BYTES as! u64), 0) != 0 {
            cuda.async_packets = false;
            return cuda_capture_upload(device, data, bytes);
        }
        cuda.packet_host = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(host));
    }
    var host: c_ptr<const void> = snapshot_packet(data, bytes);
    return if host != null { cuda_capture_upload_async(device, host, bytes, 0) } else { cuda_capture_upload(device, data, bytes) };
}

fn activation_input(data: const u8[:], staging: device_buffer&, device: u64&) -> i32 {
    var tls = cuda_thread_state();
    let host = cuda_slice_address(data);
    var materialized: u64 = 0;
    let generated = inkling_loop_materialize(host, data.len, materialized);
    if generated < 0 { return -1; }
    if generated > 0 {
        device = materialized;
        return 0;
    }
    if tls[0].activations.count > 0 || tls[0].activations.large_output[1] > 0 ||
        tls[0].i2_depthmix_input.device != 0 || tls[0].i2_residual_device != 0 {
        let resident = activation_device(host, data.len);
        if resident != 0 {
            let depthmix = tls[0].i2_depthmix_input.device;
            if depthmix != 0 && resident >= depthmix && resident - depthmix < tls[0].i2_depthmix_input.bytes {
                tls[0].i2_depthmix_input.reads += 1;
            }
            device = resident;
            tls[0].activations.hits += 1;
            return 0;
        }
        let status = activation_sync_range(host, data.len, false);
        if status != 0 { return status; }
    }
    let bytes = data.len as! u64;
    let reserved = reserve(staging, bytes);
    if reserved != 0 { return reserved; }
    let uploaded = upload_packet(staging.address, unsafe_c_ptr<c_ptr<const void>>(host), bytes);
    if uploaded == 0 { device = staging.address; }
    return uploaded;
}

type CudaInputSlice = u8[:];
type CudaStagingPointer = c_ptr<device_buffer>;

fn upload_inputs(data: const CudaInputSlice[:], buffers: const CudaStagingPointer[:],
        addresses: u64[:], count: i64) -> i32 {
    if count < 0 || count > data.len || count > buffers.len || count > addresses.len { return -1; }
    for i in count {
        if data[i].len == 0 { continue; }
        let address = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(buffers[i]));
        if address == 0 { return -1; }
        var staging = fg_cuda_host_view_device_buffers(address, 1);
        let status = activation_input(data[i], staging[0], addresses[i]);
        if status != 0 { return status; }
    }
    return 0;
}

export fn fg_cuda_free_bytes() -> i64 {
    let active: i32 = start_context();
    if active != 1 { return active; }
    var bytes: u64 = 0;
    var total: u64 = 0;
    let status: i32 = cuda_capture_memory_info(cuda_kernel_argument_address(bytes), cuda_kernel_argument_address(total));
    return if finish_context() == 0 && status == 0 && bytes <= (INT64_MAX as! u64) { bytes as! i64 } else { -1 };
}

extern fn fg_cuda_host_view_device_buffers(address: u64, length: i64) -> device_buffer[:];
extern "fg_cuda_driver_buffer_slice" fn state_views_address(buffers: const device_buffer[:]) -> u64;

fn prepare_lazy_state(ranges: const i64[:], logical: u64, eager: device_buffer&,
        views: device_buffer[:]& ) -> i32 {
    let count = ranges.len / 4;
    var recurrent: u64 = 0;
    var total: u64 = 0;
    for i in count {
        let at = 4 * i;
        let bytes = ranges[at + 1];
        let kind = ranges[at + 3];
        if bytes <= 0 || (bytes as! u64) > logical - total || kind < 0 || kind > 2 { return -1; }
        total += bytes as! u64;
        if kind != 0 {
            if bytes % 16 != 0 { return -1; }
            recurrent += bytes as! u64;
        }
    }
    if total != logical || total == recurrent { return -1; }
    let address = fg_cuda_host_calloc(count as! u64, 40);
    if address == 0 { return 2; }
    var buffers = fg_cuda_host_view_device_buffers(address, count);
    var error: i32 = 0;
    if recurrent != 0 { error = reserve(eager, recurrent); }
    if error == 0 && recurrent != 0 { error = cuda_capture_zero(eager.address, 0, recurrent); }
    if error != 0 {
        fg_cuda_host_free(address);
        return error;
    }
    var offset: u64 = 0;
    for i in count {
        let at = 4 * i;
        if ranges[at + 3] == 0 { continue; }
        buffers[i].address = eager.address + offset;
        buffers[i].bytes = ranges[at + 1] as! u64;
        offset += buffers[i].bytes;
    }
    views = buffers;
    return 0;
}

export fn fg_cuda_load_model_state(ranges: i64[:], bytes: i64) -> i64 {
    if cuda_slice_address(ranges) == 0 || ranges.len <= 0 || ranges.len % 4 != 0 || bytes <= 0 ||
        (ranges.len as! u64) > SIZE_MAX / 8 { return -1; }
    let metadata_bytes: u64 = (ranges.len as! u64) * 8;
    let records = fg_cuda_host_malloc(metadata_bytes);
    if records == 0 { return -1; }
    fg_cuda_host_memcpy(records, cuda_slice_address(ranges), metadata_bytes);
    if start_context() != 1 { fg_cuda_host_free(records); return -1; }
    var staging: device_buffer = default<device_buffer>();
    var views = fg_cuda_host_view_device_buffers(0, 0);
    var error: i32 = if (cuda.state_logical_bytes != 0) { -1 } else { 0 };
    if error == 0 && cuda.lazy_kv {
        error = prepare_lazy_state(ranges, bytes as! u64, staging, views);
        if error != 0 { error = release_buffer(&staging); }
    }
    if error == 0 && views.len == 0 {
        error = reserve(&staging, (bytes as! u64));
        if error == 0 { error = cuda_capture_zero(staging.address, 0, staging.bytes); }
    }
    if error == 0 {
        cuda.state = staging;
        cuda.state_ranges = fg_cuda_host_view_i64((records as! u64), (ranges.len as! i64));
        cuda.state_views = views;
        cuda.state_logical_bytes = (bytes as! u64);
    } else {
        if (staging.address != 0) { cuda_capture_release(staging.address); }
        fg_cuda_host_free(records);
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_27((if error != 0 { error } else { popped }));
        return -1;
    }
    return bytes;
}

export fn fg_cuda_model_state_bytes() -> i64 {
    return (cuda.state_logical_bytes as! i64);
}

export fn fg_cuda_model_state_startup_bytes() -> i64 {
    return (cuda.state.bytes as! i64);
}

const STATE_NOT_FOUND: i64 = -2;
const STATE_EAGER: i64 = -1;

fn state_region(host: u64, bytes: i64, kind: i64, offset: u64&) -> i64 {
    if cuda.state_logical_bytes == 0 { return STATE_NOT_FOUND; }
    if cuda.state_views.len != 0 {
        let index = qr_state_index(cuda.state_ranges, host, bytes, kind);
        if index < 0 || index / 4 >= cuda.state_views.len { return STATE_NOT_FOUND; }
        offset = host - (cuda.state_ranges[index] as! u64);
        return index / 4;
    }
    let at = qr_state_offset(cuda.state_ranges, host, bytes, kind);
    if at < 0 || (at as! u64) > cuda.state.bytes ||
        (bytes as! u64) > cuda.state.bytes - (at as! u64) { return STATE_NOT_FOUND; }
    offset = at as! u64;
    return STATE_EAGER;
}

fn state_address(host: u64, bytes: i64, kind: i64) -> u64 {
    var offset: u64 = 0;
    let index = state_region(host, bytes, kind, offset);
    if index == STATE_NOT_FOUND { return 0; }
    if index == STATE_EAGER { return cuda.state.address + offset; }
    return cuda.state_views[index].address + offset;
}

// Bound, map and zero a lazy state span while the CUDA context is held.
fn map_state(index: i64, offset: u64, bytes: u64, address: u64&) -> i32 {
    address = 0;
    if index == STATE_NOT_FOUND { return 0; }
    if index == STATE_EAGER {
        address = cuda.state.address + offset;
        return 0;
    }
    var buffer .= cuda.state_views[index];
    if buffer.vmm_failed { return -1; }
    if offset + bytes > buffer.bytes {
        let limit = cuda.state_ranges[4 * index + 1] as! u64;
        let status = reserve_vmm(buffer, offset + bytes, limit, true);
        if status != 0 { return status; }
    }
    address = buffer.address + offset;
    return 0;
}

fn map_state_address(host: u64, bytes: i64, kind: i64, address: u64&) -> i32 {
    var offset: u64 = 0;
    let index = state_region(host, bytes, kind, offset);
    return map_state(index, offset, bytes as! u64, address);
}

export fn fg_cuda_reset_model_state() -> i64 {
    var thread_storage = cuda_thread_state();
    thread_storage[0].published_state.bytes = 0;
    if (cuda.state_logical_bytes == 0) { return 0; }
    if push_context() != 1 { return -1; }
    var status: i32 = if (cuda.state.address != 0) { cuda_capture_zero(cuda.state.address, 0, cuda.state.bytes) } else { 0 };
    block {
        var i: i64 = 0;
        while status == 0 && i < cuda.state_views.len {
            let buffer = cuda.state_views[i];
            if buffer.vmm_failed { status = -1; }
            else if buffer.reservation_bytes != 0 && buffer.bytes != 0 { status = cuda_capture_zero(buffer.address, 0, buffer.bytes); }
            i += 1;
        }
    };
    return if finish_context() == 0 && status == 0 { 0 } else { -1 };
}

export fn fg_cuda_state_write(host: u64, bytes: i64) -> i64 {
    var offset: u64 = 0;
    let buffer = state_region(host, bytes, -1, &offset);
    if buffer == STATE_NOT_FOUND { return 0; }
    if push_context() != 1 { return -1; }
    var target: u64 = 0;
    var status: i32 = map_state(buffer, offset, (bytes as! u64), &target);
    if status == 0 { status = cuda_capture_upload(target, unsafe_c_ptr<c_ptr<const void>>(host), bytes as! u64); }
    return if finish_context() == 0 && status == 0 { 0 } else { -1 };
}

fn read_state(host: u64, bytes: i64, kind: i64) -> i64 {
    var offset: u64 = 0;
    let buffer = state_region(host, bytes, kind, &offset);
    if buffer == STATE_NOT_FOUND { return 0; }
    if push_context() != 1 { return -1; }
    var source: u64 = 0;
    var status: i32 = map_state(buffer, offset, (bytes as! u64), &source);
    if status == 0 { status = cuda_capture_download((host as! u64), source, (bytes as! u64)); }
    return if finish_context() == 0 && status == 0 { 0 } else { -1 };
}

export fn fg_cuda_state_read(host: u64, bytes: i64) -> i64 {
    return read_state(host, bytes, -1);
}

export fn fg_cuda_conv_read(host: u64, bytes: i64) -> i64 {
    return read_state(host, bytes, 2);
}

export fn fg_cuda_gdn_state_read(state: f32[:]) -> i64 {
    if state.len < 0 || state.len > INT64_MAX / (4 as! i64) { return -1; }
    return fg_cuda_state_read(((cuda_slice_address(state)) as! u64), state.len * (4 as! i64));
}

export fn fg_cuda_kv_read(k: u8[:], v: u8[:]) -> i64 {
    var key_offset: u64 = 0;
    var value_offset: u64 = 0;
    let key_buffer = state_region(((cuda_slice_address(k)) as! u64), k.len, 0, &key_offset);
    let value_buffer = state_region(((cuda_slice_address(v)) as! u64), v.len, 0, &value_offset);
    if key_buffer == STATE_NOT_FOUND && value_buffer == STATE_NOT_FOUND { return 0; }
    if push_context() != 1 { return -1; }
    var key: u64 = 0;
    var value: u64 = 0;
    var status: i32 = map_state(key_buffer, key_offset, (k.len as! u64), &key);
    if status == 0 { status = map_state(value_buffer, value_offset, (v.len as! u64), &value); }
    if status == 0 && key != 0 { status = cuda_capture_download(cuda_slice_address(k), key, (k.len as! u64)); }
    if status == 0 && value != 0 { status = cuda_capture_download(cuda_slice_address(v), value, (v.len as! u64)); }
    return if finish_context() == 0 && status == 0 { 0 } else { -1 };
}

fn prefill_fwht(fallback: u64, fixed: u64[2], width: i32, rows: i64) -> u64 {
    if (width != 64 && width != 256) || rows < 128 || fg143_cpu_prefill() == 0 { return fallback; }
    let index: i64 = if width == 256 { 1 } else { 0 };
    return if fixed[index] != 0 { fixed[index] } else { fallback };
}

fn set_q4_rows(source: u64, source_bytes: i64, stride: i64, target: u64, target_bytes: i64, width: i64, rows: i64, rotation: i32, scale: f32) -> i64 {
    var thread_storage = cuda_thread_state();
    if source == 0 || source % 4 != 0 || width <= 0 || width > (INT_MAX as! i64) || width % 32 != 0 ||
        stride < 4 * width || stride % 4 != 0 || source_bytes < 4 * width ||
        rows <= 0 || rows > (INT_MAX as! i64) ||
        (rows > 1 && stride > (source_bytes - 4 * width) / (rows - 1)) { return -1; }
    var blocks: u32 = ((width / 32) as! u32);
    var tasks: u64 = (rows as! u64) * (blocks as! u64);
    if tasks > (INT_MAX as! u64) * 128 || target_bytes != ((18 * tasks) as! i64) { return -1; }
    var offset: u64 = 0;
    let buffer = state_region(target, target_bytes, 0, &offset);
    if buffer == STATE_NOT_FOUND { return 0; }
    let active: i32 = start_context();
    if active != 1 { return active; }

    var input: u64 = 0;
    var destination: u64 = 0;
    var error: i32 = map_state(buffer, offset, (target_bytes as! u64), &destination);
    if error == 0 { error = activation_input(fg_cuda_host_view_u8(((source as! u64) as! u64), (source_bytes as! i64)), &cuda.input, &input); }
    var row_stride: u64 = (stride as! u64);
    var args: u64[5] = [
        cuda_kernel_argument_address(input), cuda_kernel_argument_address(destination), cuda_kernel_argument_address(row_stride),
        cuda_kernel_argument_address(blocks), cuda_kernel_argument_address(tasks),
    ];
    if error == 0 {
        if rotation > 0 {
            var groups: i64 = rows * (width / rotation);
            var fused: u64[5] = [
                cuda_kernel_argument_address(input), cuda_kernel_argument_address(destination), cuda_kernel_argument_address(rotation),
                cuda_kernel_argument_address(groups), cuda_kernel_argument_address(scale),
            ];
            let kernel: u64 = prefill_fwht(cuda.fwht_q4, cuda.fwht_q4_fixed, rotation, groups);
            error = cuda_capture_launch(kernel, (((groups + 3) / 4) as! u32), 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(fused), 0);
            if error == 0 && kernel != cuda.fwht_q4 {
                if !thread_storage[0].cuda_local_set_q4_rows_logged_3124 { thread_storage[0].cuda_local_set_q4_rows_logged_3124 = true; fg_cuda_host_diagnostic_28(); }
            }
        } else {
            error = cuda_capture_launch(cuda.q4_rows, (((tasks + 127) / 128) as! u32), 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        }
        if error == 0 { error = finish_device_state_write(); }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_29((if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    thread_storage[0].cuda_q4_writes += 1;
    return 1;
}

export fn fg_cuda_set_q4_rows(source: u64, source_bytes: i64, stride: i64, target: u64, target_bytes: i64, width: i64, rows: i64) -> i64 {
    return set_q4_rows(source, source_bytes, stride, target, target_bytes, width, rows, 0, (0.0 as! f32));
}

export fn fg_cuda_fwht_q4_enabled() -> bool {
    var thread_storage = cuda_thread_state(); return (cuda.fwht_q4 != 0) && thread_storage[0].activations.active; }

export fn fg_cuda_fwht_q4_rows(input: u64, target: u64, width: i64, rows: i64, rotation: i32, scale: f32) -> i64 {
    if !fg_cuda_fwht_q4_enabled() || (rotation != 64 && rotation != 256) ||
        width <= 0 || width > 65536 || rows <= 0 || rows > 4096 || (width % rotation != 0) { return 0; }
    let result: i64 = set_q4_rows(input, 4 * width * rows, 4 * width, target,
        18 * (width / 32) * rows, width, rows, rotation, scale);
    if result > 0 && (cuda_env("GOOSE_GRAPH_LOG") != 0) { fg_cuda_host_diagnostic_30(); }
    return result;
}

export fn fg_cuda_match_published_state(dst: u64, src: u64, bytes: i64) -> u8 {
    var thread_storage = cuda_thread_state();
    if thread_storage[0].published_state.bytes <= 0 || thread_storage[0].published_state.bytes != bytes ||
        thread_storage[0].published_state.source != src || thread_storage[0].published_state.destination != dst { return 0; }
    thread_storage[0].published_state.bytes = 0;
    return 1;
}

export fn fg_cuda_state_graph_done() -> i32 {
    var thread_storage = cuda_thread_state();
    if thread_storage[0].published_state.bytes == 0 { return 0; }
    fg_cuda_host_diagnostic_31();
    return -1;
}

export fn fg_cuda_load_dense(ranges: i64[:], bytes: i64) -> i64 {
    if cuda_slice_address(ranges) == 0 || ranges.len <= 0 || ranges.len % 3 != 0 || bytes <= 0 ||
        (ranges.len as! u64) > SIZE_MAX / 8 { return -1; }
    let metadata_bytes: u64 = (ranges.len as! u64) * 8;
    let records_address = fg_cuda_host_malloc(metadata_bytes);
    if records_address == 0 { return -1; }
    fg_cuda_host_memcpy(records_address, cuda_slice_address(ranges), metadata_bytes);
    let records = fg_cuda_host_view_i64(records_address, ranges.len);
    if start_context() != 1 { fg_cuda_host_free(records_address); return -1; }
    if (cuda.dense.address != 0) {
        finish_context();
        fg_cuda_host_free(records_address);
        return -1;
    }
    var staging: device_buffer = default<device_buffer>();
    var status = reserve(staging, bytes as! u64);
    block {
        var i: i64 = 0;
        while status == 0 && i < ranges.len {
            let span: i64 = records[i + 1];
            let at: i64 = records[i + 2];
            if records[i] == 0 || span <= 0 || at < 0 || at > bytes || span > bytes - at {
                status = -1;
                break;
            }
            status = cuda_capture_upload(staging.address + (at as! u64),
                unsafe_c_ptr<c_ptr<const void>>((records[i] as! u64)), (span as! u64));
            i += 3;
        }
    };
    if status == 0 {
        cuda.dense = staging;
        cuda.dense_ranges = records;
    } else {
        if (staging.address != 0) { cuda_capture_release(staging.address); }
        fg_cuda_host_free(records_address);
    }
    let popped: i32 = finish_context();
    if status != 0 || popped != 0 {
        fg_cuda_host_diagnostic_32((if status != 0 { status } else { popped as! i32 }));
        return -1;
    }
    return bytes;
}

export fn fg_cuda_dense_bytes() -> i64 {
    return (cuda.dense.bytes as! i64);
}

const EXPERT_LAYOUT_GROUP: i64 = 9;
const EXPERT_LAYOUT_LAYER: i64 = 4;
const TEMP_PREFILL_MIN_EXPERTS: i64 = 64;


fn expert_prefill_layout(cache: expert_cache&, groups: i64[:], bank_bytes: u64) -> bool {
    var thread_storage = cuda_thread_state();
    cache.bank_bytes = bank_bytes;
    if giga_profile_selected || thread_storage[0].cuda_inkling_quants ||
        !cuda_option("GOOSE_CUDA_PREFILL_TEMP_WEIGHTS") { return true; }
    cache.prefill_wait_decode = cuda_option("GOOSE_CUDA_PREFILL_TEMP_WARM_ONLY");
    cache.prefill_double = cuda_option("GOOSE_CUDA_PREFILL_TEMP_DOUBLE");
    cache.prefill_compact = cache.prefill_double && cuda_option("GOOSE_CUDA_PREFILL_TEMP_COMPACT");
    let planes: u64 = if cache.prefill_double && !cache.prefill_compact { 2 } else { 1 };
    var plan: u64[3];
    if !fg143_expert_prefill_plan(groups, bank_bytes, planes, 0, plan) { return false; }
    if plan[1] == 0 { return true; }
    let budget: u64 = if cache.prefill_compact { cuda_env("GOOSE_CUDA_PREFILL_TEMP_MIB") } else { 0 };
    if budget != 0 {
        let mib = fg_cuda_host_positive_env(budget);
        if mib <= 0 ||
            !fg143_expert_prefill_plan(groups, bank_bytes, planes, mib as! u64, plan) { return false; }
    }
    cache.prefill_offset = plan[0];
    cache.prefill_bytes = plan[1];
    cache.prefill_workspace = plan[2];
    return true;
}

fn rebalance_apex_cache(groups: i64[:], bytes: i64, layer_words: i64) -> bool {
    var disabled: u64 = cuda_env("GOOSE_CUDA_DISABLE_CACHE_BALANCE");
    if cuda_env_equal(disabled, "1") { return false; }
    return fg143_expert_balance_apex(groups, bytes, layer_words);
}

var empty_expert_cache: expert_cache;

export fn fg_cuda_load_experts(groups: i64[:], layers: i64[:], bytes: i64, words32: i64, words64: i64) -> i64 {
    if cuda_slice_address(groups) == 0 || cuda_slice_address(layers) == 0 || groups.len <= 0 || layers.len <= 0 ||
        (groups.len as! u64) > SIZE_MAX / 8 ||
        (layers.len as! u64) > SIZE_MAX / 8 ||
        bytes <= 0 || words32 <= 0 || words64 <= 0 ||
        (words32 as! u64) > SIZE_MAX / 4 ||
        (words64 as! u64) > SIZE_MAX / 8 { return -1; }
    var cache = empty_expert_cache;
    cache.groups = fg_cuda_host_view_i64(fg_cuda_host_malloc((groups.len as! u64) * 8), groups.len);
    cache.layers = fg_cuda_host_view_i64(fg_cuda_host_malloc((layers.len as! u64) * 8), layers.len);
    cache.work = fg_cuda_host_view_i64(fg_cuda_host_calloc(words64 as! u64, 8), words64);
    cache.state = fg_cuda_host_calloc(words32 as! u64, 4);
    if cuda_slice_address(cache.groups) == 0 || cuda_slice_address(cache.layers) == 0 || cuda_slice_address(cache.work) == 0 ||
        cache.state == 0 || !expert_prefill_layout(cache, groups, (bytes as! u64)) ||
        start_context() != 1 {
        free_expert_metadata(cache);
        return -1;
    }
    let total_bytes: u64 = if cache.prefill_bytes != 0 { cache.prefill_offset + cache.prefill_workspace } else { cache.bank_bytes };
    var available: u64 = 0;
    var total: u64 = 0;
    var status: i32 = if (cuda.experts.weights.address != 0) { -1 } else { 0 };
    if status == 0 && cache.prefill_bytes != 0 &&
        (cuda_capture_memory_info(cuda_kernel_argument_address(available), cuda_kernel_argument_address(total)) != 0 || total_bytes > available) { status = -1; }
    if status == 0 && cache.prefill_bytes != 0 && cache.prefill_double {
        const disable_timing: i64 = 2;
        status = if (cuda.expert_stream != 0) { fg_cuda_driver_event_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cache.prefill_release)), (disable_timing as! u32)) } else { -1 };
        if status == 0 { status = fg_cuda_driver_event_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cache.prefill_ready)), (disable_timing as! u32)); }
    }
    if status == 0 { status = reserve(cache.weights, total_bytes); }
    if status == 0 && cache.prefill_bytes != 0 {
        let plane_count: u32 = if cache.prefill_double { 2 } else { 1 };
        fg_cuda_host_diagnostic_33(cache.bank_bytes, total_bytes - cache.prefill_offset, plane_count);
    }
    if status == 0 {
        fg_cuda_host_memcpy(cuda_slice_address(cache.groups), cuda_slice_address(groups), (groups.len as! u64) * 8);
        fg_cuda_host_memcpy(cuda_slice_address(cache.layers), cuda_slice_address(layers), (layers.len as! u64) * 8);
        if rebalance_apex_cache(cache.groups, bytes, layers.len) {
            fg_cuda_host_diagnostic_34();
        }
        ec_reset(cache.groups, cache.layers, cache.state, cache.work);
        var disable_one_touch: u64 = cuda_env("GOOSE_CUDA_DISABLE_ONE_TOUCH_SLRU");
        cache.one_touch = bytes == 6592856064 && groups.len == 18 && layers.len == 160 &&
            (!cuda_env_equal(disable_one_touch, "1"));
        if cache.one_touch {
            exp1744_init(0, null);
            fg_cuda_host_diagnostic_35();
        }
        cuda.experts = cache;
    } else {
        if cache.prefill_ready != 0 { fg_cuda_driver_event_destroy(cache.prefill_ready); }
        if cache.prefill_release != 0 { fg_cuda_driver_event_destroy(cache.prefill_release); }
        free_expert_metadata(cache);
    }
    let popped: i32 = finish_context();
    if status != 0 || popped != 0 {
        fg_cuda_host_diagnostic_36((if status != 0 { status } else { popped as! i32 }));
        return -1;
    }
    return bytes;
}

export fn fg_cuda_expert_bytes() -> i64 {
    return (cuda.experts.bank_bytes as! i64);
}

export fn fg_cuda_expert_pin_info(info: i64[:]) -> i64 {
    var enable: u64 = cuda_env("GGML_CUDA_REGISTER_HOST");
    var disable: u64 = cuda_env("GGML_CUDA_NO_PINNED");
    let requested: bool = if (enable != 0) { cuda_env_equal(enable, "1") } else { (disable == 0) || cuda_env_equal(disable, "0") };
    if !requested || (cuda.host_register == 0) || (cuda.host_unregister == 0) || info.len != 4 { return 0; }
    const mib: i64 = 1024 * 1024;
    var setting: u64 = cuda_env("GOOSE_EXPERT_PIN_MIB");
    var budget: i64 = 4096 * mib;
    if (setting != 0) {
        let value = fg_cuda_host_positive_env(setting);
        if value <= 0 || value > INT64_MAX / mib {
            fg_cuda_host_diagnostic_37();
            return 0;
        }
        budget = (value as! i64) * mib;
    }
    let page = fg_cuda_host_page_size();
    let pages = fg_cuda_host_physical_pages(page);
    let available: u64 = fg143_storage_ram_available();
    if page <= 0 || pages <= 0 || pages > INT64_MAX / page || available > (INT64_MAX as! u64) { return 0; }
    info[0] = page;
    info[1] = pages * page;
    info[2] = (available as! i64);
    info[3] = budget;
    return 1;
}

export fn fg_cuda_expert_pin_reason(reason: i64, bytes: i64, available: i64) {
    if reason == 1 { fg_cuda_host_diagnostic_108(bytes, available); } else { fg_cuda_host_diagnostic_109(bytes, available); }
}

export fn fg_cuda_host_register(address: u64, bytes: i64) -> i64 {
    const portable: u32 = 1;
    const read_only: u32 = 8;
    if address == 0 || bytes <= 0 || cuda.host_register == 0 { return -1; }
    let status = fg_cuda_driver_host_register_typed(unsafe_c_ptr<c_ptr<void>>(address),
        bytes as! u64, portable | read_only);
    if status != 0 { fg_cuda_host_diagnostic_38(status); }
    return status as! i64;
}

export fn fg_cuda_host_unregister(address: u64) {
    let status = fg_cuda_driver_host_unregister_typed(unsafe_c_ptr<c_ptr<void>>(address));
    if status != 0 { fg_cuda_host_diagnostic_39(status); }
}

export fn fg_cuda_expert_pin_register(spans: i64[:], bytes: i64) {
    if spans.len <= 0 || spans.len % 2 != 0 || (spans.len as! u64) > SIZE_MAX / 8 ||
        cuda.expert_pins.len > 0 || cuda.host_register == 0 || cuda.host_unregister == 0 { return; }
    let length = (spans.len as! u64) * 8;
    let address = fg_cuda_host_malloc(length);
    if address == 0 { return; }
    fg_cuda_host_memcpy(address, cuda_slice_address(spans), length);
    let pinned = fg_cuda_host_view_i64(address, spans.len);
    if start_context() == 1 {
        let count = ec_manage_pins(pinned, 1);
        if count > 0 {
            cuda.expert_pins = pinned;
            cuda.expert_pinned_bytes = bytes;
            fg_cuda_host_diagnostic_40(count, bytes);
        }
        finish_context();
    }
    if cuda.expert_pins.len == 0 { fg_cuda_host_free(address); }
}

export fn fg_cuda_expert_record(hits: i64, fills: i64, evictions: i64, bytes: i64) {
    cuda.experts.hits += (hits as! u64);
    cuda.experts.fills += (fills as! u64);
    cuda.experts.evictions += (evictions as! u64);
    cuda.experts.uploaded_bytes += (bytes as! u64);
}

export fn fg_cuda_expert_stats(stats: i64[:]) {
    if stats.len != 6 { return; }
    fg_cuda_host_lock(CUDA_HOST_CONTEXT_LOCK);
    stats[0] = (cuda.experts.hits as! i64);
    stats[1] = (cuda.experts.fills as! i64);
    stats[2] = (cuda.experts.evictions as! i64);
    stats[3] = (cuda.experts.uploaded_bytes as! i64);
    stats[4] = cuda.expert_pinned_bytes;
    stats[5] = cuda.expert_pins.len / 2;
    if cuda.experts.prefill_bytes != 0 {
        fg_cuda_host_diagnostic_41((cuda.experts.prefill_projections as! u64), (cuda.experts.prefill_hits as! u64), (cuda.experts.prefill_upload_bytes as! u64), (cuda.experts.prefill_prefetch_used as! u64), (cuda.experts.prefill_prefetch_discarded as! u64));
        if cuda.experts.prefill_compact {
            fg_cuda_host_diagnostic_42((cuda.experts.prefill_full_buffer as! u64), (cuda.experts.prefill_skipped_capacity as! u64));
        }
    }
    fg_cuda_host_unlock(CUDA_HOST_CONTEXT_LOCK);
}

fn init_expert_host_ring() {
    var ring .= cuda.expert_host;
    if !ring.enabled || ring.storage != 0 { return; }
    const disable_timing: i64 = 2;
    const bytes: i64 = EXPERT_HOST_SLOTS * EXPERT_HOST_SLOT_BYTES;
    var info: i64[4];
    let headroom: i64 = 8 * 1024 * MIB_BYTES;
    if fg_cuda_expert_pin_info(info) == 0 ||
        info[3] < cuda.expert_pinned_bytes || info[3] - cuda.expert_pinned_bytes < bytes ||
        info[2] < headroom + bytes {
        ring.enabled = false;
        return;
    }
    for i in 0 .. EXPERT_HOST_SLOTS {
        if fg_cuda_driver_event_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(ring.ready[i])), (disable_timing as! u32)) != 0 {
            ring.enabled = false;
            return;
        }
    }
    var storage: c_ptr<void> = null;
    if fg_cuda_driver_host_alloc_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(storage)), (bytes as! u64), 0) != 0 {
        ring.enabled = false;
        return;
    }
    ring.storage = fg_cuda_replay_node_address(unsafe_c_ptr<c_ptr<void>>(storage));
}



export fn fg_cuda_gigachat4_small_expert_wave(enabled: u8) {
    var thread_storage = cuda_thread_state();
    thread_storage[0].giga_small_expert_wave = enabled != 0;
}

export fn fg_cuda_expert_stage_enabled(active_experts: i64, stage_all: u8) -> u8 {
    var thread_storage = cuda_thread_state();
    let planes: u64 = if stage_all != 0 { 3 } else { 2 };
    if cuda_local_fg_cuda_expert_stage_enabled_small_enabled_3445 < 0 && thread_storage[0].cuda_inkling_quants && active_experts <= 8 {
        cuda_local_fg_cuda_expert_stage_enabled_small_enabled_3445 = if cuda_option("I2_STAGE_SMALL_EXPERTS") { 1 } else { 0 };
    }
    let small_inkling: bool = thread_storage[0].cuda_inkling_quants && cuda_local_fg_cuda_expert_stage_enabled_small_enabled_3445 == 1 &&
        cuda.expert_prefill_staging && cuda.expert_host.enabled;
    if active_experts <= 0 || (stage_all == 0 && active_experts <= 8 && !small_inkling && !thread_storage[0].giga_small_expert_wave) ||
        !thread_storage[0].activations.active || !thread_storage[0].activations.defer_waits ||
        !cuda.defer_matrix_waits || (cuda.expert_stream == 0) ||
        (active_experts as! u64) > SIZE_MAX / planes / 32 { return 0; }
    init_expert_host_ring();
    if cuda.expert_pins.len <= 0 && !cuda.expert_host.enabled { return 0; }
    let capacity: u64 = (active_experts as! u64) * planes;
    if capacity > cuda.expert_upload_capacity {
        let jobs = fg_cuda_host_realloc(expert_jobs_address(cuda.expert_upload_jobs), capacity * 32);
        if jobs == 0 { return 0; }
        cuda.expert_upload_jobs = fg_cuda_host_view_expert_jobs(jobs, capacity as! i64);
        cuda.expert_upload_capacity = capacity;
    }
    cuda.expert_current_staging = stage_all == 0 && cuda.expert_prefill_staging &&
        (cuda.expert_host.enabled || cuda.expert_pins.len > 0);
    return 1;
}

fn expert_source_pinned(source: u64, bytes: u64) -> bool {
    return fg143_expert_source_pinned(cuda.expert_pins, source, bytes);
}

// Forward declaration: submit_pageable_expert

fn expert_upload(offset: i64, source: u64, bytes: i64, staged: bool) -> i64 {
    var tls = cuda_thread_state();
    let bank: const device_buffer& = cuda.experts.weights;
    if bank.address == 0 || source == 0 || offset < 0 || bytes <= 0 { return -1; }
    let at = offset as! u64;
    let length = bytes as! u64;
    if at > bank.bytes || length > bank.bytes - at { return -1; }
    let address = bank.address + at;
    let host = unsafe_c_ptr<c_ptr<const void>>(source);
    let pinned = (staged || cuda.expert_current_staging) && expert_source_pinned(source, length);
    let current = !staged && tls[0].inkling_decode_overlap && cuda.expert_current_staging;
    let ring_available = cuda.expert_host.enabled && bytes <= EXPERT_HOST_SLOT_BYTES;
    let job = expert_upload_job { destination: address, source: source, bytes: length, pageable: !pinned, alignment_padding: [0, 0, 0, 0, 0, 0, 0] };
    var status: i32 = 0;
    if (staged || current) && (pinned || ring_available) {
        if cuda.expert_upload_count >= cuda.expert_upload_capacity { return -1; }
        if cuda.expert_upload_count == 0 {
            // Fence earlier readers before revisiting the chosen cache slots.
            cuda.expert_upload_split = 0;
            cuda.expert_slots_ready = cuda.expert_slots_ring[cuda.expert_slots_index];
            let events = EXPERT_SLOT_EVENTS as! u32;
            cuda.expert_slots_index = (cuda.expert_slots_index + 1) % events;
            if cuda.expert_slots_records < events { cuda.expert_slots_records += 1; }
            status = fg_cuda_driver_event_record(cuda.expert_slots_ready, 0);
        }
        if status == 0 {
            cuda.expert_upload_jobs[cuda.expert_upload_count] = job;
            cuda.expert_upload_count += 1;
            if current { tls[0].inkling_overlap_current_uploads += 1; }
        }
    } else if !staged && cuda.expert_current_staging && pinned {
        tls[0].activations.pending = true;
        status = cuda_capture_upload_async(address, host, length, 0);
    } else if !staged && cuda.expert_current_staging && ring_available {
        tls[0].activations.pending = true;
        status = submit_pageable_expert(job, 0);
    } else if cuda.expert_async_copies && ring_available {
        tls[0].activations.pending = true;
        status = if pinned { cuda_capture_upload_async(address, host, length, 0) }
            else { submit_pageable_expert(job, 0) };
    } else {
        status = cuda_capture_upload(address, host, length);
    }
    if status != 0 { fg_cuda_host_diagnostic_43(status); }
    return if status == 0 { 0 } else { -1 };
}

export fn fg_cuda_expert_upload(offset: i64, source: u64, bytes: i64) -> i64 {
    return expert_upload(offset, source, bytes, false);
}

export fn fg_cuda_expert_stage_upload(offset: i64, source: u64, bytes: i64) -> i64 {
    return expert_upload(offset, source, bytes, true);
}

fn reset_expert_stage() {
    var thread_storage = cuda_thread_state();
    var cache .= cuda.experts;
    cuda.expert_upload_count = 0;
    thread_storage[0].inkling_overlap_current_uploads = 0;
    ec_reset(cache.groups, cache.layers, cache.state, cache.work);
}

fn seal_expert_host_slot(stream: u64) -> i32 {
    var ring .= cuda.expert_host;
    if ring.used == 0 { return 0; }
    let error: i32 = fg_cuda_driver_event_record(ring.ready[ring.index], stream);
    if error == 0 {
        ring.busy[ring.index] = true;
        ring.index = (ring.index + 1) % (EXPERT_HOST_SLOTS as! u64);
        ring.used = 0;
    }
    return error;
}

fn reserve_expert_host_slot(bytes: u64, stream: u64, storage: u64&) -> i32 {
    var ring .= cuda.expert_host;
    var error: i32 = 0;
    if bytes > (EXPERT_HOST_SLOT_BYTES as! u64) - ring.used { error = seal_expert_host_slot(stream); }
    if error == 0 && ring.busy[ring.index] {
        error = fg_cuda_driver_event_sync(ring.ready[ring.index]);
        if error == 0 { ring.busy[ring.index] = false; }
    }
    if error != 0 { return error; }
    storage = ring.storage + ring.index * (EXPERT_HOST_SLOT_BYTES as! u64) + ring.used;
    ring.used += bytes;
    return 0;
}

fn submit_pageable_expert(job: const expert_upload_job&, stream: u64) -> i32 {
    var storage: u64 = 0;
    let error: i32 = reserve_expert_host_slot(job.bytes, stream, &storage);
    if error != 0 { return error; }
    fg_cuda_host_memcpy(storage, job.source, job.bytes);
    return cuda_capture_upload_async(job.destination, unsafe_c_ptr<c_ptr<const void>>(storage), job.bytes, stream);
}

fn submit_parallel_experts(jobs: const expert_upload_job[:], count: u64, bytes: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    var storage: u64 = 0;
    var error: i32 = reserve_expert_host_slot(bytes, cuda.expert_stream, &storage);
    if error != 0 { return error; }
    var split: u64 = 0;
    var first_bytes: u64 = 0;
    while split + 1 < count && first_bytes < bytes / 2 {
        first_bytes += jobs[split].bytes;
        split += 1;
    }
    fg_cuda_host_worker_lock();
    let storage_address = storage;
    expert_copy_worker.task = expert_copy_task { jobs: expert_jobs_window(jobs, 0, split),
        count: split, destination: storage_address };
    fg_cuda_host_worker_signal_ready();
    fg_cuda_host_worker_unlock();
    copy_expert_payloads(expert_copy_task { jobs: expert_jobs_window(jobs, split, count - split),
        count: count - split, destination: storage_address + first_bytes });
    fg_cuda_host_worker_lock();
    while expert_copy_worker.task.count != 0 { fg_cuda_host_worker_wait_done(); }
    fg_cuda_host_worker_unlock();
    block {
        var i: u64 = 0;
        while error == 0 && i < count {
            error = cuda_capture_upload_async(jobs[i].destination, unsafe_c_ptr<c_ptr<const void>>(storage), jobs[i].bytes, cuda.expert_stream);
            storage += jobs[i].bytes;
            i += 1;
        }
    };
    if error == 0 { thread_storage[0].activations.expert_parallel_batches += 1; }
    return error;
}

fn submit_expert_uploads(limit: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    if limit == 0 { return 0; }
    if limit > cuda.expert_upload_count { return -1; }
    // Existing readers release cache slots before this stream writes them.
    // Consumers wait on the recorded completion event.
    var status: i32 = fg_cuda_driver_stream_wait_event(cuda.expert_stream, cuda.expert_slots_ready, 0);
    let parallel: bool = expert_copy_worker.running && limit >= 32;
    block {
        var i: u64 = 0;
        while status == 0 && i < limit {
            let job = cuda.expert_upload_jobs[i];
            thread_storage[0].activations.pending = true;
            var bytes: u64 = 0;
            let count: u64 = if parallel && job.pageable {
                fg143_expert_pageable_run(expert_job_bytes(expert_jobs_window(cuda.expert_upload_jobs, i, limit - i), limit - i),
                    EXPERT_HOST_SLOT_BYTES as! u64, fg_cuda_host_view_u64(cuda_kernel_argument_address(bytes), 1)) as! u64
            } else { 0 };
            if count >= 4 {
                status = submit_parallel_experts(expert_jobs_window(cuda.expert_upload_jobs, i, count), count, bytes);
                i += count;
            } else {
                status = if job.pageable { submit_pageable_expert(job, cuda.expert_stream) } else { cuda_capture_upload_async(job.destination, unsafe_c_ptr<c_ptr<const void>>(job.source), job.bytes, cuda.expert_stream) };
                i += 1;
            }
        }
    };
    if status == 0 { status = seal_expert_host_slot(cuda.expert_stream); }
    if status == 0 { status = fg_cuda_driver_event_record(cuda.expert_uploads_ready, cuda.expert_stream); }
    if status == 0 {
        cuda.expert_upload_count -= limit;
        if (cuda.expert_upload_count != 0) {
            let base = expert_jobs_address(cuda.expert_upload_jobs);
            fg_cuda_host_memmove(base, base + limit * 32, cuda.expert_upload_count * 32);
        }
        thread_storage[0].inkling_overlap_current_uploads = if limit >= thread_storage[0].inkling_overlap_current_uploads { 0 } else { thread_storage[0].inkling_overlap_current_uploads - limit };
        cuda.expert_upload_queued = true;
        return 0;
    }
    if cuda_capture_sync() != 0 { cuda.expert_host.enabled = false; }
    cuda.expert_host.used = 0;
    reset_expert_stage();
    fg_cuda_host_diagnostic_44((status as! i32));
    return -1;
}

const EXPERT_UPLOAD_BATCH_MAX: i64 = 24;

fn submit_expert_stage_range(begin: u64, end: u64, batch_enabled: bool) -> i32 {
    var thread_storage = cuda_thread_state();
    const stream_order: i64 = 1;
    let n: u64 = end - begin;
    let jobs = expert_jobs_window(cuda.expert_upload_jobs, begin, n);
    if batch_enabled && (cuda.upload_batch != 0) && n > 1 && n <= (EXPERT_UPLOAD_BATCH_MAX as! u64) && fg143_expert_pinned_batch(expert_job_bytes(jobs, n)) {
        var dst: u64[EXPERT_UPLOAD_BATCH_MAX];
        var src: u64[EXPERT_UPLOAD_BATCH_MAX];
        var bytes: u64[EXPERT_UPLOAD_BATCH_MAX];
        block {
            var i: u64 = 0;
            while i < n {
                dst[i] = jobs[i].destination;
                src[i] = jobs[i].source;
                bytes[i] = jobs[i].bytes;
                i += 1;
            }
        };
        var attributes: memcpy_batch_attributes = default<memcpy_batch_attributes>();
        attributes.source_access_order = stream_order as! i32;
        var first: u64 = 0;
        thread_storage[0].activations.pending = true;
        let status: i32 = fg_cuda_driver_upload_batch_typed(unsafe_c_ptr<c_ptr<u64>>(fg_cuda_driver_words_address(dst)), unsafe_c_ptr<c_ptr<u64>>(fg_cuda_driver_words_address(src)), unsafe_c_ptr<c_ptr<u64>>(fg_cuda_driver_words_address(bytes)), n, unsafe_c_address<c_ptr<void>>(attributes), unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(first)), 1, unsafe_c_ptr<c_ptr<void>>(cuda.expert_stream));
        if status == 0 && !cuda_local_submit_expert_stage_range_reported_3670 {
            cuda_local_submit_expert_stage_range_reported_3670 = true;
            fg_cuda_host_diagnostic_45();
        }
        return status;
    }
    let parallel: bool = expert_copy_worker.running && end - begin >= 32;
    var status: i32 = 0;
    block {
        var i: u64 = begin;
        while status == 0 && i < end {
            let job = cuda.expert_upload_jobs[i];
            thread_storage[0].activations.pending = true;
            var bytes: u64 = 0;
            let count: u64 = if parallel && job.pageable {
                fg143_expert_pageable_run(expert_job_bytes(expert_jobs_window(cuda.expert_upload_jobs, i, end - i), end - i),
                    EXPERT_HOST_SLOT_BYTES as! u64, fg_cuda_host_view_u64(cuda_kernel_argument_address(bytes), 1)) as! u64
            } else { 0 };
            if count >= 4 {
                status = submit_parallel_experts(expert_jobs_window(cuda.expert_upload_jobs, i, count), count, bytes);
                i += count;
            } else {
                status = if job.pageable { submit_pageable_expert(job, cuda.expert_stream) } else { cuda_capture_upload_async(job.destination, unsafe_c_ptr<c_ptr<const void>>(job.source), job.bytes, cuda.expert_stream) };
                i += 1;
            }
        }
    };
    return status;
}

fn expert_slot_release() -> u64 {
    // The newest release fences the immediately preceding wave. Using the one
    // recorded before it lets the next wave's transfers run while the current
    // layer computes; a wave's destination slots were last read by an evicted
    // wave many layers earlier, so one wave of slack keeps the order intact.
    if cuda.expert_slots_records < 2 { return cuda.expert_slots_ready; }
    return cuda.expert_slots_ring[(cuda.expert_slots_index + EXPERT_SLOT_EVENTS - 2) % EXPERT_SLOT_EVENTS];
}

fn submit_expert_stage(batch_enabled: bool) -> i32 {
    if cuda.expert_upload_count == 0 { return 0; }
    // Existing readers release cache slots before this stream writes them.
    // Consumers wait on the recorded completion events.
    var status: i32 = fg_cuda_driver_stream_wait_event(cuda.expert_stream, expert_slot_release(), 0);
    // The routed gate and up projections read the first two planes, so the
    // third plane may keep transferring while they run. Split the wave only
    // when both parts are nonempty and the caller asked for it.
    let total: u64 = cuda.expert_upload_count;
    let split: u64 = if cuda.expert_split_planes && cuda.expert_upload_split > 0 &&
        cuda.expert_upload_split < total { cuda.expert_upload_split } else { total };
    if status == 0 { status = submit_expert_stage_range(0, split, batch_enabled); }
    if status == 0 && split < total {
        status = fg_cuda_driver_event_record(cuda.expert_planes_ready, cuda.expert_stream);
        if status == 0 { status = submit_expert_stage_range(split, total, batch_enabled); }
    }
    if status == 0 { status = seal_expert_host_slot(cuda.expert_stream); }
    if status == 0 { status = fg_cuda_driver_event_record(cuda.expert_uploads_ready, cuda.expert_stream); }
    if status == 0 {
        cuda.expert_planes_split = split < total;
        cuda.expert_upload_count = 0;
        cuda.expert_upload_split = 0;
        cuda.expert_upload_queued = true;
        return 0;
    }
    if cuda_capture_sync() != 0 { cuda.expert_host.enabled = false; }
    cuda.expert_host.used = 0;
    reset_expert_stage();
    fg_cuda_host_diagnostic_46((status as! i32));
    return -1;
}

fn begin_expert_prepare(defer_prior_wave: bool) -> i32 {
    var thread_storage = cuda_thread_state();
    thread_storage[0].decode_capture.scope = false;
    if decode_capture_submit() != 0 { return -1; }
    if cuda.expert_upload_count != 0 {
        reset_expert_stage();
        return -1;
    }
    if cuda.expert_upload_queued && !defer_prior_wave {
        // The current projection may consume a plane staged by the last one.
        if fg_cuda_driver_stream_wait_event(0, cuda.expert_uploads_ready, 0) != 0 { return -1; }
        cuda.expert_upload_queued = false;
    }
    return 0;
}

struct CudaState_i2_expert_trace {
    active: bool,
    started: bool,
    position: i32,
    visit: i32,
}

fn i2_trace_experts(tensor: u64, active: i64[:]) {
    var thread_storage = cuda_thread_state();
    const experts: i64 = 128;
    const layers: i64 = 24;
    var cache .= cuda.experts;
    if !thread_storage[0].i2_expert_trace.active || cache.groups.len != 9 || active.len != experts ||
        cache.layers.len != 4 * layers || cache.groups[3] != experts { return; }
    var layer: i32 = 0;
    while layer < layers && (cache.layers[4 * layer + 1] as! u64) != tensor { layer += 1; }
    if layer == layers { return; }
    let slots: i64 = cache.groups[5];
    if !thread_storage[0].i2_expert_trace.started {
        let keys: const i32[:] = fg_cuda_host_view_i32(cache.state + (4 * cache.groups[7]) as! u64, slots);
        fg_cuda_host_diagnostic_47((slots as! i64));
        block {
            var i: i64 = 0;
            while i < slots {
                fg_cuda_host_diagnostic_48((keys[i] as! i32));
                i += 1;
            }
        };
        fg_cuda_host_diagnostic_114();
        thread_storage[0].i2_expert_trace.started = true;
    }
    var bits: u64[2];
    block {
        var i: i32 = 0;
        while i < experts {
            if active[i] >= 0 { bits[i / 64] |= (1 as! u64) << ((i % 64) as! u64); }
            i += 1;
        }
    };
    let visit = thread_storage[0].i2_expert_trace.visit;
    thread_storage[0].i2_expert_trace.visit += 1;
    fg_cuda_host_diagnostic_49((thread_storage[0].i2_expert_trace.position as! i32), (visit as! i32), (layer as! i32), (bits[1] as! u64), (bits[0] as! u64));
}

// Driver dependency: decode-lfu
const I2_LFU_EXPERTS: i64 = 128; const I2_LFU_LAYERS: i64 = 24; const I2_LFU_KEYS: i64 = I2_LFU_LAYERS * I2_LFU_EXPERTS;

extern fn i2_lfu_init(argc: i32, argv: c_ptr<c_ptr<i8>>);
extern fn i2_ec_prepare_lfu(groups: i64[:], layers: i64[:], words32: u64, words64: i64[:], tensor: u64, bytes: i64, heads: i64[:], result: i64[:], stage_all: u8) -> i64;

struct CudaState_i2_lfu {
    frequency: u32[I2_LFU_KEYS],
    last_visit: u32[I2_LFU_KEYS],
    visit: u32,
    active: bool,
}

export fn i2_lfu_init_goose() {
    i2_lfu_init(0, null);
}

fn i2_lfu_phase(prefill: bool, position: i32, count: i32) {
    var thread_storage = cuda_thread_state();
    if (cuda_local_i2_lfu_phase_enabled_3799 < 0) {
        var setting: u64 = cuda_env("I2_DECODE_LFU");
        cuda_local_i2_lfu_phase_enabled_3799 = if cuda_env_equal(setting, "1") { 1 } else { 0 };
    }
    if cuda_local_i2_lfu_phase_enabled_3799 == 0 { return; }
    var setting: u64 = cuda_env("I2_PREFILL_LFU");
    let use_prefill = prefill && count == 4096 && !cuda_env_equal(setting, "0");
    if (prefill && position == 0) || (!use_prefill && thread_storage[0].cuda_local_i2_lfu_phase_prefill_trained_3805) {
        var cleared: CudaState_i2_lfu = default<CudaState_i2_lfu>();
        thread_storage[0].i2_lfu = cleared;
    }
    if use_prefill && !thread_storage[0].cuda_local_i2_lfu_phase_prefill_trained_3805 {
        fg_cuda_host_diagnostic_50();
    }
    thread_storage[0].cuda_local_i2_lfu_phase_prefill_trained_3805 = use_prefill;
    thread_storage[0].i2_lfu.active = (use_prefill || !prefill) && count > 0;
    if (thread_storage[0].i2_lfu.active) {

        fg_cuda_host_once_lfu();
    }
}

fn i2_lfu_layer(tensor: u64, count: i64) -> i32 {
    var thread_storage = cuda_thread_state();
    let cache: const expert_cache& = cuda.experts;
    if (!thread_storage[0].i2_lfu.active || count != I2_LFU_EXPERTS ||
        cache.groups.len != 9 || cache.groups[3] != I2_LFU_EXPERTS ||
        cache.layers.len != 4 * I2_LFU_LAYERS) { return -1; }
    block {
        var layer: i32 = 0;
        while layer < I2_LFU_LAYERS {
            let entry = fg_cuda_host_view_i64(cuda_slice_address(cache.layers) + (32 * layer) as! u64, 4);
            if ((entry[1] as! u64) == tensor || (entry[3] as! u64) == tensor) { return layer; }
            layer += 1;
        }
    };
    return -1;
}

fn i2_lfu_observe(layer: i32, tensor: u64, active: i64[:]) {
    var thread_storage = cuda_thread_state();
    if ((cuda.experts.layers[4 * layer + 1] as! u64) != tensor) { return; }
    thread_storage[0].i2_lfu.visit += 1;
    block {
        var e: i32 = 0;
        var cuda_first_e = true;
        while true {
            if cuda_first_e { cuda_first_e = false; } else { e += 1; }
            if !(e < I2_LFU_EXPERTS) { break; }
            if (active[e] < 0) { continue; }
            let key = (layer as! i64) * I2_LFU_EXPERTS + (e as! i64);
            thread_storage[0].i2_lfu.frequency[key] += 1;
            thread_storage[0].i2_lfu.last_visit[key] = thread_storage[0].i2_lfu.visit;
        }
    };
}

export fn fg_i2_lfu_victims(address: u64, slots: i64, layer: i64, seen: i64[:], generation: i64, victims: i64[:]) -> i64 {
    var thread_storage = cuda_thread_state();
    if (address == 0 || slots <= 0 || slots > I2_LFU_KEYS || layer < 0 || layer >= I2_LFU_LAYERS ||
        cuda_slice_address(seen) == 0 || seen.len != I2_LFU_EXPERTS || cuda_slice_address(victims) == 0 || victims.len <= 0 || victims.len > I2_LFU_EXPERTS) { return -1; }
    let keys: const i32[:] = fg_cuda_host_view_i32(address, slots);
    var scores: u64[I2_LFU_EXPERTS];
    var count: i64 = 0;
    block {
        var slot: i64 = 0;
        var cuda_first_slot = true;
        while true {
            if cuda_first_slot { cuda_first_slot = false; } else { slot += 1; }
            if !(slot < slots) { break; }
            let key: i32 = keys[slot];
            if (key >= I2_LFU_KEYS) { return -1; }
            if (key >= 0 && key / I2_LFU_EXPERTS == layer && seen[key % I2_LFU_EXPERTS] == generation) { continue; }
            // The Inkling context limit allows fewer than 2^26 logical visits.
            // Empty slots precede all occupied ones, then frequency, age and ID.
            let score: u64 = if key < 0 { 0 } else { ((thread_storage[0].i2_lfu.frequency[key] as! u64) << 38) |
                ((thread_storage[0].i2_lfu.last_visit[key] as! u64) << 12) | ((key + 1) as! u64) };
            var index: i64 = count;
            if (index == victims.len) {
                index -= 1;
                if (score >= scores[index]) { continue; }
            } else {
                count += 1;
            }
            while (index > 0 && score < scores[index - 1]) {
                scores[index] = scores[index - 1];
                victims[index] = victims[index - 1];
                index -= 1;
            }
            scores[index] = score;
            victims[index] = slot;
        }
    };
    return count;
}

// Forward declaration: i2_warm_visit
// Forward declaration: i2_warm_bucket_ranks
extern fn i2_ec_prepare_buckets(groups: i64[:], layers: i64[:], words32: u64, words64: i64[:], tensor: u64, bytes: i64, heads: i64[:], result: i64[:], stage_all: u8, ranks: i64[:], links: i64[:]) -> i64;


fn prepare_expert_offsets(tensor: u64, bytes: i64, active: i64[:], offsets: i64[:], stage_all: bool, compact: bool, next_plane: bool) -> i64 {
    var thread_storage = cuda_thread_state();
    var cache .= cuda.experts;
    if thread_storage[0].cuda_inkling_quants {
        i2_trace_experts(tensor, active);
        i2_warm_visit(tensor, active);
    }
    let state = cache.state;
    let stage_flag: u8 = if stage_all { 1 } else { 0 };
    let compact_flag: u8 = if compact { 1 } else { 0 };
    let ranks = if thread_storage[0].cuda_inkling_quants { i2_warm_bucket_ranks() } else { fg_cuda_host_view_i64(0, 0) };
    var status: i64;
    if thread_storage[0].cuda_inkling_quants {
        let layer: i32 = i2_lfu_layer(tensor, active.len);
        if layer >= 0 {
            status = i2_ec_prepare_lfu(cache.groups, cache.layers, state, cache.work,
                tensor, bytes, active, offsets, stage_flag);
            if status == 1 { i2_lfu_observe(layer, tensor, active); }
        } else if ranks.len != 0 {
            var links: i64[3072];
            status = i2_ec_prepare_buckets(cache.groups, cache.layers, state, cache.work,
                tensor, bytes, active, offsets, stage_flag, ranks, links);
        } else {
            status = inkling_ec_prepare(cache.groups, cache.layers, state, cache.work,
                tensor, bytes, active, offsets, stage_flag);
        }
    } else {
        let policy: i64 = if compact { (if next_plane { 2 } else { 1 }) } else { 0 };
        status = if cache.one_touch { exp1744_prepare(cache.groups, cache.layers, state, cache.work, tensor, bytes,
                active, offsets, stage_flag, compact_flag, policy) } else { if compact { ec_prepare_compact(cache.groups, cache.layers, state, cache.work, tensor, bytes, active, offsets, stage_flag) } else { ec_prepare(cache.groups, cache.layers, state, cache.work, tensor, bytes, active, offsets, stage_flag) } };
    }
    if cuda.expert_current_staging {
        cuda.expert_current_staging = false;
        if seal_expert_host_slot(0) != 0 {
            cuda_capture_sync();
            cuda.expert_host.used = 0;
            cuda.expert_host.enabled = false;
            reset_expert_stage();
            return -1;
        }
    }
    if status == 1 && cache.prefill_wait_decode && fg143_cpu_prefill() == 0 { cache.decode_seen = true; }
    if status < 0 { reset_expert_stage(); }
    return status;
}

fn prefill_upload_misses(cache: expert_cache&, tensor: u64, bytes: u64, active: const i64[:],
        slots: const i32[:], scratch: u64, stream: u64) -> i32 {
    var tls = cuda_thread_state();
    var packed: u64 = 0;
    var expert: i64 = 0;
    while expert < active.len {
        if active[expert] < 0 || slots[expert] >= 0 {
            expert += 1;
            continue;
        }
        var last = expert + 1;
        while last < active.len && active[last] >= 0 && slots[last] < 0 { last += 1; }
        let count = ((last - expert) as! u64) * bytes;
        let offset = (expert as! u64) * bytes;
        let source = tensor + offset;
        let host = unsafe_c_ptr<c_ptr<const void>>(source);
        let destination = cache.weights.address + scratch + (if cache.prefill_compact { packed } else { offset });
        packed += count;
        tls[0].activations.pending = true;
        var status: i32;
        if expert_source_pinned(source, count) {
            status = cuda_capture_upload_async(destination, host, count, stream);
        } else {
            if stream != 0 { return -1; }
            status = cuda_capture_upload(destination, host, count);
        }
        if status != 0 { return -1; }
        cache.prefill_upload_bytes += count;
        expert = last;
    }
    return 0;
}

// Scratch belongs to the projection; the release and readiness events fence
// the alternate scratch slab before transferring and consuming a new plane.
fn prepare_prefill_plane(tensor: u64, bytes: i64, active: i64[:], offsets: i64[:]) -> i64 {
    let tls = cuda_thread_state();
    var cache .= cuda.experts;
    let next = cache.prefill_next_tensor;
    cache.prefill_next_tensor = 0;
    if next != 0 && fg_cuda_driver_stream_wait_event(0, cache.prefill_ready, 0) != 0 { return -1; }
    if cache.prefill_bytes == 0 || fg143_cpu_prefill() == 0 || !tls[0].activations.active ||
        active.len < TEMP_PREFILL_MIN_EXPERTS || (cache.prefill_wait_decode && !cache.decode_seen) { return 0; }
    if bytes <= 0 || offsets.len < active.len { return -1; }

    var active_bits: u64[64] = [0;64];
    let pipeline = cache.prefill_double && active.len <= 4096;
    var selected: i64 = 0;
    for i in active.len {
        if active[i] < 0 { continue; }
        selected += 1;
        if pipeline { active_bits[i / 64] |= (1 as! u64) << ((i % 64) as! u64); }
    }
    if selected < TEMP_PREFILL_MIN_EXPERTS { return 0; }

    let length = bytes as! u64;
    let experts = active.len as! u64;
    for i in (cache.layers.len / EXPERT_LAYOUT_LAYER) {
        let layer_at = i * EXPERT_LAYOUT_LAYER;
        let group_index = cache.layers[layer_at];
        if group_index < 0 || group_index >= cache.groups.len / EXPERT_LAYOUT_GROUP { continue; }
        let group_at = group_index * EXPERT_LAYOUT_GROUP;
        let group = fg_cuda_host_view_i64(cuda_slice_address(cache.groups) + (group_at * 8) as! u64, EXPERT_LAYOUT_GROUP);
        if group[3] != active.len || group[5] <= 0 { continue; }
        var plane: i64 = 0;
        var plane_offset: u64 = 0;
        while plane < 3 && (cache.layers[layer_at + 1 + plane] as! u64) != tensor {
            plane_offset += group[plane] as! u64;
            plane += 1;
        }
        if plane == 3 || group[plane] != bytes { continue; }
        if length > cache.prefill_bytes / experts { return -1; }

        let slots: const i32[:] = fg_cuda_host_view_i32(cache.work[i] as! u64, active.len);
        let slab_bytes = if cache.prefill_compact { (cache.prefill_workspace / 2) & ~(255 as! u64) }
            else { cache.prefill_bytes };
        var misses: u64 = 0;
        if cache.prefill_compact {
            for e in active.len { if active[e] >= 0 && slots[e] < 0 { misses += 1; } }
        }
        let fits = !cache.prefill_compact || misses * length <= slab_bytes;
        if !fits { cache.prefill_full_buffer += 1; }
        let prefetched = pipeline && fits && next == tensor && cache.prefill_next_bytes == bytes &&
            cache.prefill_next_count == active.len && cache.prefill_next_fills == cache.fills &&
            active_bits == cache.prefill_next_active;
        if next != 0 && !prefetched { cache.prefill_prefetch_discarded += 1; }
        if prefetched { cache.prefill_prefetch_used += 1; }
        let slab: u32 = if prefetched { cache.prefill_next_slot } else { 0 };
        let scratch = cache.prefill_offset + (slab as! u64) * slab_bytes;
        let span = (group[0] as! u64) + (group[1] as! u64) + (group[2] as! u64);
        var hits: u64 = 0;
        var packed: u64 = 0;
        for e in active.len {
            offsets[e] = -1;
            if active[e] < 0 { continue; }
            let slot = slots[e] as! i64;
            if slot < 0 {
                offsets[e] = (scratch + (if cache.prefill_compact { packed } else { (e as! u64) * length })) as! i64;
                packed += length;
            } else {
                if slot >= group[5] { return -1; }
                let offset = (group[6] as! u64) + (slot as! u64) * span + plane_offset;
                if offset > cache.bank_bytes || length > cache.bank_bytes - offset { return -1; }
                offsets[e] = offset as! i64;
                hits += 1;
            }
        }
        if !prefetched && prefill_upload_misses(cache, tensor, length, active, slots, scratch, 0) != 0 {
            return -1;
        }
        if pipeline && plane != 2 && hits < (selected as! u64) {
            let next_plane = if prefetched { 2 } else { 1 - plane };
            let source = cache.layers[layer_at + 1 + next_plane] as! u64;
            let next_bytes = group[next_plane] as! u64;
            let next_slab = 1 - slab;
            let next_scratch = cache.prefill_offset + (next_slab as! u64) * slab_bytes;
            let next_fits = !cache.prefill_compact || misses * next_bytes <= slab_bytes;
            if !fits || !next_fits { cache.prefill_skipped_capacity += 1; }
            if fits && next_fits && expert_source_pinned(source, next_bytes * experts) {
                var status = fg_cuda_driver_event_record(cache.prefill_release, 0);
                if status == 0 { status = fg_cuda_driver_stream_wait_event(cuda.expert_stream, cache.prefill_release, 0); }
                if status == 0 {
                    status = prefill_upload_misses(cache, source, next_bytes, active, slots, next_scratch, cuda.expert_stream);
                }
                if status == 0 { status = fg_cuda_driver_event_record(cache.prefill_ready, cuda.expert_stream); }
                if status != 0 { cuda_capture_sync(); return -1; }
                cache.prefill_next_tensor = source;
                cache.prefill_next_slot = next_slab;
                cache.prefill_next_bytes = next_bytes as! i64;
                cache.prefill_next_count = active.len;
                cache.prefill_next_fills = cache.fills;
                cache.prefill_next_active = active_bits;
            }
        }
        cache.prefill_projections += 1;
        cache.prefill_hits += hits;
        return 1;
    }
    return 0;
}

export fn fg_cuda_prepare_experts(tensor: u64, bytes: i64, active: i64[:], offsets: i64[:]) -> i64 {
    if begin_expert_prepare(false) != 0 { return -1; }
    if (cuda.experts.weights.address == 0) { return 0; }
    let temporary: i64 = prepare_prefill_plane(tensor, bytes, active, offsets);
    if temporary != 0 { return temporary; }
    return prepare_expert_offsets(tensor, bytes, active, offsets, false, false, false);
}

export fn fg_cuda_glu_pack_offer(plan: i64[:]) -> bool {
    var thread_storage = cuda_thread_state();
    if (cuda.mmq_pack_glu == 0) || (cuda.mmq_matrix == 0) || !cuda.activation_reference ||
        !thread_storage[0].activations.active || !thread_storage[0].activations.defer_waits || plan.len != 8 || !cuda.mmq_shared ||
        (cuda.experts.weights.address == 0) ||
        !fg143_routed_layer_matches(cuda.experts.layers, fg_cuda_host_view_i64((cuda_slice_address(plan)), (3 as! i64))) { return false; }
    let gate: u64 = (plan[3] as! u64);
    let up: u64 = (plan[4] as! u64);
    let bytes: i64 = plan[6];
    var a: u64 = activation_device(gate, bytes);
    var b: u64 = activation_device(up, bytes);
    if a == 0 || b == 0 || start_context() != 1 { return false; }
    let extent: i32 = 2;
    var layout: i32 = if plan[7] == 12 { 1 } else { if (cuda.mmq_q6_matrix != 0) { 3 } else { 0 } };
    var first: i32 = 0;
    var words: i32 = 3;
    var jobs: u64 = 0;
    let rows: u64 = (bytes as! u64) / 2048;
    var error: i32 = reserve(&cuda.mmq_packed, rows * 288 * (extent as! u64));
    if error == 0 {
        var args: u64[8] = [
            cuda_kernel_argument_address(a), cuda_kernel_argument_address(b), cuda_kernel_argument_address(jobs),
            cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(extent), cuda_kernel_argument_address(layout),
            cuda_kernel_argument_address(first), cuda_kernel_argument_address(words),
        ];
        thread_storage[0].activations.pending = true;
        error = cuda_capture_launch(cuda.mmq_pack_glu, (2 * extent) as! u32, (rows as! u32), 1, 32, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 { return false; }
    fg143_activation_discard_overwritten(activation_ranges(), gate, bytes);
    fg143_activation_discard_overwritten(activation_ranges(), up, bytes);
    fg143_activation_discard_overwritten(activation_large_range(), gate, bytes);
    fg143_activation_discard_overwritten(activation_large_range(), up, bytes);
    thread_storage[0].matrix_glu.target = (plan[5] as! u64);
    thread_storage[0].matrix_glu.bytes = bytes;
    thread_storage[0].matrix_glu.packed = false;
    return true;
}

export fn fg_cuda_glu_pack_close() -> bool {
    var thread_storage = cuda_thread_state();
    let packed: bool = thread_storage[0].matrix_glu.packed;
    thread_storage[0].matrix_glu.target = 0;
    thread_storage[0].matrix_glu_active = false;
    return packed;
}

export fn fg_cuda_begin(input: u64, input_bytes: i64, output: u64, output_bytes: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    let active: i32 = start_context();
    if active != 1 { return active; }
    thread_storage[0].cuda_matrix_kind = -1;
    thread_storage[0].matrix_input = 0;
    thread_storage[0].matrix_glu_active = false;
    thread_storage[0].matrix_reusable_host = 0;
    if thread_storage[0].matrix_glu.target != 0 {
        var status: i32 = if input == thread_storage[0].matrix_glu.target && input_bytes == thread_storage[0].matrix_glu.bytes { activation_output(output, output_bytes, &thread_storage[0].matrix_output) } else { -1 };
        if status == 0 {
            thread_storage[0].matrix_glu_active = true;
            return 1;
        }
        finish_context();
        return -1;
    }
    let source: const u8[:] = fg_cuda_host_view_u8(((input as! u64) as! u64), input_bytes as! i64);
    let staged = if cuda.stage_matrix_input {
        fg_cuda_host_view_u64(cuda_kernel_argument_address(thread_storage[0].matrix_input), 1)
    } else { fg_cuda_host_view_u64(0, 0) };
    var status: i32 = activation_output_for_input(output, output_bytes, &thread_storage[0].matrix_output, source, staged);
    if status == 0 && thread_storage[0].matrix_input == 0 { status = activation_input(source, &cuda.input, &thread_storage[0].matrix_input); }
    if status == 0 {
        if cuda.mmq_shared_reuse && thread_storage[0].activations.active && thread_storage[0].activations.step >= 0 &&
            input_bytes > 0 && output_bytes > 0 &&
            (if input < output { output - input >= (input_bytes as! u64) } else { input - output >= (output_bytes as! u64) }) &&
            thread_storage[0].matrix_input == activation_device(input, input_bytes) {
            thread_storage[0].matrix_reusable_host = input;
            thread_storage[0].matrix_input_bytes = input_bytes;
        }
        return 1;
    }
    finish_context();
    fg_cuda_host_diagnostic_51((status as! i32));
    return -1;
}

fn launch_mmq_stream(weight: u64, stride: u64, extent: i32, kind: i32, rows: i32, first: i32, first_route: i32, job_words: i32, batch: i32, workers: i32, input_stride: i64) -> i32 {
    var thread_storage = cuda_thread_state();
    let outputs: i64 = ((1 + (rows - 1) / 64) as! i64) * batch;
    let fixup: bool = outputs % workers != 0;
    var error: i32 = if fixup { reserve(&cuda.mmq_partials, (workers as! u64) * 64 * 8 * 4) } else { 0 };
    if error != 0 { return error; }
    var args: u64[15] = [
        cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(thread_storage[0].matrix_output),
        cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_tiles.address), cuda_kernel_argument_address(cuda.mmq_partials.address),
        cuda_kernel_argument_address(stride), cuda_kernel_argument_address(extent), cuda_kernel_argument_address(kind),
        cuda_kernel_argument_address(rows), cuda_kernel_argument_address(first), cuda_kernel_argument_address(first_route),
        cuda_kernel_argument_address(job_words), cuda_kernel_argument_address(batch), cuda_kernel_argument_address(input_stride),
    ];
    error = cuda_capture_launch(cuda.mmq_stream, (workers as! u32), 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
    if error != 0 { return error; }
    thread_storage[0].cuda_mmq_stream_windows += 1;
    if fixup {
        var merge: u64[9] = [
            cuda_kernel_argument_address(thread_storage[0].matrix_output), cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_tiles.address),
            cuda_kernel_argument_address(cuda.mmq_partials.address), cuda_kernel_argument_address(extent), cuda_kernel_argument_address(rows),
            cuda_kernel_argument_address(first), cuda_kernel_argument_address(job_words), cuda_kernel_argument_address(batch),
        ];
        error = cuda_capture_launch(cuda.mmq_stream_fixup, (workers as! u32), 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(merge), 0);
        if error == 0 { thread_storage[0].cuda_mmq_fixups += 1; }
    }
    return error;
}

fn finish_matrix_launches(error: i32) -> i32 {
    var thread_storage = cuda_thread_state();
    // Jobs and intermediate buffers are ordered on the legacy stream.
    // Host publication and graph completion already wait for their writes.
    if error == 0 && cuda.defer_matrix_waits && thread_storage[0].activations.defer_waits {
        thread_storage[0].activations.pending = true;
        return 0;
    }
    let sync_error: i32 = cuda_capture_sync();
    return if error == 0 { sync_error } else { error };
}

fn launch_mmq(weight: u64, stride: u64, extent: i32, kind: i32, rows: i32, jobs: i64[:], job_words: i32, requested_shared_rows: i64, requested_input_stride: i64) -> i32 {
    var thread_storage = cuda_thread_state();
    var shared_rows = requested_shared_rows;
    var input_stride = requested_input_stride;
    const staging_budget: i64 = 16 * 1024 * 1024;
    const q4_kind: i32 = 12;
    const q6_kind: i32 = 14;
    const mmq_d4: i32 = 0;
    const mmq_ds4: i32 = 1;
    const mmq_d4_reciprocal: i32 = 3;
    const mmq_ds4_reciprocal: i32 = 4;
    const wide_reference_columns: i32 = 4096;
    const reference_tile_width: i32 = 128;
    let count = (jobs.len / (job_words as! i64)) as! i32;
    if extent > INT_MAX / 256 || (count as! u64) > SIZE_MAX / 24 { return -1; }
    let reference_matrix = if kind == q6_kind && rows % 128 == 0 { cuda.mmq_q6_matrix }
        else if kind == q4_kind { cuda.mmq_q4_matrix } else { 0 };
    let dense_reference = reference_matrix != 0 && job_words == 2 &&
        ((1 + (rows - 1) / 128) as! i64) * ((1 + (count - 1) / 128) as! i64) <= (1073741823 / extent) as! i64;
    let indexed_reference = reference_matrix != 0 && job_words == 3 &&
        ((kind == q4_kind && rows == 512 && extent == 8) || (kind == q6_kind && rows == 2048 && extent == 2));
    let reference = dense_reference || indexed_reference;
    let routed_x64 = if kind == q4_kind { cuda.mmq_q4_routed_x64 } else { cuda.mmq_q6_routed_x64 };
    let routed_shared = if routed_x64 != 0 { routed_x64 } else if kind == q4_kind { cuda.mmq_q4_routed_shared }
        else { cuda.mmq_q6_routed_shared };
    let dense_shared = if kind == q4_kind { cuda.mmq_q4_dense_shared } else { cuda.mmq_q6_dense_shared };
    let dense_x64 = if kind == q4_kind { cuda.mmq_q4_dense_x64 } else { cuda.mmq_q6_dense_x64 };
    let dense_x128 = if kind == q4_kind { cuda.mmq_q4_dense_x128 } else { cuda.mmq_q6_dense_x128 };
    let wide_geometry = count == wide_reference_columns || (cuda.mmq_dense_partial &&
        (count - 1) / reference_tile_width == (wide_reference_columns - 1) / reference_tile_width);
    let dense_wide = dense_shared != 0 && dense_reference && wide_geometry && rows % 128 == 0;
    let reference_tiles = ((rows / 128) as! i64) * ((wide_reference_columns / reference_tile_width) as! i64);
    let sms = cuda.multiprocessors as! i64;
    // A complete reference tile has a single K interval at this occupancy.
    let unsplit = dense_wide && sms > 0 &&
        100 * reference_tiles / (((reference_tiles - 1) / sms + 1) * sms) >= 90;
    let dense_m64 = kind == q4_kind && cuda.mmq_q4_dense_m64 != 0 && unsplit;
    let dense_q6_half_k = kind == q6_kind && cuda.mmq_q6_dense_half_k != 0 && unsplit;
    let routed_wide = indexed_reference && routed_shared != 0;
    let routed_x128 = if !routed_wide { 0 } else if kind == q4_kind { cuda.mmq_q4_routed_x128 }
        else { cuda.mmq_q6_routed_x128 };
    let routed_down = kind == q4_kind && job_words == 3 && rows == 2048 && extent == 2 &&
        count >= wide_reference_columns && cuda.mmq_q4_routed_down_m128 != 0;
    let routes_per_tile: i32 = if routed_down || routed_x128 != 0 { 128 }
        else if routed_wide { if routed_x64 != 0 { 64 } else { 32 } } else { 8 };
    var reference_columns: i32 = if indexed_reference { -1 } else if dense_reference { count } else { 0 };
    var layout = if kind == q4_kind { if reference { mmq_ds4_reciprocal } else { mmq_ds4 } }
        else { if reference { mmq_d4_reciprocal } else { mmq_d4 } };
    let tiles_address = fg_cuda_host_malloc((count as! u64) * 24);
    if tiles_address == 0 { return -1; }
    let tiles = fg_cuda_host_view_i64(tiles_address, 3 * (count as! i64));
    let tile_count = fg143_mmq_tiles(jobs, job_words as! i64, routes_per_tile as! i64, tiles);
    var window_tiles = fg143_mmq_window(extent as! i64, tile_count, routes_per_tile as! i64, staging_budget);
    if !cuda.mmq_window && window_tiles > 64 { window_tiles = 64; }
    if tile_count <= 0 || tile_count > (count as! i64) || window_tiles <= 0 {
        fg_cuda_host_free(tiles_address);
        return -1;
    }
    let tile_bytes = (tile_count as! u64) * 24;
    if thread_storage[0].matrix_glu_active {
        shared_rows = thread_storage[0].matrix_glu.bytes / 2048;
        input_stride = 2048;
    }
    var shared = thread_storage[0].matrix_glu_active || (cuda.mmq_shared &&
        shared_rows > 0 && shared_rows < (count as! i64) && shared_rows <= 65535 &&
        input_stride >= (extent as! i64) * 1024 &&
        (shared_rows as! u64) * (extent as! u64) * 288 <= (staging_budget as! u64));
    var packed_bytes: u64 = 0;
    var error: i32 = 0;
    loop {
        if cuda.mmq_align_window && dense_wide && !shared {
            let group: i64 = if dense_x128 != 0 && window_tiles >= 16 { 16 }
                else if dense_x64 != 0 && window_tiles >= 8 { 8 } else { 4 };
            window_tiles = fg143_mmq_dense_window(tile_count, window_tiles, group);
        }
        let window_routes = window_tiles * (routes_per_tile as! i64);
        let count64 = count as! i64;
        let routes = (if shared { shared_rows } else if count64 < window_routes { count64 } else { window_routes }) as! u64;
        if routes == 0 || (extent as! u64) > SIZE_MAX / 288 / routes {
            fg_cuda_host_free(tiles_address);
            return -1;
        }
        packed_bytes = routes * (extent as! u64) * 288;
        error = reserve(&cuda.mmq_packed, packed_bytes);
        if error == 2 && shared && !thread_storage[0].matrix_glu_active {
            shared = false;
            continue;
        }
        if error != 2 || window_tiles == 1 { break; }
        window_tiles /= 2;
    }
    if error == 0 { error = reserve(&cuda.mmq_tiles, tile_bytes); }
    if error == 0 && routed_x128 == 0 {
        error = cuda_capture_upload(cuda.mmq_tiles.address, unsafe_c_ptr<c_ptr<const void>>(tiles_address), tile_bytes);
    }
    if !shared { input_stride = 0; }
    let cache_shared = shared && job_words == 3 && thread_storage[0].matrix_reusable_host != 0;
    let dense_stride = (extent as! i64) * 1024;
    let cache_dense = cuda.mmq_dense_reuse && !shared && job_words == 2 && kind == q4_kind &&
        window_tiles >= tile_count && thread_storage[0].matrix_reusable_host != 0 &&
        fg143_mmq_linear_jobs(jobs, dense_stride, thread_storage[0].matrix_input_bytes);
    let cache_rows: i64 = if cache_dense { count as! i64 } else { shared_rows };
    let cache_stride: i64 = if cache_dense { dense_stride } else { input_stride };
    let reuse_pack: bool = (cache_shared || cache_dense) &&
        thread_storage[0].shared_activation_pack.dense == cache_dense && thread_storage[0].shared_activation_pack.host == thread_storage[0].matrix_reusable_host &&
        thread_storage[0].shared_activation_pack.step == thread_storage[0].activations.step - 1 &&
        thread_storage[0].shared_activation_pack.input == thread_storage[0].matrix_input && thread_storage[0].shared_activation_pack.packed == cuda.mmq_packed.address &&
        thread_storage[0].shared_activation_pack.bytes == thread_storage[0].matrix_input_bytes && thread_storage[0].shared_activation_pack.rows == cache_rows &&
        thread_storage[0].shared_activation_pack.stride == cache_stride && thread_storage[0].shared_activation_pack.extent == extent &&
        thread_storage[0].shared_activation_pack.layout == layout;
    thread_storage[0].shared_activation_pack.host = 0;
    if error == 0 && shared {
        if thread_storage[0].matrix_glu_active { thread_storage[0].matrix_glu.packed = true; }
        if cuda.mmq_window { window_tiles = if tile_count < 8191 { tile_count } else { 8191 }; }
        if thread_storage[0].matrix_glu_active {
            // SwiGLU has already been packed while both operands were live.
        } else if !reuse_pack {
            var args: u64[5] = [
                cuda_kernel_argument_address(thread_storage[0].matrix_input), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(extent),
                cuda_kernel_argument_address(layout), cuda_kernel_argument_address(input_stride),
            ];
            error = cuda_capture_launch(cuda.mmq_pack_shared, ((2 * extent) as! u32), (shared_rows as! u32), 1,
                32, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        } else {
            thread_storage[0].activations.shared_pack_reuses += 1;
        }
    }
    var first: i32 = 0;
    var first_route: i32 = 0;
    var weights_prepared: bool = false;
    var pack_args: u64[7] = [
        cuda_kernel_argument_address(thread_storage[0].matrix_input), cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_packed.address),
        cuda_kernel_argument_address(extent), cuda_kernel_argument_address(layout), cuda_kernel_argument_address(first_route),
        cuda_kernel_argument_address(job_words),
    ];
    var matrix_args: u64[15] = [
        cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(thread_storage[0].matrix_output),
        cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_tiles.address), cuda_kernel_argument_address(stride),
        cuda_kernel_argument_address(extent), cuda_kernel_argument_address(kind), cuda_kernel_argument_address(rows),
        cuda_kernel_argument_address(first), cuda_kernel_argument_address(first_route), cuda_kernel_argument_address(job_words),
        cuda_kernel_argument_address(input_stride), cuda_kernel_argument_address(reference_columns), cuda_kernel_argument_address(cuda.multiprocessors),
    ];
    while error == 0 && (first as! i64) < tile_count {
        let remaining = tile_count - (first as! i64);
        let batch = (if remaining > window_tiles { window_tiles } else { remaining }) as! i32;
        let end = first + batch;
        let last = 3 * ((end - 1) as! i64);
        first_route = tiles[3 * (first as! i64)] as! i32;
        let routes = (tiles[last] + tiles[last + 1] - (first_route as! i64)) as! u32;
        let q6_half_k = dense_q6_half_k && !shared && first % 16 == 0 && first_route == 8 * first;
        var columns = routes as! i32;
        var wide: i32 = 0;
        if routed_x128 != 0 {
            let window_offset = 24 * (first as! u64);
            let window = tiles_address + window_offset;
            wide = fg143_mmq_wide_first(fg_cuda_host_view_i64(window, 3 * (batch as! i64))) as! i32;
            error = upload_packet(cuda.mmq_tiles.address + window_offset,
                                  unsafe_c_ptr<c_ptr<const void>>(window), 24 * (batch as! u64));
        }
        if error == 0 && !shared && !reuse_pack {
            if q6_half_k {
                var args: u64[7] = [
                    cuda_kernel_argument_address(thread_storage[0].matrix_input), cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_packed.address),
                    cuda_kernel_argument_address(extent), cuda_kernel_argument_address(layout), cuda_kernel_argument_address(first_route),
                    cuda_kernel_argument_address(columns),
                ];
                error = cuda_capture_launch(cuda.mmq_pack_k_major, ((2 * extent) as! u32), routes, 1,
                    32, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            } else {
                error = cuda_capture_launch(cuda.mmq_pack, ((2 * extent) as! u32), routes, 1,
                    32, 1, 1, 0, 0, fg_cuda_driver_words_address(pack_args), 0);
            }
        }
        if error == 0 {
            let workers: i32 = if cuda.mmq_stream != 0 && !reference {
                fg143_mmq_stream_workers(rows as! i64, batch as! i64, extent as! i64,
                                         cuda.multiprocessors as! i64) as! i32
            } else { 0 };
            if routed_wide {
                var tile: i32 = first;
                var args: u64[11] = [
                    cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(thread_storage[0].matrix_output),
                    cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_tiles.address), cuda_kernel_argument_address(stride),
                    cuda_kernel_argument_address(extent), cuda_kernel_argument_address(rows), cuda_kernel_argument_address(tile),
                    cuda_kernel_argument_address(first_route), cuda_kernel_argument_address(input_stride),
                ];
                if wide > 0 {
                    let tall: u64 = if rows % 128 != 0 { 0 } else { if kind == q4_kind { cuda.mmq_q4_routed_m128 } else { cuda.mmq_q6_routed_pipeline } };
                    let q6_pipeline: bool = tall != 0 && kind == q6_kind;
                    let kernel = if tall != 0 { tall } else { routed_x128 };
                    let row_tile: u32 = if tall != 0 { 128 } else { 16 };
                    let shared_bytes: u32 = if q6_pipeline { Q6_ROUTED_SHARED_BYTES as! u32 } else { 0 };
                    error = cuda_capture_launch(kernel, (rows as! u32) / row_tile,
                        wide as! u32, 1, 256, 1, 1, shared_bytes, 0, fg_cuda_driver_words_address(args), 0);
                    if error == 0 && q6_pipeline { thread_storage[0].activations.q6_routed_pipelined += 1; }
                    if error == 0 && wide < batch { thread_storage[0].cuda_matrix_launches += 1; }
                    tile += wide;
                }
                if error == 0 && wide < batch {
                    error = cuda_capture_launch(routed_shared, (rows as! u32) / 16, ((batch - wide) as! u32),
                        1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
                }
            } else if routed_down {
                var tile: i32 = first;
                var args: u64[11] = [
                    cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(thread_storage[0].matrix_output),
                    cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_tiles.address), cuda_kernel_argument_address(stride),
                    cuda_kernel_argument_address(extent), cuda_kernel_argument_address(rows), cuda_kernel_argument_address(tile),
                    cuda_kernel_argument_address(first_route), cuda_kernel_argument_address(input_stride),
                ];
                error = cuda_capture_launch(cuda.mmq_q4_routed_down_m128, (rows as! u32) / 128, (batch as! u32),
                    1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            } else if q6_half_k {
                let pipeline: bool = cuda.mmq_q6_dense_pipeline != 0;
                let matrix: u64 = if pipeline { cuda.mmq_q6_dense_pipeline } else { cuda.mmq_q6_dense_half_k };
                var args: u64[8] = [
                    cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(thread_storage[0].matrix_output),
                    cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(stride), cuda_kernel_argument_address(extent),
                    cuda_kernel_argument_address(first_route), cuda_kernel_argument_address(columns),
                ];
                error = cuda_capture_launch(matrix, (rows as! u32) / 128, 1 + (routes - 1) / 128,
                    1, 256, 1, 1, (if pipeline { Q6_PIPE_SHARED_BYTES } else { Q6_HALF_K_SHARED_BYTES }) as! u32, 0, fg_cuda_driver_words_address(args), 0);
                if error == 0 && pipeline { thread_storage[0].activations.q6_pipelined += 1; }
            } else if dense_m64 && !shared && first % 16 == 0 && first_route == 8 * first {
                let large: bool = (cuda.mmq_q4_dense_large != 0) && routes >= cuda.mmq_q4_large_cols &&
                    (rows as! u32) % cuda.mmq_q4_large_rows == 0;
                // The same legacy stream orders the temporary weight image
                // with every producer and consumer of attention scratch.
                let prepare: bool = large && (cuda.mmq_q4_dense_prepared != 0) && routes >= 1024 && extent <= 65535 / 4 &&
                    cuda.attention_scratch.address != 0 && (rows as! u64) * (extent as! u64) * 320 <= cuda.attention_scratch.bytes;
                var matrix_weights: u64 = if prepare { cuda.attention_scratch.address } else { weight };
                if prepare && !weights_prepared {
                    var args: u64[4] = [
                        cuda_kernel_argument_address(weight), cuda_kernel_argument_address(matrix_weights), cuda_kernel_argument_address(stride),
                        cuda_kernel_argument_address(extent),
                    ];
                    error = cuda_capture_launch(cuda.mmq_q4_prepare, (rows as! u32) / 16, 4 * (extent as! u32), 1,
                        32, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
                    if error == 0 {
                        weights_prepared = true;
                        thread_storage[0].activations.q4_prepared += 1;
                    }
                }
                let pipeline: bool = prepare && cuda.mmq_q4_dense_pipeline != 0;
                let matrix: u64 = if pipeline { cuda.mmq_q4_dense_pipeline } else { if prepare { cuda.mmq_q4_dense_prepared } else { if large { cuda.mmq_q4_dense_large } else { if (cuda.mmq_q4_dense_m128 != 0) { cuda.mmq_q4_dense_m128 } else { cuda.mmq_q4_dense_m64 } } } };
                let row_tile: u32 = if large { cuda.mmq_q4_large_rows } else { if (cuda.mmq_q4_dense_m128 != 0) { 128 } else { 64 } };
                let column_tile: u32 = if large { cuda.mmq_q4_large_cols } else { 128 };
                var args: u64[8] = [
                    cuda_kernel_argument_address(matrix_weights), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(thread_storage[0].matrix_output),
                    cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(stride), cuda_kernel_argument_address(extent),
                    cuda_kernel_argument_address(first_route), cuda_kernel_argument_address(columns),
                ];
                if error == 0 { error = cuda_capture_launch(matrix, (rows as! u32) / row_tile,
                    1 + (routes - 1) / column_tile, 1, 256, 1, 1,
                    if pipeline { Q4_PIPE_SHARED_BYTES as! u32 } else { 0 }, 0, fg_cuda_driver_words_address(args), 0); }
                if error == 0 && pipeline { thread_storage[0].activations.q4_pipelined += 1; }
            } else if dense_wide && !shared && first % 4 == 0 &&
                first_route == 8 * first && routes >= 32 {
                let x128: bool = dense_x128 != 0 && first % 16 == 0 && batch % 16 == 0 && routes >= 128;
                let x64: bool = dense_x64 != 0 && first % 8 == 0 && batch % 8 == 0 && routes >= 64;
                let group: u32 = if x128 { 16 } else { if x64 { 8 } else { 4 } };
                let matrix: u64 = if x128 { dense_x128 } else { if x64 { dense_x64 } else { dense_shared } };
                let complete: u32 = routes / (8 * group);
                var args: u64[9] = [
                    cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(thread_storage[0].matrix_output),
                    cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(stride), cuda_kernel_argument_address(extent),
                    cuda_kernel_argument_address(rows), cuda_kernel_argument_address(first_route), cuda_kernel_argument_address(cuda.multiprocessors),
                ];
                error = cuda_capture_launch(matrix, (rows as! u32) / 16, complete,
                    1, if x128 { 256 } else { 128 }, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
                first += ((complete * group) as! i32);
                if error == 0 && first < end {
                    // Its packed rows remain in the same window as the wide prefix.
                    thread_storage[0].cuda_matrix_launches += 1;
                    error = cuda_capture_launch(reference_matrix, (1 as! u32) + ((rows as! u32) - 1) / 64,
                        ((end - first) as! u32), 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(matrix_args), 0);
                }
            } else if workers > 0 {
                error = launch_mmq_stream(weight, stride, extent, kind, rows, first, first_route, job_words, batch, workers, input_stride);
            } else {
                var matrix: u64 = if reference { reference_matrix } else { cuda.mmq_matrix };
                error = cuda_capture_launch(matrix, (1 as! u32) + ((rows as! u32) - 1) / 64, (batch as! u32), 1,
                                    128, 1, 1, 0, 0, fg_cuda_driver_words_address(matrix_args), 0);
            }
            if error == 0 { thread_storage[0].cuda_matrix_launches += 1; }
        }
        first = end;
    }
    fg_cuda_host_free(tiles_address);
    if first != 0 { error = finish_matrix_launches(error); }
    if error != 0 {
        fg_cuda_host_diagnostic_52((error as! i32), (packed_bytes as! u64));
        return -1;
    }
    if cache_shared || cache_dense {
        thread_storage[0].shared_activation_pack = CudaState_shared_activation_pack {
            host: thread_storage[0].matrix_reusable_host, input: thread_storage[0].matrix_input, packed: cuda.mmq_packed.address,
            bytes: thread_storage[0].matrix_input_bytes, step: thread_storage[0].activations.step, rows: cache_rows, stride: cache_stride,
            extent: extent, layout: layout, dense: cache_dense,
        };
        if reuse_pack && cache_dense { thread_storage[0].activations.dense_pack_reuses += 1; }
    }
    thread_storage[0].cuda_mmq_projections += 1;
    return 0;
}

fn launch_cuda_kernel_range(kernels: const kernel_node_params[:], first: u64,
        count: u64, stream: u64) -> i32 {
    let available = kernels.len as! u64;
    if first > available || count > available - first { return -1; }
    for i in (first as! i64) .. ((first + count) as! i64) {
        let node = kernels[i];
        let status = cuda_capture_launch(node.function, node.grid_x, node.grid_y, node.grid_z,
            node.block_x, node.block_y, node.block_z, node.shared_bytes, stream, node.params, node.extra);
        if status != 0 { return status; }
    }
    return 0;
}

fn launch_cuda_kernels(kernels: const kernel_node_params[:], count: u64, stream: u64) -> i32 {
    return launch_cuda_kernel_range(kernels, 0, count, stream);
}

fn capture_cuda_graph(kernels: const kernel_node_params[:], first: u64,
        count: u64, graph: u64&) -> i32 {
    let available = kernels.len as! u64;
    if first > available || count > available - first { return -1; }
    // All capture scopes share a stream with legacy-stream ordering.
    if cuda.graph_stream == 0 {
        let status = fg_cuda_driver_stream_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.graph_stream)), 0);
        if status != 0 { return status; }
    }
    let began = fg_cuda_driver_graph_begin(cuda.graph_stream, 1);
    if began != 0 { return began; }
    let status = launch_cuda_kernel_range(kernels, first, count, cuda.graph_stream);
    let ended = fg_cuda_driver_graph_end_typed(unsafe_c_ptr<c_ptr<void>>(cuda.graph_stream), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(graph)));
    return if status != 0 { status } else { ended };
}

fn cached_graph_for(key: u64[MMVQ_GRAPH_KEY_WORDS],
        kernels: const kernel_node_params[:], kernel_count: u64) -> u64 {
    if cuda.mmvq_graph_cache.len == 0 { return 0; }
    var hash: u64 = 14695981039346656037;
    for i in MMVQ_GRAPH_KEY_WORDS {
        hash = (hash ^ (key[i] >> 32)) * 1099511628211;
        hash = (hash ^ (key[i] & (UINT32_MAX as u64))) * 1099511628211;
    }
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccd;
    hash ^= hash >> 33;

    var entry: i64 = -1;
    var uninstantiated: i64 = -1;
    for probe in 8 {
        let slot = (((hash + (probe as! u64)) % (MMVQ_GRAPH_SLOTS as! u64)) as! i64) * MMVQ_GRAPH_WORDS;
        var equal = true;
        for word in MMVQ_GRAPH_KEY_WORDS {
            if cuda.mmvq_graph_cache[slot + word] != key[word] {
                equal = false;
                break;
            }
        }
        if equal { entry = slot; break; }
        if cuda.mmvq_graph_cache[slot] == 0 { uninstantiated = slot; break; }
        if cuda.mmvq_graph_cache[slot + MMVQ_GRAPH_KEY_WORDS] == 0 && uninstantiated < 0 {
            uninstantiated = slot;
        }
    }
    if entry < 0 {
        // Admit recurring signatures without evicting live executables.
        if uninstantiated >= 0 {
            for word in MMVQ_GRAPH_KEY_WORDS { cuda.mmvq_graph_cache[uninstantiated + word] = key[word]; }
        }
        return 0;
    }
    var executable .= cuda.mmvq_graph_cache[entry + MMVQ_GRAPH_KEY_WORDS];
    if executable != 0 { return executable; }
    var graph: u64 = 0;
    var error = capture_cuda_graph(kernels, 0, kernel_count, graph);
    if error == 0 {
        error = fg_cuda_driver_graph_instantiate_typed(
            unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(executable)),
            unsafe_c_ptr<c_ptr<void>>(graph), 0);
    }
    if graph != 0 { fg_cuda_driver_graph_destroy(graph); }
    if error == 0 { return executable; }
    if cuda_env("GOOSE_GRAPH_LOG") != 0 { fg_cuda_host_diagnostic_53(error); }
    release_mmvq_graphs();
    return 0;
}

fn launch_mmvq(weight: u64, stride: u64, extent: i32, kind: i32, rows: i32, count: i32, words: i32, dense_offsets: const i64[:]) -> i32 {
    var thread_storage = cuda_thread_state();
    thread_storage[0].shared_activation_pack.host = 0;
    let packed_bytes: u64 = (extent as! u64) * 288 * (count as! u64);
    var error: i32 = reserve(&cuda.mmq_packed, packed_bytes);
    if error == 0 {
        var input: u64 = thread_storage[0].matrix_input;
        var output: u64 = thread_storage[0].matrix_output;
        if dense_offsets.len != 0 {
            input += (dense_offsets[0] as! u64);
            output += (dense_offsets[1] as! u64);
        }
        var packet_pack: u64[5] = [
            cuda_kernel_argument_address(input), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(cuda.jobs.address),
            cuda_kernel_argument_address(extent), cuda_kernel_argument_address(words),
        ];
        var packet_matrix: u64[8] = [
            cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(output),
            cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(stride), cuda_kernel_argument_address(extent),
            cuda_kernel_argument_address(kind), cuda_kernel_argument_address(words),
        ];
        var dense_pack: u64[3] = [
            cuda_kernel_argument_address(input), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(extent),
        ];
        var dense_matrix: u64[6] = [
            cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(output),
            cuda_kernel_argument_address(stride), cuda_kernel_argument_address(extent), cuda_kernel_argument_address(kind),
        ];
        let pack = if dense_offsets.len != 0 { cuda_slice_address(dense_pack) } else { cuda_slice_address(packet_pack) };
        let matrix = if dense_offsets.len != 0 { cuda_slice_address(dense_matrix) } else { cuda_slice_address(packet_matrix) };
        let pack_function: u64 = if dense_offsets.len != 0 { cuda.mmvq_dense_pack } else { cuda.mmvq_pack };
        let matrix_function: u64 = if dense_offsets.len != 0 { cuda.mmvq_dense_row } else { cuda.mmvq };
        var graph: u64 = 0;
        if !thread_storage[0].decode_capture.scope && cuda.mmvq_graph_enabled && cuda.mmvq_graph_cache.len != 0 && thread_storage[0].activations.active && thread_storage[0].activations.defer_waits &&
            (count == 1 || count == 8) {
            let kernels: kernel_node_params[2] = [
                kernel_node_params { function: pack_function, grid_x: ((2 * extent) as! u32), grid_y: (count as! u32),
                  grid_z: 1, block_x: 128, block_y: 1, block_z: 1, params: pack, extra: 0, kernel: 0, context: 0 },
                kernel_node_params { function: matrix_function, grid_x: (rows as! u32), grid_y: (count as! u32),
                  grid_z: 1, block_x: 128, block_y: 1, block_z: 1, params: matrix, extra: 0, kernel: 0, context: 0 },
            ];
            // The ordered descriptor upload provides the current expert slots.
            let key_prefix: u64[9] = [weight, stride, input, output,
                cuda.mmq_packed.address, if dense_offsets.len != 0 { 0 } else { cuda.jobs.address },
                ((extent as! u64) << 32) | (rows as! u64),
                ((count as! u64) << 32) | (words as! u64), (kind as! u64)];
            var key: u64[MMVQ_GRAPH_KEY_WORDS] = [0; MMVQ_GRAPH_KEY_WORDS];
            for key_index in key_prefix.len { key[key_index] = key_prefix[key_index]; }
            graph = cached_graph_for(key, kernels, 2);
        }
        if graph != 0 {
            error = fg_cuda_driver_graph_launch(graph, cuda.graph_stream);
        } else {
            error = cuda_capture_launch(pack_function, ((2 * extent) as! u32), (count as! u32), 1, 128, 1, 1, 0, 0, pack, 0);
            if error == 0 {
                error = cuda_capture_launch(matrix_function, (rows as! u32), (count as! u32), 1, 128, 1, 1, 0, 0, matrix, 0);
            }
        }
        error = finish_matrix_launches(error);
    }
    if error != 0 {
        fg_cuda_host_diagnostic_54((error as! i32));
        return -1;
    }
    thread_storage[0].cuda_matrix_launches += 1;
    thread_storage[0].cuda_matrix_kind = kind;
    return 0;
}

export fn fg_cuda_inkling_prefill_readback(bytes: u64) -> u64 {
    var thread_storage = cuda_thread_state();
    let capacity: u64 = 36 * (MIB_BYTES as! u64);
    if !thread_storage[0].cuda_inkling_quants || bytes == 0 || bytes > capacity || start_context() != 1 { return 0; }
    if (cuda.inkling_prefill_readback == 0) && (cuda.host_alloc != 0) && (cuda.host_free != 0) {
        var buffer: c_ptr<void> = null;
        if fg_cuda_driver_host_alloc_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(buffer)), capacity, 0) == 0 { cuda.inkling_prefill_readback = fg_cuda_replay_node_address(buffer); }
    }
    return if finish_context() == 0 { cuda.inkling_prefill_readback } else { 0 };
}

export fn fg_cuda_enable_inkling_quants(enabled: bool) {
    var thread_storage = cuda_thread_state();
    thread_storage[0].cuda_inkling_quants = enabled;
}

export fn fg_cuda_inkling_quants_enabled() -> u8 {
    var thread_storage = cuda_thread_state();
    return if thread_storage[0].cuda_inkling_quants { 1 } else { 0 };
}

// Driver dependency: grouped-moe-driver
extern fn fg_cuda_image_grouped_moe_cubin() -> u64;

fn launch_grouped_iq4_mmq(weight: u64, stride: u64, width: i32, rows: i32, kind: i32, jobs: i64[:]) -> i32 {
    var thread_storage = cuda_thread_state();
    let count: i32 = ((jobs.len / 3) as! i32);
    let wide: bool = cuda_option("I2_MOE_TILE16");
    let tile32: bool = wide && count >= 4096 && cuda_option("I2_MOE_TILE32");
    let quant_index: i64 = if kind == 23 { 0 } else { 1 };
    let tile_index: i64 = if tile32 { 4 } else if wide { 2 } else { 0 };
    let entry = quant_index + tile_index;
    let kernels = ["goose_i2_xs_mmq", "goose_i2_nl_mmq",
        "goose_i2_xs_mmq16", "goose_i2_nl_mmq16", "goose_i2_xs_mmq32", "goose_i2_nl_mmq32"];
    let tiles_address = fg_cuda_host_malloc(24 * (count as! u64));
    if tiles_address == 0 { return -1; }
    var tiles = fg_cuda_host_view_i64(tiles_address, 3 * (count as! i64));
    var tile_count = fg143_mmq_tiles(jobs, 3, 8, tiles);
    if (wide && tile_count > 0) {
        var write: i64 = 0;
        block {
            var read: i64 = 0;
            while read < tile_count {
                let first: i64 = tiles[3 * read];
                var size: i64 = tiles[3 * read + 1];
                let plane: i64 = tiles[3 * read + 2];
                let limit: i64 = if tile32 { 32 } else { 16 };
                while (size % 8 == 0 && size < limit && read + 1 < tile_count &&
                       tiles[3 * (read + 1)] == first + size && tiles[3 * (read + 1) + 2] == plane) {
                    size += tiles[3 * (read + 1) + 1];
                    read += 1;
                }
                tiles[3 * write] = first;
                tiles[3 * write + 1] = size;
                tiles[3 * write + 2] = plane;
                write += 1;
                read += 1;
            }
        };
        tile_count = write;
    }
    var error: i32 = if tile_count < 1 || tile_count > 65535 { -1 } else { 0 };
    let bytes: u64 = ((if tile_count > 0 { tile_count } else { 0 }) as! u64) * 3 * 8;
    if error == 0 {
        error = cuda_ensure_module(&cuda_saved_launch_grouped_iq4_mmq_module, fg_cuda_image_grouped_moe_cubin());
    }
    let module = cuda_saved_launch_grouped_iq4_mmq_module;
    if error == 0 { error = cuda_ensure_kernel(&cuda_saved_launch_grouped_iq4_mmq_pack, module, "goose_i2_moe_pack"); }
    if error == 0 {
        error = cuda_ensure_kernel(&cuda_saved_launch_grouped_iq4_mmq_multiply[entry], module, kernels[entry]);
    }
    if error == 0 { error = reserve(&cuda.mmq_tiles, bytes); }
    if error == 0 { error = reserve(&cuda.mmq_packed, (count as! u64) * (width as! u64) * 2); }
    if error == 0 {
        error = upload_packet(cuda.mmq_tiles.address, unsafe_c_ptr<c_ptr<const void>>(tiles_address), bytes);
    }
    fg_cuda_host_free(tiles_address);
    var pack_args: u64[5] = [
        cuda_kernel_argument_address(thread_storage[0].matrix_input),
        cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(cuda.mmq_packed.address),
        cuda_kernel_argument_address(width), cuda_kernel_argument_address(count),
    ];
    if error == 0 {
        error = cuda_capture_launch(cuda_saved_launch_grouped_iq4_mmq_pack, ((width + 255) as! u32) / 256,
            count as! u32, 1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(pack_args), 0);
    }
    var multiply_args: u64[8] = [
        cuda_kernel_argument_address(weight), cuda_kernel_argument_address(cuda.mmq_packed.address),
        cuda_kernel_argument_address(thread_storage[0].matrix_output), cuda_kernel_argument_address(cuda.jobs.address),
        cuda_kernel_argument_address(cuda.mmq_tiles.address), cuda_kernel_argument_address(stride),
        cuda_kernel_argument_address(width), cuda_kernel_argument_address(rows),
    ];
    if error == 0 {
        error = cuda_capture_launch(cuda_saved_launch_grouped_iq4_mmq_multiply[entry], ((rows + 63) as! u32) / 64,
            tile_count as! u32, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(multiply_args), 0);
    }
    return finish_matrix_launches(error);
}

// Driver dependency: inkling-matrix-driver
extern fn fg_cuda_image_matrix_cubin() -> u64;
extern fn fg_cuda_image_q8_packed_cubin() -> u64;

fn cuda_ensure_module(module: u64&, image: u64) -> i32 {
    if module != 0 { return 0; }
    return fg_cuda_driver_module_load_typed(
        unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)),
        unsafe_c_ptr<c_ptr<const void>>(image));
}

fn cuda_ensure_kernel(kernel: u64&, module: u64, name: const u8[:]) -> i32 {
    if kernel != 0 { return 0; }
    return cuda_lookup_kernel(cuda_kernel_argument_address(kernel), module, name);
}

fn i2_packed_q8(weight: u64, stride: u64, width: i32, rows: i32, count: i32, jobs: u64) -> i32 {
    var tls = cuda_thread_state();
    let shape: bool = (width == 2304 && (rows == 1024 || rows == 2304 || rows == 3072)) ||
        (width == 3072 && rows == 2304);
    var control_setting: u64 = cuda_env("I2_PACK_CONTROL_SHAPES");
    let extra: i32 = if (cuda_env_equal(control_setting, "0")) { 0 } else { if width == 64 && rows == 2304 && count % 16 == 0 { 1 } else { if width == 4608 && rows == 64 { 2 } else { if width == 2304 && rows == 384 { 3 } else { 0 } } } };
    if ((!shape && extra == 0) || count < 512 || count > 4096 || count % 8 != 0) { return 1; }
    if (cuda_local_i2_packed_q8_enabled_4709 < 0) {
        var setting: u64 = cuda_env("I2_Q8_PACKED_INPUT");
        cuda_local_i2_packed_q8_enabled_4709 = if cuda_env_equal(setting, "1") { 1 } else { 0 };
    }
    if ((cuda_local_i2_packed_q8_enabled_4709 == 0)) { return 1; }

    var expand_setting: u64 = cuda_env("I2_Q8_FP32_EXPAND");
    let expanded: bool = count == 4096 && (shape || extra == 3) &&
        (!cuda_env_equal(expand_setting, "0"));
    var wide_setting: u64 = cuda_env("I2_Q8_EXACT_TILE16");
    let wide: bool = extra == 1 || (shape && count % 16 == 0 && rows != 1024 &&
        (!cuda_env_equal(wide_setting, "0")));
    let warps: i32 = if expanded { (if width == 3072 || extra == 3 { 2 } else { 4 }) } else { (if wide { (if (width == 3072 || extra == 1) { 2 } else { 4 }) } else { (if extra == 2 { 4 } else { 8 }) }) };
    let tile: i32 = if wide || expanded { 16 } else { 8 };
    var module .= cuda_saved_i2_packed_q8_module;
    var pack .= cuda_saved_i2_packed_q8_pack;
    var multiply .= cuda_saved_i2_packed_q8_multiply;
    var narrow .= cuda_saved_i2_packed_q8_narrow4;
    var wide2 .= cuda_saved_i2_packed_q8_wide2;
    var wide4 .= cuda_saved_i2_packed_q8_wide4;
    var expand .= cuda_saved_i2_packed_q8_expand;
    var fp32w2 .= cuda_saved_i2_packed_q8_fp32w2;
    var fp32w4 .= cuda_saved_i2_packed_q8_fp32w4;
    var error = cuda_ensure_module(module, fg_cuda_image_q8_packed_cubin());
    if error == 0 { error = cuda_ensure_kernel(pack, module, "i2_pack_q8_input8"); }
    if error == 0 { error = cuda_ensure_kernel(multiply, module, "i2_q8_packed8"); }
    if error == 0 && extra == 2 { error = cuda_ensure_kernel(narrow, module, "i2_q8_packed4"); }
    if error == 0 && wide && warps == 2 { error = cuda_ensure_kernel(wide2, module, "i2_q8_packed16w2"); }
    if error == 0 && wide && warps == 4 { error = cuda_ensure_kernel(wide4, module, "i2_q8_packed16w4"); }
    if error == 0 && expanded { error = cuda_ensure_kernel(expand, module, "i2_q8_expand_f32"); }
    if error == 0 && expanded && warps == 2 { error = cuda_ensure_kernel(fp32w2, module, "i2_q8_fp32w2"); }
    if error == 0 && expanded && warps == 4 { error = cuda_ensure_kernel(fp32w4, module, "i2_q8_fp32w4"); }
    if error != 0 { return -1; }

    // The regular and grouped matrix launches reuse this work buffer on
    // the same stream. No separate persistent allocation is needed.
    let pack_bytes: u64 = (count as! u64) * (width as! u64) * 4;
    let weight_bytes: u64 = if expanded { (rows as! u64) * (width as! u64) * 4 } else { 0 };
    error = reserve(&cuda.mmq_packed, pack_bytes + weight_bytes);
    if (error == 2) { return 1; }
    if error != 0 { return -1; }
    if tls[0].activations.active && tls[0].activations.defer_waits { tls[0].activations.pending = true; }
    var pack_args: u64[4] = [cuda_kernel_argument_address(tls[0].matrix_input), cuda_kernel_argument_address(jobs), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(width)];
    var weights_device: u64 = if expanded { cuda.mmq_packed.address + pack_bytes } else { weight };
    var matrix_args: u64[6] = [cuda_kernel_argument_address(weights_device), cuda_kernel_argument_address(cuda.mmq_packed.address), cuda_kernel_argument_address(tls[0].matrix_output), cuda_kernel_argument_address(jobs), cuda_kernel_argument_address(stride), cuda_kernel_argument_address(width)];
    error = cuda_capture_launch(pack, ((width + 127) / 128) as! u32, (count / 8) as! u32,
        1, 128, 1, 1, 0, 0, cuda_slice_address(pack_args), 0);
    if error == 0 && expanded {
        var args: u64[4] = [cuda_kernel_argument_address(weight), cuda_kernel_argument_address(weights_device), cuda_kernel_argument_address(stride), cuda_kernel_argument_address(width)];
        error = cuda_capture_launch(expand, ((width + 127) / 128) as! u32, rows as! u32,
            1, 128, 1, 1, 0, 0, cuda_slice_address(args), 0);
    }
    let kernel = if expanded {
        if warps == 2 { fp32w2 } else { fp32w4 }
    } else if wide {
        if warps == 2 { wide2 } else { wide4 }
    } else if extra == 2 { narrow } else { multiply };
    if error == 0 {
        error = cuda_capture_launch(kernel, (rows / warps) as! u32, (count / tile) as! u32, 1,
            (32 * warps) as! u32, 1, 1, 0, 0, cuda_slice_address(matrix_args), 0);
    }
    if error == 0 { error = finish_matrix_launches(error); }
    if error != 0 { return -1; }
    tls[0].cuda_matrix_launches += 1;
    tls[0].cuda_matrix_kind = 8;
    if (expanded && !cuda_local_i2_packed_q8_expand_reported_4763) {
        fg_cuda_host_diagnostic_55();
        cuda_local_i2_packed_q8_expand_reported_4763 = true;
    }
    let mask = (1 as! u32) << (extra as! u32);
    if extra != 0 && (cuda_local_i2_packed_q8_extra_reported_4768 & mask) == 0 {
        cuda_local_i2_packed_q8_extra_reported_4768 |= mask;
        fg_cuda_host_diagnostic_56((width as! i32), (rows as! i32), (tile as! i32), (warps as! i32));
    }
    if (wide && !cuda_local_i2_packed_q8_wide_reported_4773) {
        cuda_local_i2_packed_q8_wide_reported_4773 = true;
        fg_cuda_host_diagnostic_57();
    }
    if (!cuda_local_i2_packed_q8_reported_4778) {
        cuda_local_i2_packed_q8_reported_4778 = true;
        fg_cuda_host_diagnostic_58();
    }
    return 0;
}


fn i2_matrix_enabled() -> bool {
    if (cuda_local_i2_matrix_enabled_enabled_4788 < 0) {
        var setting: u64 = cuda_env("I2_SPECIALIZED_MATVEC");
        cuda_local_i2_matrix_enabled_enabled_4788 = if cuda_env_equal(setting, "1") { 1 } else { 0 };
    }
    return cuda_local_i2_matrix_enabled_enabled_4788 != 0;
}

fn i2_launch_matrix(weight: u64, stride: u64, width: i32, kind: i32, rows: i32,
        count: i32, words: i32, job_address: u64) -> i32 {
    var tls = cuda_thread_state();
    if kind == 8 && words == 2 {
        let selected = i2_packed_q8(weight, stride, width, rows, count, job_address);
        if selected <= 0 { return selected; }
    }
    let verify = tls[0].i2_exact_verify && words == 3 && (kind == 20 || kind == 23);
    let xs_blocks = kind == 23 && (count <= 8 || verify) && width % 256 == 0 &&
        cuda_option("I2_DECODE_IQ4_XS_BLOCKS");
    let index: i32 = if xs_blocks { 3 } else if kind == 8 { 0 } else if kind == 20 { 1 } else { 2 };
    let name: const u8[:] = if index == 0 { "goose_i2_matrix_q8_0" }
        else if index == 1 { "goose_i2_matrix_iq4_nl" }
        else if index == 2 { "goose_i2_matrix_iq4_xs" }
        else { "goose_i2_matrix_iq4_xs_blocks" };
    var module .= cuda_saved_i2_launch_matrix_module;
    var kernel .= cuda_saved_i2_launch_matrix_kernels[index];
    var status = cuda_ensure_module(module, fg_cuda_image_matrix_cubin());
    if status == 0 { status = cuda_ensure_kernel(kernel, module, name); }

    var tile: i32 = if count >= 16 && !verify { 4 } else { 1 };
    if kind == 8 && words == 2 && count >= 16 && cuda_env_equal(cuda_env("I2_Q8_TILE8"), "1") { tile = 8; }
    var base: i32 = 0;
    while base < count && status == 0 {
        let limit = 65535 * tile;
        let size = if count - base > limit { limit } else { count - base };
        let args = [
            cuda_kernel_argument_address(weight), cuda_kernel_argument_address(tls[0].matrix_input),
            cuda_kernel_argument_address(tls[0].matrix_output), cuda_kernel_argument_address(job_address),
            cuda_kernel_argument_address(stride), cuda_kernel_argument_address(width),
            cuda_kernel_argument_address(rows), cuda_kernel_argument_address(size),
            cuda_kernel_argument_address(base), cuda_kernel_argument_address(words), cuda_kernel_argument_address(tile),
        ];
        status = cuda_capture_launch(kernel, ((rows as! u32) + 3) / 4,
            ((size + tile - 1) / tile) as! u32, 1, 128, 1, 1, 0, 0, cuda_slice_address(args), 0);
        if status == 0 { tls[0].cuda_matrix_launches += 1; }
        base += size;
    }
    if status == 0 { status = finish_matrix_launches(status); }
    if status != 0 {
        fg_cuda_host_diagnostic_59(status);
        return -1;
    }
    tls[0].cuda_matrix_kind = kind;
    if verify && !cuda_local_i2_launch_matrix_reported_verify_4841 {
        fg_cuda_host_diagnostic_60();
        cuda_local_i2_launch_matrix_reported_verify_4841 = true;
    }
    let mask = (1 as! u32) << (index as! u32);
    if (cuda_local_i2_launch_matrix_reported_4803 & mask) == 0 {
        cuda_local_i2_launch_matrix_reported_4803 |= mask;
        fg_cuda_host_diagnostic_61(kind, tile);
    }
    return 0;
}

fn giga_dense_matrix(extent: i64, rows: i64, count: i32) -> u64 {
    let tls = cuda_thread_state();
    if !tls[0].giga_small_expert_wave { return 0; }
    let key = extent == 128 && rows == 512;
    let value = extent == 512 && rows == 128;
    let mla = (count == 2048 || count == 4096) && (key || value) &&
        !cuda_env_equal(cuda_env("GOOSE_GIGA_MLA_PROJECTION_MMA"), "0");
    if extent != 16 && !mla { return 0; }

    var module .= cuda_saved_giga_dense_matrix_module;
    if module == 0 && cuda_ensure_module(module, fg_cuda_image_giga_dense_image()) == 0 {
        cuda_ensure_kernel(cuda_saved_giga_dense_matrix_kernel, module, "goose_dense_mma");
    }
    return cuda_saved_giga_dense_matrix_kernel;
}

export fn inkling_draft_verification(active: bool) {
    var thread_storage = cuda_thread_state();
    thread_storage[0].i2_exact_verify = active;
}

fn launch_matrix(weight: u64, row_stride: i64, extent: i64, kind: i64, rows: i64, jobs: i64[:], job_words: i32, shared_rows: i64, input_stride: i64) -> i32 {
    var thread_storage = cuda_thread_state();
    let words = job_words as! i64;
    let max_i32 = INT_MAX as! i64;
    let k_quant: bool = kind == 12 || kind == 14;
    let inkling: bool = thread_storage[0].cuda_inkling_quants && (kind == 8 || kind == 20 || kind == 23);
    if weight == 0 || job_words <= 0 || rows <= 0 || rows > max_i32 || extent <= 0 || extent > max_i32 ||
        (kind != 0 && !k_quant && !inkling) || (inkling && extent % (if kind == 23 { 256 } else { 32 }) != 0) ||
        jobs.len <= 0 || jobs.len % words != 0 || jobs.len / words > max_i32 ||
        (jobs.len as! u64) > SIZE_MAX / 8 { return -1; }
    if thread_storage[0].matrix_glu_active && (kind == 0 || extent != 2 || job_words != 3 ||
        shared_rows != 0 || jobs.len / words < 16) { return -1; }
    if !thread_storage[0].cuda_inkling_quants && kind != 0 && extent <= max_i32 / 256 && job_words == 2 && jobs.len == 2 &&
        (cuda.mmvq_dense_pack != 0) && (cuda.mmvq_dense_row != 0) && jobs[0] >= 0 && jobs[1] >= 0 &&
        (jobs[0] as! u64) <= UINT64_MAX - thread_storage[0].matrix_input &&
        (jobs[1] as! u64) <= UINT64_MAX - thread_storage[0].matrix_output {
        if !cuda_local_launch_matrix_dense_mmvq_logged_4888 && fg143_cpu_prefill() == 0 {
            cuda_local_launch_matrix_dense_mmvq_logged_4888 = true;
            fg_cuda_host_diagnostic_62();
        }
        return launch_mmvq(weight, (row_stride as! u64), (extent as! i32), (kind as! i32),
            (rows as! i32), 1, job_words, jobs);
    }
    if inkling && kind == 8 && job_words == 2 && jobs.len == 2 &&
        jobs[0] == 0 && jobs[1] == 0 && i2_matrix_enabled() {
        var setting: u64 = cuda_env("I2_RESIDENT_Q8_JOB");
        if !cuda_env_equal(setting, "0") {
            var error: i32 = 0;
            if (cuda_saved_launch_matrix_zero_job.address == 0) {
                error = reserve(&cuda_saved_launch_matrix_zero_job, 2 * 8);
                if error == 0 { error = cuda_capture_zero(cuda_saved_launch_matrix_zero_job.address, 0, 2 * 8); }
                if error != 0 && cuda_saved_launch_matrix_zero_job.address != 0 { release_buffer(&cuda_saved_launch_matrix_zero_job); }
            }
            if error != 0 { return -1; }
            return i2_launch_matrix(weight, (row_stride as! u64), (extent as! i32),
                (kind as! i32), (rows as! i32), 1, 2, cuda_saved_launch_matrix_zero_job.address);
        }
    }
    let job_bytes: u64 = (jobs.len as! u64) * 8;
    var error: i32 = reserve(&cuda.jobs, job_bytes);
    if error == 0 && thread_storage[0].cuda_inkling_quants {
        error = upload_packet(cuda.jobs.address, unsafe_c_ptr<c_ptr<const void>>(cuda_slice_address(jobs)), job_bytes);
    } else if error == 0 {
        // Only the large packs pay the synchronous copy; a small pack keeps the
        // pinned packet ring it already used, so decode is untouched.
        let queued: i32 = if job_bytes > 4096 { upload_jobs_async(cuda.jobs.address, unsafe_c_ptr<c_ptr<const void>>(cuda_slice_address(jobs)), job_bytes) } else { -2 };
        error = if queued == -2 { upload_packet(cuda.jobs.address, unsafe_c_ptr<c_ptr<const void>>(cuda_slice_address(jobs)), job_bytes) } else { queued };
    }
    if error == 0 && (cuda.mmq_matrix != 0) && k_quant && jobs.len / words >= 16 {
        let result: i32 = launch_mmq(weight, (row_stride as! u64), (extent as! i32), (kind as! i32),
                                      (rows as! i32), jobs, job_words, shared_rows, input_stride);
        if result == 0 { thread_storage[0].cuda_matrix_kind = (kind as! i32); }
        return result;
    }

    let count: i32 = ((jobs.len / words) as! i32);
    if error == 0 && inkling && (kind == 20 || kind == 23) && job_words == 3 &&
            !thread_storage[0].i2_exact_verify && count >= 16 && count <= 32768 && extent <= 8192 && rows >= 32 &&
            row_stride % 2 == 0 && row_stride >= (if kind == 23 { extent / 256 * 136 } else { extent / 32 * 18 }) &&
            cuda_option("I2_GROUPED_MOE_MMQ") {
        let result: i32 = launch_grouped_iq4_mmq(weight, (row_stride as! u64), (extent as! i32), (rows as! i32), (kind as! i32), jobs);
        if result == 0 {
            thread_storage[0].cuda_matrix_launches += 1;
            thread_storage[0].cuda_matrix_kind = (kind as! i32);
        }
        return result;
    }
    if error == 0 && inkling && i2_matrix_enabled() {
        return i2_launch_matrix(weight, (row_stride as! u64), (extent as! i32), (kind as! i32),
                                (rows as! i32), count, job_words, cuda.jobs.address);
    }
    if error == 0 && (cuda.mmvq != 0) && k_quant && extent <= max_i32 / 256 &&
        (count == 1 || (job_words == 3 && count == 8) ||
         (cuda.mmvq_dense_tail && job_words == 2 && count <= 8)) {
        return launch_mmvq(weight, (row_stride as! u64), (extent as! i32), (kind as! i32), (rows as! i32), count, job_words, fg_cuda_host_view_i64(0, 0));
    }
    let dense_shape: bool = kind == 0 && job_words == 2 && rows >= 16 &&
        count >= 16 && count <= 8 * 65535;
    var dense_kernel: u64 = cuda.dense_mma;
    if error == 0 && dense_shape {
        let giga_kernel: u64 = giga_dense_matrix(extent, rows, count);
        if giga_kernel != 0 { dense_kernel = giga_kernel; }
    }
    if error == 0 && dense_kernel != 0 && dense_shape {
        var k: i32 = (extent as! i32);
        var m: i32 = (rows as! i32);
        var jobs_count: i32 = count;
        // These SM89 router geometries were traced in the frozen reference.
        var partitions: i32 = 0;
        if (cuda.dense_mma_reference != 0) && rows == 256 && extent == 2048 {
            if count == 124 { partitions = 4; }
            if count == 4096 { partitions = 1; }
        }
        var matrix: u64 = if partitions > 0 { cuda.dense_mma_reference } else { dense_kernel };
        var args: u64[9] = [
            cuda_kernel_argument_address(weight), cuda_kernel_argument_address(thread_storage[0].matrix_input), cuda_kernel_argument_address(thread_storage[0].matrix_output),
            cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(row_stride), cuda_kernel_argument_address(k),
            cuda_kernel_argument_address(m), cuda_kernel_argument_address(jobs_count), cuda_kernel_argument_address(partitions),
        ];
        error = cuda_capture_launch(matrix, (1 as! u32) + ((rows as! u32) - 1) / 64, (1 as! u32) + ((count as! u32) - 1) / 8, 1,
            128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        if error == 0 { error = finish_matrix_launches(error); }
        if error != 0 {
            fg_cuda_host_diagnostic_63((error as! i32));
            return -1;
        }
        thread_storage[0].cuda_matrix_launches += 1;
        thread_storage[0].cuda_dense_mma += 1;
        thread_storage[0].cuda_matrix_kind = 0;
        return 0;
    }

    var stride: u64 = (row_stride as! u64);
    var width: i32 = (extent as! i32);
    var node_type: i32 = (kind as! i32);
    var start: i32 = 0;
    var args: u64[9] = [
        cuda_kernel_argument_address(weight), cuda_kernel_argument_address(thread_storage[0].matrix_input), cuda_kernel_argument_address(thread_storage[0].matrix_output),
        cuda_kernel_argument_address(cuda.jobs.address), cuda_kernel_argument_address(stride), cuda_kernel_argument_address(width),
        cuda_kernel_argument_address(node_type), cuda_kernel_argument_address(start), cuda_kernel_argument_address(job_words),
    ];
    let lanes: u32 = if kind == 14 { 64 } else { 32 };
    while error == 0 && start < count {
        let batch: i32 = if count - start > 65535 { 65535 } else { count - start };
        error = cuda_capture_launch(cuda.kernel, (rows as! u32), (batch as! u32), 1,
                            lanes, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        if error == 0 { thread_storage[0].cuda_matrix_launches += 1; }
        start += batch;
    }
    if start != 0 { error = finish_matrix_launches(error); }
    if error != 0 { fg_cuda_host_diagnostic_64((error as! i32)); }
    else {
        thread_storage[0].cuda_matrix_kind = node_type;
        if inkling {
            let bit: u32 = (1 as! u32) << node_type;
            if (thread_storage[0].cuda_local_launch_matrix_reported_5008 & bit) == 0 {
                thread_storage[0].cuda_local_launch_matrix_reported_5008 |= bit;
                fg_cuda_host_diagnostic_65((node_type as! i32));
            }
        }
    }
    return if error == 0 { 0 } else { -1 };
}

// ---- Expert prefetch from a one-layer-ahead route prediction (EXP-PF1) ----
// At the decode FFN of layer L the router of layer L+1 is applied to the FFN
// input of layer L on a side stream. After the FFN of L is submitted, the
// predicted experts of L+1 go through the regular SLRU prepare path, so their
// misses are copied while L computes. The prediction only chooses which expert
// weights are copied early; the model always uses its own routes, and every
// consumer still waits on the regular readiness events. GOOSE_EXPERT_PREFETCH=0
// disables it.
const PF_LAYERS: i64 = 64;
const PF_EXPERTS: i64 = 256;
const PF_MAX_K: i64 = 32;
struct CudaExpertPrefetchLayer {
    w: u64[3], plane: i64[3], down_base: u64, down_plane: u64,
    disjoint: bool, ok: bool,
}
struct CudaExpertPrefetch {
    state: i32,                 // 0 untried, 1 ready, -1 off
    k: u32,
    route: u64,
    stream: u64,
    dev_logits: u64,
    dev_counter: u64,
    dev_host: u64,
    host_ids: u64,   // [0] sequence, [1..k] predicted ids
    seq: i32,
    router_host: u64[PF_LAYERS],
    router_dev: u64[PF_LAYERS],
    routers: i32,
    layer_up: u64[PF_LAYERS],
    layers: i32,
    plan: CudaExpertPrefetchLayer[PF_LAYERS],
    pending: i32,
    launched: u64,
    waves: u64,
    jobs: u64,
    skipped: u64,
    ns_launch: u64,
    ns_wait: u64,
    ns_prepare: u64,
    ns_submit: u64,
}
fn cuda_expert_prefetch_state() -> CudaExpertPrefetch {
    var state: CudaExpertPrefetch = default<CudaExpertPrefetch>();
    state.pending = -1;
    return state;
}
var pf: CudaExpertPrefetch = cuda_expert_prefetch_state();
fn pf_now() -> u64 { return fg_cuda_host_monotonic_ns(); }

fn pf_init() -> bool {
    if pf.state != 0 { return pf.state == 1; }
    pf.state = -1;
    var off: u64 = cuda_env("GOOSE_EXPERT_PREFETCH");
    if cuda_env_equal(off, "0") { return false; }
    var k: u64 = cuda_env("GOOSE_EXPERT_PREFETCH_K");
    pf.k = fg_cuda_host_env_u32(k, 8);
    if pf.k == 0 || pf.k > (PF_MAX_K as! u32) { return false; }
    let host_device = fg_cuda_driver_host_device_pointer_bind(cuda.library);
    var module: u64 = 0;
    var host: c_ptr<void> = null;
    if host_device == 0 || (cuda.host_alloc == 0) || (cuda.zero == 0) ||
        fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(module)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_pred_image())) != 0 ||
        cuda_lookup_kernel(cuda_kernel_argument_address(pf.route), module, "goose_pred_route") != 0 ||
        fg_cuda_driver_stream_create_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(pf.stream)), 1 /* CU_STREAM_NON_BLOCKING */) != 0 ||
        cuda_capture_alloc(cuda_kernel_argument_address(pf.dev_logits), (PF_EXPERTS as! u64) * 4) != 0 ||
        cuda_capture_alloc(cuda_kernel_argument_address(pf.dev_counter), 64) != 0 || cuda_capture_zero(pf.dev_counter, 0, 64) != 0 ||
        fg_cuda_driver_host_alloc_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(host)), ((PF_MAX_K + 1) as! u64) * 4, 2 /* CU_MEMHOSTALLOC_DEVICEMAP */) != 0 ||
        fg_cuda_driver_host_device_pointer_typed(unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(pf.dev_host)), unsafe_c_ptr<c_ptr<void>>(host), 0) != 0 {
        fg_cuda_host_diagnostic_66();
        return false;
    }
    pf.host_ids = fg_cuda_replay_node_address(host);
    fg_cuda_host_store_release_u32(pf.host_ids, 0);
    pf.state = 1;
    fg_cuda_host_diagnostic_67((pf.k as! u32));
    return true;
}

// A dense F32 256 x hidden matrix is a router; first-seen order is the layer order.
fn pf_note_router(host: u64, device: u64, kind: i64, rows: i64, row_stride: i64, bytes: i64) {
    if kind != 0 || rows != PF_EXPERTS || row_stride <= 0 || bytes != rows * row_stride || device == 0 { return; }
    block {
        var i: i32 = 0;
        while i < pf.routers {
            if pf.router_host[i] == host { return; }
            i += 1;
        }
    };
    if pf.routers == (PF_LAYERS as! i32) { return; }
    pf.router_host[pf.routers] = host;
    pf.router_dev[pf.routers] = device; pf.routers += 1;
}

fn pf_layer(up: u64) -> i32 {
    block {
        var i: i32 = 0;
        while i < pf.layers {
            if pf.layer_up[i] == up { return i; }
            i += 1;
        }
    };
    if pf.layers == (PF_LAYERS as! i32) { return -1; }
    pf.layer_up[pf.layers] = up;
    let layer = pf.layers;
    pf.layers += 1;
    return layer;
}

fn pf_launch(layer: i32, input: u64, width: i64) {
    pf.pending = -1;
    let next: i32 = layer + 1;
    if !pf_init() || layer < 0 || next >= pf.layers || pf.routers != pf.layers || !pf.plan[next].ok { return; }
    let t0: u64 = pf_now();
    var w: u64 = pf.router_dev[next];
    let iw = width as! i32;
    let k = pf.k as! i32;
    pf.seq = if pf.seq == 0x7fffffff { 1 } else { pf.seq + 1 };
    let seq = pf.seq;
    var args: u64[8] = [
        cuda_kernel_argument_address(w), cuda_kernel_argument_address(input), cuda_kernel_argument_address(pf.dev_logits),
        cuda_kernel_argument_address(pf.dev_counter), cuda_kernel_argument_address(pf.dev_host), cuda_kernel_argument_address(iw),
        cuda_kernel_argument_address(k), cuda_kernel_argument_address(seq),
    ];
    if cuda_capture_launch(pf.route, PF_EXPERTS as! u32, 1, 1, 256, 1, 1, 0, pf.stream, fg_cuda_driver_words_address(args), 0) != 0 {
        pf.state = -1;
        fg_cuda_host_diagnostic_68();
        return;
    }
    pf.pending = next;
    pf.launched += 1;
    pf.ns_launch += pf_now() - t0;
}

fn pf_submit() -> i32 {
    let next: i32 = pf.pending;
    pf.pending = -1;
    if next < 0 || pf.state != 1 { return 0; }
    let t0: u64 = pf_now();
    if cuda.expert_upload_count != 0 { pf.skipped += 1; return 0; }
    // The kernel publishes the sequence number after the ids; a late answer is skipped.
    block {
        var spin: u32 = 0;
        while fg_cuda_host_load_acquire_u32(pf.host_ids) != (pf.seq as! u32) {
            if spin > 2000000 { pf.skipped += 1; return 0; }
            fg_cuda_host_cpu_pause();
            spin += 1;
        }
    };
    let t1: u64 = pf_now();
    pf.ns_wait += t1 - t0;
    var heads: i64[PF_MAX_K];
    var offsets: i64[PF_MAX_K];
    let k: u32 = pf.k;
    block {
        var i: u32 = 0;
        while i < k {
            let id: i64 = (fg_cuda_host_load_acquire_u32(pf.host_ids + (4 * (1 + i)) as! u64) as! i32) as! i64;
            if id < 0 || id >= PF_EXPERTS { pf.skipped += 1; return 0; }
            var at: u32 = i;
            while at != 0 && heads[at - 1] > id { heads[at] = heads[at - 1]; at -= 1; }
            heads[at] = id;
            i += 1;
        }
    };
    let active = fg_cuda_host_view_i64(cuda_slice_address(heads), (k as! i64));
    let out = fg_cuda_host_view_i64(cuda_slice_address(offsets), (k as! i64));
    for p in 0 .. 3 {
        let ready: i64 = prepare_expert_offsets(pf.plan[next].w[p], pf.plan[next].plane[p], active, out, true, true, p != 0);
        if ready != 1 {
            reset_expert_stage();
            pf.skipped += 1;
            return 0;
        }
    }
    let jobs: u64 = cuda.expert_upload_count;
    let t2: u64 = pf_now();
    pf.ns_prepare += t2 - t1;
    if jobs == 0 { return 0; }
    if cuda.expert_split_planes && pf.plan[next].disjoint
        { cuda.expert_upload_split = fg143_expert_down_split(
            expert_job_bytes(cuda.expert_upload_jobs, jobs),
            pf.plan[next].down_base, pf.plan[next].down_plane, PF_EXPERTS as! u64) as! u64; }
    let status: i32 = submit_expert_stage(cuda.experts.one_touch);
    if status == 0 { pf.waves += 1; pf.jobs += jobs; }
    pf.ns_submit += pf_now() - t2;
    if pf.waves % 10000 == 0
        { fg_cuda_host_diagnostic_69((pf.launched as! u64), (pf.waves as! u64), (pf.jobs as! u64), (pf.skipped as! u64), ((pf.ns_launch / 1000) as! u64), ((pf.ns_wait / 1000) as! u64), ((pf.ns_prepare / 1000) as! u64), ((pf.ns_submit / 1000) as! u64)); }
    return status;
}
// ---- end EXP-PF1 ----

export fn fg_cuda_plane(weights: u64, weight_bytes: i64, row_stride: i64, extent: i64, kind: i64, rows: i64, jobs: i64[:]) -> i64 {
    var weight: u64 = 0;
    if cuda.dense_ranges.len > 0 {
        let offset: i64 = dw_cuda_dense_offset(cuda.dense_ranges, weights, weight_bytes);
        if offset >= 0 { weight = cuda.dense.address + (offset as! u64); }
    }
    if weight != 0 { pf_note_router(weights, weight, kind, rows, row_stride, weight_bytes); }
    if weight == 0 && activation_input(fg_cuda_host_view_u8(((weights as! u64) as! u64), (weight_bytes as! i64)), &cuda.weight, &weight) != 0 {
        fg_cuda_host_diagnostic_70();
        return -1;
    }
    let status = launch_matrix(weight, row_stride, extent, kind, rows, jobs, 2, 0, 0);
    return status as! i64;
}

extern fn i2_prior_init(argc: i32, argv: c_ptr<c_ptr<i8>>);
extern fn i2_slru_prefill_prior(keys: u64, prev: u64, next: u64, prot: u64, head: u64, tail: u64, frequency: i64[:], visits: i64[:], entries: i64[:], scratch: i64[:], protected_count: i64[:], experts: i64) -> i64;

const i2_prior_layers: i64 = 24;
const i2_prior_experts: i64 = 128;
const i2_prior_cap: i64 = i2_prior_layers * i2_prior_experts;

struct CudaState_i2_prior {
    track: bool,
    frequency: i64[i2_prior_cap],
    visits: i64[i2_prior_layers],
}

export fn init_inkling_prior() {
    i2_prior_init(0, null);
}

fn i2_apply_prior(frequency: i64[:], visits: i64[:]) {
    if frequency.len != i2_prior_cap || visits.len != i2_prior_layers { return; }
    var cache .= cuda.experts;
    if cache.groups.len == 9 && cache.layers.len == 4 * i2_prior_layers && cache.state != 0 {
        let g = cache.groups;
        let slots: i64 = g[5];
        let scalar: i64 = g[8] + g[3];
        if g[3] == i2_prior_experts && slots >= 2 && slots <= i2_prior_cap && scalar + 1 < cache.work.len {

            fg_cuda_host_once_prior();
            var entries: i64[i2_prior_cap];
            var scratch: i64[i2_prior_cap];
            let keys = cache.state + (4 * g[7]) as! u64;
            let stride = (4 * slots) as! u64;
            let head: u64 = keys + 4 * stride;
            i2_slru_prefill_prior(keys, keys + stride, keys + 2 * stride, keys + 3 * stride, head, head + 8,
                frequency, visits,
                fg_cuda_host_view_i64(cuda_slice_address(entries), (slots as! i64)), fg_cuda_host_view_i64(cuda_slice_address(scratch), (slots as! i64)),
                fg_cuda_host_view_i64((cuda_slice_address(cache.work) + (8 * (scalar + 1)) as! u64), (1 as! i64)), i2_prior_experts);
        }
    }
}

const i2_warm_visits: i64 = 48;
struct CudaState_i2_warm {
    track: bool,
    reuse: bool,
    count: i32,
    previous_count: i32,
    step: i32,
    sequence: u8[i2_warm_visits],
    last_sequence: u8[i2_warm_visits],
    recorded: u64[i2_prior_cap],
    last: u64[i2_prior_cap],
    rank: i64[i2_prior_cap],
    policy_ns: u64,
    buckets: bool,
    measure: bool,
}

extern fn i2_bucket_init(argc: i32, argv: c_ptr<c_ptr<i8>>);

export fn init_inkling_buckets() {
    i2_bucket_init(0, null);
}

extern fn fg_cuda_host_thread_ns() -> i64;

export fn fg_i2_warm_tick() -> i64 {
    let tls = cuda_thread_state();
    return if tls[0].i2_warm.measure { fg_cuda_host_thread_ns() } else { 0 };
}

export fn fg_i2_warm_spent(start: i64) {
    if start <= 0 { return; }
    let end = fg_i2_warm_tick();
    if end < start { return; }
    var tls = cuda_thread_state();
    tls[0].i2_warm.policy_ns += (end - start) as! u64;
}

fn i2_warm_bucket_ranks() -> i64[:] {
    var tls = cuda_thread_state();
    var warm .= tls[0].i2_warm;
    if !warm.track || !warm.reuse || !warm.buckets { return fg_cuda_host_view_i64(0, 0); }
    return fg_cuda_host_view_i64(cuda_slice_address(warm.rank), warm.rank.len);
}

fn i2_warm_phase(prefill: bool, position: i32, count: i32) {
    var thread_storage = cuda_thread_state();
    if cuda_local_i2_warm_phase_enabled_5272 < 0 { cuda_local_i2_warm_phase_enabled_5272 = if cuda_option("I2_WARM_ROUTE_REUSE") { 1 } else { 0 }; }
    if (cuda_local_i2_warm_phase_enabled_5272 == 0) { return; }

    if prefill && position == 0 {
        thread_storage[0].i2_warm.count = count;
        thread_storage[0].i2_warm.step = 0;
        thread_storage[0].i2_warm.track = true;
        thread_storage[0].i2_warm.reuse = thread_storage[0].i2_warm.previous_count == count;
        thread_storage[0].i2_warm.buckets = cuda_option("I2_WARM_ROUTE_BUCKETS");
        thread_storage[0].i2_warm.measure = cuda_env("I2_WARM_ROUTE_LOG") != 0;
        thread_storage[0].i2_warm.policy_ns = 0;
        if thread_storage[0].i2_warm.reuse && thread_storage[0].i2_warm.buckets {

            fg_cuda_host_once_buckets();
            let start: i64 = fg_i2_warm_tick();
            block {
                var i: i64 = 0;
                while i < i2_prior_cap {
                    let remaining: u64 = thread_storage[0].i2_warm.last[i];
                    thread_storage[0].i2_warm.rank[i] = if remaining != 0 { i2_warm_visits - cuda_trailing_zeroes(remaining) } else { 0 };
                    i += 1;
                }
            };
            fg_i2_warm_spent(start);
        }
        fg_cuda_host_memset(cuda_slice_address(thread_storage[0].i2_warm.recorded), 0, (thread_storage[0].i2_warm.recorded.len as! u64) * 8);
        return;
    }
    if !thread_storage[0].i2_warm.track { return; }
    if !prefill && thread_storage[0].i2_warm.measure
        { if thread_storage[0].i2_warm.buckets { fg_cuda_host_diagnostic_110(thread_storage[0].i2_warm.policy_ns); } else { fg_cuda_host_diagnostic_111(thread_storage[0].i2_warm.policy_ns); } }
    if !prefill && thread_storage[0].i2_warm.step as! i64 == i2_warm_visits {
        fg_cuda_host_memcpy(cuda_slice_address(thread_storage[0].i2_warm.last), cuda_slice_address(thread_storage[0].i2_warm.recorded), (thread_storage[0].i2_warm.last.len as! u64) * 8);
        fg_cuda_host_memcpy(cuda_slice_address(thread_storage[0].i2_warm.last_sequence), cuda_slice_address(thread_storage[0].i2_warm.sequence), thread_storage[0].i2_warm.sequence.len as! u64);
        thread_storage[0].i2_warm.previous_count = thread_storage[0].i2_warm.count;
    } else {
        thread_storage[0].i2_warm.previous_count = 0;
    }
    thread_storage[0].i2_warm.track = false;
    thread_storage[0].i2_warm.reuse = false;
}

// Called under cuda_lock. One observation per gate/up projection.
fn cuda_trailing_zeroes(nonzero: u64) -> i64 {
    var value = nonzero;
    var count: i64 = 0;
    while value != 0 && (value & 1) == 0 {
        value >>= 1;
        count += 1;
    }
    return count;
}

fn i2_warm_visit(tensor: u64, active: i64[:]) {
    var tls = cuda_thread_state();
    var warm .= tls[0].i2_warm;
    let cache: const expert_cache& = cuda.experts;
    if !warm.track || active.len != i2_prior_experts ||
        cache.groups.len != 9 || cache.layers.len != 4 * i2_prior_layers { return; }
    for layer in i2_prior_layers {
        if (cache.layers[4 * layer + 1] as! u64) != tensor { continue; }
        if (warm.step as! i64) >= i2_warm_visits {
            warm.track = false;
            warm.reuse = false;
            warm.previous_count = 0;
            return;
        }
        let step = warm.step as! i64;
        warm.step += 1;
        warm.sequence[step] = layer as! u8;
        if warm.reuse && warm.last_sequence[step] != (layer as! u8) { warm.reuse = false; }
        let bit = (1 as! u64) << (step as! u64);
        let base = layer * i2_prior_experts;
        if warm.reuse && warm.buckets {
            let begin = fg_i2_warm_tick();
            for expert in i2_prior_experts {
                let key = base + expert;
                let old = warm.last[key];
                if (old & bit) == 0 { continue; }
                let remaining = old >> ((step + 1) as! u64);
                warm.rank[key] = if remaining != 0 {
                    i2_warm_visits - step - 1 - cuda_trailing_zeroes(remaining)
                } else { 0 };
            }
            fg_i2_warm_spent(begin);
        } else if warm.reuse {
            let begin = fg_i2_warm_tick();
            var score: i64[i2_prior_cap];
            var visits: i64[i2_prior_layers];
            for i in visits.len { visits[i] = 1; }
            for i in score.len {
                let remaining = warm.last[i] >> (step as! u64);
                score[i] = if remaining != 0 { i2_warm_visits - step - cuda_trailing_zeroes(remaining) } else { 0 };
            }
            i2_apply_prior(score, visits);
            fg_i2_warm_spent(begin);
        }
        for expert in i2_prior_experts {
            if active[expert] >= 0 { warm.recorded[base + expert] |= bit; }
        }
        return;
    }
}

export fn fg_cuda_inkling_expert_phase(prefill: bool, position: i32, count: i32) {
    var thread_storage = cuda_thread_state();
    if !thread_storage[0].cuda_inkling_quants { return; }
    if cuda_local_fg_cuda_inkling_expert_phase_trace_enabled_5378 < 0 {
        var trace: u64 = cuda_env("I2_EXPERT_ROUTE_TRACE");
        cuda_local_fg_cuda_inkling_expert_phase_trace_enabled_5378 = if cuda_env_equal(trace, "1") { 1 } else { 0 };
    }
    if prefill && position == 0 { thread_storage[0].i2_expert_trace.started = false; }
    i2_lfu_phase(prefill, position, count);
    thread_storage[0].i2_expert_trace.active = (cuda_local_fg_cuda_inkling_expert_phase_trace_enabled_5378 != 0) && !prefill && count == 1;
    thread_storage[0].i2_expert_trace.position = position;
    thread_storage[0].i2_expert_trace.visit = 0;
    thread_storage[0].inkling_decode_overlap = !prefill && count == 1 &&
        cuda_option("I2_DECODE_EXPERT_HIT_OVERLAP");
    if thread_storage[0].inkling_overlap_groups != 0 && (cuda_env("I2_DECODE_EXPERT_HIT_OVERLAP_LOG") != 0) {
        fg_cuda_host_diagnostic_71((thread_storage[0].inkling_overlap_groups as! u64), (thread_storage[0].inkling_overlap_resident as! u64), (thread_storage[0].inkling_overlap_missing as! u64));
    }
    thread_storage[0].inkling_overlap_missing = 0;
    thread_storage[0].inkling_overlap_resident = 0;
    thread_storage[0].inkling_overlap_groups = 0;
    if cuda_env("I2_WARM_ROUTE_LOG") != 0 {
        if (prefill && position == 0) || (!prefill && thread_storage[0].cuda_local_fg_cuda_inkling_expert_phase_measuring_5396) {
            var stats: i64[6];
            fg_cuda_expert_stats(fg_cuda_host_view_i64(cuda_slice_address(stats), (6 as! i64)));
            if prefill {
                thread_storage[0].cuda_local_fg_cuda_inkling_expert_phase_begin_bytes_5395 = stats[3] as! u64;
                thread_storage[0].cuda_local_fg_cuda_inkling_expert_phase_begin_fills_5395 = stats[1] as! u64;
                thread_storage[0].cuda_local_fg_cuda_inkling_expert_phase_measuring_5396 = true;
            } else {
                fg_cuda_host_diagnostic_72(((stats[1] as! u64) - thread_storage[0].cuda_local_fg_cuda_inkling_expert_phase_begin_fills_5395 as! u64), ((stats[3] as! u64) - thread_storage[0].cuda_local_fg_cuda_inkling_expert_phase_begin_bytes_5395 as! u64));
                thread_storage[0].cuda_local_fg_cuda_inkling_expert_phase_measuring_5396 = false;
            }
        }
    }
    i2_warm_phase(prefill, position, count);
    if cuda_local_fg_cuda_inkling_expert_phase_enabled_5376 < 0 { cuda_local_fg_cuda_inkling_expert_phase_enabled_5376 = if cuda_option("I2_EXPERT_PREFILL_PRIOR") { 1 } else { 0 }; }
    if (cuda_local_fg_cuda_inkling_expert_phase_enabled_5376 == 0) { return; }
    if prefill {
        if position == 0 {
            thread_storage[0].i2_prior = cuda_zero_CudaState_i2_prior();
            thread_storage[0].i2_prior.track = true;
        }
        return;
    }
    if !thread_storage[0].i2_prior.track { return; }
    thread_storage[0].i2_prior.track = false;
    fg_cuda_host_lock(CUDA_HOST_CONTEXT_LOCK);
    i2_apply_prior(thread_storage[0].i2_prior.frequency, thread_storage[0].i2_prior.visits);
    fg_cuda_host_unlock(CUDA_HOST_CONTEXT_LOCK);
}

// Driver dependency: expert-decode-overlap
const EXPERT_OVERLAP_ROUTES: i64 = 8;
const EXPERT_OVERLAP_JOB_WORDS: i64 = 3;

fn launch_expert_overlap(bank: u64, weight_bytes: i64, row_stride: i64, extent: i64, kind: i64, rows: i64, jobs: i64[:], shared_rows: i64, input_stride: i64) -> i32 {
    var thread_storage = cuda_thread_state();
    let pending = thread_storage[0].inkling_overlap_current_uploads;
    if pending == 0 {
        return launch_matrix(bank, row_stride, extent, kind, rows, jobs, 3, shared_rows, input_stride);
    }

    let words = EXPERT_OVERLAP_JOB_WORDS;
    var resident: i64[EXPERT_OVERLAP_ROUTES * EXPERT_OVERLAP_JOB_WORDS];
    var awaiting: i64[EXPERT_OVERLAP_ROUTES * EXPERT_OVERLAP_JOB_WORDS];
    var hit_words: i64 = 0;
    var miss_words: i64 = 0;
    if (jobs.len <= EXPERT_OVERLAP_ROUTES * words) {
        block {
            var at: i64 = 0;
            while at < jobs.len {
                let begin: u64 = bank + (jobs[at + 2] as! u64);
                let end: u64 = begin + (weight_bytes as! u64);
                var pending_source: bool = false;
                block {
                    var n: u64 = 0;
                    while n < pending {
                        let upload = cuda.expert_upload_jobs[n];
                        if (begin < upload.destination + upload.bytes && upload.destination < end) {
                            pending_source = true;
                            break;
                        }
                        n += 1;
                    }
                };
                if pending_source {
                    for word in words { awaiting[miss_words + word] = jobs[at + word]; }
                    miss_words += words;
                } else {
                    for word in words { resident[hit_words + word] = jobs[at + word]; }
                    hit_words += words;
                }
                at += words;
            }
        };
    }

    let hit = fg_cuda_host_view_i64(cuda_slice_address(resident), hit_words);
    let miss = if jobs.len <= EXPERT_OVERLAP_ROUTES * words {
        fg_cuda_host_view_i64(cuda_slice_address(awaiting), miss_words)
    } else { jobs };
    var status: i32 = 0;
    if hit.len != 0 { status = launch_matrix(bank, row_stride, extent, kind, rows, hit, 3,
                                        shared_rows, input_stride); }
    if (status == 0) { status = submit_expert_uploads(pending); }
    if status == 0 && miss.len != 0 {
        status = fg_cuda_driver_stream_wait_event(0, cuda.expert_uploads_ready, 0);
        if (status == 0) { cuda.expert_upload_queued = false; }
        if (status == 0) { status = launch_matrix(bank, row_stride, extent, kind, rows, miss, 3,
                                                shared_rows, input_stride); }
    }
    if status == 0 && hit.len != 0 && miss.len != 0 {
        thread_storage[0].inkling_overlap_groups += 1;
        thread_storage[0].inkling_overlap_resident += (hit.len as! u64) / (words as! u64);
        thread_storage[0].inkling_overlap_missing += (miss.len as! u64) / (words as! u64);
    }
    return status;
}

export fn fg_cuda_expert_batch(weight_bytes: i64, row_stride: i64, extent: i64, kind: i64, rows: i64, jobs: i64[:], shared_rows: i64, input_stride: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    let bank: const device_buffer& = cuda.experts.weights;
    if (bank.address == 0) || weight_bytes <= 0 || (weight_bytes as! u64) > bank.bytes ||
        jobs.len <= 0 || jobs.len % 3 != 0 || jobs.len / 3 > INT_MAX { return -1; }
    block {
        var i: i64 = 2;
        while i < jobs.len {
            let offset: i64 = jobs[i];
            if offset < 0 || (offset as! u64) > bank.bytes - (weight_bytes as! u64) { return -1; }
            i += 3;
        }
    };
    if !thread_storage[0].cuda_inkling_quants {
        var status: i32 = launch_matrix(bank.address, row_stride, extent, kind, rows, jobs, 3, shared_rows, input_stride);
        if status == 0 { status = submit_expert_stage(false); }
        if status == 0 { thread_storage[0].cuda_cached_projections += 1; }
        return status;
    }
    if thread_storage[0].i2_prior.track && thread_storage[0].cuda_inkling_quants && kind == 23 && cuda.experts.groups.len == 9 {
        let g = cuda.experts.groups;
        let span: i64 = g[0] + g[1] + g[2];
        if span > 0 && g[3] == i2_prior_experts {
            let keys: const i32[:] = fg_cuda_host_view_i32(cuda.experts.state + (4 * g[7]) as! u64, g[5]);
            let first: i64 = (jobs[2] - g[6]) / span;
            if first >= 0 && first < g[5] && keys[first] >= 0 && keys[first] < i2_prior_cap
                { thread_storage[0].i2_prior.visits[keys[first] / i2_prior_experts] += 1; }
            block {
                var i: i64 = 2;
                while i < jobs.len {
                    let slot: i64 = (jobs[i] - g[6]) / span;
                    if slot >= 0 && slot < g[5] && keys[slot] >= 0 && keys[slot] < i2_prior_cap
                        { thread_storage[0].i2_prior.frequency[keys[slot]] += 1; }
                    i += 3;
                }
            };
        }
    }
    var status: i32 = launch_expert_overlap(bank.address, weight_bytes, row_stride, extent, kind, rows,
                                       jobs, shared_rows, input_stride);
    if status == 0 { status = submit_expert_uploads(cuda.expert_upload_count); }
    if status == 0 { thread_storage[0].cuda_cached_projections += 1; }
    return status;
}

export fn fg_cuda_routed_graph_enabled() -> u8 {
    var thread_storage = cuda_thread_state();
    return if cuda.routed_graph_enabled && cuda.mmvq_graph_cache.len != 0 && cuda.experts.weights.address != 0 &&
        thread_storage[0].activations.active && thread_storage[0].activations.defer_waits { 1 } else { 0 };
}

export fn fg_cuda_full_ffn_enabled() -> u8 {
    return if cuda.full_ffn_enabled { 1 } else { 0 };
}

fn ffn_kernel(f: u64, gx: u32, gy: u32, threads: u32, args: u64[:]) -> kernel_node_params {
    return kernel_node_params { function: f, grid_x: gx, grid_y: gy, grid_z: 1,
        block_x: threads, block_y: 1, block_z: 1, params: fg_cuda_driver_words_address(args) };
}

fn dense_weight(host: u64, bytes: i64, device: u64&) -> bool {
    if (cuda.dense.address == 0) { return false; }
    let offset: i64 = dw_cuda_dense_offset(cuda.dense_ranges, host, bytes);
    if offset < 0 { return false; }
    device = cuda.dense.address + (offset as! u64);
    return true;
}

fn read_routed_ids(host: u64) -> i64 {
    var thread_storage = cuda_thread_state();
    let seq: u32 = thread_storage[0].route_compact_pending.mailbox_sequence;
    if seq == 0 || thread_storage[0].route_compact_pending.ids_host != host
        { return fg_cuda_activation_read_address(host, 32); }
    let ranges = activation_ranges();
    let found: i64 = fg143_activation_overlap(ranges, host, 32, 0);
    if found < 0 || fg143_activation_overlap(ranges, host, 32, found + 1) >= 0 ||
        (thread_storage[0].activations.large_output[1] > 0 &&
         fg143_activation_overlap(activation_large_range(), host, 32, 0) >= 0)
        { return fg_cuda_activation_read_address(host, 32); }
    let record = found * ACTIVATION_WORDS;
    if (thread_storage[0].activations.ranges[record + 0] as! u64) != host || thread_storage[0].activations.ranges[record + 1] != 32 || thread_storage[0].activations.ranges[record + 3] != 0
        { return fg_cuda_activation_read_address(host, 32); }

    if push_context() != 1 { return -1; }
    let error: i32 = decode_capture_submit();
    var ready: bool = false;
    let verify = fg_cuda_host_verify_mailbox();
    let force_fallback = verify && (thread_storage[0].mailbox_verified + thread_storage[0].mailbox_forced + 1) % 257 == 0;
    if error == 0 && !force_fallback {
        for spin in 0 .. 200000 {
            if fg_cuda_host_load_acquire_u32((cuda.route_mailbox_host as! u64) + 32) == seq {
                fg_cuda_host_memcpy((host as! u64), cuda.route_mailbox_host, 32);
                thread_storage[0].activations.ranges[record + 3] = 1;
                ready = true;
                break;
            }
            fg_cuda_host_cpu_pause();
        }
    }
    if verify {
        var verify_error: i32 = 0;
        if ready {
            var expected: u32[8];
            verify_error = fg_cuda_driver_download(cuda_slice_address(expected), cuda.activations.address + (thread_storage[0].activations.ranges[record + 2] as! u64), 32);
            if verify_error == 0 {
                let actual = fg_cuda_host_view_i32(host, 8);
                for lane in 8 {
                    if expected[lane] != (actual[lane] as! u32) {
                        fg_cuda_host_diagnostic_73();
                        verify_error = -1;
                        break;
                    }
                }
            }
            thread_storage[0].mailbox_verified += 1;
        } else if force_fallback {
            thread_storage[0].mailbox_forced += 1;
        }
        if verify_error != 0 {
            finish_context();
            return -1;
        }
        if (thread_storage[0].mailbox_verified + thread_storage[0].mailbox_forced) % 10000 == 0 {
            fg_cuda_host_diagnostic_74(thread_storage[0].mailbox_verified, thread_storage[0].mailbox_forced);
        }
    }
    if finish_context() != 0 || error != 0 { return -1; }
    if !ready { thread_storage[0].mailbox_fallbacks += 1; }
    thread_storage[0].mailbox_reads += 1;
    if thread_storage[0].mailbox_reads == 1 || thread_storage[0].mailbox_reads % 10000 == 0 {
        fg_cuda_host_diagnostic_75((thread_storage[0].mailbox_reads as! u32), (thread_storage[0].mailbox_fallbacks as! u32));
    }
    return if ready { 0 } else { fg_cuda_activation_read_address(host, 32) };
}

// Three expert job planes precede normalization, IDs and shared jobs.
const ROUTED_PACKET_PLANE_BYTES: u64 = 24 * 8;
const ROUTED_PACKET_CONFIG_BYTES: u64 = 22 * 8;
const ROUTED_PACKET_IDS_BYTES: u64 = 8 * 4;
const ROUTED_PACKET_BYTES: u64 = 3 * ROUTED_PACKET_PLANE_BYTES + ROUTED_PACKET_CONFIG_BYTES +
    ROUTED_PACKET_IDS_BYTES + 2 * 8;

struct RoutedGraphPacket {
    jobs: i64[72],
    config: i64[22],
    ids: i32[8],
    dense_jobs: i64[2],
}

const ROUTED_GRAPH_ROUTES: i64 = 8;
const ROUTED_GRAPH_EXPERTS: i64 = 256;

export fn fg_cuda_routed_graph(plan: i64[:], glu: i64[:], norm: i64[:], shared: i64[:], step: i64, deadline: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    let routes = ROUTED_GRAPH_ROUTES;
    let experts = ROUTED_GRAPH_EXPERTS;
    const words: i64 = 3;
    if fg_cuda_routed_graph_enabled() == 0 { return 0; }
    if plan.len != 10 || glu.len != 26 || norm.len != 22 || (shared.len != 0 && shared.len != 138) { return -1; }
    let width: i64 = plan[7];
    let hidden: i64 = plan[8];
    let used: i64 = routes;
    if width <= 0 || width > 8192 || (width % 256 != 0) || hidden <= 0 || hidden > 8192 || (hidden % 256 != 0) ||
        (plan[9] != 12 && plan[9] != 14) ||
        norm[4] != experts || norm[12] != routes { return -1; }
    if !fg143_routed_layer_matches(cuda.experts.layers, plan) { return 0; }
    let full: bool = shared.len != 0;
    let shared_hidden: i64 = if full { shared[6] } else { 0 };
    var shared_down_kind: i32 = if full { (shared[7] as! i32) } else { 0 };
    if full && (!cuda.full_ffn_enabled || shared_hidden <= 0 || shared_hidden > 8192 ||
        (shared_hidden % 256 != 0) || (shared_down_kind != 12 && shared_down_kind != 14)) { return 0; }
    var shared_extent: i32 = ((shared_hidden / 256) as! i32);
    var shared_stride: u64 = (shared_extent as! u64) * (if shared_down_kind == 12 { 144 } else { 210 });
    var router_stride: u64 = (width as! u64) * 4;
    var dense_weights: u64[4];
    if full {
        let up_bytes: i64 = (width / 256) * 144 * shared_hidden;
        if !dense_weight((shared[0] as! u64), up_bytes, &dense_weights[0]) ||
            !dense_weight((shared[1] as! u64), up_bytes, &dense_weights[1]) ||
            !dense_weight((shared[2] as! u64), (shared_stride as! i64) * width, &dense_weights[2]) ||
            !dense_weight((shared[3] as! u64), (router_stride as! i64), &dense_weights[3]) { return 0; }
    }
    let input_host: u64 = (plan[3] as! u64);
    let probs_host: u64 = (plan[4] as! u64);
    let ids_host: u64 = (plan[5] as! u64);
    let residual_host: u64 = if full { (shared[4] as! u64) } else { 0 };
    let out_host: u64 = ((if full { shared[5] } else { plan[6] }) as! u64);
    let output_bytes: i64 = width * 4;
    if activation_device(input_host, output_bytes) == 0 || activation_device(probs_host, experts * 4) == 0 { return 0; }
    if full && activation_device(residual_host, output_bytes) == 0 { return 0; }
    // A planned host slot can hold the FFN input and then the residual
    // result. Keep its exact device record live until the final add.
    var reused_input: i64 = -1;
    if full && out_host == input_host {
        block {
            var i: i64 = 0;
            while i < thread_storage[0].activations.count {
                let record = i * ACTIVATION_WORDS;
                if (thread_storage[0].activations.ranges[record] as! u64) == input_host &&
                    thread_storage[0].activations.ranges[record + 1] == output_bytes {
                    reused_input = record;
                    break;
                }
                i += 1;
            }
        };
        if reused_input < 0 { return 0; }
    }
    decode_split_armed = 0;
    let read_status = read_routed_ids(ids_host);
    if read_status != 0 { return -1; }
    var ids: i32[ROUTED_GRAPH_ROUTES];
    fg_cuda_host_memcpy(cuda_slice_address(ids), ids_host, (ids.len as! u64) * 4);
    let stage_all: bool = full && cuda.expert_decode_overlap && thread_storage[0].activations.defer_waits;
    let compact: bool = stage_all && cuda.compact_moe_ids && fg143_cpu_prefill() == 0;
    let compact_weights: bool = compact && cuda.route_weights.address != 0 &&
        thread_storage[0].route_compact_pending.ids_host == ids_host && thread_storage[0].route_compact_pending.scores_host == probs_host &&
        thread_storage[0].route_compact_pending.scores_device == activation_device(probs_host, experts * 4) &&
        norm[0] == 4 && norm[4] == experts && norm[12] == routes &&
        (norm[21] == 0 || norm[21] == 1065353216);
    thread_storage[0].route_compact_pending.ids_host = 0;
    var heads: i64[ROUTED_GRAPH_EXPERTS];
    var offsets: i64[ROUTED_GRAPH_EXPERTS];
    var rank: u32[ROUTED_GRAPH_ROUTES];
    if compact {
        var order: u32[ROUTED_GRAPH_ROUTES];
        block {
            var r: i64 = 0;
            while r < routes {
                let id = ids[r] as! i64;
                if id < 0 || id >= experts { return 0; }
                var at = r;
                while at > 0 && heads[at - 1] > id {
                    heads[at] = heads[at - 1];
                    order[at] = order[at - 1];
                    at -= 1;
                }
                if at > 0 && heads[at - 1] == id { return 0; }
                heads[at] = id;
                order[at] = r as! u32;
                r += 1;
            }
        };
        block {
            var i: i64 = 0;
            while i < routes {
                rank[order[i]] = i as! u32;
                i += 1;
            }
        };
    } else {
        block {
            var e: i64 = 0;
            while e < experts {
                heads[e] = -1;
                e += 1;
            }
        };
        block {
            var r: i64 = 0;
            while r < routes {
                let id = ids[r] as! i64;
                if id < 0 || id >= experts || heads[id] >= 0 { return 0; }
                heads[id] = (r as! i64);
                r += 1;
            }
        };
    }
    if reused_input < 0 && fg_cuda_activation_prepare(out_host, output_bytes, true, false, step, deadline) != 0 { return -1; }
    let active: i32 = start_context();
    if active != 1 { return active; }

    // Both Q4_K projections consume the same packed input. The down pack
    // can reuse it after the two matvecs and SwiGLU have completed.
    var input_extent: i32 = ((width / 256) as! i32);
    var down_extent: i32 = ((hidden / 256) as! i32);
    var hidden_rows: i32 = (hidden as! i32);
    var output_rows: i32 = (width as! i32);
    var q4: i32 = 12;
    var down_kind: i32 = (plan[9] as! i32);
    var job_words = words as! i32;
    var stride_up: u64 = (input_extent as! u64) * 144;
    var stride_down: u64 = (down_extent as! u64) * (if down_kind == 12 { 144 } else { 210 });
    let routed_pack: u64 = ((if input_extent > down_extent { input_extent } else { down_extent }) as! u64) * 288 * (routes as! u64);
    let shared_pack: u64 = (shared_extent as! u64) * 288;
    let packed_bytes: u64 = if routed_pack > shared_pack { routed_pack } else { shared_pack };
    let pack_space: u64 = packed_bytes + (if stage_all { shared_pack } else { 0 });
    let middle_bytes: u64 = (hidden as! u64) * (routes as! u64) * 4;
    let down_bytes: u64 = (width as! u64) * (routes as! u64) * 4;
    var packet: RoutedGraphPacket;
    let shared_bytes: u64 = if full { ((3 * shared_hidden + 4 * width + 2) as! u64) * 4 } else { 0 };
    let scratch_bytes: u64 = ROUTED_PACKET_BYTES + pack_space + 3 * middle_bytes + down_bytes + (routes as! u64) * 4 + shared_bytes;
    var error: i32 = reserve(&cuda.routed_graph_scratch, scratch_bytes);
    if error == 2 { return if finish_context() == 0 { 0 } else { -1 }; }
    var output: u64 = if reused_input >= 0 { cuda.activations.address + (thread_storage[0].activations.ranges[reused_input + 2] as! u64) } else { 0 };
    if error == 0 && reused_input < 0 { error = activation_output(out_host, output_bytes, &output); }
    let input: u64 = activation_device(input_host, output_bytes);
    let probs: u64 = activation_device(probs_host, experts * 4);
    var residual: u64 = if full { activation_device(residual_host, output_bytes) } else { 0 };
    if error == 0 && (input == 0 || probs == 0 || (full && residual == 0) || (reused_input >= 0 && input != output)) {
        return if finish_context() == 0 { 0 } else { -1 };
    }
    let pf_this: i32 = if compact && stage_all && error == 0 { pf_layer((plan[0] as! u64)) } else { -1 };
    if pf_this >= 0 {
        let gate_up_bytes: u64 = stride_up * (hidden as! u64) * (experts as! u64);
        let down_all: u64 = stride_down * (width as! u64) * (experts as! u64);
        let down_base: u64 = (plan[2] as! u64);
        var disjoint: bool = true;
        for p in 0 .. 2 {
            let source: u64 = (plan[p] as! u64);
            let overlaps = if source <= down_base { down_base - source < gate_up_bytes }
                else { source - down_base < down_all };
            if overlaps { disjoint = false; }
        }
        pf.plan[pf_this].w[0] = (plan[0] as! u64);
        pf.plan[pf_this].w[1] = (plan[1] as! u64);
        pf.plan[pf_this].w[2] = (plan[2] as! u64);
        let gate_up_plane = (stride_up * (hidden as! u64)) as! i64;
        pf.plan[pf_this].plane[1] = gate_up_plane;
        pf.plan[pf_this].plane[0] = gate_up_plane;
        pf.plan[pf_this].plane[2] = ((stride_down * (width as! u64)) as! i64);
        pf.plan[pf_this].down_base = down_base;
        pf.plan[pf_this].down_plane = ((stride_down * (width as! u64)) as! u64);
        pf.plan[pf_this].disjoint = disjoint;
        pf.plan[pf_this].ok = true;
        pf_launch(pf_this, input, width);
    }
    if stage_all && error == 0 {
        if cuda_local_fg_cuda_routed_graph_defer_prior_wave_5763 < 0 {
            var setting: u64 = cuda_env("GOOSE_DEFER_PREFETCH_FENCE");
            cuda_local_fg_cuda_routed_graph_defer_prior_wave_5763 = if cuda_env_equal(setting, "0") { 0 } else { 1 };
        }
        // Shared FFN uses dense weights. The expert consumer fences below
        // cover the previous wave and any newer copies on the same stream.
        let defer: bool = (cuda_local_fg_cuda_routed_graph_defer_prior_wave_5763 != 0) && compact && cuda.decode_graph_enabled;
        if defer && cuda.expert_upload_queued {
            thread_storage[0].cuda_local_fg_cuda_routed_graph_deferred_5772 += 1;
            if thread_storage[0].cuda_local_fg_cuda_routed_graph_deferred_5772 == 1 || thread_storage[0].cuda_local_fg_cuda_routed_graph_deferred_5772 % 10000 == 0
                { fg_cuda_host_diagnostic_76((thread_storage[0].cuda_local_fg_cuda_routed_graph_deferred_5772 as! u32)); }
        }
        error = begin_expert_prepare(defer);
    }
    let active_routes = fg_cuda_host_view_i64(cuda_slice_address(heads),
        if compact { routes } else { experts });
    let device_offsets = fg_cuda_host_view_i64(cuda_slice_address(offsets), active_routes.len);
    block {
        var p: i64 = 0;
        while p < 3 && error == 0 {
            let plane: i64 = ((if p == 2 { stride_down * (width as! u64) } else { stride_up * (hidden as! u64) }) as! i64);
            let weight: u64 = (plan[p] as! u64);
            let ready: i64 = if stage_all { prepare_expert_offsets(weight, plane, active_routes, device_offsets, true, compact, p != 0) } else { fg_cuda_prepare_experts(weight, plane, active_routes, device_offsets) };
            if ready == 0 && p == 0 { return if finish_context() == 0 { 0 } else { -1 }; }
            if ready != 1 || (!stage_all && cuda.expert_upload_count != 0) {
                reset_expert_stage();
                error = -1;
                break;
            }
            block {
                var r: i64 = 0;
                while r < routes {
                    let offset: i64 = offsets[if compact { rank[r] } else { (ids[r] as! u32) }];
                    if offset < 0 || (offset as! u64) > cuda.experts.weights.bytes ||
                        (plane as! u64) > cuda.experts.weights.bytes - (offset as! u64) {
                        error = -1;
                        break;
                    }
                    let job = p * routes * words + r * words;
                    packet.jobs[job] = if p == 2 { (r as! i64) * hidden * 4 } else { 0 };
                    packet.jobs[job + 1] = (r as! i64) * (if p == 2 { width } else { hidden }) * 4;
                    packet.jobs[job + 2] = offset;
                    r += 1;
                }
            };
            p += 1;
        }
        if stage_all && error == 0 {
            if cuda.expert_split_planes {
                let gate_up_bytes: u64 = stride_up * (hidden as! u64) * (experts as! u64);
                let down_bytes_all: u64 = stride_down * (width as! u64) * (experts as! u64);
                let down_base: u64 = (plan[2] as! u64);
                var disjoint: bool = true;
                for p in 0 .. 2 {
                    let source: u64 = (plan[p] as! u64);
                    let overlaps = if source <= down_base {
                        down_base - source < gate_up_bytes
                    } else {
                        source - down_base < down_bytes_all
                    };
                    if overlaps { disjoint = false; }
                }
                if disjoint {
                    cuda.expert_upload_split = fg143_expert_down_split(expert_job_bytes(cuda.expert_upload_jobs, cuda.expert_upload_count),
                        down_base, stride_down * (width as! u64), experts as! u64) as! u64;
                }
            }
            error = submit_expert_stage(compact && cuda.experts.one_touch);
        }
        if error == 0 {
            for i in 22 { packet.config[i] = norm[i]; }
            for i in 8 { packet.ids[i] = ids[i]; }
            error = upload_packet(cuda.routed_graph_scratch.address,
                unsafe_c_address<c_ptr<const void>>(packet), ROUTED_PACKET_BYTES);
        }
        if error == 0 {
            let scratch: u64 = cuda.routed_graph_scratch.address;
            var jobs_up: u64 = scratch;
            var jobs_gate: u64 = jobs_up + ROUTED_PACKET_PLANE_BYTES;
            var jobs_down: u64 = jobs_gate + ROUTED_PACKET_PLANE_BYTES;
            var norm_cfg: u64 = jobs_down + ROUTED_PACKET_PLANE_BYTES;
            var selected: u64 = norm_cfg + ROUTED_PACKET_CONFIG_BYTES;
            var packed: u64 = scratch + ROUTED_PACKET_BYTES;
            var packed_shared: u64 = if stage_all { packed + packed_bytes } else { packed };
            var up: u64 = packed + pack_space;
            var gate: u64 = up + middle_bytes;
            var activated: u64 = gate + middle_bytes;
            var down: u64 = activated + middle_bytes;
            var normalized: u64 = down + down_bytes;
            var bank: u64 = cuda.experts.weights.address;
            var vector: i64[26];
            for i in 26 { vector[i] = glu[i]; }
            if cuda.activation_reference { vector[0] = 15; }
            var zero: f32 = (0.0 as! f32);
            var pack_up: u64[5] = [
                cuda_kernel_argument_address(input), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(jobs_up),
                cuda_kernel_argument_address(input_extent), cuda_kernel_argument_address(job_words),
            ];
            var matrix_up: u64[8] = [
                cuda_kernel_argument_address(bank), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(up),
                cuda_kernel_argument_address(jobs_up), cuda_kernel_argument_address(stride_up), cuda_kernel_argument_address(input_extent),
                cuda_kernel_argument_address(q4), cuda_kernel_argument_address(job_words),
            ];
            var matrix_gate: u64[8] = [
                cuda_kernel_argument_address(bank), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(gate),
                cuda_kernel_argument_address(jobs_gate), cuda_kernel_argument_address(stride_up), cuda_kernel_argument_address(input_extent),
                cuda_kernel_argument_address(q4), cuda_kernel_argument_address(job_words),
            ];
            var gl: u64[6] = [
                cuda_kernel_argument_address(gate), cuda_kernel_argument_address(up), cuda_kernel_argument_address(activated), cuda_slice_address(vector),
                cuda_kernel_argument_address(zero), cuda_kernel_argument_address(zero),
            ];
            var pack_down: u64[5] = [
                cuda_kernel_argument_address(activated), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(jobs_down),
                cuda_kernel_argument_address(down_extent), cuda_kernel_argument_address(job_words),
            ];
            var matrix_down: u64[8] = [
                cuda_kernel_argument_address(bank), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(down),
                cuda_kernel_argument_address(jobs_down), cuda_kernel_argument_address(stride_down), cuda_kernel_argument_address(down_extent),
                cuda_kernel_argument_address(down_kind), cuda_kernel_argument_address(job_words),
            ];
            var normalize: u64[4] = [
                cuda_kernel_argument_address(probs), cuda_kernel_argument_address(selected), cuda_kernel_argument_address(normalized),
                cuda_kernel_argument_address(norm_cfg),
            ];
            var routed_out: u64 = if full { normalized + (routes as! u64) * 4 } else { output };
            var normalized_input: u64 = if compact_weights { cuda.route_weights.address } else { normalized };
            var reduce: u64[5] = [
                cuda_kernel_argument_address(down), cuda_kernel_argument_address(normalized_input), cuda_kernel_argument_address(routed_out),
                cuda_kernel_argument_address(width), cuda_kernel_argument_address(used),
            ];
            var dense_jobs: u64 = selected + ROUTED_PACKET_IDS_BYTES;
            let shared_row_bytes: u64 = (shared_hidden as! u64) * 4;
            var sh_up: u64 = routed_out + (output_bytes as! u64);
            var sh_gate: u64 = sh_up + shared_row_bytes;
            var sh_glu: u64 = sh_gate + shared_row_bytes;
            var sh_down: u64 = sh_glu + shared_row_bytes;
            var route_score: u64 = sh_down + (output_bytes as! u64);
            var route_gate: u64 = route_score + 4;
            var product: u64 = route_gate + 4;
            var mixed: u64 = product + (output_bytes as! u64);
            var dense_words: i32 = 2;
            var f32: i32 = 0;
            var first: i32 = 0;
            var shared_rows: i32 = (shared_hidden as! i32);
            var plans: i64[26][5];
            if full {
                for row in 5 {
                    for column in 26 { plans[row][column] = shared[8 + row * 26 + column]; }
                }
            }
            if full && cuda.activation_reference { plans[0][0] = 15; plans[1][0] = 13; }
            var dense_up: u64[8] = [
                cuda_kernel_argument_address(dense_weights[0]), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(sh_up),
                cuda_kernel_argument_address(dense_jobs), cuda_kernel_argument_address(stride_up), cuda_kernel_argument_address(input_extent),
                cuda_kernel_argument_address(q4), cuda_kernel_argument_address(dense_words),
            ];
            var dense_gate: u64[8] = [
                cuda_kernel_argument_address(dense_weights[1]), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(sh_gate),
                cuda_kernel_argument_address(dense_jobs), cuda_kernel_argument_address(stride_up), cuda_kernel_argument_address(input_extent),
                cuda_kernel_argument_address(q4), cuda_kernel_argument_address(dense_words),
            ];
            var dense_glu: u64[6] = [
                cuda_kernel_argument_address(sh_gate), cuda_kernel_argument_address(sh_up), cuda_kernel_argument_address(sh_glu), cuda_slice_address(plans[0]),
                cuda_kernel_argument_address(zero), cuda_kernel_argument_address(zero),
            ];
            var dense_pack: u64[5] = [
                cuda_kernel_argument_address(sh_glu), cuda_kernel_argument_address(packed_shared), cuda_kernel_argument_address(dense_jobs),
                cuda_kernel_argument_address(shared_extent), cuda_kernel_argument_address(dense_words),
            ];
            var dense_down: u64[8] = [
                cuda_kernel_argument_address(dense_weights[2]), cuda_kernel_argument_address(packed_shared), cuda_kernel_argument_address(sh_down),
                cuda_kernel_argument_address(dense_jobs), cuda_kernel_argument_address(shared_stride), cuda_kernel_argument_address(shared_extent),
                cuda_kernel_argument_address(shared_down_kind), cuda_kernel_argument_address(dense_words),
            ];
            var dense_router: u64[9] = [
                cuda_kernel_argument_address(dense_weights[3]), cuda_kernel_argument_address(input), cuda_kernel_argument_address(route_score),
                cuda_kernel_argument_address(dense_jobs), cuda_kernel_argument_address(router_stride), cuda_kernel_argument_address(output_rows),
                cuda_kernel_argument_address(f32), cuda_kernel_argument_address(first), cuda_kernel_argument_address(dense_words),
            ];
            var unused: u64 = 0;
            var sigmoid: u64[6] = [
                cuda_kernel_argument_address(route_score), cuda_kernel_argument_address(unused), cuda_kernel_argument_address(route_gate), cuda_slice_address(plans[1]),
                cuda_kernel_argument_address(zero), cuda_kernel_argument_address(zero),
            ];
            var scale: u64[6] = [
                cuda_kernel_argument_address(sh_down), cuda_kernel_argument_address(route_gate), cuda_kernel_argument_address(product), cuda_slice_address(plans[2]),
                cuda_kernel_argument_address(zero), cuda_kernel_argument_address(zero),
            ];
            var add_shared: u64[6] = [
                cuda_kernel_argument_address(routed_out), cuda_kernel_argument_address(product), cuda_kernel_argument_address(mixed), cuda_slice_address(plans[3]),
                cuda_kernel_argument_address(zero), cuda_kernel_argument_address(zero),
            ];
            var add_residual: u64[6] = [
                cuda_kernel_argument_address(mixed), cuda_kernel_argument_address(residual), cuda_kernel_argument_address(output), cuda_slice_address(plans[4]),
                cuda_kernel_argument_address(zero), cuda_kernel_argument_address(zero),
            ];
            let fused_tail: bool = full && cuda.moe_add_shared_residual != 0;
            if fused_tail && !cuda_local_fg_cuda_routed_graph_fused_tail_logged_5935 {
                cuda_local_fg_cuda_routed_graph_fused_tail_logged_5935 = true;
                fg_cuda_host_diagnostic_77();
            }
            var fused_add: u64[5] = [
                cuda_kernel_argument_address(routed_out), cuda_kernel_argument_address(sh_down), cuda_kernel_argument_address(route_gate),
                cuda_kernel_argument_address(residual), cuda_kernel_argument_address(output),
            ];
            let shared_fused: bool = stage_all && compact && cuda.experts.one_touch &&
                (cuda.mmvq_shared_up_gate_glu != 0) && width == 2048 && shared_hidden == 512 &&
                plans[0][0] == 15 && plans[0][1] == shared_hidden &&
                plans[0][2] == shared_hidden && plans[0][3] == 1 &&
                plans[0][6] == shared_hidden && plans[0][14] == shared_hidden &&
                plans[0][10] == 4 && plans[0][18] == 4 && plans[0][22] == 0;
            var shared_fused_args: u64[6] = [
                cuda_kernel_argument_address(dense_weights[0]), cuda_kernel_argument_address(dense_weights[1]), cuda_kernel_argument_address(packed),
                cuda_kernel_argument_address(sh_glu), cuda_kernel_argument_address(stride_up), cuda_kernel_argument_address(input_extent),
            ];
            let shared_tail_count: u64 = if fused_tail { 5 } else { 6 };
            var kernels: kernel_node_params[18];
            var shared_head: kernel_node_params[2];
            var shared_tail: kernel_node_params[6];
            if full {
                shared_head[0] = ffn_kernel(cuda.mmvq, (shared_rows as! u32), 1, 128, dense_up);
                shared_head[1] = ffn_kernel(cuda.mmvq, (shared_rows as! u32), 1, 128, dense_gate);
                shared_tail[0] = ffn_kernel(cuda.vector, (plans[0][24] as! u32), 1, 128, dense_glu);
                shared_tail[1] = ffn_kernel(cuda.mmvq_pack, 2 * (shared_extent as! u32), 1, 128, dense_pack);
                shared_tail[2] = ffn_kernel(cuda.mmvq, (output_rows as! u32), 1, 128, dense_down);
                shared_tail[3] = ffn_kernel(cuda.kernel, 1, 1, 32, dense_router);
                shared_tail[4] = ffn_kernel(cuda.vector, (plans[1][24] as! u32), 1, 128, sigmoid);
                shared_tail[5] = ffn_kernel(cuda.vector, (plans[2][24] as! u32), 1, 128, scale);
            }
            let normalize_routes: kernel_node_params = ffn_kernel(cuda.route_norm, 1, 1, 32, normalize);
            var count: u64 = 0;
            kernels[count] = ffn_kernel(cuda.mmvq_pack, 2 * (input_extent as! u32), routes as! u32, 128, pack_up); count += 1;
            if stage_all {
                // Shared FFN and route normalization do not read the expert bank.
                if shared_fused {
                    if !cuda_local_fg_cuda_routed_graph_reported_5973 {
                        cuda_local_fg_cuda_routed_graph_reported_5973 = true;
                        fg_cuda_host_diagnostic_78();
                    }
                    kernels[count] = ffn_kernel(cuda.mmvq_shared_up_gate_glu, shared_rows as! u32, 1, 128, shared_fused_args); count += 1;
                } else {
                    for node in shared_head.len { kernels[count + (node as! u64)] = shared_head[node]; }
                    count += 2;
                }
                let skip_glu: u64 = if shared_fused { 1 } else { 0 };
                for node in (skip_glu as! i64) .. (shared_tail_count as! i64) { kernels[count + (node as! u64) - skip_glu] = shared_tail[node]; }
                count += shared_tail_count - skip_glu;
                if !compact_weights { kernels[count] = normalize_routes; count += 1; }
            }
            let expert_wait_index: u64 = count;
            let fused_up_gate: bool = stage_all && compact && cuda.experts.one_touch && (cuda.mmvq_up_gate_glu != 0) &&
                width == 2048 && hidden == 512 && vector[0] == 15 && vector[1] == hidden * routes &&
                vector[2] == hidden && vector[3] == routes &&
                vector[6] == hidden && vector[7] == routes && vector[14] == hidden && vector[15] == routes &&
                vector[10] == 4 && vector[11] == hidden * 4 && vector[22] == 0 &&
                vector[18] == 4 && vector[19] == hidden * 4;
            var fused_glu: u64[7] = [
                cuda_kernel_argument_address(bank), cuda_kernel_argument_address(packed), cuda_kernel_argument_address(activated),
                cuda_kernel_argument_address(jobs_up), cuda_kernel_argument_address(jobs_gate), cuda_kernel_argument_address(stride_up),
                cuda_kernel_argument_address(input_extent),
            ];
            if fused_up_gate {
                if !cuda_local_fg_cuda_routed_graph_reported_6001 {
                    cuda_local_fg_cuda_routed_graph_reported_6001 = true;
                    fg_cuda_host_diagnostic_79();
                }
                kernels[count] = ffn_kernel(cuda.mmvq_up_gate_glu, hidden_rows as! u32, routes as! u32, 128, fused_glu); count += 1;
            } else {
                kernels[count] = ffn_kernel(cuda.mmvq, (hidden_rows as! u32), routes as! u32, 128, matrix_up); count += 1;
                kernels[count] = ffn_kernel(cuda.mmvq, (hidden_rows as! u32), routes as! u32, 128, matrix_gate); count += 1;
            }
            if full && !stage_all {
                for node in shared_head.len { kernels[count + (node as! u64)] = shared_head[node]; }
                count += 2;
            }
            if !fused_up_gate { kernels[count] = ffn_kernel(cuda.vector, (vector[24] as! u32), 1, 128, gl); count += 1; }
            kernels[count] = ffn_kernel(cuda.mmvq_pack, 2 * (down_extent as! u32), routes as! u32, 128, pack_down); count += 1;
            // The down plane is only read from here on, so its transfer can continue
            // while the gate and up projections above run.
            let expert_down_wait_index: u64 = if stage_all && cuda.expert_planes_split { count } else { 0 };
            let short_down: bool = (cuda.mmvq_short != 0) && down_extent <= 2 && output_rows % 4 == 0;
            kernels[count] = ffn_kernel(if short_down { cuda.mmvq_short } else { cuda.mmvq },
                ((if short_down { output_rows / 4 } else { output_rows }) as! u32), routes as! u32, 128, matrix_down); count += 1;
            if !stage_all { kernels[count] = normalize_routes; count += 1; }
            kernels[count] = ffn_kernel(cuda.moe_weighted, (1 as! u32), (((width + 255) / 256) as! u32), 256, reduce); count += 1;
            if full {
                if !stage_all {
                    for node in (shared_tail_count as! i64) { kernels[count + (node as! u64)] = shared_tail[node]; }
                    count += shared_tail_count;
                }
                if fused_tail {
                    kernels[count] = ffn_kernel(cuda.moe_add_shared_residual, ((width / 128) as! u32), 1, 128, fused_add); count += 1;
                } else {
                    kernels[count] = ffn_kernel(cuda.vector, (plans[3][24] as! u32), 1, 128, add_shared); count += 1;
                    kernels[count] = ffn_kernel(cuda.vector, (plans[4][24] as! u32), 1, 128, add_residual); count += 1;
                }
            }
            let key_prefix: u64[16] = [bank, scratch, input, probs, output,
                (width as! u64), (hidden as! u64), (vector[0] as! u64), UINT64_MAX - (down_kind as! u64), residual,
                dense_weights[0], dense_weights[1], dense_weights[2], dense_weights[3],
                (shared_hidden as! u64), (shared_down_kind as! u64)];
            var key: u64[MMVQ_GRAPH_KEY_WORDS];
            for key_index in key_prefix.len { key[key_index] = key_prefix[key_index]; }
            if full && cuda.decode_graph_enabled { thread_storage[0].decode_capture.scope = true; }
            let graph: u64 = if thread_storage[0].decode_capture.scope { 0 } else { cached_graph_for(key, kernels, count) };
            if stage_all {
                error = launch_cuda_kernels(kernels, expert_wait_index, 0);
                if error == 0 {
                    error = if cuda.expert_planes_split { decode_wait_for_expert_planes() } else { decode_wait_for_experts() };
                }
                let middle_end: u64 = if expert_down_wait_index != 0 { expert_down_wait_index } else { count };
                if error == 0 { error = launch_cuda_kernel_range(kernels, expert_wait_index, middle_end - expert_wait_index, 0); }
                if error == 0 && expert_down_wait_index != 0 {
                    error = decode_wait_for_experts();
                    if error == 0 { error = launch_cuda_kernel_range(kernels, expert_down_wait_index, count - expert_down_wait_index, 0); }
                }
                if error == 0 { cuda.expert_upload_queued = false; }
                if error == 0 && cuda.decode_early_ffn && cuda.decode_reuse && full && thread_storage[0].decode_capture.capturing {
                    error = decode_capture_submit();
                    if error == 0 && !cuda_local_fg_cuda_routed_graph_reported_6056 {
                        cuda_local_fg_cuda_routed_graph_reported_6056 = true;
                        fg_cuda_host_diagnostic_80();
                    }
                }
            } else {
                error = if graph != 0 { fg_cuda_driver_graph_launch(graph, cuda.graph_stream) } else { launch_cuda_kernels(kernels, count, 0) };
            }
            if error == 0 && graph != 0 {
                if full { thread_storage[0].cuda_full_ffn_replays += 1; }
                else { thread_storage[0].cuda_routed_replays += 1; }
            }
            if error != 0 && thread_storage[0].decode_capture.capturing {
                thread_storage[0].decode_capture.scope = false;
                decode_capture_discard();
            }
            error = finish_matrix_launches(error);
            if error == 0 && reused_input >= 0 {
                thread_storage[0].activations.ranges[reused_input + 3] = 0;
                thread_storage[0].activations.ranges[reused_input + 4] = deadline;
            } else if error == 0 {
                error = activation_publish(out_host, output_bytes, output, true);
            }
        }
        if error == 0 && pf_this >= 0 && pf.pending >= 0 { error = pf_submit(); }
        else { pf.pending = -1; }
        if stage_all && error != 0 { reset_expert_stage(); }
        let popped: i32 = finish_context();
        if error != 0 || popped != 0 {
            fg_cuda_host_diagnostic_81((if error != 0 { error } else { popped as! i32 }));
            return -1;
        }
        if compact {
            if decode_split_count < 0 {
                decode_split_count = fg_cuda_host_env_atoi(cuda_env("GOOSE_DECODE_SPLIT_COUNT"), 2);
            }
            decode_split_armed = decode_split_count;
        }
        if compact {
            if !cuda_local_fg_cuda_routed_graph_reported_6097 {
                cuda_local_fg_cuda_routed_graph_reported_6097 = true;
                fg_cuda_host_diagnostic_82();
            }
        }
        if compact_weights {
            thread_storage[0].cuda_local_fg_cuda_routed_graph_consumed_6104 += 1;
            if thread_storage[0].cuda_local_fg_cuda_routed_graph_consumed_6104 == 1 || thread_storage[0].cuda_local_fg_cuda_routed_graph_consumed_6104 % 10000 == 0 || thread_storage[0].cuda_local_fg_cuda_routed_graph_consumed_6104 == 40880
                { fg_cuda_host_diagnostic_83((thread_storage[0].cuda_local_fg_cuda_routed_graph_consumed_6104 as! u64)); }
        }
        if full { thread_storage[0].cuda_full_ffn_groups += 1; }
        else { thread_storage[0].cuda_routed_groups += 1; }
        thread_storage[0].cuda_matrix_launches += if full { 7 } else { 3 };
        thread_storage[0].cuda_cached_projections += 3;
        thread_storage[0].cuda_matrices += if full { 7 } else { 3 };
        if full { thread_storage[0].cuda_f32_matrices += 1; }
        thread_storage[0].cuda_vectors += if full { 6 } else { 1 };
        thread_storage[0].cuda_routes[4] += 1;
        thread_storage[0].cuda_moe_weighted_tiles += 1;
        return 1;
            p += 1;
        }
    };

export fn fg_cuda_take_routed_graph(groups: u64, replays: u64, full: u64, full_replays: u64) {
    var full_replays_out = fg_cuda_host_view_u64(full_replays, 1);
    var full_out = fg_cuda_host_view_u64(full, 1);
    var replays_out = fg_cuda_host_view_u64(replays, 1);
    var groups_out = fg_cuda_host_view_u64(groups, 1);
    var thread_storage = cuda_thread_state();
    groups_out[0] = thread_storage[0].cuda_routed_groups;
    replays_out[0] = thread_storage[0].cuda_routed_replays;
    thread_storage[0].cuda_routed_replays = 0;
    thread_storage[0].cuda_routed_groups = 0;
    full_out[0] = thread_storage[0].cuda_full_ffn_groups;
    full_replays_out[0] = thread_storage[0].cuda_full_ffn_replays;
    thread_storage[0].cuda_full_ffn_replays = 0;
    thread_storage[0].cuda_full_ffn_groups = 0;
}

export fn fg_cuda_take_matrix_launches(launches: u64, cached: u64) {
    var cached_out = fg_cuda_host_view_u64(cached, 1);
    var launches_out = fg_cuda_host_view_u64(launches, 1);
    var thread_storage = cuda_thread_state();
    launches_out[0] = thread_storage[0].cuda_matrix_launches;
    cached_out[0] = thread_storage[0].cuda_cached_projections;
    thread_storage[0].cuda_matrix_launches = 0;
    thread_storage[0].cuda_cached_projections = 0;
}

export fn fg_cuda_take_mmq() -> u64 {
    var thread_storage = cuda_thread_state();
    let count: u64 = thread_storage[0].cuda_mmq_projections;
    thread_storage[0].cuda_mmq_projections = 0;
    return count;
}

export fn fg_cuda_take_mmq_stream(windows: u64, fixups: u64) {
    var fixups_out = fg_cuda_host_view_u64(fixups, 1);
    var windows_out = fg_cuda_host_view_u64(windows, 1);
    var thread_storage = cuda_thread_state();
    windows_out[0] = thread_storage[0].cuda_mmq_stream_windows;
    fixups_out[0] = thread_storage[0].cuda_mmq_fixups;
    thread_storage[0].cuda_mmq_fixups = 0;
    thread_storage[0].cuda_mmq_stream_windows = 0;
}

export fn fg_cuda_take_dense_mma() -> u64 {
    var thread_storage = cuda_thread_state();
    let count: u64 = thread_storage[0].cuda_dense_mma;
    thread_storage[0].cuda_dense_mma = 0;
    return count;
}

export fn fg_cuda_finish(output: u64, output_bytes: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    var error: i32 = 0;
    if cuda.expert_upload_count != 0 {
        reset_expert_stage();
        error = -1;
    }
    if error == 0 && output != 0 {
        error = activation_publish(output, output_bytes, thread_storage[0].matrix_output, true);
    }
    if error == 0 && output != 0 && thread_storage[0].matrix_glu_active && (cuda_env("GOOSE_GRAPH_LOG") != 0)
        { fg_cuda_host_diagnostic_84(); }
    let pop_error: i32 = finish_context();
    if error == 0 { error = pop_error; }
    if error != 0 {
        fg_cuda_host_diagnostic_85((error as! i32));
        return -1;
    }
    if output != 0 {
        thread_storage[0].cuda_matrices += 1;
        if thread_storage[0].cuda_matrix_kind == 0 { thread_storage[0].cuda_f32_matrices += 1; }
    }
    return 0;
}

export fn fg_cuda_take_matrices() -> u64 {
    var thread_storage = cuda_thread_state();
    let matrices: u64 = thread_storage[0].cuda_matrices;
    thread_storage[0].cuda_matrices = 0;
    return matrices;
}

export fn fg_cuda_take_f32_matrices() -> u64 {
    var thread_storage = cuda_thread_state();
    let matrices: u64 = thread_storage[0].cuda_f32_matrices;
    thread_storage[0].cuda_f32_matrices = 0;
    return matrices;
}

fn flash_mma_shape(c: const i64[:], mask_bytes: i64) -> bool {
    return (cuda.flash_mma != 0) && mask_bytes == 0 && c[0] == 256 && c[1] == 256 &&
        (c[2] >= 8 || (cuda.flash_tail_mma && c[2] == 4)) &&
        c[4] > 0 && c[4] <= 65535 && c[5] == c[4] && c[3] == 8 * c[4] &&
        c[6] >= c[2] && c[6] <= INT_MAX - 31 && c[21] > 0 && c[21] <= c[6] - c[2] + 1 &&
        c[7] > 0 && c[7] % 8 == 0 && c[8] > 0 && c[8] % 8 == 0;
}

fn flash_vec_shape(c: const i64[:], mask_bytes: i64) -> bool {
    return cuda.flash_vec != 0 && mask_bytes == 0 && c[0] == 256 && c[1] == 256 && c[2] == 1 &&
        c[3] > 0 && c[3] <= 256 && c[4] > 0 && c[5] == c[4] && c[3] % c[4] == 0 &&
        c[6] > 0 && c[6] <= INT_MAX - 127 && c[21] >= 0 && c[21] <= c[6] && c[14] == 1024;
}

fn capture_flash_graph(kernels: const kernel_node_params[:], first: u64, count: u64) -> i32 {
    var error: i32 = capture_cuda_graph(kernels, first, count, cuda.flash_graph);
    if error != 0 { return error; }
    var nodes: u64 = 1;
    error = fg_cuda_driver_graph_roots_typed(unsafe_c_ptr<c_ptr<void>>(cuda.flash_graph), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.flash_graph_nodes[0])), unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(nodes)));
    if error != 0 || nodes != 1 { return if error != 0 { error } else { -1 }; }
    block {
        var i: u64 = 1;
        while i < count {
            nodes = 1;
            error = fg_cuda_driver_graph_dependents_typed(unsafe_c_ptr<c_ptr<void>>(cuda.flash_graph_nodes[i - 1]), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.flash_graph_nodes[i])), unsafe_c_ptr<c_ptr<u64>>(unsafe_c_address<c_ptr<void>>(nodes)));
            if error != 0 || nodes != 1 { return if error != 0 { error } else { -1 }; }
            i += 1;
        }
    };
    error = fg_cuda_driver_graph_instantiate_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(cuda.flash_graph_exec)), unsafe_c_ptr<c_ptr<void>>(cuda.flash_graph), 0);
    if error == 0 { cuda.flash_graph_node_count = count; }
    return error;
}

fn launch_flash_chain(kernels: const kernel_node_params[:], first: u64, count: u64) -> i32 {
    var thread_storage = cuda_thread_state();
    if cuda.flash_graph_enabled && !thread_storage[0].decode_capture.scope {
        var error: i32 = 0;
        if (cuda.flash_graph_exec != 0) && cuda.flash_graph_node_count != count { release_flash_graph(); }
        if (cuda.flash_graph_exec == 0) {
            error = capture_flash_graph(kernels, first, count);
        } else {
            block {
                var i: u64 = 0;
                while i < count && error == 0 {
                    error = fg_cuda_driver_graph_set_kernel_typed(unsafe_c_ptr<c_ptr<void>>(cuda.flash_graph_exec), unsafe_c_ptr<c_ptr<void>>(cuda.flash_graph_nodes[i]), unsafe_c_address<c_ptr<const void>>(kernels[first + i]));
                    i += 1;
                }
            };
        }
        if error == 0 { return fg_cuda_driver_graph_launch(cuda.flash_graph_exec, cuda.graph_stream); }
        if cuda_env("GOOSE_GRAPH_LOG") != 0 { fg_cuda_host_diagnostic_86((error as! i32)); }
        release_flash_graph();
        cuda.flash_graph_enabled = false;
    }
    return launch_cuda_kernel_range(kernels, first, count, 0);
}

fn launch_flash_vec(c: const i64[:], staged: const u64[:], output: u64, scale: f32, executed: bool&) -> i32 {
    var thread_storage = cuda_thread_state();
    const vector_bytes: i64 = 1024;
    const packed_query_bytes: i64 = 320;
    const min_prepack_parts: i64 = 64;
    var keys: i32 = ((if c[21] == 0 { c[6] } else { c[21] }) as! i32);
    var heads: i32 = (c[3] as! i32);
    var parts: i32 = (fg143_flash_vector_parts(keys) as! i32);
    var matrix: u64 = output;
    var meta: u64 = 0;
    var query: u64 = staged[0];
    var prepacked: bool = (cuda.flash_vec_pack != 0) && (parts as! i64) >= min_prepack_parts;
    if parts > 1 {
        let vectors: u64 = (heads as! u64) * (parts as! u64);
        let span: u64 = vectors * (vector_bytes as! u64);
        let scratch: u64 = span + vectors * 2 * 4;
        let extra: u64 = if prepacked { (heads as! u64) * (packed_query_bytes as! u64) } else { 0 };
        var error: i32 = reserve(&cuda.attention_scratch, scratch + extra);
        if error == 2 && prepacked {
            prepacked = false;
            error = reserve(&cuda.attention_scratch, scratch);
        }
        if error == 2 { return 0; }
        if error != 0 { return error; }
        matrix = cuda.attention_scratch.address;
        meta = matrix + span;
        if prepacked { query = matrix + scratch; }
    }
    let pack_in_graph: bool = prepacked && cuda.flash_graph_enabled && cuda.flash_graph_pack && !thread_storage[0].decode_capture.scope;
    var pack_args: u64[4] = [
        cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(query), cuda_kernel_argument_address(staged[4]),
        cuda_kernel_argument_address(scale),
    ];
    let pack: kernel_node_params = kernel_node_params { function: cuda.flash_vec_pack, grid_x: (heads as! u32), grid_y: 1, grid_z: 1, block_x: 32, block_y: 1, block_z: 1, params: fg_cuda_driver_words_address(pack_args) };
    if prepacked && !pack_in_graph {
        let error: i32 = launch_cuda_kernels([pack], 1, 0);
        if error != 0 { return error; }
    }
    var args: u64[8] = [
        cuda_kernel_argument_address(query), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(staged[2]),
        cuda_kernel_argument_address(matrix), cuda_kernel_argument_address(meta), cuda_kernel_argument_address(staged[4]),
        cuda_kernel_argument_address(keys), cuda_kernel_argument_address(scale),
    ];
    let paired: bool = prepacked && (cuda.flash_vec_paired != 0) && c[4] > 0 && heads % c[4] == 0 &&
        (heads / c[4]) % 2 == 0;
    if paired && !cuda_local_launch_flash_vec_paired_logged_6295 {
        cuda_local_launch_flash_vec_paired_logged_6295 = true;
        fg_cuda_host_diagnostic_87();
    }
    let quad: bool = paired && (cuda.flash_vec_quad != 0) && (heads / c[4]) % 4 == 0;
    if quad && !cuda_local_launch_flash_vec_quad_logged_6301 {
        cuda_local_launch_flash_vec_quad_logged_6301 = true;
        fg_cuda_host_diagnostic_88();
    }
    let key_bound: bool = quad && (cuda.flash_vec_quad_key_bound != 0) &&
        heads == 16 && c[4] == 2 && parts == 64 && keys >= 32768 && keys <= 33279;
    if key_bound && !cuda_local_launch_flash_vec_key_bound_logged_6308 {
        cuda_local_launch_flash_vec_key_bound_logged_6308 = true;
        fg_cuda_host_diagnostic_89();
    }
    let function: u64 = if key_bound { cuda.flash_vec_quad_key_bound } else { if quad { cuda.flash_vec_quad } else { if paired { cuda.flash_vec_paired } else { if prepacked { cuda.flash_vec_packed } else { cuda.flash_vec } } } };
    let vector: kernel_node_params = kernel_node_params { function: function, grid_x: ((heads / (if quad { 4 } else { if paired { 2 } else { 1 } })) as! u32), grid_y: (parts as! u32), grid_z: 1, block_x: 128, block_y: 1, block_z: 1, params: fg_cuda_driver_words_address(args) };
    var merge_args: u64[5] = [
        cuda_kernel_argument_address(matrix), cuda_kernel_argument_address(meta), cuda_kernel_argument_address(output),
        cuda_kernel_argument_address(parts), cuda_kernel_argument_address(heads),
    ];
    let merge: kernel_node_params = kernel_node_params { function: cuda.flash_vec_merge, grid_x: (heads as! u32), grid_y: 1, grid_z: 1, block_x: 256, block_y: 1, block_z: 1, params: fg_cuda_driver_words_address(merge_args) };
    let kernels: kernel_node_params[3] = [pack, vector, merge];
    let error: i32 = if parts > 1 {
        launch_flash_chain(kernels, if pack_in_graph { 0 } else { 1 }, if pack_in_graph { 3 } else { 2 })
    } else {
        launch_cuda_kernels([vector], 1, 0)
    };
    if error == 0 { executed = true; }
    else { cuda_capture_sync(); }
    return error;
}

fn launch_flash_mma(c: const i64[:], staged: const u64[:], output: u64, scale: f32, executed: bool&) -> i32 {
    var keys: i64 = c[6];
    var padded: i64 = (keys + 31) & ~31;
    let span: u64 = (padded as! u64) * (c[4] as! u64) * 512;
    if span > SIZE_MAX / 2 { return -1; }
    var error: i32 = reserve(&cuda.attention_scratch, ((2 * span) as! u64));
    if error == 2 { return 0; } // The scalar path can use smaller scratch waves.
    if error != 0 { return error; }
    let heads: u32 = (c[4] as! u32);
    let columns: u32 = if c[2] == 4 { 4 } else { 8 };
    let tiles_per_head: u32 = (((c[2] + (columns as! i64) - 1) / (columns as! i64)) as! u32);
    let tiles: u64 = (tiles_per_head as! u64) * (heads as! u64);
    let stream: bool = if c[2] == 4 { cuda.flash_tail_mma } else { cuda.flash_stream_all };
    var workers: u32 = if stream { (fg143_flash_stream_workers(c[2], c[4], keys, cuda.multiprocessors) as! u32) } else { 0 };
    var fix: u64 = 0;
    if workers != 0 && tiles % (workers as! u64) != 0 {
        let fix_bytes: u64 = (workers as! u64) * (columns as! u64) * 8 * (256 + 4) * 4;
        // Flash and MMQ consume this scratch on the same legacy stream.
        // Reuse the already allocated MMQ pack when it fits.
        error = reserve(&cuda.mmq_packed, fix_bytes);
        if error == 2 { workers = 0; error = 0; }
        else if error != 0 { return error; }
        else { fix = cuda.mmq_packed.address; }
    }
    var keys_half: u64 = cuda.attention_scratch.address;
    var values_half: u64 = keys_half + span;
    var k_row: i64 = c[9];
    var k_head: i64 = c[10];
    var v_row: i64 = c[11];
    var v_head: i64 = c[12];
    var k_args: u64[6] = [
        cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(keys_half), cuda_kernel_argument_address(keys),
        cuda_kernel_argument_address(padded), cuda_kernel_argument_address(k_row), cuda_kernel_argument_address(k_head),
    ];
    var v_args: u64[6] = [
        cuda_kernel_argument_address(staged[2]), cuda_kernel_argument_address(values_half), cuda_kernel_argument_address(keys),
        cuda_kernel_argument_address(padded), cuda_kernel_argument_address(v_row), cuda_kernel_argument_address(v_head),
    ];
    error = cuda_capture_launch(cuda.flash_stage, (padded as! u32), heads, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(k_args), 0);
    if error == 0 { error = cuda_capture_launch(cuda.flash_stage, (padded as! u32), heads, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(v_args), 0); }
    if error == 0 {
        const FLASH_MMA_SHARED_BYTES: u32 = 33792;
        if workers != 0 {
            var args: u64[7] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(keys_half), cuda_kernel_argument_address(values_half),
                cuda_kernel_argument_address(output), cuda_kernel_argument_address(fix), cuda_kernel_argument_address(staged[4]),
                cuda_kernel_argument_address(scale),
            ];
            error = cuda_capture_launch(cuda.flash_stream, workers, 1, 1, 128, 1, 1, FLASH_MMA_SHARED_BYTES, 0, fg_cuda_driver_words_address(args), 0);
        } else {
            var args: u64[6] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(keys_half), cuda_kernel_argument_address(values_half),
                cuda_kernel_argument_address(output), cuda_kernel_argument_address(staged[4]), cuda_kernel_argument_address(scale),
            ];
            error = cuda_capture_launch(cuda.flash_mma, tiles_per_head, heads, 1, 128, 1, 1, FLASH_MMA_SHARED_BYTES, 0, fg_cuda_driver_words_address(args), 0);
        }
    }
    if error == 0 && fix != 0 {
        var queries: i32 = (c[2] as! i32);
        var query_heads: i32 = (c[3] as! i32);
        if (workers as! u64) % tiles == 0 {
            var blocks_per_tile: i32 = (((workers as! u64) / tiles) as! i32);
            var args: u64[6] = [
                cuda_kernel_argument_address(output), cuda_kernel_argument_address(fix), cuda_kernel_argument_address(queries),
                cuda_kernel_argument_address(query_heads), cuda_kernel_argument_address(workers), cuda_kernel_argument_address(blocks_per_tile),
            ];
            error = cuda_capture_launch(cuda.flash_fixup_uniform, (tiles as! u32), columns, 8, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        } else {
            var key_count: i32 = (keys as! i32);
            var args: u64[5] = [
                cuda_kernel_argument_address(output), cuda_kernel_argument_address(fix), cuda_kernel_argument_address(queries),
                cuda_kernel_argument_address(query_heads), cuda_kernel_argument_address(key_count),
            ];
            error = cuda_capture_launch(cuda.flash_fixup_general, workers, columns, 8, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        }
    }
    if error == 0 { executed = true; }
    else { cuda_capture_sync(); }
    return error;
}

export fn fg_cuda_q4_attention(config: i64[:], q: u8[:], k: u8[:], v: u8[:], mask: u8[:], out: u8[:], scale: f32) -> i64 {
    var thread_storage = cuda_thread_state();
    if config.len != 22 || config[17] <= 0 || config[17] > 65535 ||
        config[18] <= 0 || config[18] > INT_MAX ||
        config[19] <= 0 || config[19] > config[18] || config[20] <= 0 ||
        config[20] % config[19] != 0 || (config[20] as! u64) > SIZE_MAX ||
        q.len <= 0 || k.len <= 0 || v.len <= 0 || mask.len < 0 || out.len <= 0 { return -1; }
    let active: i32 = start_context();
    if active != 1 { return active; }

    var key: u64 = 0;
    var value: u64 = 0;
    var error: i32 = map_state_address(((cuda_slice_address(k)) as! u64), k.len, 0, &key);
    if error == 0 { error = map_state_address(((cuda_slice_address(v)) as! u64), v.len, 0, &value); }
    let resident: bool = key != 0 && value != 0;
    let empty = fg_cuda_host_view_u8(0, 0);
    let data: CudaInputSlice[5] = [
        q, if key != 0 { empty } else { k }, if value != 0 { empty } else { v },
        mask, fg_cuda_host_view_u8(cuda_slice_address(config), config.len * 8),
    ];
    let buffers: CudaStagingPointer[5] = [
        unsafe_c_address<c_ptr<device_buffer>>(cuda.input),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.weight),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_value),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.jobs),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_config),
    ];
    var staged: u64[5] = [0, key, value, 0, 0];
    var output: u64 = 0;
    let host: u64 = ((cuda_slice_address(out)) as! u64);
    if error == 0 { error = activation_output(host, out.len, &output); }
    if error == 0 { error = upload_inputs(data, buffers, staged, data.len); }

    var mma: bool = false;
    if error == 0 && flash_mma_shape(config, mask.len) {
        error = launch_flash_mma(config, staged, output, scale, &mma);
    }
    var vector: bool = false;
    if error == 0 && flash_vec_shape(config, mask.len) {
        error = launch_flash_vec(config, staged, output, scale, &vector);
    }
    let tiled: bool = mma || vector;

    let tasks: i32 = (config[18] as! i32);
    let requested: i32 = (config[19] as! i32);
    let task_bytes: u64 = ((config[20] / (requested as! i64)) as! u64);
    var capacity: i32 = requested;
    if error == 0 && !tiled {
        loop {
            error = reserve(&cuda.attention_scratch, task_bytes * (capacity as! u64));
            if error != 2 || capacity <= 1 { break; }
            capacity /= 2;
        }
        if error == 0 && capacity != requested && (cuda_env("GOOSE_GRAPH_LOG") != 0) {
            fg_cuda_host_diagnostic_90((capacity as! i32), (requested as! i32));
        }
    }
    let tiles: u32 = (config[17] as! u32);
    var start: i32 = 0;
    var tile_args: u64[8] = [
        cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(staged[2]),
        cuda_kernel_argument_address(staged[3]), cuda_kernel_argument_address(cuda.attention_scratch.address), cuda_kernel_argument_address(staged[4]),
        cuda_kernel_argument_address(scale), cuda_kernel_argument_address(start),
    ];
    var merge_args: u64[4] = [
        cuda_kernel_argument_address(cuda.attention_scratch.address), cuda_kernel_argument_address(output), cuda_kernel_argument_address(staged[4]),
        cuda_kernel_argument_address(start),
    ];
    while error == 0 && !tiled && start < tasks {
        let count: i32 = if tasks - start < capacity { tasks - start } else { capacity };
        error = cuda_capture_launch(cuda.attention_tiles, (count as! u32), tiles, 1, 32, 1, 1, 0, 0, fg_cuda_driver_words_address(tile_args), 0);
        if error == 0 { error = cuda_capture_launch(cuda.attention_merge, (count as! u32), 1, 1, 32, 1, 1, 0, 0, fg_cuda_driver_words_address(merge_args), 0); }
        start += count;
    }
    if error == 0 { error = activation_publish(host, out.len, output, false); }
    if error != 0 && start > 0 { cuda_capture_sync(); }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_91((if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    thread_storage[0].cuda_attentions += 1;
    if mma { thread_storage[0].cuda_flash_mmas += 1; }
    if config[21] > 0 { thread_storage[0].cuda_causal_attentions += 1; }
    if resident { thread_storage[0].cuda_resident_kv += 1; }
    return 1;
}

export fn fg_cuda_take_attentions() -> u64 {
    var thread_storage = cuda_thread_state();
    let attentions: u64 = thread_storage[0].cuda_attentions;
    thread_storage[0].cuda_attentions = 0;
    return attentions;
}

export fn fg_cuda_take_flash_mmas() -> u64 {
    var thread_storage = cuda_thread_state();
    let count: u64 = thread_storage[0].cuda_flash_mmas;
    thread_storage[0].cuda_flash_mmas = 0;
    return count;
}

export fn fg_cuda_take_causal_attentions() -> u64 {
    var thread_storage = cuda_thread_state();
    let count: u64 = thread_storage[0].cuda_causal_attentions;
    thread_storage[0].cuda_causal_attentions = 0;
    return count;
}

export fn fg_cuda_gdn(plan: i64[:], q: f32[:], k: f32[:], v: f32[:], gate: f32[:], beta: f32[:], initial: f32[:], out: f32[:]) -> i64 {
    var thread_storage = cuda_thread_state();
    if plan.len != 50 || plan[25] <= 0 || plan[25] > INT_MAX ||
        plan[27] <= 0 || plan[27] > 65535 ||
        plan[34] <= 0 || plan[34] > 65535 ||
        (plan[35] != 16 && plan[35] != 32) || plan[36] != 4 ||
        plan[48] < 0 || plan[48] >= out.len ||
        out.len <= 0 || out.len > INT64_MAX / (4 as! i64) { return -1; }
    let tensors = [q, k, v, gate, beta, initial];
    let empty = fg_cuda_host_view_u8(0, 0);
    var data: CudaInputSlice[6] = [empty, empty, empty, empty, empty, empty];
    block {
        var i: i64 = 0;
        while i < tensors.len {
            if cuda_slice_address(tensors[i]) == 0 || tensors[i].len <= 0 ||
                tensors[i].len > INT64_MAX / (4 as! i64) { return -1; }
            data[i] = fg_cuda_host_view_u8((cuda_slice_address(tensors[i])), (tensors[i].len * (4 as! i64) as! i64));
            i += 1;
        }
    };
    let active: i32 = start_context();
    if active != 1 { return active; }

    let initial_host: u64 = ((cuda_slice_address(initial)) as! u64);
    let heads: i64 = plan[25];
    let sequences: i64 = plan[27];
    let mma: bool = (cuda.gdn_mma != 0) && plan[24] == 128 && plan[44] == 16 &&
        heads >= 16 && heads <= 64 && heads % 16 == 0 && sequences <= INT_MAX / heads &&
        plan[26] >= 64 && plan[26] <= INT64_MAX - 32 && plan[30] == 1 && plan[40] == 0;
    let state_bytes: i64 = data[5].len;
    let score_bytes: u64 = (plan[48] as! u64) * 4;
    let initial_device: u64 = state_address(initial_host, state_bytes, 1);
    let publish: bool = initial_device != 0 && plan[49] == 1 && plan[30] == 1 &&
        out.len - plan[48] == initial.len;
    if initial_device != 0 { data[5] = fg_cuda_host_view_u8((0 as! u64), (0 as! i64)); }
    var buffers = [unsafe_c_address<c_ptr<device_buffer>>(cuda.input), unsafe_c_address<c_ptr<device_buffer>>(cuda.weight), unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_value),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.jobs), unsafe_c_address<c_ptr<device_buffer>>(cuda.gdn_beta), unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_scratch)];
    var staged: u64[6] = [0, 0, 0, 0, 0, initial_device];
    var output: u64 = 0;
    let host: u64 = ((cuda_slice_address(out)) as! u64);
    let output_bytes: u64 = if publish { score_bytes } else { (out.len as! u64) * 4 };
    var error: i32 = if publish && thread_storage[0].published_state.bytes > 0 { -1 } else { activation_output(host, (output_bytes as! i64), &output) };
    if error == 0 { error = upload_inputs(data, buffers, staged, data.len); }

    var packet: u64[57];
    if error == 0 {
        for i in staged.len { packet[i] = staged[i]; }
        packet[6] = output;
        packet[7] = if publish { initial_device } else { output + score_bytes };
        for i in packet.len - 8 { packet[8 + i] = plan[i] as! u64; }
    }
    var args: u64[1] = [
        cuda_slice_address(packet),
    ];
    if error == 0 {
        if mma {
            let sliced: bool = (cuda.gdn_mma_v32 != 0) && sequences <= INT_MAX / (4 * heads);
            error = cuda_capture_launch(if sliced { cuda.gdn_mma_v32 } else { cuda.gdn_mma },
                ((heads * sequences * (if sliced { 4 } else { 1 })) as! u32), 1, 1, 256, 1, 1,
                (if sliced { GDN_V32_SHARED_BYTES } else { GDN_MMA_SHARED_BYTES }) as! u32,
                0, fg_cuda_driver_words_address(args), 0);
            if error == 0 && sliced && (cuda_env("GOOSE_GRAPH_LOG") != 0)
                { fg_cuda_host_diagnostic_92(); }
        } else {
            let scalar: u64 = if plan[38] != 0 && (cuda.gdn_decode != 0) { cuda.gdn_decode } else { cuda.gdn };
            error = cuda_capture_launch(scalar, (heads as! u32), (sequences as! u32),
                (plan[34] as! u32), ((plan[35] * plan[36]) as! u32), 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        }
        if error == 0 {
            error = activation_publish(host, (output_bytes as! i64), output, false);
            if error != 0 { cuda_capture_sync(); }
        }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_93((if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    thread_storage[0].cuda_gdns += 1;
    if mma { thread_storage[0].cuda_gdn_mmas += 1; }
    if publish {
        thread_storage[0].published_state.source = ((cuda_slice_address(out)) as! u64) + score_bytes;
        thread_storage[0].published_state.destination = initial_host;
        thread_storage[0].published_state.bytes = state_bytes;
        thread_storage[0].cuda_resident_gdn += 1;
    }
    return 1;
}

export fn fg_cuda_take_gdns() -> u64 {
    var thread_storage = cuda_thread_state();
    let gdns: u64 = thread_storage[0].cuda_gdns;
    thread_storage[0].cuda_gdns = 0;
    return gdns;
}

export fn fg_cuda_take_gdn_mmas() -> u64 {
    var thread_storage = cuda_thread_state();
    let count: u64 = thread_storage[0].cuda_gdn_mmas;
    thread_storage[0].cuda_gdn_mmas = 0;
    return count;
}

export fn fg_cuda_take_resident_state(kv: u64, gdn: u64) {
    var gdn_out = fg_cuda_host_view_u64(gdn, 1);
    var kv_out = fg_cuda_host_view_u64(kv, 1);
    var thread_storage = cuda_thread_state();
    kv_out[0] = thread_storage[0].cuda_resident_kv;
    gdn_out[0] = thread_storage[0].cuda_resident_gdn;
    thread_storage[0].cuda_resident_kv = 0;
    thread_storage[0].cuda_resident_gdn = 0;
}

export fn fg_cuda_take_q4_writes() -> u64 {
    var thread_storage = cuda_thread_state();
    let writes: u64 = thread_storage[0].cuda_q4_writes;
    thread_storage[0].cuda_q4_writes = 0;
    return writes;
}

fn vector_input(data: u8[:], staging: device_buffer&, device: u64&, shape: i64[:], pack: bool) -> i32 {
    let host = cuda_slice_address(data);
    if !pack || activation_device(host, data.len) != 0 { return activation_input(data, staging, device); }
    var error = activation_sync_range(host, data.len, false);
    if error != 0 { return error; }
    let row_bytes = (shape[0] as! u64) * 4;
    let compact_bytes = row_bytes * (shape[1] as! u64) * (shape[2] as! u64) * (shape[3] as! u64);
    if compact_bytes > (data.len as! u64) { return -1; }
    let compact = fg_cuda_host_malloc(compact_bytes);
    if compact == 0 { return -1; }
    var dest = compact;
    for plane in shape[3] {
        for column in shape[2] {
            for row in shape[1] {
                let offset = plane * shape[7] + column * shape[6] + row * shape[5];
                fg_cuda_host_memcpy(dest, host + (offset as! u64), row_bytes);
                dest += row_bytes;
            }
        }
    }
    error = reserve(staging, compact_bytes);
    if error == 0 {
        error = cuda_capture_upload(staging.address, unsafe_c_ptr<c_ptr<const void>>(compact), compact_bytes);
    }
    fg_cuda_host_free(compact);
    if error != 0 { return error; }
    var stride: u64 = 4;
    for i in 4 {
        shape[4 + i] = stride as! i64;
        stride *= shape[i] as! u64;
    }
    device = staging.address;
    return 0;
}

fn vector_aligned(base: u64, stride: const i64[:]) -> bool {
    return (base | (stride[1] as! u64) | (stride[2] as! u64) | (stride[3] as! u64)) % 16 == 0;
}

struct CudaState_giga_vector {
    module: u64,
    vector: u64, flat: u64, flat4: u64,
    available: bool,
}
var giga_vector: CudaState_giga_vector;

fn load_giga_vector() -> bool {
    if giga_vector.module != 0 { return giga_vector.available; }
    if fg_cuda_driver_module_load_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(giga_vector.module)), unsafe_c_ptr<c_ptr<const void>>(fg_cuda_image_giga_vector_image())) != 0 { return false; }
    giga_vector.available = cuda_lookup_kernel(cuda_kernel_argument_address(giga_vector.vector), giga_vector.module, "goose_vector_ops") == 0;
    cuda_lookup_kernel(cuda_kernel_argument_address(giga_vector.flat), giga_vector.module, "goose_vector_flat");
    cuda_lookup_kernel(cuda_kernel_argument_address(giga_vector.flat4), giga_vector.module, "goose_vector_flat4");
    return giga_vector.available;
}

export fn fg_cuda_vector(config: i64[:], x: u8[:], y: u8[:], out: u8[:], param0: f32, param1: f32) -> i64 {
    var thread_storage = cuda_thread_state();
    if config.len != 26 || config[24] <= 0 || config[24] > INT_MAX ||
        (config[25] != 32 && config[25] != 128) ||
        x.len <= 0 || y.len < 0 || out.len <= 0 { return -1; }
    if cuda_slice_address(x) % 4 != 0 || cuda_slice_address(out) % 4 != 0 ||
        (y.len > 0 && cuda_slice_address(y) % 4 != 0) { return 0; }
    let active: i32 = start_context();
    if active != 1 { return active; }
    let giga_clamped: bool = config[0] == 16 || config[0] == 17;
    var vector: u64 = cuda.vector;
    var flat: u64 = cuda.vector_flat;
    var flat4: u64 = cuda.vector_flat4;
    if giga_clamped {
        if !thread_storage[0].giga_small_expert_wave || !load_giga_vector() { return if finish_context() == 0 { 0 } else { -1 }; }
        vector = giga_vector.vector;
        flat = giga_vector.flat;
        flat4 = giga_vector.flat4;
    }
    var plan: i64[26];
    for i in plan.len { plan[i] = config[i]; }
    let plan_address = cuda_slice_address(plan);
    let config_address = cuda_slice_address(config);
    const sigmoid_operation: i64 = 7;
    const silu_operation: i64 = 8;
    const swiglu_operation: i64 = 10;
    const sigmoid_reference_operation: i64 = 13;
    const silu_reference_operation: i64 = 14;
    const swiglu_reference_operation: i64 = 15;
    if cuda.activation_reference {
        if plan[0] == sigmoid_operation { plan[0] = sigmoid_reference_operation; }
        if plan[0] == silu_operation { plan[0] = silu_reference_operation; }
        if plan[0] == swiglu_operation { plan[0] = swiglu_reference_operation; }
    }
    var staged: u64[2] = [0, 0];
    var output: u64 = 0;
    let host: u64 = ((cuda_slice_address(out)) as! u64);
    var error: i32 = 0;
    if (plan[23] & 4) != 0 {
        error = activation_reserve_output((out.len as! u64));
        output = cuda.output.address;
    } else {
        error = activation_output(host, out.len, &output);
    }
    if error == 0 { error = vector_input(x, &cuda.input, &staged[0], fg_cuda_host_view_i64(plan_address + 48, 8), (plan[23] & 1) != 0); }
    if error == 0 && y.len > 0 {
        let pack: bool = (plan[23] & 2) != 0;
        let resident: bool = (cuda.decode_graph_enabled || thread_storage[0].cuda_inkling_quants) && !pack &&
            dense_weight(((cuda_slice_address(y)) as! u64), y.len, &staged[1]);
        if !resident { error = vector_input(y, &cuda.weight, &staged[1], fg_cuda_host_view_i64(plan_address + 112, 8), pack); }
    }
    if error == 0 {
        const rms_operation: i64 = 11;
        const flat_plan: i64 = 8;
        let width: i64 = plan[2];
        let reference: bool = (cuda.rms_reference != 0) && plan[0] == rms_operation && width <= INT_MAX &&
            plan[10] == 4 && (y.len == 0 || plan[18] == 4);
        if flat != 0 && (plan[23] & flat_plan) != 0 && plan[1] >= 16384 && plan[25] == 128 {
            let use4: bool = flat4 != 0 && plan[1] % 4 == 0 &&
                (staged[0] | staged[1] | output) % 16 == 0;
            var work: i64 = if use4 { plan[1] / 4 } else { plan[1] };
            let groups: u32 = if use4 { ((1 + (work - 1) / 128) as! u32) } else { (plan[24] as! u32) };
            var args: u64[7] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(output),
                cuda_kernel_argument_address(plan[0]), cuda_kernel_argument_address(work), cuda_kernel_argument_address(param0),
                cuda_kernel_argument_address(param1),
            ];
            error = cuda_capture_launch(if use4 { flat4 } else { flat }, groups, 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            if error == 0 {
                thread_storage[0].activations.flat_vectors += 1;
                if use4 { thread_storage[0].activations.flat4_vectors += 1; }
            }
        } else if reference {
            let vec: bool = width % 4 == 0 && width <= 16384 && output % 16 == 0 &&
                vector_aligned(((cuda_slice_address(x)) as! u64), fg_cuda_host_view_i64(config_address + 80, 4)) &&
                vector_aligned(staged[0], fg_cuda_host_view_i64(plan_address + 80, 4)) && (y.len == 0 || (plan[14] == width &&
                    vector_aligned(((cuda_slice_address(y)) as! u64), fg_cuda_host_view_i64(config_address + 144, 4)) && vector_aligned(staged[1], fg_cuda_host_view_i64(plan_address + 144, 4))));
            let vectors: i32 = if vec { (if width <= 4096 { 1 } else { 4 }) } else { 0 };
            let lanes: u32 = if vec { (if width <= 128 { 32 } else { if width <= 512 { 128 } else { if width <= 1024 { 256 } else { if width <= 2048 { 512 } else { 1024 } } } }) } else { (if width < 1024 { 256 } else { 1024 }) };
            if vec && width == 2048 && plan[1] == width && plan[24] == 1 && (plan[23] & 16) == 0 &&
                (cuda.rms_decode_2048 != 0) && fg143_cpu_prefill() == 0 {
                if !cuda_local_fg_cuda_vector_rms_decode_logged_6757 {
                    cuda_local_fg_cuda_vector_rms_decode_logged_6757 = true;
                    fg_cuda_host_diagnostic_94();
                }
                var args: u64[4] = [
                    cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(output),
                    cuda_kernel_argument_address(param0),
                ];
                error = cuda_capture_launch(cuda.rms_decode_2048, 1, 1, 1, 512, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            } else if (cuda.rms_reference_one != 0) && vectors == 1 && fg143_cpu_prefill() != 0 {
                var args: u64[6] = [
                    cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(output), cuda_slice_address(plan),
                    cuda_kernel_argument_address(param0), cuda_kernel_argument_address(param1),
                ];
                let warp4: bool = width == 128 && (cuda.rms_reference_one_warp4 != 0) && plan[24] >= 128;
                let blocks: u32 = if warp4 { 1 + ((plan[24] as! u32) - 1) / 4 } else { (plan[24] as! u32) };
                let kernel: u64 = if warp4 { cuda.rms_reference_one_warp4 } else { cuda.rms_reference_one };
                error = cuda_capture_launch(kernel, blocks, 1, 1, if warp4 { 128 } else { lanes }, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            } else {
                var args: u64[7] = [
                    cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(output), cuda_slice_address(plan),
                    cuda_kernel_argument_address(param0), cuda_kernel_argument_address(vectors), cuda_kernel_argument_address(param1),
                ];
                error = cuda_capture_launch(cuda.rms_reference, (plan[24] as! u32), 1, 1, lanes, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            }
        } else {
            var args: u64[6] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(output), cuda_slice_address(plan),
                cuda_kernel_argument_address(param0), cuda_kernel_argument_address(param1),
            ];
            error = cuda_capture_launch(vector, (config[24] as! u32), 1, 1,
                (config[25] as! u32), 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        }
        if error == 0 { error = activation_publish(host, out.len, output, false); }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_95((if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    if giga_clamped && (cuda_env("GOOSE_GRAPH_LOG") != 0)
        { fg_cuda_host_diagnostic_96((plan[1] as! i64)); }
    thread_storage[0].cuda_vectors += 1;
    return 1;
}

export fn fg_cuda_rms_scale(plan: i64[:], input: u64, input_bytes: i64, output: u64, bytes: i64, step: i64, deadline: i64, eps: f32, scale: f32) -> i64 {
    var thread_storage = cuda_thread_state();
    if !cuda.rms_postscale || !thread_storage[0].activations.active { return 0; }
    if fg_cuda_activation_prepare(output, bytes, true, false, step, deadline) != 0 { return -1; }
    let status: i64 = fg_cuda_vector(plan, fg_cuda_host_view_u8(((input as! u64) as! u64), (input_bytes as! i64)),
        fg_cuda_host_view_u8((0 as! u64), (0 as! i64)), fg_cuda_host_view_u8(((output as! u64) as! u64), (bytes as! i64)), eps, scale);
    if status > 0 && (cuda_env("GOOSE_GRAPH_LOG") != 0) { fg_cuda_host_diagnostic_97(); }
    return status;
}

export fn fg_cuda_take_vectors() -> u64 {
    var thread_storage = cuda_thread_state();
    let vectors: u64 = thread_storage[0].cuda_vectors;
    thread_storage[0].cuda_vectors = 0;
    return vectors;
}

export fn fg_cuda_direct_ssm_enabled() -> bool {
    var thread_storage = cuda_thread_state();
    return (cuda.ssm_conv_direct != 0) && (cuda.ssm_conv_save_tail != 0) && thread_storage[0].activations.active && thread_storage[0].activations.defer_waits;
}

export fn fg_cuda_direct_ssm(plan: i64[:], step: i64, deadline: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    if plan.len != 7 || !fg_cuda_direct_ssm_enabled() { return 0; }
    let history: u64 = (plan[0] as! u64);
    let input: u64 = (plan[1] as! u64);
    let weights: u64 = (plan[2] as! u64);
    let output: u64 = (plan[3] as! u64);
    var channels: u64 = (plan[4] as! u64);
    var tokens: u64 = (plan[5] as! u64);
    var silu: i32 = (plan[6] as! i32);
    let bytes: i64 = ((4 * channels * tokens) as! i64);
    var state: u64 = state_address(history, ((12 * channels) as! i64), 2);
    if state == 0 { return 0; }
    // The graph can reuse the projection's host slot for the convolution.
    // Keep the old device record until the convolution and tail finish reading.
    let reused: bool = input == output;
    if reused { activation_prepare_target(output, bytes, true, false, step, deadline); }
    else if fg_cuda_activation_prepare(output, bytes, true, false, step, deadline) != 0 { return -1; }
    let active: i32 = start_context();
    if active != 1 { return active; }
    var result: u64 = 0;
    var mixed: u64 = 0;
    var filter: u64 = 0;
    let projection: const u8[:] = fg_cuda_host_view_u8(input, bytes);
    var error: i32 = activation_output_for_input(output, bytes, &result, projection, fg_cuda_host_view_u64(cuda_kernel_argument_address(mixed), 1));
    if error == 0 && mixed == 0 { error = activation_input(projection, &cuda.input, &mixed); }
    let filter_bytes: i64 = ((16 * channels) as! i64);
    if error == 0 && !dense_weight(weights, filter_bytes, &filter)
        { error = activation_input(fg_cuda_host_view_u8(weights, filter_bytes), &cuda.weight, &filter); }
    if error == 0 {
        let groups: u32 = (((channels + 127) / 128) as! u32);
        var conv: u64[7] = [
            cuda_kernel_argument_address(state), cuda_kernel_argument_address(mixed), cuda_kernel_argument_address(filter),
            cuda_kernel_argument_address(result), cuda_kernel_argument_address(channels), cuda_kernel_argument_address(tokens),
            cuda_kernel_argument_address(silu),
        ];
        var tail: u64[4] = [
            cuda_kernel_argument_address(state), cuda_kernel_argument_address(mixed), cuda_kernel_argument_address(channels),
            cuda_kernel_argument_address(tokens),
        ];
        error = cuda_capture_launch(cuda.ssm_conv_direct, groups, (((tokens + 31) / 32) as! u32), 1,
            128, 1, 1, 0, 0, fg_cuda_driver_words_address(conv), 0);
        if error == 0 { error = cuda_capture_launch(cuda.ssm_conv_save_tail, groups, 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(tail), 0); }
        if error == 0 && reused {
            fg143_activation_discard_overwritten(activation_ranges(), output, bytes);
            fg143_activation_discard_overwritten(activation_large_range(), output, bytes);
        }
        if error == 0 { error = activation_publish(output, bytes, result, false); }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_98((if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    thread_storage[0].cuda_convs += 1;
    if cuda_env("GOOSE_GRAPH_LOG") != 0 { fg_cuda_host_diagnostic_99(if reused { 1 } else { 0 }); }
    return 1;
}

export fn fg_cuda_position(config: i64[:], x: u8[:], y: u8[:], z: u8[:], out: u8[:]) -> i64 {
    var thread_storage = cuda_thread_state();
    const rope_operation: i64 = 0;
    const conv_operation: i64 = 1;
    const rope_decode_operation: i64 = 2;
    if config.len != 27 || config[0] < rope_operation || config[0] > rope_decode_operation ||
        config[1] <= 0 || config[1] > INT_MAX ||
        config[2] <= 0 || config[2] > 65535 ||
        x.len <= 0 || y.len <= 0 || z.len < 0 || out.len <= 0 { return -1; }
    let rope: bool = config[0] != conv_operation;
    if rope && (config[3] <= 0 || config[3] > INT_MAX || config[4] <= 0) { return -1; }
    let active: i32 = start_context();
    if active != 1 { return active; }
    var staged: u64[4] = [0, 0, 0, 0];
    var output: u64 = 0;
    let resident: bool = cuda.decode_graph_enabled && !rope &&
        dense_weight(((cuda_slice_address(y)) as! u64), y.len, &staged[1]);
    let data: CudaInputSlice[4] = [x, if resident { fg_cuda_host_view_u8((0 as! u64), (0 as! i64)) } else { y }, z,
        fg_cuda_host_view_u8(cuda_slice_address(config), config.len * 8)];
    let buffers: CudaStagingPointer[4] = [unsafe_c_address<c_ptr<device_buffer>>(cuda.input), unsafe_c_address<c_ptr<device_buffer>>(cuda.weight), unsafe_c_address<c_ptr<device_buffer>>(cuda.jobs), unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_config)];
    let host: u64 = ((cuda_slice_address(out)) as! u64);
    var error: i32 = activation_output(host, out.len, &output);
    let rope_decode: bool = config[0] == rope_decode_operation && cuda.rope_decode != 0;
    if error == 0 && rope && !rope_decode { error = reserve(&cuda.attention_scratch, (config[4] as! u64)); }
    if error == 0 { error = upload_inputs(data, buffers, staged, data.len); }
    let gx: u32 = (config[1] as! u32);
    let gy: u32 = (config[2] as! u32);
    if error == 0 {
        if rope_decode {
            let lanes: u32 = ((config[5] as! u32) + 63) / 64 * 32;
            var args: u64[5] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(staged[2]),
                cuda_kernel_argument_address(output), cuda_kernel_argument_address(staged[3]),
            ];
            error = cuda_capture_launch(cuda.rope_decode, (config[6] as! u32), 1, 1, lanes, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        } else if rope {
            var phase_args: u64[4] = [
                cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(staged[2]), cuda_kernel_argument_address(cuda.attention_scratch.address),
                cuda_kernel_argument_address(staged[3]),
            ];
            var apply_args: u64[4] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(output), cuda_kernel_argument_address(cuda.attention_scratch.address),
                cuda_kernel_argument_address(staged[3]),
            ];
            error = cuda_capture_launch(cuda.rope_phases, (config[3] as! u32), 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(phase_args), 0);
            if error == 0 { error = cuda_capture_launch(cuda.rope_apply, gx, 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(apply_args), 0); }
        } else {
            var args: u64[4] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(output),
                cuda_kernel_argument_address(staged[3]),
            ];
            const taps: i64 = 4;
            const tile: i64 = 32;
            const channels: i64 = 128;
            const grid_z_limit: i64 = 65535;
            let tokens: i64 = config[7];
            let tiled: bool = (cuda.ssm_conv_long != 0) && config[6] == taps && config[5] % channels == 0 &&
                tokens > tile && tokens <= (tile as! i64) * grid_z_limit;
            let gz: u32 = if tiled { 1 + ((tokens - 1) as! u32) / (tile as! u32) } else { 1 };
            let decode: bool = (cuda.ssm_conv_decode != 0) && config[6] == taps &&
                tokens == 1 && config[8] == 4 && config[12] == 4;
            var conv: u64 = if decode { cuda.ssm_conv_decode } else { if tiled { cuda.ssm_conv_long } else { cuda.ssm_conv } };
            error = cuda_capture_launch(conv, gx, gy, gz, channels as! u32, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        }
        if error == 0 { error = activation_publish(host, out.len, output, false); }
        if error != 0 { cuda_capture_sync(); }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        let status = if error != 0 { error } else { popped };
        if rope { fg_cuda_host_diagnostic_112(status); } else { fg_cuda_host_diagnostic_113(status); }
        return -1;
    }
    if rope { thread_storage[0].cuda_ropes += 1; }
    else { thread_storage[0].cuda_convs += 1; }
    return 1;
}

export fn fg_cuda_take_position(ropes: u64, convs: u64) {
    var convs_out = fg_cuda_host_view_u64(convs, 1);
    var ropes_out = fg_cuda_host_view_u64(ropes, 1);
    var thread_storage = cuda_thread_state();
    ropes_out[0] = thread_storage[0].cuda_ropes;
    convs_out[0] = thread_storage[0].cuda_convs;
    thread_storage[0].cuda_ropes = 0;
    thread_storage[0].cuda_convs = 0;
}

export fn fg_cuda_routing(config: i64[:], x: u8[:], y: u8[:], out: u8[:]) -> i64 {
    var thread_storage = cuda_thread_state();
    if config.len != 22 || config[0] < 0 || config[0] >= 5 ||
        config[1] <= 0 || config[1] > INT_MAX ||
        (config[2] != 32 && config[2] != 128) || x.len <= 0 || y.len < 0 || out.len <= 0 { return -1; }
    let active: i32 = start_context();
    if active != 1 { return active; }
    let op: i64 = config[0];
    let compact: bool = op == 1 && config[3] != 0;
    let topk: bool = compact && cuda.moe_topk;
    if op == 1 {
        thread_storage[0].route_compact_pending.ids_host = 0;
        thread_storage[0].route_compact_pending.mailbox_sequence = 0;
    }
    var scale: f32 = (0.0 as! f32);
    if op == 0 {
        let bits: u32 = (config[21] as! u32);
        fg_cuda_host_memcpy(cuda_kernel_argument_address(scale), cuda_kernel_argument_address(bits), 4);
    }
    let fixed_softmax: bool = op == 0 && (cuda.route_softmax_fixed != 0) && thread_storage[0].activations.retire &&
        cuda.decode_reuse && fg143_cpu_prefill() == 0 && x.len == 1024 && y.len == 0 && out.len == 1024 &&
        config[1] == 1 && config[2] == 32 && config[4] == 256 &&
        config[5] == 1 && config[6] == 1 && config[7] == 1 &&
        config[8] == 4 && config[12] == 256 &&
        scale > (0.0 as! f32) && scale <= (1.0 as! f32);
    var compact_weights: bool = topk && (cuda.route_compact8 != 0) && thread_storage[0].activations.retire && cuda.decode_reuse &&
        fg143_cpu_prefill() == 0 && x.len == 1024 && y.len == 0 && out.len == 32 &&
        config[4] == 256 && config[5] == 1 && config[6] == 1 &&
        config[7] == 1 && config[8] == 4 && config[12] == 8;
    let groups: i64 = if compact && !topk { config[5] } else { config[1] };
    let operations: u64[5] = [cuda.route_reduce, if topk { cuda.route_topk } else { cuda.route_sort },
        cuda.route_gather, cuda.route_reduce, cuda.route_norm];
    let data: CudaInputSlice[3] = [x, y, fg_cuda_host_view_u8(cuda_slice_address(config), config.len * 8)];
    var buffers = [unsafe_c_address<c_ptr<device_buffer>>(cuda.input), unsafe_c_address<c_ptr<device_buffer>>(cuda.weight), unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_config)];
    var staged: u64[3];
    var output: u64 = 0;
    let host: u64 = ((cuda_slice_address(out)) as! u64);
    var error: i32 = activation_output(host, out.len, &output);
    if error == 0 { error = upload_inputs(data, buffers, staged, data.len); }
    if error == 0 {
        var args: u64[4] = [
            cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(output),
            cuda_kernel_argument_address(staged[2]),
        ];
        if compact_weights && cuda.route_weights.address == 0 && reserve(&cuda.route_weights, 32) != 0
            { compact_weights = false; }
        if fixed_softmax {
            var softmax_args: u64[3] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(output), cuda_kernel_argument_address(scale),
            ];
            error = cuda_capture_launch(cuda.route_softmax_fixed, 1, 1, 1, 32, 1, 1, 0, 0, fg_cuda_driver_words_address(softmax_args), 0);
            if error == 0 { thread_storage[0].cuda_local_fg_cuda_routing_launches_7003 += 1; }
            if error == 0 && (thread_storage[0].cuda_local_fg_cuda_routing_launches_7003 == 1 || thread_storage[0].cuda_local_fg_cuda_routing_launches_7003 == 40880)
                { fg_cuda_host_diagnostic_100((thread_storage[0].cuda_local_fg_cuda_routing_launches_7003 as! u64)); }
        } else if compact_weights {
            var default_scale: f32 = (1.0 as! f32);
            var compact_args: u64[4] = [
                cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(output), cuda_kernel_argument_address(cuda.route_weights.address),
                cuda_kernel_argument_address(default_scale),
            ];
            if (cuda.route_mailbox_kernel != 0) && (output & (15 as! u32)) == 0 {
                cuda.route_mailbox_sequence += 1;
                if cuda.route_mailbox_sequence == 0 { cuda.route_mailbox_sequence += 1; }
                let seq = cuda.route_mailbox_sequence;
                var mailbox_args: u64[6] = [
                    cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(output), cuda_kernel_argument_address(cuda.route_weights.address),
                    cuda_kernel_argument_address(default_scale), cuda_kernel_argument_address(cuda.route_mailbox_device), cuda_kernel_argument_address(seq),
                ];
                error = cuda_capture_launch(cuda.route_mailbox_kernel, 1, 1, 1, 32, 1, 1, 0, 0, fg_cuda_driver_words_address(mailbox_args), 0);
                if error == 0 { thread_storage[0].route_compact_pending.mailbox_sequence = seq; }
            } else {
                error = cuda_capture_launch(cuda.route_compact8, 1, 1, 1, 32, 1, 1, 0, 0, fg_cuda_driver_words_address(compact_args), 0);
            }
        } else {
            error = cuda_capture_launch(operations[op], (groups as! u32), 1, 1,
                (config[2] as! u32), 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        }
        if error == 0 { error = activation_publish(host, out.len, output, false); }
        if compact_weights && error == 0 {
            thread_storage[0].route_compact_pending.ids_host = host;
            thread_storage[0].route_compact_pending.scores_host = ((cuda_slice_address(x)) as! u64);
            thread_storage[0].route_compact_pending.scores_device = staged[0];
        }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_101((op as! i64), (if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    thread_storage[0].cuda_routes[op] += 1;
    return 1;
}

export fn fg_cuda_take_routes(routes: u64) {
    var thread_storage = cuda_thread_state();
    let address = routes;
    let source = cuda_slice_address(thread_storage[0].cuda_routes);
    let bytes = (thread_storage[0].cuda_routes.len as! u64) * 8;
    fg_cuda_host_memcpy(address, source, bytes);
    fg_cuda_host_memset(source, 0, bytes);
}

export fn fg_cuda_blit(config: i64[:], x: u8[:], y: u8[:], out: u8[:]) -> i64 {
    var thread_storage = cuda_thread_state();
    if config.len != 17 || config[0] < 0 || config[0] >= 3 ||
        config[1] <= 0 || config[1] > INT_MAX || x.len <= 0 || y.len < 0 || out.len <= 0 { return -1; }
    let mode: i64 = config[0];
    let host_x: u64 = ((cuda_slice_address(x)) as! u64);
    let host_y: u64 = ((cuda_slice_address(y)) as! u64);
    let host_out: u64 = ((cuda_slice_address(out)) as! u64);
    var lhs: u64 = state_address(host_x, x.len, 2);
    var rhs: u64 = state_address(host_y, y.len, 2);
    var dest: u64 = if mode == 2 { state_address(host_out, out.len, 2) } else { 0 };
    if mode == 2 && dest == 0 { return 0; }
    if lhs == 0 && rhs == 0 && activation_device(host_x, x.len) == 0 && activation_device(host_y, y.len) == 0 { return 0; }
    let active: i32 = start_context();
    if active != 1 { return active; }

    var error: i32 = if dest != 0 { 0 } else { activation_output(host_out, out.len, &dest) };
    let data: CudaInputSlice[3] = [if lhs != 0 { fg_cuda_host_view_u8((0 as! u64), (0 as! i64)) } else { x }, if rhs != 0 { fg_cuda_host_view_u8((0 as! u64), (0 as! i64)) } else { y },
        fg_cuda_host_view_u8(cuda_slice_address(config), config.len * 8)];
    let buffers: CudaStagingPointer[3] = [unsafe_c_address<c_ptr<device_buffer>>(cuda.input), unsafe_c_address<c_ptr<device_buffer>>(cuda.weight), unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_config)];
    var staged: u64[3] = [lhs, rhs, 0];
    if error == 0 { error = upload_inputs(data, buffers, staged, data.len); }
    if error == 0 {
        var args: u64[4] = [
            cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]), cuda_kernel_argument_address(dest),
            cuda_kernel_argument_address(staged[2]),
        ];
        let prefix: i64 = config[4];
        let width: i64 = config[5];
        let channels: i64 = config[6];
        let tiled: bool = (cuda.blit_transpose != 0) && mode == 0 && config[3] == 0 &&
            prefix > 0 && prefix <= 8 && width - prefix >= 32 && width - prefix <= (32 as! i64) * 65535 &&
            channels >= 32 && channels <= INT_MAX && config[7] == 1 && config[8] == 1 &&
            config[9] == 4 && config[10] >= 4 * prefix && config[10] % 4 == 0 &&
            config[14] == 4 && config[13] >= 4 * channels && config[13] % 4 == 0;
        if tiled {
            error = cuda_capture_launch(cuda.blit_transpose, (((channels + 31) / 32) as! u32),
                (((width - prefix + 31) / 32) as! u32), 1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            if error == 0 && (cuda_env("GOOSE_GRAPH_LOG") != 0)
                { fg_cuda_host_diagnostic_102(); }
        } else {
            error = cuda_capture_launch(cuda.blit, (config[1] as! u32), 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        }
        if error == 0 {
            error = if mode == 2 { finish_device_state_write() } else { activation_publish(host_out, out.len, dest, false) };
        }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_103((mode as! i64), (if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    thread_storage[0].cuda_blits[mode] += 1;
    return 1;
}

export fn fg_cuda_take_blits(blits: u64) {
    var thread_storage = cuda_thread_state();
    let address = blits;
    let source = cuda_slice_address(thread_storage[0].cuda_blits);
    let bytes = (thread_storage[0].cuda_blits.len as! u64) * 8;
    fg_cuda_host_memcpy(address, source, bytes);
    fg_cuda_host_memset(source, 0, bytes);
}

export fn fg_cuda_fwht(src: u8[:], dst: u8[:], width: i32, rows: i64, scale: f32) -> i64 {
    var thread_storage = cuda_thread_state();
    if width > 512 { return 0; }
    let blocks: i64 = (rows + 3) / 4;
    if blocks <= 0 || blocks > INT_MAX { return -1; }
    let active: i32 = start_context();
    if active != 1 { return active; }
    var input: u64 = 0;
    var output: u64 = 0;
    let host: u64 = ((cuda_slice_address(dst)) as! u64);
    var error: i32 = activation_output(host, dst.len, &output);
    if error == 0 { error = activation_input(src, &cuda.input, &input); }
    if error == 0 {
        var args: u64[5] = [
            cuda_kernel_argument_address(input), cuda_kernel_argument_address(output), cuda_kernel_argument_address(width),
            cuda_kernel_argument_address(rows), cuda_kernel_argument_address(scale),
        ];
        let kernel: u64 = prefill_fwht(cuda.fwht, cuda.fwht_fixed, width, rows);
        error = cuda_capture_launch(kernel, (blocks as! u32), 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        if error == 0 && kernel != cuda.fwht {
            if !thread_storage[0].cuda_local_fg_cuda_fwht_logged_7128 { thread_storage[0].cuda_local_fg_cuda_fwht_logged_7128 = true; fg_cuda_host_diagnostic_104(); }
        }
        if error == 0 { error = activation_publish(host, dst.len, output, false); }
    }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_105((if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    thread_storage[0].cuda_fwhts += 1;
    return 1;
}

export fn fg_cuda_take_fwhts() -> u64 {
    var thread_storage = cuda_thread_state();
    let count: u64 = thread_storage[0].cuda_fwhts;
    thread_storage[0].cuda_fwhts = 0;
    return count;
}

export fn fg_cuda_moe_epilogue_enabled() -> u8 {
    var thread_storage = cuda_thread_state();
    return if cuda.moe_prefill_epilogue != 0 && thread_storage[0].activations.active &&
        thread_storage[0].activations.defer_waits { 1 } else { 0 };
}

export fn fg_cuda_moe_epilogue(routed: u64, shared: u64, gate: u64, residual: u64, output: u64, rows: i64, step: i64, deadline: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    if fg_cuda_moe_epilogue_enabled() == 0 || rows < 2 || rows > 4096 { return 0; }
    let bytes: i64 = rows * 8192;
    let gate_bytes: i64 = rows * 4;
    if activation_device(routed, bytes) == 0 || activation_device(shared, bytes) == 0 ||
        activation_device(gate, gate_bytes) == 0 || activation_device(residual, bytes) == 0 { return 0; }
    if !thread_storage[0].activations.retire || !thread_storage[0].activations.reuse { return 0; }
    let slot: i64 = fg143_activation_overlap(activation_ranges(), routed, bytes, 0);
    if slot < 0 { return 0; }
    let record = slot * ACTIVATION_WORDS;
    if (thread_storage[0].activations.ranges[record + 0] as! u64) != routed || thread_storage[0].activations.ranges[record + 1] != bytes || thread_storage[0].activations.ranges[record + 4] != step + 1 { return 0; }
    if fg_cuda_activation_prepare(output, bytes, true, true, step, deadline) != 0 { return -1; }
    if (thread_storage[0].activations.ranges[record + 0] as! u64) != routed || thread_storage[0].activations.ranges[record + 1] != bytes { return 0; }
    let r = activation_device(routed, bytes);
    let s = activation_device(shared, bytes);
    let g = activation_device(gate, gate_bytes);
    let x = activation_device(residual, bytes);
    if r == 0 || s == 0 || g == 0 || x == 0 || (r | s | x) % 16 != 0 { return 0; }
    if (if r <= s { s - r < (bytes as! u64) } else { r - s < (bytes as! u64) }) ||
        (if r <= x { x - r < (bytes as! u64) } else { r - x < (bytes as! u64) }) ||
        (if r <= g { g - r < (bytes as! u64) } else { r - g < (gate_bytes as! u64) }) { return 0; }
    if start_context() != 1 { return -1; }
    let vectors: u32 = (rows as! u32) * 512;
    var dst: u64 = r;
    var args: u64[6] = [
        cuda_kernel_argument_address(r), cuda_kernel_argument_address(s), cuda_kernel_argument_address(g),
        cuda_kernel_argument_address(x), cuda_kernel_argument_address(dst), cuda_kernel_argument_address(vectors),
    ];
    thread_storage[0].activations.pending = true;
    let error: i32 = cuda_capture_launch(cuda.moe_prefill_epilogue, (vectors + 127) / 128, 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
    if finish_context() != 0 || error != 0 { return -1; }
    thread_storage[0].activations.ranges[record + 0] = (output as! i64);
    thread_storage[0].activations.ranges[record + 3] = 0;
    thread_storage[0].activations.ranges[record + 4] = deadline;
    thread_storage[0].cuda_local_fg_cuda_moe_epilogue_fused_7184 += 1;
    if thread_storage[0].cuda_local_fg_cuda_moe_epilogue_fused_7184 == 1 || thread_storage[0].cuda_local_fg_cuda_moe_epilogue_fused_7184 % 40 == 0
        { fg_cuda_host_diagnostic_106((thread_storage[0].cuda_local_fg_cuda_moe_epilogue_fused_7184 as! u64)); }
    return 1;
}

export fn fg_cuda_moe_weighted(x: u8[:], weights: u8[:], dst: u8[:], width: i64, used: i64, count: i64, tile: i64) -> i64 {
    var thread_storage = cuda_thread_state();
    let active: i32 = start_context();
    if active != 1 { return active; }
    let x_row: i64 = x.len / count;
    let w_row: i64 = weights.len / count;
    let out_row: i64 = dst.len / count;
    let columns: u32 = (((width + 255) / 256) as! u32);
    let host: u64 = ((cuda_slice_address(dst)) as! u64);
    var output: u64 = 0;
    var error: i32 = activation_output(host, dst.len, &output);
    block {
        var first: i64 = 0;
        while first < count && error == 0 {
            let tokens: i64 = if count - first < tile { count - first } else { tile };
            var input: u64 = 0;
            var scale: u64 = 0;
            var dest: u64 = output + ((first * out_row) as! u64);
            error = activation_input(fg_cuda_host_view_u8((cuda_slice_address(x) + (first * x_row) as! u64), (tokens * x_row as! i64)), &cuda.input, &input);
            if error == 0 {
                error = activation_input(fg_cuda_host_view_u8((cuda_slice_address(weights) + (first * w_row) as! u64), (tokens * w_row as! i64)), &cuda.weight, &scale);
            }
            if error == 0 {
                var args: u64[5] = [
                    cuda_kernel_argument_address(input), cuda_kernel_argument_address(scale), cuda_kernel_argument_address(dest),
                    cuda_kernel_argument_address(width), cuda_kernel_argument_address(used),
                ];
                error = cuda_capture_launch(cuda.moe_weighted, (tokens as! u32), columns, 1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
                if error == 0 { thread_storage[0].cuda_moe_weighted_tiles += 1; }
            }
            first += tile;
        }
    };
    if error == 0 { error = activation_publish(host, dst.len, output, false); }
    let popped: i32 = finish_context();
    if error != 0 || popped != 0 {
        fg_cuda_host_diagnostic_107((if error != 0 { error } else { popped as! i32 }));
        return -1;
    }
    return 1;
}

export fn fg_cuda_take_moe_weighted() -> u64 {
    var thread_storage = cuda_thread_state();
    let count: u64 = thread_storage[0].cuda_moe_weighted_tiles;
    thread_storage[0].cuda_moe_weighted_tiles = 0;
    return count;
}

export fn fg_cuda_giga_partial_pin() -> u8 {
    var setting: u64 = cuda_env("GOOSE_GIGA_PARTIAL_PIN");
    return if !cuda_env_equal(setting, "0") { 1 } else { 0 };
}

var giga_kv_resident: bool;

export fn fg_cuda_giga_kv_configure(capacity: i64) -> u8 {
    var mode: u64 = cuda_env("GOOSE_GIGA_KV_RESIDENT");
    let explicit_resident: bool = cuda_env_equal(mode, "1");
    if (mode != 0 && !explicit_resident) || (!explicit_resident && capacity > 36864) { return 0; }
    var lazy: u64 = cuda_env("GOOSE_KV_LAZY_VMM");
    let allow_lazy: bool = !cuda_env_equal(lazy, "0");
    if !allow_lazy && !explicit_resident { return 0; }
    if start_context() != 1 { return 0; }
    var device: i32 = 0;
    var available: u64 = 0;
    var total: u64 = 0;
    if allow_lazy && !cuda.lazy_kv && fg_cuda_driver_device_get_typed(unsafe_c_ptr<c_ptr<i32>>(unsafe_c_address<c_ptr<void>>(device)), 0) == 0 { init_vmm(device, true); }
    let admitted: bool = cuda_capture_memory_info(cuda_kernel_argument_address(available), cuda_kernel_argument_address(total)) == 0 &&
        available >= 2048 * (MIB_BYTES as! u64) && (cuda.lazy_kv || explicit_resident);
    let popped: i32 = finish_context();
    giga_kv_resident = admitted && popped == 0;
    return if giga_kv_resident { 1 } else { 0 };
}

export fn fg_cuda_giga_kv_write_enabled() -> u8 {
    var thread_storage = cuda_thread_state();
    return if thread_storage[0].giga_small_expert_wave && giga_kv_resident { 1 } else { 0 };
}

extern fn fg_cuda_image_tanh_cubin() -> u64;
extern fn fg_cuda_image_situ_cubin() -> u64;
extern fn fg_cuda_host_diagnostic_115(status: i32, popped: i32) -> i32;

var cuda_unary_modules: u64[2];
var cuda_unary_kernels: u64[2];

export fn fg_cuda_unary_begin() -> i32 { return start_context(); }
export fn fg_cuda_unary_end() -> i32 { return finish_context(); }

export fn fg_cuda_unary_prepare(kind: i32, input: const u8[:], output: u8[:], plan: u64[:]) -> i32 {
    if kind < 0 || kind > 1 || plan.len != 3 { return -1; }
    let image = if kind == 0 { fg_cuda_image_tanh_cubin() } else { fg_cuda_image_situ_cubin() };
    var status = cuda_ensure_module(&cuda_unary_modules[kind], image);
    if status == 0 {
        let name = if kind == 0 { "goose_tanh_f32" } else { "goose_i2_moe_situ" };
        status = cuda_ensure_kernel(&cuda_unary_kernels[kind], cuda_unary_modules[kind], name);
    }
    plan[0] = cuda_unary_kernels[kind];
    var source: u64 = 0;
    var destination: u64 = 0;
    if status == 0 {
        status = activation_output_for_input(cuda_slice_address(output), output.len, &destination,
                                             input, fg_cuda_host_view_u64(0, 0));
    }
    if status == 0 { status = activation_input(input, &cuda.input, &source); }
    plan[1] = source;
    plan[2] = destination;
    return status;
}

export fn fg_cuda_unary_launch(kind: i32, plan: const u64[:], elements: u32) -> i32 {
    if kind < 0 || kind > 1 || plan.len != 3 || elements == 0 { return -1; }
    var reference: u32 = if cuda.activation_reference { 1 } else { 0 };
    var args: u64[4] = [
        cuda_kernel_argument_address(plan[1]), cuda_kernel_argument_address(plan[2]),
        cuda_kernel_argument_address(elements),
        if kind == 0 { 0 } else { cuda_kernel_argument_address(reference) },
    ];
    return fg_cuda_driver_launch(plan[0], (elements + 255) / 256, 1, 1, 256, 1, 1, 0, 0,
                                 fg_cuda_driver_words_address(args), 0);
}

export fn fg_cuda_unary_publish(output: u8[:], device: u64) -> i32 {
    return activation_publish(cuda_slice_address(output), output.len, device, false);
}

export fn fg_cuda_unary_tanh_error(status: i32, popped: i32) {
    fg_cuda_host_diagnostic_115(status, popped);
}

extern fn fg_cuda_image_giga_dsa_image() -> u64;
extern fn giga_cuda_dsa_mask_plan(cells: i64, count: i64, rows: i64, prefix: i64, mask: i64, picks: i64, out: i64, plan: i64[:]) -> bool;
extern fn giga_cuda_dsa_top_valid(cells: i64, rows: i64, scores: i64, ids: i64) -> bool;
extern fn giga_cuda_dsa_score_valid(config: const i64[:], lengths: const i64[:]) -> bool;
extern fn giga_cuda_dsa_score_columns(cells: i64, shared: bool) -> i32;
extern fn fg_cuda_host_diagnostic_116(rows: i64, cells: i64) -> i32;
extern fn fg_cuda_host_diagnostic_117(rows: i64, cells: i64) -> i32;
extern fn fg_cuda_host_diagnostic_118(cells: i64, rows: i64) -> i32;

struct GigaDsaKernels {
    module: u64,
    score: u64,
    shared_query: u64,
    wide_top: u64,
    clear_mask: u64,
    scatter_mask: u64,
}
var giga_dsa: GigaDsaKernels;

fn load_giga_dsa() -> bool {
    if giga_dsa.module != 0 { return giga_dsa.score != 0; }
    let loaded = cuda_ensure_module(&giga_dsa.module, fg_cuda_image_giga_dsa_image());
    if loaded != 0 { return false; }
    let module = giga_dsa.module;
    if cuda_ensure_kernel(&giga_dsa.score, module, "goose_dsa_score") != 0 ||
        cuda_ensure_kernel(&giga_dsa.shared_query, module, "goose_giga_dsa_score_shared_q") != 0 ||
        cuda_ensure_kernel(&giga_dsa.wide_top, module, "goose_giga_dsa_top_wide") != 0 ||
        cuda_ensure_kernel(&giga_dsa.clear_mask, module, "goose_giga_mask_clear") != 0 ||
        cuda_ensure_kernel(&giga_dsa.scatter_mask, module, "goose_giga_mask_scatter") != 0 {
        giga_dsa.score = 0;
        return false;
    }
    return true;
}

export fn fg_cuda_giga_dsa_mask(mask: u8[:], picks: u8[:], out: u8[:], cells: i64, count: i64, rows: i64, prefix: i64) -> i64 {
    let tls = cuda_thread_state();
    if !tls[0].giga_small_expert_wave || rows > 4096 { return 0; }
    var plan: i64[5];
    if !giga_cuda_dsa_mask_plan(cells, count, rows, prefix, mask.len, picks.len, out.len, plan) { return -1; }
    let active = start_context();
    if active != 1 { return active as! i64; }
    if !load_giga_dsa() { return if finish_context() == 0 { 0 } else { -1 }; }

    let host = cuda_slice_address(out);
    var output: u64 = 0;
    var device_mask: u64 = 0;
    var device_picks: u64 = 0;
    var status = activation_output(host, out.len, &output);
    if status == 0 { status = activation_input(mask, &cuda.input, &device_mask); }
    if status == 0 { status = activation_input(picks, &cuda.jobs, &device_picks); }
    var clear_args: u64[4] = [cuda_kernel_argument_address(device_mask), cuda_kernel_argument_address(output),
                            cuda_kernel_argument_address(plan[0]), cuda_kernel_argument_address(plan[2])];
    var scatter: u64[7] = [
        cuda_kernel_argument_address(device_mask), cuda_kernel_argument_address(device_picks),
        cuda_kernel_argument_address(output), cuda_kernel_argument_address(cells),
        cuda_kernel_argument_address(count), cuda_kernel_argument_address(plan[1]),
        cuda_kernel_argument_address(prefix),
    ];
    if status == 0 {
        status = fg_cuda_driver_launch(giga_dsa.clear_mask, plan[3] as! u32, 1, 1, 256, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(clear_args), 0);
    }
    if status == 0 {
        status = fg_cuda_driver_launch(giga_dsa.scatter_mask, plan[4] as! u32, 1, 1, 256, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(scatter), 0);
    }
    if status == 0 { status = activation_publish(host, out.len, output, false); }
    if finish_context() != 0 || status != 0 { return -1; }
    if cuda_env("GOOSE_GRAPH_LOG") != 0 { fg_cuda_host_diagnostic_116(rows, cells); }
    return 1;
}

export fn fg_cuda_giga_dsa_top_wide(scores: u8[:], ids: u8[:], cells: i64, rows: i64) -> i64 {
    let tls = cuda_thread_state();
    if !tls[0].giga_small_expert_wave || rows < 128 { return 0; }
    if !giga_cuda_dsa_top_valid(cells, rows, scores.len, ids.len) { return -1; }
    let active = start_context();
    if active != 1 { return active as! i64; }
    if !load_giga_dsa() { return if finish_context() == 0 { 0 } else { -1 }; }

    let host = cuda_slice_address(ids);
    var input: u64 = 0;
    var output: u64 = 0;
    var status = activation_output(host, ids.len, &output);
    if status == 0 { status = activation_input(scores, &cuda.input, &input); }
    var args: u64[3] = [cuda_kernel_argument_address(input), cuda_kernel_argument_address(output),
                       cuda_kernel_argument_address(cells)];
    if status == 0 {
        status = fg_cuda_driver_launch(giga_dsa.wide_top, rows as! u32, 1, 1, 256, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(args), 0);
    }
    if status == 0 { status = activation_publish(host, ids.len, output, false); }
    if finish_context() != 0 || status != 0 { return -1; }
    if cuda_env("GOOSE_GRAPH_LOG") != 0 { fg_cuda_host_diagnostic_117(rows, cells); }
    return 1;
}

export fn fg_cuda_dsa_score(config: i64[:], q: u8[:], k: u8[:], weights: u8[:], mask: u8[:], out: u8[:]) -> i64 {
    let tls = cuda_thread_state();
    if !tls[0].giga_small_expert_wave { return 0; }
    let lengths: i64[5] = [q.len, k.len, weights.len, mask.len, out.len];
    if !giga_cuda_dsa_score_valid(config, lengths) { return -1; }
    let active = start_context();
    if active != 1 { return active as! i64; }
    if !load_giga_dsa() { return if finish_context() == 0 { 0 } else { -1 }; }

    var key: u64 = 0;
    var status = map_state_address(cuda_slice_address(k), k.len, 0, &key);
    let empty = fg_cuda_host_view_u8(0, 0);
    let inputs: CudaInputSlice[5] = [q, if key != 0 { empty } else { k }, weights, mask,
                                    fg_cuda_host_view_u8(cuda_slice_address(config), config.len * 8)];
    let buffers: CudaStagingPointer[5] = [
        unsafe_c_address<c_ptr<device_buffer>>(cuda.input),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.weight),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_value),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.jobs),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_config),
    ];
    var staged: u64[5] = [0, key, 0, 0, 0];
    let host = cuda_slice_address(out);
    var output: u64 = 0;
    if status == 0 { status = activation_output(host, out.len, &output); }
    if status == 0 { status = upload_inputs(inputs, buffers, staged, inputs.len); }
    if status == 0 {
        var args: u64[6] = [
            cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]),
            cuda_kernel_argument_address(staged[2]), cuda_kernel_argument_address(staged[3]),
            cuda_kernel_argument_address(output), cuda_kernel_argument_address(staged[4]),
        ];
        let shared_query = giga_dsa.shared_query != 0 && config[1] >= 128;
        let kernel = if shared_query { giga_dsa.shared_query } else { giga_dsa.score };
        let columns = giga_cuda_dsa_score_columns(config[0], shared_query);
        status = fg_cuda_driver_launch(kernel, columns as! u32, config[1] as! u32, 1, 128, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(args), 0);
        if status == 0 && shared_query && cuda_env("GOOSE_GRAPH_LOG") != 0 {
            fg_cuda_host_diagnostic_118(config[0], config[1]);
        }
    }
    if status == 0 { status = activation_publish(host, out.len, output, false); }
    if finish_context() != 0 || status != 0 { return -1; }
    return 1;
}

const CERTIFIED_DSA_KERNELS: i64 = 10;
extern fn fg_cuda_image_giga_certified_dsa_image() -> u64;
extern fn giga_dsa_schedule_workspace(cells: i64, rows: i64) -> u64;
extern fn giga_dsa_schedule(config: const i64[:], buffers: const u64[:], workspace_bytes: u64) -> i32;
extern fn fg_cuda_host_diagnostic_119(cells: i64, rows: i64, tile: i64, scratch: u64) -> i32;

struct CertifiedDsaState {
    module: u64,
    kernels: u64[CERTIFIED_DSA_KERNELS],
    scratch: device_buffer,
}
var certified_dsa: CertifiedDsaState;

fn load_certified_dsa() -> bool {
    if certified_dsa.module != 0 { return true; }
    if fg_cuda_driver_zero_async_bind(cuda.library) == 0 { return false; }
    let unload = fg_cuda_driver_module_unload_bind(cuda.library);
    if cuda_ensure_module(&certified_dsa.module, fg_cuda_image_giga_certified_dsa_image()) != 0 {
        return false;
    }
    let names = [
        "goose_dsa_pack_power2", "goose_dsa_bound_metadata", "goose_dsa_bound_query",
        "goose_dsa_integer_score", "goose_dsa_score_cutoff", "goose_dsa_compact",
        "goose_dsa_replay", "goose_dsa_finalize", "goose_dsa_fallback_score", "goose_dsa_fallback_top",
    ];
    for i in CERTIFIED_DSA_KERNELS {
        if cuda_lookup_kernel(cuda_kernel_argument_address(certified_dsa.kernels[i]),
                              certified_dsa.module, names[i]) != 0 {
            if unload != 0 { fg_cuda_driver_module_unload(certified_dsa.module); }
            certified_dsa.module = 0;
            return false;
        }
    }
    return true;
}

export fn giga_dsa_schedule_zero(address: u64, bytes: u64) -> i32 {
    return fg_cuda_driver_zero_async(address, 0, bytes, 0);
}

export fn giga_dsa_schedule_launch(kernel: i32, columns: i64, rows: i64, threads: i64, packed: const u64[:]) -> i32 {
    if kernel < 0 || (kernel as! i64) >= CERTIFIED_DSA_KERNELS ||
        packed.len < 1 || packed.len > CERTIFIED_DSA_KERNELS { return -1; }
    var args: u64[CERTIFIED_DSA_KERNELS];
    for i in packed.len { args[i] = cuda_kernel_argument_address(packed[i]); }
    return fg_cuda_driver_launch(certified_dsa.kernels[kernel], columns as! u32, rows as! u32, 1,
                                 threads as! u32, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
}

export fn fg_cuda_giga_dsa_certified(config: i64[:], q: u8[:], k: u8[:], weights: u8[:], mask: u8[:], ids: u8[:]) -> i64 {
    if config.len != 6 || cuda_slice_address(config) == 0 || cuda_slice_address(q) == 0 ||
        cuda_slice_address(k) == 0 || cuda_slice_address(weights) == 0 ||
        cuda_slice_address(mask) == 0 || cuda_slice_address(ids) == 0 { return 0; }
    let cells = config[0];
    let rows = config[1];
    let stride = config[2];
    let scratch = giga_dsa_schedule_workspace(cells, rows);
    if scratch == 0 || stride < 256 || stride > INT_MAX || stride % 2 != 0 ||
        config[3] != 32768 || config[4] != 256 || config[5] != 2 * cells ||
        q.len != rows * 32768 || k.len < (cells - 1) * stride + 256 ||
        weights.len != rows * 256 || mask.len != rows * cells * 2 || ids.len != rows * 8192 { return 0; }

    let active = start_context();
    if active != 1 { return active as! i64; }
    let tls = cuda_thread_state();
    if !tls[0].giga_small_expert_wave || !load_certified_dsa() {
        return if finish_context() == 0 { 0 } else { -1 };
    }
    let tile = if rows < 512 { rows } else { 512 };
    let score_bytes = (cells as! u64) * (tile as! u64) * 4;
    if reserve(&certified_dsa.scratch, score_bytes + scratch) != 0 {
        return if finish_context() == 0 { 0 } else { -1 };
    }
    var key: u64 = 0;
    var output: u64 = 0;
    var status = map_state_address(cuda_slice_address(k), k.len, 0, &key);
    let empty = fg_cuda_host_view_u8(0, 0);
    let inputs: CudaInputSlice[5] = [q, if key != 0 { empty } else { k }, weights, mask,
                                    fg_cuda_host_view_u8(cuda_slice_address(config), config.len * 8)];
    let buffers: CudaStagingPointer[5] = [
        unsafe_c_address<c_ptr<device_buffer>>(cuda.input),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.weight),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_value),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.jobs),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_config),
    ];
    var staged: u64[5] = [0, key, 0, 0, 0];
    let host = cuda_slice_address(ids);
    if status == 0 { status = activation_output(host, ids.len, &output); }
    if status == 0 { status = upload_inputs(inputs, buffers, staged, inputs.len); }
    if status == 0 {
        let scores = certified_dsa.scratch.address;
        let scheduled: u64[8] = [staged[0], staged[1], staged[2], staged[3],
                                 scores, output, staged[4], scores + score_bytes];
        if giga_dsa_schedule(config, scheduled, scratch) != 1 { status = -1; }
    }
    if status == 0 { status = activation_publish(host, ids.len, output, false); }
    if status == 0 && cuda_env("GOOSE_GIGACHAT4_TRACE") != 0 {
        fg_cuda_host_diagnostic_119(cells, rows, tile, score_bytes + scratch);
    }
    return if finish_context() == 0 && status == 0 { 1 } else { -1 };
}

export fn fg_cuda_giga_dsa_certified_enabled() -> u8 {
    let tls = cuda_thread_state();
    return if tls[0].giga_small_expert_wave && cuda_env_equal(cuda_env("GOOSE_GIGA_DSA_CERTIFIED"), "1") { 1 } else { 0 };
}

export fn fg_cuda_giga_dsa_certified_address(config: i64[:], inputs: u64[:], sizes: i64[:], out: u64, out_bytes: i64) -> i64 {
    if inputs.len != 4 || sizes.len != 4 { return 0; }
    return fg_cuda_giga_dsa_certified(config,
        fg_cuda_host_view_u8(inputs[0], sizes[0]), fg_cuda_host_view_u8(inputs[1], sizes[1]),
        fg_cuda_host_view_u8(inputs[2], sizes[2]), fg_cuda_host_view_u8(inputs[3], sizes[3]),
        fg_cuda_host_view_u8(out, out_bytes));
}

extern fn fg_cuda_image_giga_mla_image() -> u64;
extern fn giga_cuda_mla_validate(config: const i64[:], lengths: const i64[:]) -> i32;
extern fn giga_cuda_mla_plan(config: const i64[:], capacity: i32, pair: bool, batch_pair: bool, mma: bool, plan: i64[:]);
extern fn giga_cuda_mla_indexed_capacity(counts: const i32[:], task_bytes: u64, capacity: i32, tasks: i32) -> i32;
extern fn giga_cuda_mla_dispatch(context: u64, tasks: i32, capacity: i32, tiles: i32, positions: i64,
                                 paired: bool, mma: bool, indexed: bool, visible: bool) -> i32;
extern fn fg_cuda_host_diagnostic_120(positions: i64, cells: i64) -> i32;
extern fn fg_cuda_host_diagnostic_121(positions: i64, cells: i64) -> i32;
extern fn fg_cuda_host_diagnostic_122(status: i32) -> i32;

struct GigaMlaState {
    module: u64,
    tiles: u64, merge: u64, pair: u64, batch_pair: u64, batch_merge: u64,
    mma: u64, visibility_tile: u64, visibility_build: u64,
    compact_mask: u64, indexed_tile: u64, indexed_merge: u64,
    indices: device_buffer, visibility: device_buffer,
    available: bool,
}
var giga_mla: GigaMlaState;

fn load_giga_mla() -> bool {
    if giga_mla.module != 0 { return giga_mla.available; }
    if cuda_ensure_module(&giga_mla.module, fg_cuda_image_giga_mla_image()) != 0 { return false; }
    let module = giga_mla.module;
    if cuda_ensure_kernel(&giga_mla.tiles, module, "goose_mla_attention_tiles") != 0 ||
        cuda_ensure_kernel(&giga_mla.merge, module, "goose_mla_attention_merge") != 0 { return false; }
    if !cuda_env_equal(cuda_env("GOOSE_GIGA_MLA_PAIR"), "0") {
        cuda_ensure_kernel(&giga_mla.pair, module, "goose_mla_attention_pair");
    }
    if !cuda_env_equal(cuda_env("GOOSE_GIGA_MLA_BATCH_PAIR"), "0") {
        cuda_ensure_kernel(&giga_mla.batch_pair, module, "goose_mla_attention_batch_pair");
        cuda_ensure_kernel(&giga_mla.batch_merge, module, "goose_mla_attention_batch_merge");
    }
    if !cuda_env_equal(cuda_env("GOOSE_GIGA_MLA_MMA"), "0") {
        if !cuda_env_equal(cuda_env("GOOSE_GIGA_MLA_PV_MMA"), "0") {
            cuda_ensure_kernel(&giga_mla.mma, module, "goose_giga_mla_mma_pv_tile");
        }
        if giga_mla.mma != 0 && !cuda_env_equal(cuda_env("GOOSE_GIGA_MLA_VISIBILITY"), "0") {
            cuda_ensure_kernel(&giga_mla.visibility_tile, module, "goose_giga_mla_mma_pv_visible_tile");
            cuda_ensure_kernel(&giga_mla.visibility_build, module, "goose_giga_mla_visibility");
        }
        if giga_mla.mma != 0 && !cuda_env_equal(cuda_env("GOOSE_GIGA_MLA_INDEXED"), "0") {
            cuda_ensure_kernel(&giga_mla.compact_mask, module, "goose_giga_mla_compact_mask");
            cuda_ensure_kernel(&giga_mla.indexed_tile, module, "goose_giga_mla_mma_pv_indexed");
            cuda_ensure_kernel(&giga_mla.indexed_merge, module, "goose_giga_mla_indexed_merge");
        }
        cuda_ensure_kernel(&giga_mla.mma, module, "goose_giga_mla_mma_tile");
    }
    giga_mla.available = true;
    return true;
}

// The wave context stores the argument arrays, the mutable start index, then the kernel tables.
const MLA_WAVE_CONTEXT_WORDS: i64 = 12;

export fn giga_mla_launch_wave(address: u64, start: i32, count: i32, tile: i32,
                               merge: i32, blocks: i32, tile_rows: i32, threads: i32) -> i32 {
    let wave = fg_cuda_host_view_u64(address, MLA_WAVE_CONTEXT_WORDS);
    var index = fg_cuda_host_view_i32(wave[2], 1);
    index[0] = start;
    let status = fg_cuda_driver_launch(wave[3 + (tile as! i64)], blocks as! u32, tile_rows as! u32, 1,
                                       threads as! u32, 1, 1, 0, 0, wave[0], 0);
    if status != 0 { return status; }
    return fg_cuda_driver_launch(wave[9 + (merge as! i64)], count as! u32, 1, 1, 32, 1, 1, 0, 0, wave[1], 0);
}

export fn fg_cuda_giga_mla_attention(config: i64[:], q: u8[:], k: u8[:], v: u8[:], mask: u8[:], out: u8[:], scale: f32) -> i64 {
    var tls = cuda_thread_state();
    if !tls[0].giga_small_expert_wave || config.len != 23 || config[22] != 1 { return 0; }
    if cuda_env_equal(cuda_env("GOOSE_GIGACHAT4_MLA_ATTENTION"), "0") { return 0; }
    let lengths: i64[5] = [q.len, k.len, v.len, mask.len, out.len];
    if giga_cuda_mla_validate(config, lengths) != 1 { return -1; }
    let active = start_context();
    if active != 1 { return active as! i64; }
    if !load_giga_mla() { return if finish_context() == 0 { 0 } else { -1 }; }

    var key: u64 = 0;
    var value: u64 = 0;
    var status = map_state_address(cuda_slice_address(k), k.len, 0, &key);
    if status == 0 { status = map_state_address(cuda_slice_address(v), v.len, 0, &value); }
    let resident = key != 0 && value != 0;
    let empty = fg_cuda_host_view_u8(0, 0);
    let inputs: CudaInputSlice[5] = [q, if key != 0 { empty } else { k }, if value != 0 { empty } else { v },
                                    mask, fg_cuda_host_view_u8(cuda_slice_address(config), config.len * 8)];
    let buffers: CudaStagingPointer[5] = [
        unsafe_c_address<c_ptr<device_buffer>>(cuda.input), unsafe_c_address<c_ptr<device_buffer>>(cuda.weight),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_value), unsafe_c_address<c_ptr<device_buffer>>(cuda.jobs),
        unsafe_c_address<c_ptr<device_buffer>>(cuda.attention_config),
    ];
    var staged: u64[5] = [0, key, value, 0, 0];
    var output: u64 = 0;
    let host = cuda_slice_address(out);
    if status == 0 { status = activation_output(host, out.len, &output); }
    if status == 0 { status = upload_inputs(inputs, buffers, staged, inputs.len); }

    let tasks = config[18] as! i32;
    let requested = config[19] as! i32;
    let task_bytes = (config[20] / (requested as! i64)) as! u64;
    var capacity = requested;
    if status == 0 {
        loop {
            status = reserve(&cuda.attention_scratch, task_bytes * (capacity as! u64));
            if status != 2 || capacity <= 1 { break; }
            capacity /= 2;
        }
        if status == 0 && capacity != requested && cuda_env("GOOSE_GRAPH_LOG") != 0 {
            fg_cuda_host_diagnostic_90(capacity, requested);
        }
    }
    let tiles = config[17] as! i32;
    let positions = config[2];
    var plan: i64[4];
    giga_cuda_mla_plan(config, capacity, giga_mla.pair != 0,
                      giga_mla.batch_pair != 0 && giga_mla.batch_merge != 0, giga_mla.mma != 0, plan);
    capacity = plan[0] as! i32;
    let paired = plan[1] != 0;
    let mma = plan[2] != 0;
    var indexed = false;
    if status == 0 && plan[3] != 0 && staged[3] != 0 &&
        giga_mla.compact_mask != 0 && giga_mla.indexed_tile != 0 && giga_mla.indexed_merge != 0 {
        let length_bytes = (positions as! u64) * 4;
        let key_bytes = length_bytes * 2048;
        let admitted = reserve(&giga_mla.indices, key_bytes + length_bytes);
        if admitted == 0 {
            let length_address = giga_mla.indices.address + key_bytes;
            var args: u64[4] = [cuda_kernel_argument_address(staged[3]), cuda_kernel_argument_address(giga_mla.indices.address),
                               cuda_kernel_argument_address(length_address), cuda_kernel_argument_address(staged[4])];
            status = fg_cuda_driver_launch(giga_mla.compact_mask, ((positions + 3) / 4) as! u32,
                                           1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            var counts: i32[4096];
            if status == 0 { status = fg_cuda_driver_download(cuda_slice_address(counts), length_address, length_bytes); }
            if status == 0 {
                let next = giga_cuda_mla_indexed_capacity(fg_cuda_host_view_i32(cuda_slice_address(counts), positions),
                                                         task_bytes, capacity, tasks);
                indexed = next > 0;
                if indexed { capacity = next; }
            }
        } else if admitted != 2 { status = admitted; }
    }
    var visible = false;
    if status == 0 && mma && !indexed && staged[3] != 0 && giga_mla.visibility_build != 0 && giga_mla.visibility_tile != 0 {
        let flag_bytes = ((positions / 8) as! u64) * (tiles as! u64) * 4;
        let admitted = reserve(&giga_mla.visibility, flag_bytes);
        if admitted == 0 {
            var args: u64[3] = [cuda_kernel_argument_address(staged[3]), cuda_kernel_argument_address(giga_mla.visibility.address),
                               cuda_kernel_argument_address(staged[4])];
            status = fg_cuda_driver_launch(giga_mla.visibility_build, (positions / 8) as! u32, tiles as! u32,
                                           1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
            visible = status == 0;
        } else if admitted != 2 { status = admitted; }
    }
    let flags = if indexed { giga_mla.indices.address } else { giga_mla.visibility.address };
    var start: i32 = -1;
    let start_address = cuda_kernel_argument_address(start);
    var tile_args: u64[9] = [
        cuda_kernel_argument_address(staged[0]), cuda_kernel_argument_address(staged[1]),
        cuda_kernel_argument_address(staged[2]), cuda_kernel_argument_address(staged[3]),
        cuda_kernel_argument_address(cuda.attention_scratch.address), cuda_kernel_argument_address(staged[4]),
        cuda_kernel_argument_address(scale), start_address, cuda_kernel_argument_address(flags),
    ];
    var merge_args: u64[4] = [cuda_kernel_argument_address(cuda.attention_scratch.address),
                              cuda_kernel_argument_address(output), cuda_kernel_argument_address(staged[4]), start_address];
    var wave: u64[MLA_WAVE_CONTEXT_WORDS] = [
        fg_cuda_driver_words_address(tile_args), fg_cuda_driver_words_address(merge_args), start_address,
        giga_mla.indexed_tile, giga_mla.visibility_tile, giga_mla.mma, giga_mla.pair, giga_mla.batch_pair, giga_mla.tiles,
        giga_mla.indexed_merge, giga_mla.batch_merge, giga_mla.merge,
    ];
    if status == 0 {
        status = giga_cuda_mla_dispatch(cuda_slice_address(wave), tasks, capacity, tiles, positions, paired, mma, indexed, visible);
    }
    if status == 0 && cuda_env("GOOSE_GRAPH_LOG") != 0 {
        if indexed { fg_cuda_host_diagnostic_120(positions, config[6]); }
        if mma { fg_cuda_host_diagnostic_121(positions, config[6]); }
    }
    if status == 0 { status = activation_publish(host, out.len, output, false); }
    if status != 0 && start >= 0 { fg_cuda_driver_sync(); }
    let popped = finish_context();
    if status != 0 || popped != 0 {
        fg_cuda_host_diagnostic_122(if status != 0 { status } else { popped });
        return -1;
    }
    tls[0].cuda_attentions += 1;
    if config[21] > 0 { tls[0].cuda_causal_attentions += 1; }
    if resident { tls[0].cuda_resident_kv += 1; }
    return 1;
}

extern fn fg_cuda_image_moe_epilogue_cubin() -> u64;
extern fn fg_cuda_host_diagnostic_123(slot: i32, position: i32, status: i32, popped: i32) -> i32;
extern fn fg_cuda_host_diagnostic_124(restore: i32, bytes: i32) -> i32;

const INKLING_EPILOGUE_VISITS: i64 = 48;
const INKLING_EPILOGUE_WIDTH: i64 = 2304;
const INKLING_EPILOGUE_ROW_BYTES: u64 = 2304 * 4;
const INKLING_EPILOGUE_HISTORY_BYTES: u64 = 3 * INKLING_EPILOGUE_ROW_BYTES;
const INKLING_EPILOGUE_BACKUP_BYTES: u64 = 48 * INKLING_EPILOGUE_HISTORY_BYTES;

struct InklingEpilogueState {
    buffer: device_buffer,
    length: i32,
}

var inkling_epilogue_states: InklingEpilogueState[INKLING_EPILOGUE_VISITS];
var inkling_epilogue_module: u64;
var inkling_epilogue_kernel: u64;
var inkling_epilogue_backup: device_buffer;

export fn fg_cuda_inkling_moe_epilogue(input: u8[:], residual: u8[:], weights: u8[:], output: u8[:],
                                       slot: i32, position: i32, rows: i32) -> i64 {
    let row_bytes = INKLING_EPILOGUE_ROW_BYTES as! i64;
    if slot < 0 || (slot as! i64) >= INKLING_EPILOGUE_VISITS || position < 0 ||
        rows <= 0 || rows > 4096 || position > 1048576 - rows ||
        input.len != row_bytes * (rows as! i64) || residual.len != input.len ||
        output.len != input.len || weights.len != 4 * row_bytes { return -1; }
    if start_context() != 1 { return -1; }

    var tls = cuda_thread_state();
    var status: i32 = if position > 0 && inkling_epilogue_states[slot].length != position { -1 } else { 0 };
    if status == 0 {
        status = cuda_ensure_module(&inkling_epilogue_module, fg_cuda_image_moe_epilogue_cubin());
    }
    if status == 0 {
        status = cuda_ensure_kernel(&inkling_epilogue_kernel, inkling_epilogue_module, "goose_i2_moe_epilogue");
    }
    if status == 0 && inkling_epilogue_states[slot].buffer.address == 0 {
        status = reserve(&inkling_epilogue_states[slot].buffer, 7 * INKLING_EPILOGUE_ROW_BYTES);
        if status == 0 {
            status = cuda_capture_upload(inkling_epilogue_states[slot].buffer.address,
                unsafe_c_ptr<c_ptr<const void>>(cuda_slice_address(weights)), weights.len as! u64);
        }
    }
    let coefficient = inkling_epilogue_states[slot].buffer.address;
    let history = coefficient + 4 * INKLING_EPILOGUE_ROW_BYTES;
    if status == 0 && position == 0 { status = cuda_capture_zero(history, 0, INKLING_EPILOGUE_HISTORY_BYTES); }

    let host = cuda_slice_address(output);
    var source: u64 = 0;
    var res: u64 = 0;
    var dest: u64 = 0;
    let retain = tls[0].i2_arena_probe.active && host != 0 && host == tls[0].inkling_moe_resident_host &&
        output.len == 36 * MIB_BYTES;
    if status == 0 && retain {
        let split = activation_usable_bytes();
        if cuda.activations.bytes - split < 6 * (output.len as! u64) { status = -1; }
        else {
            let slot_address = tls[0].inkling_moe_output_slot;
            dest = if slot_address != 0 { slot_address } else {
                cuda.activations.address + split + 3 * (output.len as! u64)
            };
        }
    } else if status == 0 {
        status = activation_output_for_input(host, output.len, &dest, input, fg_cuda_host_view_u64(0, 0));
    }
    if status == 0 { status = activation_input(input, &cuda.input, &source); }
    if status == 0 { status = activation_input(residual, &cuda.weight, &res); }
    if status == 0 {
        var args: u64[6] = [cuda_kernel_argument_address(source), cuda_kernel_argument_address(res),
            cuda_kernel_argument_address(coefficient), cuda_kernel_argument_address(history),
            cuda_kernel_argument_address(dest), cuda_kernel_argument_address(rows)];
        status = fg_cuda_driver_launch(inkling_epilogue_kernel, (INKLING_EPILOGUE_WIDTH / 128) as! u32,
                                       1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
    }
    if status == 0 && !retain { status = activation_publish(host, output.len, dest, false); }
    if status == 0 {
        tls[0].inkling_moe_current = if retain { dest } else { 0 };
        inkling_epilogue_states[slot].length = position + rows;
    }
    let popped = finish_context();
    if status == 0 && popped == 0 { return 1; }
    fg_cuda_host_diagnostic_123(slot, position, status, popped);
    return -1;
}

export fn inkling_draft_moe_snapshot(restore: bool, position: i32) -> bool {
    if start_context() != 1 { return false; }
    var status = fg_cuda_driver_sync();
    if status == 0 && !restore { status = reserve(&inkling_epilogue_backup, INKLING_EPILOGUE_BACKUP_BYTES); }
    if status == 0 && inkling_epilogue_backup.bytes < INKLING_EPILOGUE_BACKUP_BYTES { status = -1; }
    for i in INKLING_EPILOGUE_VISITS {
        if status != 0 { break; }
        let state = inkling_epilogue_states[i];
        if state.buffer.address == 0 || (!restore && state.length != position) ||
            (restore && state.length < position) {
            status = -1;
            break;
        }
        let history = state.buffer.address + 4 * INKLING_EPILOGUE_ROW_BYTES;
        let backup = inkling_epilogue_backup.address + (i as! u64) * INKLING_EPILOGUE_HISTORY_BYTES;
        status = if restore { cuda_capture_copy_device(history, backup, INKLING_EPILOGUE_HISTORY_BYTES) }
            else { cuda_capture_copy_device(backup, history, INKLING_EPILOGUE_HISTORY_BYTES) };
        if status == 0 && restore { inkling_epilogue_states[i].length = position; }
    }
    let popped = finish_context();
    if status != 0 || popped != 0 { return false; }
    fg_cuda_host_diagnostic_124(if restore { 1 } else { 0 }, INKLING_EPILOGUE_BACKUP_BYTES as! i32);
    return true;
}

extern fn fg_cuda_image_i2_parallel_conv_image() -> u64;
extern fn fg_cuda_host_diagnostic_125() -> i32;

const INKLING_CONV_ROW_BYTES: u64 = 2304 * 4;
const INKLING_CONV_HISTORY_BYTES: u64 = 3 * INKLING_CONV_ROW_BYTES;

struct InklingNativeKvSlot {
    cache: device_buffer,
    prepare_state: device_buffer,
    length: i32,
    rows: i32,
    sliding: bool,
    format: i32,
}

struct InklingNativeKvState {
    module: u64,
    pack: u64, attention: u64, attention_decode: u64, attention_prefill: u64, merge: u64,
    slots: InklingNativeKvSlot[INKLING_EPILOGUE_VISITS],
    window: device_buffer, input: device_buffer, staging: device_buffer,
    partials: device_buffer, projection: device_buffer, raw: device_buffer,
    projected_input: device_buffer, prepared_kv: device_buffer,
    calls: u64, queries: u64, uploads: u64,
}
var inkling_native_kv: InklingNativeKvState;

struct InklingParallelConv {
    module: u64,
    apply: u64,
    commit: u64,
    state: device_buffer,
    reported: bool,
}
var inkling_parallel_conv: InklingParallelConv;

export fn inkling_loop_storage() -> u64 {
    return if inkling_native_kv.projected_input.bytes >= 3 * (i2_loop_plane as! u64) {
        inkling_native_kv.projected_input.address
    } else { 0 };
}

export fn inkling_prefill_full_scratch(bytes: u64) -> u64 {
    return if inkling_native_kv.raw.bytes >= bytes { inkling_native_kv.raw.address } else { 0 };
}

export fn fg_cuda_i2_prefill_conv(weights: u64, history: u64) {
    var tls = cuda_thread_state();
    tls[0].inkling_conv_weights = weights;
    tls[0].inkling_conv_history = history;
}

fn inkling_native_upload(destination: u64, source: u64, bytes: u64) -> i32 {
    let status = fg_cuda_driver_upload(destination, source, bytes);
    if status == 0 { inkling_native_kv.uploads += bytes; }
    return status;
}

fn inkling_native_parallel_conv(source: u64, output: u64, rows: i32) -> i32 {
    let tls = cuda_thread_state();
    var status = cuda_ensure_module(&inkling_parallel_conv.module, fg_cuda_image_i2_parallel_conv_image());
    let module = inkling_parallel_conv.module;
    if status == 0 { status = cuda_ensure_kernel(&inkling_parallel_conv.apply, module, "i2_postconv_parallel"); }
    if status == 0 { status = cuda_ensure_kernel(&inkling_parallel_conv.commit, module, "i2_postconv_commit"); }
    if status == 0 { status = reserve(&inkling_parallel_conv.state, 7 * INKLING_CONV_ROW_BYTES); }
    if status != 0 { return status; }

    let weights = inkling_parallel_conv.state.address;
    let history = weights + 4 * INKLING_CONV_ROW_BYTES;
    status = inkling_native_upload(weights, tls[0].inkling_conv_weights, 4 * INKLING_CONV_ROW_BYTES);
    if status == 0 { status = inkling_native_upload(history, tls[0].inkling_conv_history, INKLING_CONV_HISTORY_BYTES); }
    if status == 0 {
        var args: u64[5] = [cuda_kernel_argument_address(source), cuda_kernel_argument_address(weights),
            cuda_kernel_argument_address(history), cuda_kernel_argument_address(output), cuda_kernel_argument_address(rows)];
        status = fg_cuda_driver_launch(inkling_parallel_conv.apply, 18, rows as! u32, 1, 128, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(args), 0);
    }
    if status == 0 {
        var save: u64[3] = [cuda_kernel_argument_address(source), cuda_kernel_argument_address(history),
                            cuda_kernel_argument_address(rows)];
        status = fg_cuda_driver_launch(inkling_parallel_conv.commit, 18, 1, 1, 128, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(save), 0);
    }
    if status == 0 {
        status = fg_cuda_driver_download(tls[0].inkling_conv_history, history, INKLING_CONV_HISTORY_BYTES);
    }
    if status == 0 && !inkling_parallel_conv.reported {
        fg_cuda_host_diagnostic_125();
        inkling_parallel_conv.reported = true;
    }
    return status;
}

extern fn fg_cuda_image_flash_native_cubin() -> u64;

const INKLING_NATIVE_KV_WIDTH: u64 = 1024;
const INKLING_NATIVE_KV_Q4: i32 = 1;
const INKLING_NATIVE_PREPARE_BYTES: u64 = 6 * INKLING_NATIVE_KV_WIDTH * 4;

struct InklingNativeRows {
    keys: u64,
    values: u64,
    positions: u64,
}

var inkling_native_backup: device_buffer;

fn inkling_native_row_bytes(format: i32) -> u64 {
    return if format == INKLING_NATIVE_KV_Q4 { INKLING_NATIVE_KV_WIDTH / 32 * 18 }
        else { INKLING_NATIVE_KV_WIDTH * 2 };
}

fn inkling_native_rows(base: u64, capacity: i32, row_bytes: u64) -> InklingNativeRows {
    let plane = (capacity as! u64) * row_bytes;
    return InklingNativeRows { keys: base, values: base + plane, positions: base + 2 * plane };
}

fn inkling_native_cache_bytes(rows: i32, row_bytes: u64) -> u64 {
    return (rows as! u64) * (2 * row_bytes + 8);
}

fn inkling_native_load() -> i32 {
    if inkling_native_kv.merge != 0 { return 0; }
    var status = cuda_ensure_module(&inkling_native_kv.module, fg_cuda_image_flash_native_cubin());
    let module = inkling_native_kv.module;
    if status == 0 { status = cuda_ensure_kernel(&inkling_native_kv.pack, module, "goose_i2_pack_f16"); }
    if status == 0 { status = cuda_ensure_kernel(&inkling_native_kv.attention, module, "goose_i2_flash_mma"); }
    if status == 0 { status = cuda_ensure_kernel(&inkling_native_kv.merge, module, "goose_i2_flash_mma_merge"); }
    return status;
}

fn inkling_native_copy(dst: InklingNativeRows, first_dst: i32, src: InklingNativeRows,
                       first_src: i32, count: i32, row_bytes: u64) -> i32 {
    if count == 0 { return 0; }
    let from = first_src as! u64;
    let to = first_dst as! u64;
    let rows = count as! u64;
    let bytes = rows * row_bytes;
    var status = fg_cuda_driver_copy_device(dst.keys + to * row_bytes, src.keys + from * row_bytes, bytes);
    if status == 0 {
        status = fg_cuda_driver_copy_device(dst.values + to * row_bytes, src.values + from * row_bytes, bytes);
    }
    if status == 0 {
        status = fg_cuda_driver_copy_device(dst.positions + to * 8, src.positions + from * 8, rows * 8);
    }
    return status;
}

fn inkling_native_pack_device(dst: u64, src: u64, rows: i32, format: i32) -> i32 {
    if format != INKLING_NATIVE_KV_Q4 {
        let elements = rows * (INKLING_NATIVE_KV_WIDTH as! i32);
        var args: u64[3] = [cuda_kernel_argument_address(src), cuda_kernel_argument_address(dst),
                           cuda_kernel_argument_address(elements)];
        return fg_cuda_driver_launch(inkling_native_kv.pack, ((elements + 255) as! u32) / 256,
                                     1, 1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
    }
    let stride = INKLING_NATIVE_KV_WIDTH * 4;
    let blocks = (INKLING_NATIVE_KV_WIDTH / 32) as! u32;
    let tasks = (rows as! u64) * (blocks as! u64);
    var args: u64[5] = [cuda_kernel_argument_address(src), cuda_kernel_argument_address(dst),
        cuda_kernel_argument_address(stride), cuda_kernel_argument_address(blocks), cuda_kernel_argument_address(tasks)];
    return fg_cuda_driver_launch(cuda.q4_rows, ((tasks + 127) / 128) as! u32,
                                 1, 1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
}

fn inkling_native_pack_host(dst: u64, src: u64, rows: i32, format: i32) -> i32 {
    let bytes = (rows as! u64) * INKLING_NATIVE_KV_WIDTH * 4;
    var status = reserve(&inkling_native_kv.staging, bytes);
    if status == 0 { status = inkling_native_upload(inkling_native_kv.staging.address, src, bytes); }
    if status != 0 { return status; }
    return inkling_native_pack_device(dst, inkling_native_kv.staging.address, rows, format);
}

export fn inkling_draft_native_snapshot(restore: bool, position: i32) -> bool {
    if start_context() != 1 { return false; }
    var status = fg_cuda_driver_sync();
    var size: u64 = 0;
    for i in INKLING_EPILOGUE_VISITS {
        if status != 0 { break; }
        let slot = inkling_native_kv.slots[i];
        if slot.cache.address == 0 || slot.prepare_state.bytes < INKLING_NATIVE_PREPARE_BYTES ||
            (!restore && slot.length != position) || (restore && slot.length < position) {
            status = -1;
        } else {
            size += INKLING_NATIVE_PREPARE_BYTES;
            if slot.sliding { size += inkling_native_cache_bytes(slot.rows, inkling_native_row_bytes(slot.format)); }
        }
    }
    if status == 0 && !restore { status = reserve(&inkling_native_backup, size); }
    if status == 0 && inkling_native_backup.bytes < size { status = -1; }
    var backup = inkling_native_backup.address;
    for i in INKLING_EPILOGUE_VISITS {
        if status != 0 { break; }
        let slot = inkling_native_kv.slots[i];
        let past = slot.prepare_state.address + slot.prepare_state.bytes - INKLING_NATIVE_PREPARE_BYTES;
        status = if restore { fg_cuda_driver_copy_device(past, backup, INKLING_NATIVE_PREPARE_BYTES) }
            else { fg_cuda_driver_copy_device(backup, past, INKLING_NATIVE_PREPARE_BYTES) };
        backup += INKLING_NATIVE_PREPARE_BYTES;
        if status == 0 && slot.sliding {
            let bytes = inkling_native_cache_bytes(slot.rows, inkling_native_row_bytes(slot.format));
            status = if restore { fg_cuda_driver_copy_device(slot.cache.address, backup, bytes) }
                else { fg_cuda_driver_copy_device(backup, slot.cache.address, bytes) };
            backup += bytes;
        }
        if status == 0 && restore { inkling_native_kv.slots[i].length = position; }
    }
    return finish_context() == 0 && status == 0;
}

export fn inkling_attention_reset() -> i32 {
    if inkling_native_kv.module == 0 { return 0; }
    if start_context() != 1 { return -1; }
    let status = fg_cuda_driver_sync();
    if status == 0 {
        for i in INKLING_EPILOGUE_VISITS { inkling_native_kv.slots[i].length = 0; }
    }
    return if finish_context() == 0 && status == 0 { 0 } else { -1 };
}

extern fn fg_cuda_image_flash_tiles_cubin() -> u64;
extern fn fg_cuda_host_diagnostic_126() -> i32;

var inkling_flash_tiles_module: u64;
var inkling_flash_tiles_kernel: u64;
var inkling_flash_tiles_mode: i32 = -1;

fn inkling_native_compute(cache: InklingNativeRows, history: i32, total: i32, count: i32,
        sliding: bool, format: i32, query: u64, relative: u64, sink: u64, query_pos: u64,
        bias: u64, output: u64) -> i32 {
    const key_part: i64 = 2048;
    const heads: u32 = 24;
    let tls = cuda_thread_state();
    let window: i64 = if sliding { 1024 } else { 0 };
    let row_bytes = inkling_native_row_bytes(format);
    let scale: f32 = 0.0883883461356163 as! f32;
    let exact_tail = count > 1 && count <= 16 && tls[0].i2_exact_verify;
    let chunk: i32 = if exact_tail { 1 } else { 128 };
    var first: i32 = 0;
    while first < count {
        let remaining = count - first;
        let queries = (if remaining < chunk { remaining } else { chunk }) as! i64;
        let prior = (history as! i64) + (first as! i64);
        let last = prior + queries;
        if last > (total as! i64) { return -1; }
        let lower = if sliding && prior >= window { prior - window + 1 } else { 0 };
        let keys = last - lower;
        let parts = (keys + key_part - 1) / key_part;
        let slots = (queries as! u64) * (heads as! u64) * (parts as! u64);
        var status = reserve(&inkling_native_kv.partials, slots * 130 * 4);
        if status != 0 { return status; }

        let partial = inkling_native_kv.partials.address;
        let meta = partial + slots * 128 * 4;
        let row = first as! u64;
        let key_row = lower as! u64;
        let q = query + row * 3072 * 2;
        let k = cache.keys + key_row * row_bytes;
        let v = cache.values + key_row * row_bytes;
        let r = relative + row * 384 * 4;
        let s = sink + row * 24 * 4;
        let kp = cache.positions + key_row * 8;
        let qp = query_pos + row * 8;
        let out = output + row * 3072 * 4;
        var q4: i32 = if format == INKLING_NATIVE_KV_Q4 { 1 } else { 0 };
        if queries == 1 && (count == 1 || exact_tail) && cuda_option("I2_FLASH_PARALLEL_SCORES") { q4 |= 2; }
        var args: u64[16] = [
            cuda_kernel_argument_address(q), cuda_kernel_argument_address(k), cuda_kernel_argument_address(v),
            cuda_kernel_argument_address(r), cuda_kernel_argument_address(bias), cuda_kernel_argument_address(s),
            cuda_kernel_argument_address(kp), cuda_kernel_argument_address(qp), cuda_kernel_argument_address(partial),
            cuda_kernel_argument_address(meta), cuda_kernel_argument_address(keys), cuda_kernel_argument_address(queries),
            cuda_kernel_argument_address(parts), cuda_kernel_argument_address(window),
            cuda_kernel_argument_address(scale), cuda_kernel_argument_address(q4),
        ];
        var merge: u64[4] = [cuda_kernel_argument_address(partial), cuda_kernel_argument_address(meta),
            cuda_kernel_argument_address(out), cuda_kernel_argument_address(parts)];
        var kernel = inkling_native_kv.attention;
        let module = inkling_native_kv.module;
        if (q4 & 3) == 3 && cuda_option("I2_DECODE_WARP_SOFTMAX") {
            status = cuda_ensure_kernel(&inkling_native_kv.attention_decode, module, "goose_i2_flash_mma_decode");
            kernel = inkling_native_kv.attention_decode;
        } else if queries > 1 && q4 == 1 && cuda_option("I2_PREFILL_PARALLEL_RELATIVE") {
            status = cuda_ensure_kernel(&inkling_native_kv.attention_prefill, module, "goose_i2_flash_mma_prefill");
            kernel = inkling_native_kv.attention_prefill;
            if status == 0 {
                if inkling_flash_tiles_mode < 0 {
                    inkling_flash_tiles_mode = if cuda_env_equal(cuda_env("I2_FLASH_TILE_SOFTMAX"), "0") { 0 } else { 1 };
                }
                if inkling_flash_tiles_mode != 0 && !sliding {
                    status = cuda_ensure_module(&inkling_flash_tiles_module, fg_cuda_image_flash_tiles_cubin());
                    if status == 0 && inkling_flash_tiles_kernel == 0 {
                        status = cuda_ensure_kernel(&inkling_flash_tiles_kernel, inkling_flash_tiles_module,
                                                     "i2_flash_prefill_all_softmax");
                        if status == 0 { fg_cuda_host_diagnostic_126(); }
                    }
                    kernel = inkling_flash_tiles_kernel;
                }
            }
        }
        if status != 0 { return status; }
        status = fg_cuda_driver_launch(kernel, ((queries + 15) / 16) as! u32, heads, parts as! u32,
                                       128, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
        if status == 0 {
            status = fg_cuda_driver_launch(inkling_native_kv.merge, (queries as! u32) * heads, 1, 1,
                                           128, 1, 1, 0, 0, fg_cuda_driver_words_address(merge), 0);
        }
        if status != 0 { return status; }
        first += chunk;
    }
    return 0;
}

extern fn fg_cuda_host_diagnostic_127(status: i32, popped: i32, slot: i32, first: i32) -> i32;

const INKLING_NATIVE_PREFILL_LIMIT: i64 = 4096;
const INKLING_NATIVE_WINDOW: i32 = 1024;
const INKLING_NATIVE_BIAS_BYTES: u64 = 16384 * 4;

struct InklingNativeBatch {
    slot: i32, capacity: i32, first: i32, count: i32,
    sliding: bool, format: i32,
    query: u64, key: u64, value: u64, relative: u64, bias: u64, sinks: u64, output: u64,
}

fn inkling_native_batch(address: u64) -> InklingNativeBatch {
    let geometry = fg_cuda_host_view_i32(address, 6);
    let pointers = fg_cuda_host_view_u64(address + 24, 7);
    return InklingNativeBatch {
        slot: geometry[0], capacity: geometry[1], first: geometry[2], count: geometry[3],
        sliding: (geometry[4] & 255) != 0, format: geometry[5],
        query: pointers[0], key: pointers[1], value: pointers[2], relative: pointers[3],
        bias: pointers[4], sinks: pointers[5], output: pointers[6],
    };
}

fn inkling_native_batch_valid(batch: const InklingNativeBatch&) -> bool {
    return batch.slot >= 0 && (batch.slot as! i64) < INKLING_EPILOGUE_VISITS &&
        batch.capacity >= 1 && batch.capacity <= 1048576 && batch.first >= 0 &&
        batch.count >= 1 && (batch.count as! i64) <= INKLING_NATIVE_PREFILL_LIMIT &&
        batch.first <= batch.capacity && batch.count <= batch.capacity - batch.first &&
        (batch.format == 0 || batch.format == INKLING_NATIVE_KV_Q4) &&
        batch.bias != 0 && batch.output != 0;
}

fn inkling_native_pack_query(destination: u64, source: u64, count: i32) -> i32 {
    let elements = count * 3072;
    let bytes = (elements as! u64) * 4;
    var status = reserve(&inkling_native_kv.staging, bytes);
    if status == 0 { status = inkling_native_upload(inkling_native_kv.staging.address, source, bytes); }
    if status != 0 { return status; }
    var args: u64[3] = [cuda_kernel_argument_address(inkling_native_kv.staging.address),
        cuda_kernel_argument_address(destination), cuda_kernel_argument_address(elements)];
    return fg_cuda_driver_launch(inkling_native_kv.pack, ((elements + 255) as! u32) / 256,
                                 1, 1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
}

extern fn fg_cuda_image_attention_gate_cubin() -> u64;

var inkling_projection_gate_module: u64;
var inkling_projection_gate_kernel: u64;

fn inkling_native_project_q8(input: u64, output: u64, weights: u64, width: i64, rows: i64, count: i32) -> i32 {
    let stride = width / 32 * 34;
    let count64 = count as! i64;
    var jobs: i64[2 * INKLING_NATIVE_PREFILL_LIMIT];
    for i in count64 {
        jobs[2 * i] = i * width * 4;
        jobs[2 * i + 1] = i * rows * 4;
    }
    var tls = cuda_thread_state();
    let previous_input = tls[0].matrix_input;
    let previous_output = tls[0].matrix_output;
    tls[0].matrix_input = input;
    tls[0].matrix_output = output;
    let work = fg_cuda_host_view_i64(cuda_slice_address(jobs), 2 * count64);
    let status = fg_cuda_plane(weights, rows * stride, stride, width, 8, rows, work);
    tls[0].matrix_input = previous_input;
    tls[0].matrix_output = previous_output;
    return status as! i32;
}

fn inkling_native_project_output(batch: const InklingNativeBatch&, projection: u64, output: u64, device_gate: u64, graph: bool) -> i32 {
    let parameters = fg_cuda_host_view_u64(projection, 3);
    let gate = parameters[0];
    let weights = parameters[1];
    let rows = fg_cuda_host_view_i32(projection + 24, 1)[0] as! i64;
    let elements = (batch.count as! u32) * 3072;
    let gate_bytes = (elements as! u64) * 4;
    let target_bytes = (batch.count as! u64) * (rows as! u64) * 4;
    var status = cuda_ensure_module(&inkling_projection_gate_module, fg_cuda_image_attention_gate_cubin());
    if status == 0 {
        status = cuda_ensure_kernel(&inkling_projection_gate_kernel, inkling_projection_gate_module,
                                     "goose_i2_attention_gate");
    }
    var staged_gate = device_gate;
    if status == 0 && staged_gate == 0 {
        status = reserve(&inkling_native_kv.staging, gate_bytes);
        staged_gate = inkling_native_kv.staging.address;
        if status == 0 { status = inkling_native_upload(staged_gate, gate, gate_bytes); }
    }
    var tls = cuda_thread_state();
    let convolve = graph && batch.count > 1 && tls[0].inkling_conv_weights != 0 && tls[0].inkling_conv_history != 0;
    let retain = graph && batch.output == tls[0].i2_residual_host && target_bytes == 36 * (MIB_BYTES as! u64) &&
        tls[0].inkling_conv_weights != 0 && tls[0].inkling_conv_history != 0;
    var destination: u64 = 0;
    if status == 0 && retain {
        if cuda.activations.address == 0 { status = reserve(&cuda.activations, cuda.activation_capacity); }
        let split = activation_usable_bytes();
        if status != 0 || cuda.activations.bytes - split < 6 * target_bytes { return -1; }
        destination = cuda.activations.address + split + 4 * target_bytes;
    } else if status == 0 && graph {
        status = activation_output(batch.output, target_bytes as! i64, &destination);
    } else if status == 0 {
        status = reserve(&inkling_native_kv.projection, target_bytes);
        destination = inkling_native_kv.projection.address;
    }
    if status == 0 {
        var args: u64[3] = [cuda_kernel_argument_address(output), cuda_kernel_argument_address(staged_gate),
                           cuda_kernel_argument_address(elements)];
        status = fg_cuda_driver_launch(inkling_projection_gate_kernel, (elements + 255) / 256,
                                       1, 1, 256, 1, 1, 0, 0, fg_cuda_driver_words_address(args), 0);
    }
    let projected = if convolve { inkling_native_kv.raw.address } else { destination };
    if convolve && inkling_native_kv.raw.bytes < target_bytes { return -1; }
    if status == 0 { status = inkling_native_project_q8(output, projected, weights, 3072, rows, batch.count); }
    if status == 0 && convolve { status = inkling_native_parallel_conv(projected, destination, batch.count); }
    if status == 0 && retain {
        if inkling_native_kv.raw.bytes < 2 * target_bytes { return -1; }
        tls[0].i2_residual_device = destination;
        tls[0].i2_attention_scratch = inkling_native_kv.raw.address;
    } else if status == 0 && graph {
        status = activation_publish(batch.output, target_bytes as! i64, destination, false);
    } else if status == 0 {
        status = fg_cuda_driver_download(batch.output, destination, target_bytes);
    }
    return status;
}

fn inkling_native_execute(address: u64, projection: u64, inputs: u64, graph: bool) -> i32 {
    if address == 0 { return -1; }
    let batch = inkling_native_batch(address);
    if !inkling_native_batch_valid(batch) { return -1; }
    if inputs == 0 && (batch.query == 0 || batch.key == 0 || batch.value == 0 ||
                      batch.relative == 0 || batch.sinks == 0) { return -1; }
    let sources = fg_cuda_host_view_u64(inputs, if inputs == 0 { 0 } else { 15 });
    if inputs != 0 {
        let weight_count: i64 = if batch.sliding { 6 } else { 8 };
        for i in 3 + weight_count { if sources[i] == 0 { return -1; } }
        for i in 11 .. 15 { if sources[i] == 0 { return -1; } }
    }
    if projection != 0 {
        let parameters = fg_cuda_host_view_u64(projection, 3);
        let rows = fg_cuda_host_view_i32(projection + 24, 1)[0];
        if (inputs == 0 && parameters[0] == 0) || parameters[1] == 0 || rows < 1 || rows > 8192 ||
            parameters[2] != (rows as! u64) * 3264 { return -1; }
    }
    if start_context() != 1 { return -1; }

    let slot = batch.slot;
    let capacity = if batch.sliding && batch.capacity > INKLING_NATIVE_WINDOW { INKLING_NATIVE_WINDOW } else { batch.capacity };
    let row_bytes = inkling_native_row_bytes(batch.format);
    let cache_bytes = inkling_native_cache_bytes(capacity, row_bytes);
    var status = decode_capture_submit();
    if status == 0 { status = inkling_native_load(); }
    let previous = inkling_native_kv.slots[slot];
    if status == 0 && (previous.length != batch.first ||
        (previous.rows != 0 && (previous.rows != capacity || previous.sliding != batch.sliding ||
                                previous.format != batch.format))) { status = -1; }
    if status == 0 && previous.rows == 0 {
        status = reserve(&inkling_native_kv.slots[slot].cache, cache_bytes + INKLING_NATIVE_BIAS_BYTES);
        if status == 0 {
            inkling_native_kv.slots[slot].rows = capacity;
            inkling_native_kv.slots[slot].sliding = batch.sliding;
            inkling_native_kv.slots[slot].format = batch.format;
            status = inkling_native_upload(inkling_native_kv.slots[slot].cache.address + cache_bytes,
                                          batch.bias, INKLING_NATIVE_BIAS_BYTES);
        }
    }
    let cache_address = inkling_native_kv.slots[slot].cache.address;
    let resident = inkling_native_rows(cache_address, capacity, row_bytes);
    var rows = resident;
    let history = if batch.sliding && batch.first > capacity { capacity } else { batch.first };
    let total = history + batch.count;
    if status == 0 && batch.sliding {
        status = reserve(&inkling_native_kv.window, inkling_native_cache_bytes(total, row_bytes));
        rows = inkling_native_rows(inkling_native_kv.window.address, total, row_bytes);
        if status == 0 { status = inkling_native_copy(rows, 0, resident, 0, history, row_bytes); }
    }

    let count = batch.count as! u64;
    let query_pos = rows.positions + (history as! u64) * 8;
    if status == 0 {
        var positions: i64[INKLING_NATIVE_PREFILL_LIMIT];
        for i in batch.count as! i64 { positions[i] = (batch.first as! i64) + i; }
        status = inkling_native_upload(query_pos, cuda_slice_address(positions), count * 8);
    }
    let q_bytes = count * 3072 * 2;
    let relative_bytes = count * 384 * 4;
    let sink_bytes = count * 24 * 4;
    let output_bytes = count * 3072 * 4;
    if status == 0 { status = reserve(&inkling_native_kv.input, q_bytes + relative_bytes + sink_bytes + output_bytes); }
    let query = inkling_native_kv.input.address;
    let relative = query + q_bytes;
    var sink = relative + relative_bytes;
    let output = sink + sink_bytes;
    var gate: u64 = 0;
    let key_dst = rows.keys + (history as! u64) * row_bytes;
    let value_dst = rows.values + (history as! u64) * row_bytes;
    if status == 0 && inputs != 0 {
        status = inkling_native_prepare_device(batch, sources, query, relative, &sink, &gate, key_dst, value_dst, graph);
    } else if status == 0 {
        if status == 0 { status = inkling_native_pack_host(key_dst, batch.key, batch.count, batch.format); }
        if status == 0 { status = inkling_native_pack_host(value_dst, batch.value, batch.count, batch.format); }
        if status == 0 { status = inkling_native_pack_query(query, batch.query, batch.count); }
        if status == 0 { status = inkling_native_upload(relative, batch.relative, relative_bytes); }
        if status == 0 { status = inkling_native_upload(sink, batch.sinks, sink_bytes); }
    }
    if status == 0 {
        status = inkling_native_compute(rows, history, total, batch.count, batch.sliding, batch.format,
            query, relative, sink, query_pos, cache_address + cache_bytes, output);
    }
    if status == 0 {
        status = if projection != 0 { inkling_native_project_output(batch, projection, output, gate, graph) }
            else { fg_cuda_driver_download(batch.output, output, output_bytes) };
    }
    if status == 0 && batch.sliding {
        let keep = if total < capacity { total } else { capacity };
        status = inkling_native_copy(resident, 0, rows, total - keep, keep, row_bytes);
    }
    if status == 0 {
        inkling_native_kv.slots[slot].length = batch.first + batch.count;
        inkling_native_kv.calls += 1;
        inkling_native_kv.queries += count;
    }
    let popped = finish_context();
    if status == 0 && popped == 0 { return 0; }
    fg_cuda_host_diagnostic_127(status, popped, slot, batch.first);
    return -1;
}


export fn inkling_attention_execute(address: u64) -> i32 {
    return inkling_native_execute(address, 0, 0, false);
}

export fn inkling_attention_execute_project(address: u64, projection: u64) -> i32 {
    if projection == 0 { return -1; }
    return inkling_native_execute(address, projection, 0, false);
}

extern fn fg_cuda_image_attention_input_cubin() -> u64;
extern fn fg_cuda_image_i2_decode_q8_image() -> u64;
var inkling_attention_input_module: u64;
var inkling_attention_prepare_kernel: u64;
var inkling_attention_update_kernel: u64;
var inkling_decode_projection_module: u64;
var inkling_decode_projection_kernel: u64;

fn inkling_native_decode_projections(input: const u64[:], output: u64,
        sources: const u64[:], widths: const i64[:], sliding: bool) -> i32 {
    var weights: u64[8];
    let count: i64 = if sliding { 6 } else { 8 };
    for i in count {
        if !dense_weight(sources[i], widths[i] * 2448, &weights[i]) { return 0; }
    }
    var status = cuda_ensure_module(&inkling_decode_projection_module, fg_cuda_image_i2_decode_q8_image());
    if status == 0 {
        status = cuda_ensure_kernel(&inkling_decode_projection_kernel, inkling_decode_projection_module,
                                     "goose_i2_matrix_q8_attention_projections");
    }
    if status != 0 { return -1; }
    var args: u64[12] = [
        cuda_kernel_argument_address(input[0]), cuda_kernel_argument_address(input[1]),
        cuda_kernel_argument_address(input[2]), cuda_kernel_argument_address(output),
        cuda_kernel_argument_address(weights[0]), cuda_kernel_argument_address(weights[1]),
        cuda_kernel_argument_address(weights[2]), cuda_kernel_argument_address(weights[3]),
        cuda_kernel_argument_address(weights[4]), cuda_kernel_argument_address(weights[5]),
        cuda_kernel_argument_address(weights[6]), cuda_kernel_argument_address(weights[7]),
    ];
    let blocks: u32 = if sliding { 2150 } else { 2166 };
    status = fg_cuda_driver_launch(inkling_decode_projection_kernel, blocks, 1, 1, 128, 1, 1, 0, 0,
                                   fg_cuda_driver_words_address(args), 0);
    if status != 0 { return -1; }
    var tls = cuda_thread_state();
    tls[0].cuda_matrix_launches += 1;
    return 1;
}

fn inkling_native_prepare_device(batch: const InklingNativeBatch&, sources: const u64[:],
        query: u64, relative: u64, sink: u64&, gate: u64&, key_dst: u64, value_dst: u64, graph: bool) -> i32 {
    let count = batch.count;
    let rows = count as! u64;
    let row_input = rows * 2304 * 4;
    let row_kv = rows * INKLING_NATIVE_KV_WIDTH * 4;
    let coefficients: u64 = (3072 + 384 + 8192) * 4;
    let slot = batch.slot;
    var status = cuda_ensure_module(&inkling_attention_input_module, fg_cuda_image_attention_input_cubin());
    let module = inkling_attention_input_module;
    if status == 0 {
        status = cuda_ensure_kernel(&inkling_attention_prepare_kernel, module, "goose_i2_prepare_attention");
    }
    if status == 0 {
        status = cuda_ensure_kernel(&inkling_attention_update_kernel, module, "goose_i2_update_history");
    }
    if status == 0 { status = reserve(&inkling_native_kv.raw, rows * 8664 * 4); }
    if status == 0 { status = reserve(&inkling_native_kv.projected_input, 3 * row_input); }
    if status == 0 { status = reserve(&inkling_native_kv.prepared_kv, 2 * row_kv); }
    if status == 0 && inkling_native_kv.slots[slot].prepare_state.address == 0 {
        status = reserve(&inkling_native_kv.slots[slot].prepare_state, coefficients + INKLING_NATIVE_PREPARE_BYTES);
        let base = inkling_native_kv.slots[slot].prepare_state.address;
        let sizes: u64[4] = [3072 * 4, 384 * 4, 4096 * 4, 4096 * 4];
        var next = base;
        for i in sizes.len {
            if status != 0 { break; }
            status = inkling_native_upload(next, sources[11 + i], sizes[i]);
            next += sizes[i];
        }
    }
    if status != 0 { return status; }
    let state = inkling_native_kv.slots[slot].prepare_state.address;
    let past = state + coefficients;
    if batch.first == 0 { status = fg_cuda_driver_zero(past, 0, INKLING_NATIVE_PREPARE_BYTES); }
    let tls = cuda_thread_state();
    let staging = inkling_native_kv.projected_input.address;
    var input: u64[3];
    for i in input.len {
        if status != 0 { return status; }
        let host = sources[i];
        input[i] = if graph { activation_device(host, row_input as! i64) } else { 0 };
        if input[i] == 0 {
            if staging == tls[0].i2_device_loop.storage || staging == tls[0].inkling_moe_current { return -1; }
            if graph { status = activation_sync_range(host, row_input as! i64, false); }
            input[i] = staging + (i as! u64) * row_input;
            if status == 0 { status = inkling_native_upload(input[i], host, row_input); }
        }
    }
    if status != 0 { return status; }
    let raw = inkling_native_kv.raw.address;
    let widths: i64[8] = [3072, 1024, 1024, 384, 24, 3072, 32, 32];
    let source_indices: i64[8] = [0, 1, 2, 0, 0, 0, 1, 2];
    let weights = fg_cuda_host_view_u64(cuda_slice_address(sources) + 24, 8);
    var fused: i32 = 0;
    if graph && count == 1 && i2_matrix_enabled() && !cuda_env_equal(cuda_env("I2_DECODE_Q8_PROJECTION_PACK"), "0") {
        fused = inkling_native_decode_projections(input, raw, weights, widths, batch.sliding);
        if fused < 0 { return -1; }
    }
    if fused == 0 {
        let projections: i64 = if batch.sliding { 6 } else { 8 };
        var next = raw;
        for i in projections {
            status = inkling_native_project_q8(input[source_indices[i]], next, weights[i], 2304, widths[i], count);
            if status != 0 { return status; }
            next += rows * (widths[i] as! u64) * 4;
        }
    }
    let key = inkling_native_kv.prepared_kv.address;
    let value = key + row_kv;
    let sliding: i32 = if batch.sliding { 1 } else { 0 };
    var prepare: u64[10] = [
        cuda_kernel_argument_address(raw), cuda_kernel_argument_address(state), cuda_kernel_argument_address(past),
        cuda_kernel_argument_address(query), cuda_kernel_argument_address(relative), cuda_kernel_argument_address(key),
        cuda_kernel_argument_address(value), cuda_kernel_argument_address(count), cuda_kernel_argument_address(batch.first),
        cuda_kernel_argument_address(sliding),
    ];
    status = fg_cuda_driver_launch(inkling_attention_prepare_kernel, 64, count as! u32,
                                   1, 128, 1, 1, 0, 0, fg_cuda_driver_words_address(prepare), 0);
    if status == 0 {
        var update: u64[3] = [cuda_kernel_argument_address(raw), cuda_kernel_argument_address(past),
                             cuda_kernel_argument_address(count)];
        status = fg_cuda_driver_launch(inkling_attention_update_kernel, 16, 1, 1, 128, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(update), 0);
    }
    if status == 0 { status = inkling_native_pack_device(key_dst, key, count, batch.format); }
    if status == 0 { status = inkling_native_pack_device(value_dst, value, count, batch.format); }
    sink = raw + rows * 5504 * 4;
    gate = raw + rows * 5528 * 4;
    return status;
}


export fn inkling_attention_execute_resident(batch: u64, inputs: u64, projection: u64) -> i32 {
    if inputs == 0 || projection == 0 { return -1; }
    return inkling_native_execute(batch, projection, inputs, false);
}

export fn inkling_attention_graph_execute(call: u64) -> i32 {
    if call == 0 { return -1; }
    return inkling_native_execute(call, call + 200, call + 80, true);
}

extern fn fg_cuda_image_attention_bridge_cubin() -> u64;
extern fn fg_cuda_host_diagnostic_128(position: i32, bytes: i32) -> i32;
extern fn fg_cuda_host_diagnostic_129(slot: i32, position: i32, status: i32, popped: i32) -> i32;

const INKLING_BRIDGE_ROW_BYTES: u64 = 2304 * 4;
const INKLING_BRIDGE_HISTORY_BYTES: u64 = 3 * INKLING_BRIDGE_ROW_BYTES;
const INKLING_BRIDGE_MAX_ROWS: i32 = 11;

struct InklingBridgeSlot {
    data: device_buffer,
    position: i32,
}

struct InklingBridgeState {
    module: u64, prepare: u64, score: u64, mix: u64,
    input: device_buffer, work: device_buffer,
    slots: InklingBridgeSlot[INKLING_EPILOGUE_VISITS],
}
var inkling_bridge: InklingBridgeState;

export fn inkling_draft_bridge_history(destination: u64, position: i32) -> bool {
    if destination == 0 || start_context() != 1 { return false; }
    var status = fg_cuda_driver_sync();
    for i in INKLING_EPILOGUE_VISITS {
        if status != 0 { break; }
        let slot = inkling_bridge.slots[i];
        if slot.data.address == 0 || slot.position != position { status = -1; }
        else {
            status = fg_cuda_driver_download(destination + (i as! u64) * INKLING_BRIDGE_HISTORY_BYTES,
                slot.data.address + 5 * INKLING_BRIDGE_ROW_BYTES, INKLING_BRIDGE_HISTORY_BYTES);
        }
    }
    let popped = finish_context();
    if status != 0 || popped != 0 { return false; }
    fg_cuda_host_diagnostic_128(position, (INKLING_EPILOGUE_VISITS * (INKLING_BRIDGE_HISTORY_BYTES as! i64)) as! i32);
    return true;
}

export fn inkling_bridge_execute(address: u64, attention: u64, output: u64) -> i32 {
    let tls = cuda_thread_state();
    if address == 0 || !tls[0].cuda_inkling_quants { return -1; }
    let geometry = fg_cuda_host_view_i32(address + 32, 3);
    let flags = fg_cuda_host_view_u8(address + 44, 2);
    let pointers = fg_cuda_host_view_u64(address, 4);
    let slot = geometry[0];
    let rows = geometry[1];
    let position = geometry[2];
    if slot < 0 || (slot as! i64) >= INKLING_EPILOGUE_VISITS ||
        rows < 0 || rows > INKLING_BRIDGE_MAX_ROWS || position < 0 || start_context() != 1 { return -1; }

    var status = cuda_ensure_module(&inkling_bridge.module, fg_cuda_image_attention_bridge_cubin());
    let module = inkling_bridge.module;
    if status == 0 { status = cuda_ensure_kernel(&inkling_bridge.prepare, module, "i2_bridge_prepare"); }
    if status == 0 { status = cuda_ensure_kernel(&inkling_bridge.score, module, "i2_bridge_score"); }
    if status == 0 { status = cuda_ensure_kernel(&inkling_bridge.mix, module, "i2_bridge_mix"); }
    let row_bytes = INKLING_BRIDGE_ROW_BYTES;
    if status == 0 { status = reserve(&inkling_bridge.slots[slot].data, 8 * row_bytes); }
    let weights = inkling_bridge.slots[slot].data.address;
    let query = weights + 4 * row_bytes;
    let history = query + row_bytes;
    if status == 0 && flags[0] != 0 {
        status = upload_packet(weights, unsafe_c_ptr<c_ptr<const void>>(pointers[0]), 4 * row_bytes);
        if status == 0 { status = upload_packet(query, unsafe_c_ptr<c_ptr<const void>>(pointers[1]), row_bytes); }
        if status == 0 {
            status = upload_packet(history, unsafe_c_ptr<c_ptr<const void>>(pointers[2]), INKLING_BRIDGE_HISTORY_BYTES);
        }
    } else if status == 0 && inkling_bridge.slots[slot].position != position { status = -1; }

    let max_rows = (INKLING_BRIDGE_MAX_ROWS as! u64) + 1;
    if status == 0 { status = reserve(&inkling_bridge.input, max_rows * row_bytes); }
    if status == 0 { status = reserve(&inkling_bridge.work, row_bytes + max_rows * 4); }
    var mixed: u64 = 0;
    let out_bytes = (2 * row_bytes) as! i64;
    if status == 0 { status = activation_output(output, out_bytes, &mixed); }
    let src = activation_device(attention, row_bytes as! i64);
    if status == 0 && src == 0 { status = -1; }
    let residual = inkling_bridge.input.address;
    if status == 0 {
        status = upload_packet(residual, unsafe_c_ptr<c_ptr<const void>>(pointers[3]), ((rows as! u64) + 1) * row_bytes);
    }
    if status == 0 {
        let past = residual + row_bytes;
        let normalized = inkling_bridge.work.address;
        let scores = normalized + row_bytes;
        let next = mixed + row_bytes;
        let add: i32 = if flags[1] != 0 { 1 } else { 0 };
        var prepare: u64[7] = [cuda_kernel_argument_address(src), cuda_kernel_argument_address(weights),
            cuda_kernel_argument_address(history), cuda_kernel_argument_address(residual), cuda_kernel_argument_address(next),
            cuda_kernel_argument_address(normalized), cuda_kernel_argument_address(add)];
        var score: u64[5] = [cuda_kernel_argument_address(past), cuda_kernel_argument_address(normalized),
            cuda_kernel_argument_address(query), cuda_kernel_argument_address(scores), cuda_kernel_argument_address(rows)];
        var mix: u64[5] = [cuda_kernel_argument_address(past), cuda_kernel_argument_address(normalized),
            cuda_kernel_argument_address(scores), cuda_kernel_argument_address(mixed), cuda_kernel_argument_address(rows)];
        status = fg_cuda_driver_launch(inkling_bridge.prepare, 1, 1, 1, 128, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(prepare), 0);
        if status == 0 {
            status = fg_cuda_driver_launch(inkling_bridge.score, (rows + 1) as! u32, 1, 1, 128, 1, 1, 0, 0,
                                           fg_cuda_driver_words_address(score), 0);
        }
        if status == 0 {
            status = fg_cuda_driver_launch(inkling_bridge.mix, 18, 1, 1, 128, 1, 1, 0, 0,
                                           fg_cuda_driver_words_address(mix), 0);
        }
    }
    if status == 0 { status = activation_publish(output, out_bytes, mixed, false); }
    if status == 0 { inkling_bridge.slots[slot].position = position + 1; }
    let popped = finish_context();
    if status == 0 && popped == 0 { return 0; }
    fg_cuda_host_diagnostic_129(slot, position, status, popped);
    return -1;
}

extern fn fg_cuda_host_diagnostic_130(calls: u64, queries: u64, resident: u64, workspace: u64, uploads: u64) -> i32;

export fn inkling_attention_report() {
    var resident: u64 = 0;
    for i in INKLING_EPILOGUE_VISITS {
        let slot = inkling_native_kv.slots[i];
        resident += slot.cache.bytes + slot.prepare_state.bytes;
    }
    let workspace = inkling_native_kv.window.bytes + inkling_native_kv.input.bytes + inkling_native_kv.staging.bytes +
        inkling_native_kv.partials.bytes + inkling_native_kv.projection.bytes + inkling_native_kv.raw.bytes +
        inkling_native_kv.projected_input.bytes + inkling_native_kv.prepared_kv.bytes;
    fg_cuda_host_diagnostic_130(inkling_native_kv.calls, inkling_native_kv.queries,
                               resident, workspace, inkling_native_kv.uploads);
}

extern fn fg_cuda_image_attention_cubin() -> u64;

struct InklingF32AttentionSlot {
    identity: u64,
    storage: device_buffer,
    rows: i64,
    last: i64,
}

struct InklingF32Attention {
    module: u64,
    kernels: u64[4],
    slots: InklingF32AttentionSlot[INKLING_EPILOGUE_VISITS],
    scratch: device_buffer,
    enabled: i32,
    calls: u64, uploads: u64, downloads: u64,
}
var inkling_f32_attention: InklingF32Attention;

fn inkling_f32_attention_slot(identity: u64) -> i64 {
    for i in INKLING_EPILOGUE_VISITS {
        if inkling_f32_attention.slots[i].identity == identity { return i; }
        if inkling_f32_attention.slots[i].identity == 0 {
            inkling_f32_attention.slots[i].identity = identity;
            inkling_f32_attention.slots[i].last = -1;
            return i;
        }
    }
    return -1;
}

fn inkling_f32_attention_load() -> i32 {
    var status = cuda_ensure_module(&inkling_f32_attention.module, fg_cuda_image_attention_cubin());
    let names = ["goose_i2_attention_score", "goose_i2_attention_softmax",
                 "goose_i2_attention_value", "goose_i2_attention_merge"];
    for i in names.len {
        if status != 0 { break; }
        status = cuda_ensure_kernel(&inkling_f32_attention.kernels[i], inkling_f32_attention.module, names[i]);
    }
    return status;
}

fn inkling_f32_upload(destination: u64, source: u64, bytes: u64) -> i32 {
    let status = fg_cuda_driver_upload(destination, source, bytes);
    if status == 0 { inkling_f32_attention.uploads += bytes; }
    return status;
}

export fn i2_cuda_attention(query: const f32[:], keys: const f32[:], values: const f32[:],
        relative: const f32[:], bias: const f32[:], sinks: const f32[:], positions: const i64[:],
        absolute: i64, window: i64, output: f32[:]) -> i64 {
    let tls = cuda_thread_state();
    if inkling_f32_attention.enabled == 0 {
        inkling_f32_attention.enabled = if cuda_env_equal(cuda_env("I2_DEVICE_ATTN"), "1") { 1 } else { -1 };
    }
    if inkling_f32_attention.enabled < 0 || !tls[0].cuda_inkling_quants { return 0; }
    let rows = positions.len;
    if rows <= 0 || rows > 1048576 || absolute < 0 || (window != 0 && window != 1024) ||
        (window == 0 && absolute >= rows) || keys.len != rows * 1024 || values.len != keys.len ||
        query.len != 3072 || output.len < 3072 || relative.len != 384 || bias.len != 16384 ||
        sinks.len != 24 { return -1; }
    let row = if window != 0 { absolute % rows } else { absolute };
    if positions[row] != absolute || start_context() != 1 { return -1; }

    var status = decode_capture_submit();
    if status == 0 { status = inkling_f32_attention_load(); }
    let identity = cuda_slice_address(positions);
    let slot = if status == 0 { inkling_f32_attention_slot(identity) } else { -1 };
    if slot < 0 { finish_context(); return -1; }
    let previous = inkling_f32_attention.slots[slot];
    if (previous.last >= 0 && absolute != previous.last + 1) ||
        (previous.rows != 0 && previous.rows != rows) { finish_context(); return -1; }

    let row_bytes: u64 = 4096;
    let kv_bytes = (rows as! u64) * row_bytes;
    let pos_bytes = (rows as! u64) * 8;
    status = reserve(&inkling_f32_attention.slots[slot].storage, 2 * kv_bytes + pos_bytes + INKLING_NATIVE_BIAS_BYTES);
    let k = inkling_f32_attention.slots[slot].storage.address;
    let v = k + kv_bytes;
    let pos = v + kv_bytes;
    let table = pos + pos_bytes;
    let tiles = (rows + 255) / 256;
    let scores_bytes = (rows as! u64) * 24 * 4;
    let partial_bytes = (tiles as! u64) * 3072 * 8;
    let fixed_bytes: u64 = (2 * 3072 + 384 + 2 * 24) * 4;
    if status == 0 { status = reserve(&inkling_f32_attention.scratch, fixed_bytes + scores_bytes + partial_bytes); }
    let q = inkling_f32_attention.scratch.address;
    let r = q + 3072 * 4;
    let sink = r + 384 * 4;
    let inverse = sink + 24 * 4;
    let out = inverse + 24 * 4;
    let scores = out + 3072 * 4;
    let partial = scores + scores_bytes;

    if status == 0 && previous.last < 0 {
        status = inkling_f32_upload(k, cuda_slice_address(keys), kv_bytes);
        if status == 0 { status = inkling_f32_upload(v, cuda_slice_address(values), kv_bytes); }
        if status == 0 { status = inkling_f32_upload(pos, identity, pos_bytes); }
        if status == 0 { status = inkling_f32_upload(table, cuda_slice_address(bias), INKLING_NATIVE_BIAS_BYTES); }
        inkling_f32_attention.slots[slot].rows = rows;
    } else if status == 0 {
        let offset = (row as! u64) * row_bytes;
        let pos_offset = (row as! u64) * 8;
        status = inkling_f32_upload(k + offset, cuda_slice_address(keys) + offset, row_bytes);
        if status == 0 { status = inkling_f32_upload(v + offset, cuda_slice_address(values) + offset, row_bytes); }
        if status == 0 { status = inkling_f32_upload(pos + pos_offset, identity + pos_offset, 8); }
    }
    if status == 0 { status = inkling_f32_upload(q, cuda_slice_address(query), 3072 * 4); }
    if status == 0 { status = inkling_f32_upload(r, cuda_slice_address(relative), 384 * 4); }
    if status == 0 { status = inkling_f32_upload(sink, cuda_slice_address(sinks), 24 * 4); }
    let scale = 0.0883883461356163 as! f32;
    var score_args: u64[11] = [
        cuda_kernel_argument_address(q), cuda_kernel_argument_address(k), cuda_kernel_argument_address(r),
        cuda_kernel_argument_address(table), cuda_kernel_argument_address(sink), cuda_kernel_argument_address(pos),
        cuda_kernel_argument_address(scores), cuda_kernel_argument_address(rows), cuda_kernel_argument_address(absolute),
        cuda_kernel_argument_address(window), cuda_kernel_argument_address(scale),
    ];
    var softmax_args: u64[3] = [cuda_kernel_argument_address(scores), cuda_kernel_argument_address(inverse),
                              cuda_kernel_argument_address(rows)];
    var value_args: u64[5] = [cuda_kernel_argument_address(v), cuda_kernel_argument_address(scores),
        cuda_kernel_argument_address(inverse), cuda_kernel_argument_address(partial), cuda_kernel_argument_address(rows)];
    var merge_args: u64[3] = [cuda_kernel_argument_address(partial), cuda_kernel_argument_address(out),
                             cuda_kernel_argument_address(tiles)];
    if status == 0 {
        status = fg_cuda_driver_launch(inkling_f32_attention.kernels[0], ((rows + 127) / 128) as! u32, 24, 1,
                                       128, 1, 1, 0, 0, fg_cuda_driver_words_address(score_args), 0);
    }
    if status == 0 {
        status = fg_cuda_driver_launch(inkling_f32_attention.kernels[1], 24, 1, 1, 128, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(softmax_args), 0);
    }
    if status == 0 {
        status = fg_cuda_driver_launch(inkling_f32_attention.kernels[2], 4, 24, tiles as! u32, 32, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(value_args), 0);
    }
    if status == 0 {
        status = fg_cuda_driver_launch(inkling_f32_attention.kernels[3], 24, 1, 1, 128, 1, 1, 0, 0,
                                       fg_cuda_driver_words_address(merge_args), 0);
    }
    if status == 0 { status = fg_cuda_driver_download(cuda_slice_address(output), out, 3072 * 4); }
    if status == 0 {
        inkling_f32_attention.slots[slot].last = absolute;
        inkling_f32_attention.calls += 1;
        inkling_f32_attention.downloads += 3072 * 4;
    }
    return if finish_context() == 0 && status == 0 { 3072 } else { -1 };
}

extern fn fg_cuda_host_diagnostic_131(calls: u64, uploads: u64, downloads: u64, resident: u64) -> i32;

export fn i2_cuda_attention_stats() {
    var resident = inkling_f32_attention.scratch.bytes;
    for i in INKLING_EPILOGUE_VISITS { resident += inkling_f32_attention.slots[i].storage.bytes; }
    fg_cuda_host_diagnostic_131(inkling_f32_attention.calls, inkling_f32_attention.uploads,
                                inkling_f32_attention.downloads, resident);
}
