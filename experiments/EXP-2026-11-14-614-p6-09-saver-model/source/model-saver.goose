// Port of llama_model_saver (src/llama-model-saver.cpp) to Goose.
// S1-S4 come verbatim from the accepted writer experiment; S5 is the
// model path: add_kv_from_model, add_tensors_from_model and the arch
// predicate. Generated slot tables and helpers precede the carried code
// so every name is defined before use.

// ---- S5: saver from a model ----------------------------------------------
//
// Port of llama_model_saver::add_kv_from_model (src/llama-model-saver.cpp
// 147-449) and add_tensors_from_model (451-488). The port never dereferences
// a llama_model: the host passes flat value tables and the resolved LLM_KV
// key names, the same way the accepted S4 writer takes a key parameter.
//
// Decisions kept here:
//   - the emission order and every native conditional (n_embd_out > 0,
//     prec_policy, swa_type, the deepseek4 compress-ratio branch, the
//     whole-or-nothing PLE group);
//   - the GGUF type of each value, taken from the native overload that the
//     hparam type selects;
//   - per-layer reduction: n_layer = n_layer_all - n_layer_nextn, the
//     all-equal collapse to one scalar, otherwise an array;
//   - the vocabulary attr -> llama_token_type switch and its UNDEFINED
//     default (the enum numbers are asserted by the gate against llama.h);
//   - a vocab-less model still writes the token-type array: the native vector
//     is zero-filled, which is LLAMA_TOKEN_TYPE_UNDEFINED;
//   - rope_scaling_factor: exactly 0 for a 1.0f train scale, else 1/scale;
//   - prec_policy: names first, then one bool per name (Q8 -> 0, else 1);
//   - the charmap: a non-empty map reaches the char overload, so native
//     aborts; the port refuses with -1 and does not write that key.
//
// The hparams themselves are input values; their meaning (which layer count,
// which epsilon) belongs to the loader and is not re-derived here.


// ---- S5 slot contract -----------------------------------------------------
//
// Key slots: every metadata key the native function writes, in first-use
// order. The host passes the resolved LLM_KV names in this order.
//
// H_*  integer/bool scalar hparams (hv), HF_* float scalars (hf),
// HA_* integer arrays (ha, stride AV), HAF_* f32 arrays (haf).
// I_*  runtime counts and string-table ranges (ix).
// S_*  fixed entries of the resolved string table.

const S_ARCH: i64 = 0;
const S_NAME: i64 = 1;
const S_ROPE_ORDER: i64 = 2;
const S_ROPE_SCALE: i64 = 3;
const S_TOK_MODEL: i64 = 4;
const S_TOK_PRE: i64 = 5;

const I_NTOKENS: i64 = 0;
const I_NTOKEN_TYPES: i64 = 1;
const I_NMERGES: i64 = 2;
const I_NLABELS: i64 = 3;
const I_NPREC: i64 = 4;
const I_TOK_FIRST: i64 = 5;
const I_MERGE_FIRST: i64 = 6;
const I_LABEL_FIRST: i64 = 7;
const I_PREC_FIRST: i64 = 8;
const I_NLAYER_ALL: i64 = 9;
const I_NLAYER_NEXTN: i64 = 10;
const I_ARCH: i64 = 11;
const I_ARCH_DSV4: i64 = 12;
const I_SWA_NONE: i64 = 13;
const I_TOKEN_NULL: i64 = 14;
const I_AV: i64 = 15;
const I_ROPE_SECTIONS_N: i64 = 16;
const I_PLE_NGRAM: i64 = 17;
const I_PLE_HEADS: i64 = 18;


// key slots, first-use order
const K_GENERAL_ARCHITECTURE: i64 = 0;
const K_GENERAL_NAME: i64 = 1;
const K_GENERAL_TENSOR_EXTRA_PREC_A4: i64 = 2;
const K_GENERAL_TENSOR_EXTRA_NAME: i64 = 3;
const K_VOCAB_SIZE: i64 = 4;
const K_CONTEXT_LENGTH: i64 = 5;
const K_EMBEDDING_LENGTH: i64 = 6;
const K_EMBEDDING_LENGTH_OUT: i64 = 7;
const K_BLOCK_COUNT: i64 = 8;
const K_LEADING_DENSE_BLOCK_COUNT: i64 = 9;
const K_FEED_FORWARD_LENGTH: i64 = 10;
const K_EXPERT_FEED_FORWARD_LENGTH: i64 = 11;
const K_EXPERT_LATENT_LENGTH: i64 = 12;
const K_MOE_LATENT_SIZE: i64 = 13;
const K_EXPERT_SHARED_FEED_FORWARD_LENGTH: i64 = 14;
const K_EXPERT_CHUNK_FEED_FORWARD_LENGTH: i64 = 15;
const K_SWIGLU_CLAMP_EXP: i64 = 16;
const K_SWIGLU_CLAMP_SHEXP: i64 = 17;
const K_USE_PARALLEL_RESIDUAL: i64 = 18;
const K_EXPERT_COUNT: i64 = 19;
const K_EXPERT_USED_COUNT: i64 = 20;
const K_EXPERT_SHARED_COUNT: i64 = 21;
const K_EXPERT_GROUP_COUNT: i64 = 22;
const K_EXPERT_GROUP_USED_COUNT: i64 = 23;
const K_EXPERT_WEIGHTS_SCALE: i64 = 24;
const K_EXPERT_WEIGHTS_NORM: i64 = 25;
const K_EXPERT_GATING_FUNC: i64 = 26;
const K_EXPERT_GROUP_SCALE: i64 = 27;
const K_EXPERTS_PER_GROUP: i64 = 28;
const K_MOE_EVERY_N_LAYERS: i64 = 29;
const K_NEXTN_PREDICT_LAYERS: i64 = 30;
const K_NUM_DEEPSTACK_LAYERS: i64 = 31;
const K_DEEPSTACK_MAPPING: i64 = 32;
const K_POOLING_TYPE: i64 = 33;
const K_LOGIT_SCALE: i64 = 34;
const K_DECODER_START_TOKEN_ID: i64 = 35;
const K_DECODER_BLOCK_COUNT: i64 = 36;
const K_ATTN_LOGIT_SOFTCAPPING: i64 = 37;
const K_ROUTER_LOGIT_SOFTCAPPING: i64 = 38;
const K_FINAL_LOGIT_SOFTCAPPING: i64 = 39;
const K_SWIN_NORM: i64 = 40;
const K_RESCALE_EVERY_N_LAYERS: i64 = 41;
const K_TIME_MIX_EXTRA_DIM: i64 = 42;
const K_TIME_DECAY_EXTRA_DIM: i64 = 43;
const K_RESIDUAL_SCALE: i64 = 44;
const K_EMBEDDING_SCALE: i64 = 45;
const K_HRM_LAYERS_PER_STACK: i64 = 46;
const K_HRM_H_CYCLES: i64 = 47;
const K_HRM_L_CYCLES: i64 = 48;
const K_HRM_PREFIX_LM: i64 = 49;
const K_TOKEN_SHIFT_COUNT: i64 = 50;
const K_INTERLEAVE_MOE_LAYER_STEP: i64 = 51;
const K_ATTENTION_HEAD_COUNT: i64 = 52;
const K_ATTENTION_HEAD_COUNT_KV: i64 = 53;
const K_ATTENTION_MAX_ALIBI_BIAS: i64 = 54;
const K_ATTENTION_CLAMP_KQV: i64 = 55;
const K_ATTENTION_KEY_LENGTH: i64 = 56;
const K_ATTENTION_VALUE_LENGTH: i64 = 57;
const K_ATTENTION_LAYERNORM_EPS: i64 = 58;
const K_ATTENTION_LAYERNORM_RMS_EPS: i64 = 59;
const K_POST_NORM_EPS: i64 = 60;
const K_ATTENTION_GROUPNORM_EPS: i64 = 61;
const K_ATTENTION_GROUPNORM_GROUPS: i64 = 62;
const K_ATTENTION_VALUE_EXPERT_COUNT: i64 = 63;
const K_ATTENTION_VALUE_EXPERT_USED_COUNT: i64 = 64;
const K_ATTENTION_CAUSAL: i64 = 65;
const K_ATTENTION_Q_LORA_RANK: i64 = 66;
const K_ATTENTION_KV_LORA_RANK: i64 = 67;
const K_ATTENTION_DECAY_LORA_RANK: i64 = 68;
const K_ATTENTION_ICLR_LORA_RANK: i64 = 69;
const K_ATTENTION_VALUE_RESIDUAL_MIX_LORA_RANK: i64 = 70;
const K_ATTENTION_GATE_LORA_RANK: i64 = 71;
const K_ATTENTION_RELATIVE_BUCKETS_COUNT: i64 = 72;
const K_ATTENTION_ROPE_PATTERN: i64 = 73;
const K_ATTENTION_SLIDING_WINDOW: i64 = 74;
const K_ATTENTION_SLIDING_WINDOW_PATTERN: i64 = 75;
const K_ATTENTION_SCALE: i64 = 76;
const K_ATTENTION_OUTPUT_SCALE: i64 = 77;
const K_ATTENTION_VALUE_SCALE: i64 = 78;
const K_ATTENTION_TEMPERATURE_LENGTH: i64 = 79;
const K_ATTENTION_TEMPERATURE_SCALE: i64 = 80;
const K_ATTENTION_KEY_LENGTH_MLA: i64 = 81;
const K_ATTENTION_VALUE_LENGTH_MLA: i64 = 82;
const K_ATTENTION_KEY_LENGTH_SWA: i64 = 83;
const K_ATTENTION_VALUE_LENGTH_SWA: i64 = 84;
const K_ATTENTION_KEY_LENGTH_MLA_SWA: i64 = 85;
const K_ATTENTION_VALUE_LENGTH_MLA_SWA: i64 = 86;
const K_ATTENTION_KV_LORA_RANK_SWA: i64 = 87;
const K_ATTENTION_INDEXER_HEAD_COUNT: i64 = 88;
const K_ATTENTION_INDEXER_KEY_LENGTH: i64 = 89;
const K_ATTENTION_INDEXER_TOP_K: i64 = 90;
const K_ATTENTION_INDEXER_BLOCK_SIZE: i64 = 91;
const K_ATTENTION_INDEXER_KPOOL: i64 = 92;
const K_ATTENTION_INDEXER_KPOOL_SELECT_TAIL: i64 = 93;
const K_ATTENTION_INDEXER_LOCAL_BLOCKS: i64 = 94;
const K_ATTENTION_INDEXER_TYPES: i64 = 95;
const K_ATTENTION_RECURRENT_LAYERS: i64 = 96;
const K_ATTENTION_OUTPUT_GROUP_COUNT: i64 = 97;
const K_ATTENTION_OUTPUT_LORA_RANK: i64 = 98;
const K_ATTENTION_COMPRESS_ROPE_FREQ_BASE: i64 = 99;
const K_ATTENTION_COMPRESS_RATIOS: i64 = 100;
const K_HYPER_CONNECTION_COUNT: i64 = 101;
const K_HYPER_CONNECTION_SINKHORN_ITERATIONS: i64 = 102;
const K_HYPER_CONNECTION_EPSILON: i64 = 103;
const K_HYPER_CONNECTION_MAGNITUDE: i64 = 104;
const K_HASH_LAYER_COUNT: i64 = 105;
const K_HYPER_CONNECTION_LOW_RANK: i64 = 106;
const K_PLE_LAYERS: i64 = 107;
const K_PLE_NGRAM_SIZE: i64 = 108;
const K_PLE_HEADS_PER_NGRAM: i64 = 109;
const K_PLE_CONV_KERNEL: i64 = 110;
const K_PLE_EOS_TOKEN_ID: i64 = 111;
const K_EMBEDDING_LENGTH_PER_LAYER: i64 = 112;
const K_PLE_LAYER_MULTIPLIERS: i64 = 113;
const K_PLE_HEAD_OFFSETS: i64 = 114;
const K_PLE_HEAD_VOCAB_SIZES: i64 = 115;
const K_ROPE_DIMENSION_COUNT: i64 = 116;
const K_ROPE_DIMENSION_COUNT_SWA: i64 = 117;
const K_ROPE_DIMENSION_SECTIONS: i64 = 118;
const K_ROPE_SECTION_ORDER: i64 = 119;
const K_ROPE_FREQ_BASE: i64 = 120;
const K_ROPE_FREQ_BASE_SWA: i64 = 121;
const K_ROPE_SCALING_TYPE: i64 = 122;
const K_ROPE_SCALING_FACTOR: i64 = 123;
const K_ROPE_SCALING_ATTN_FACTOR: i64 = 124;
const K_ROPE_SCALING_ORIG_CTX_LEN: i64 = 125;
const K_ROPE_SCALING_FINETUNED: i64 = 126;
const K_ROPE_SCALING_YARN_LOG_MUL: i64 = 127;
const K_ROPE_SCALING_YARN_EXT_FACTOR: i64 = 128;
const K_ROPE_SCALING_YARN_ATTN_FACTOR: i64 = 129;
const K_ROPE_SCALING_YARN_BETA_FAST: i64 = 130;
const K_ROPE_SCALING_YARN_BETA_SLOW: i64 = 131;
const K_SSM_INNER_SIZE: i64 = 132;
const K_SSM_CONV_KERNEL: i64 = 133;
const K_SSM_STATE_SIZE: i64 = 134;
const K_SSM_TIME_STEP_RANK: i64 = 135;
const K_SSM_GROUP_COUNT: i64 = 136;
const K_SSM_DT_B_C_RMS: i64 = 137;
const K_KDA_HEAD_DIM: i64 = 138;
const K_KDA_SAFE_GATE: i64 = 139;
const K_KDA_GATE_LOWER_BOUND: i64 = 140;
const K_WKV_HEAD_SIZE: i64 = 141;
const K_TOKENIZER_MODEL: i64 = 142;
const K_TOKENIZER_PRE: i64 = 143;
const K_TOKENIZER_LIST: i64 = 144;
const K_TOKENIZER_TOKEN_TYPE: i64 = 145;
const K_TOKENIZER_TOKEN_TYPE_COUNT: i64 = 146;
const K_TOKENIZER_SCORES: i64 = 147;
const K_TOKENIZER_MERGES: i64 = 148;
const K_TOKENIZER_BOS_ID: i64 = 149;
const K_TOKENIZER_EOS_ID: i64 = 150;
const K_TOKENIZER_EOT_ID: i64 = 151;
const K_TOKENIZER_EOM_ID: i64 = 152;
const K_TOKENIZER_UNK_ID: i64 = 153;
const K_TOKENIZER_SEP_ID: i64 = 154;
const K_TOKENIZER_PAD_ID: i64 = 155;
const K_TOKENIZER_ADD_BOS: i64 = 156;
const K_TOKENIZER_ADD_EOS: i64 = 157;
const K_TOKENIZER_ADD_SEP: i64 = 158;
const K_TOKENIZER_ADD_PREFIX: i64 = 159;
const K_TOKENIZER_REMOVE_EXTRA_WS: i64 = 160;
const K_TOKENIZER_PRECOMPILED_CHARSMAP: i64 = 161;
const K_TOKENIZER_FIM_PRE_ID: i64 = 162;
const K_TOKENIZER_FIM_SUF_ID: i64 = 163;
const K_TOKENIZER_FIM_MID_ID: i64 = 164;
const K_TOKENIZER_FIM_PAD_ID: i64 = 165;
const K_TOKENIZER_FIM_REP_ID: i64 = 166;
const K_TOKENIZER_FIM_SEP_ID: i64 = 167;
const K_POSNET_EMBEDDING_LENGTH: i64 = 168;
const K_POSNET_BLOCK_COUNT: i64 = 169;
const K_CONVNEXT_EMBEDDING_LENGTH: i64 = 170;
const K_CONVNEXT_BLOCK_COUNT: i64 = 171;
const K_CLASSIFIER_OUTPUT_LABELS: i64 = 172;
const K_SHORTCONV_L_CACHE: i64 = 173;
const K_XIELU_ALPHA_N: i64 = 174;
const K_XIELU_ALPHA_P: i64 = 175;
const K_XIELU_BETA: i64 = 176;
const K_XIELU_EPS: i64 = 177;
const K_ATTN_RES_BLOCK_SIZE: i64 = 178;
const K_ACTIVATION_SITU_BETA: i64 = 179;
const K_ACTIVATION_SITU_LINEAR_BETA: i64 = 180;
const K_DENSE_2_FEAT_IN: i64 = 181;
const K_DENSE_2_FEAT_OUT: i64 = 182;
const K_DENSE_3_FEAT_IN: i64 = 183;
const K_DENSE_3_FEAT_OUT: i64 = 184;


// integer/bool scalar hparams
const H_CALL_N_TOKENS: i64 = 0;
const H_HPARAMS_N_CTX_TRAIN: i64 = 1;
const H_HPARAMS_N_EMBD: i64 = 2;
const H_HPARAMS_N_EMBD_OUT_IMPL: i64 = 3;
const H_HPARAMS_N_LAYER_ALL: i64 = 4;
const H_HPARAMS_N_LAYER_DENSE_LEAD: i64 = 5;
const H_HPARAMS_N_EXPERT_LATENT: i64 = 6;
const H_HPARAMS_MOE_LATENT_SIZE: i64 = 7;
const H_HPARAMS_N_FF_SHEXP: i64 = 8;
const H_HPARAMS_N_FF_CHEXP: i64 = 9;
const H_HPARAMS_USE_PAR_RES: i64 = 10;
const H_HPARAMS_N_EXPERT: i64 = 11;
const H_CALL_N_EXPERT_USED: i64 = 12;
const H_HPARAMS_N_EXPERT_SHARED: i64 = 13;
const H_HPARAMS_N_EXPERT_GROUPS: i64 = 14;
const H_HPARAMS_N_GROUP_USED: i64 = 15;
const H_HPARAMS_EXPERT_WEIGHTS_NORM: i64 = 17;
const H_HPARAMS_EXPERT_GATING_FUNC: i64 = 18;
const H_HPARAMS_N_GROUP_EXPERTS: i64 = 20;
const H_HPARAMS_MOE_EVERY_N_LAYERS: i64 = 21;
const H_HPARAMS_N_LAYER_NEXTN: i64 = 22;
const H_HPARAMS_N_DEEPSTACK_LAYERS: i64 = 23;
const H_CAST_POOLING_TYPE: i64 = 24;
const H_HPARAMS_DEC_START_TOKEN_ID: i64 = 26;
const H_HPARAMS_DEC_N_LAYER: i64 = 27;
const H_HPARAMS_SWIN_NORM: i64 = 31;
const H_HPARAMS_RESCALE_EVERY_N_LAYERS: i64 = 32;
const H_HPARAMS_TIME_MIX_EXTRA_DIM: i64 = 33;
const H_HPARAMS_TIME_DECAY_EXTRA_DIM: i64 = 34;
const H_HPARAMS_N_HRM_LAYERS_PER_STACK: i64 = 37;
const H_HPARAMS_N_HRM_H_CYCLES: i64 = 38;
const H_HPARAMS_N_HRM_L_CYCLES: i64 = 39;
const H_HPARAMS_HRM_PREFIX_LM: i64 = 40;
const H_HPARAMS_TOKEN_SHIFT_COUNT: i64 = 41;
const H_HPARAMS_N_MOE_LAYER_STEP: i64 = 42;
const H_HPARAMS_N_EMBD_HEAD_K_FULL: i64 = 45;
const H_HPARAMS_N_EMBD_HEAD_V_FULL: i64 = 46;
const H_HPARAMS_N_NORM_GROUPS: i64 = 51;
const H_HPARAMS_N_VALUE_EXPERT: i64 = 52;
const H_HPARAMS_N_VALUE_EXPERT_USED: i64 = 53;
const H_HPARAMS_CAUSAL_ATTN: i64 = 54;
const H_HPARAMS_N_LORA_Q: i64 = 55;
const H_HPARAMS_N_LORA_KV: i64 = 56;
const H_HPARAMS_N_LORA_DECAY: i64 = 57;
const H_HPARAMS_N_LORA_ICLR: i64 = 58;
const H_HPARAMS_N_LORA_VALUE_RES_MIX: i64 = 59;
const H_HPARAMS_N_LORA_GATE: i64 = 60;
const H_HPARAMS_N_REL_ATTN_BKTS: i64 = 61;
const H_HPARAMS_N_SWA: i64 = 62;
const H_HPARAMS_SWA_TYPE: i64 = 63;
const H_HPARAMS_ATTN_TEMP_LENGTH: i64 = 67;
const H_HPARAMS_N_EMBD_HEAD_K_MLA_IMPL: i64 = 69;
const H_HPARAMS_N_EMBD_HEAD_V_MLA_IMPL: i64 = 70;
const H_HPARAMS_N_EMBD_HEAD_K_SWA: i64 = 71;
const H_HPARAMS_N_EMBD_HEAD_V_SWA: i64 = 72;
const H_HPARAMS_N_EMBD_HEAD_K_MLA_SWA: i64 = 73;
const H_HPARAMS_N_EMBD_HEAD_V_MLA_SWA: i64 = 74;
const H_HPARAMS_N_LORA_KV_SWA: i64 = 75;
const H_HPARAMS_INDEXER_N_HEAD: i64 = 76;
const H_HPARAMS_INDEXER_HEAD_SIZE: i64 = 77;
const H_HPARAMS_INDEXER_TOP_K: i64 = 78;
const H_HPARAMS_INDEXER_BLOCK_SIZE: i64 = 79;
const H_HPARAMS_INDEXER_KPOOL: i64 = 80;
const H_HPARAMS_INDEXER_KPOOL_SELECT_TAIL: i64 = 81;
const H_HPARAMS_INDEXER_LOCAL_BLOCKS: i64 = 82;
const H_HPARAMS_DSV4_O_GROUP_COUNT: i64 = 83;
const H_HPARAMS_DSV4_O_LORA_RANK: i64 = 84;
const H_HPARAMS_DSV4_HC_MULT: i64 = 86;
const H_HPARAMS_DSV4_HC_SINKHORN_ITERS: i64 = 87;
const H_HPARAMS_DSV4_HASH_LAYER_COUNT: i64 = 90;
const H_HPARAMS_HC_LOW_RANK: i64 = 91;
const H_HPARAMS_PLE_N_HEADS: i64 = 92;
const H_HPARAMS_PLE_NGRAM_SIZE: i64 = 93;
const H_HPARAMS_PLE_HEADS_PER_NGRAM: i64 = 94;
const H_HPARAMS_PLE_CONV_KERNEL: i64 = 95;
const H_HPARAMS_PLE_EOS_TOKEN_ID: i64 = 96;
const H_HPARAMS_PLE_HEAD_DIM: i64 = 97;
const H_HPARAMS_N_ROT_FULL: i64 = 98;
const H_HPARAMS_N_ROT_SWA: i64 = 99;
const H_HPARAMS_N_CTX_ORIG_YARN: i64 = 104;
const H_HPARAMS_ROPE_FINETUNED: i64 = 105;
const H_HPARAMS_SSM_D_INNER: i64 = 111;
const H_HPARAMS_SSM_D_CONV: i64 = 112;
const H_HPARAMS_SSM_D_STATE: i64 = 113;
const H_HPARAMS_SSM_DT_RANK: i64 = 114;
const H_HPARAMS_SSM_N_GROUP: i64 = 115;
const H_HPARAMS_SSM_DT_B_C_RMS: i64 = 116;
const H_HPARAMS_N_EMBD_HEAD_KDA: i64 = 117;
const H_HPARAMS_KDA_SAFE_GATE: i64 = 118;
const H_HPARAMS_WKV_HEAD_SIZE: i64 = 120;
const H_CALL_N_TOKEN_TYPES: i64 = 121;
const H_CALL_TOKEN_BOS: i64 = 122;
const H_CALL_TOKEN_EOS: i64 = 123;
const H_CALL_TOKEN_EOT: i64 = 124;
const H_CALL_TOKEN_EOM: i64 = 125;
const H_CALL_TOKEN_UNK: i64 = 126;
const H_CALL_TOKEN_SEP: i64 = 127;
const H_CALL_TOKEN_PAD: i64 = 128;
const H_CALL_GET_ADD_BOS: i64 = 129;
const H_CALL_GET_ADD_EOS: i64 = 130;
const H_CALL_GET_ADD_SEP: i64 = 131;
const H_CALL_GET_ADD_SPACE_PREFIX: i64 = 132;
const H_CALL_GET_REMOVE_EXTRA_WHITESPACES: i64 = 133;
const H_CALL_TOKEN_FIM_PRE: i64 = 134;
const H_CALL_TOKEN_FIM_SUF: i64 = 135;
const H_CALL_TOKEN_FIM_MID: i64 = 136;
const H_CALL_TOKEN_FIM_PAD: i64 = 137;
const H_CALL_TOKEN_FIM_REP: i64 = 138;
const H_CALL_TOKEN_FIM_SEP: i64 = 139;
const H_POSNET_N_EMBD: i64 = 140;
const H_POSNET_N_LAYER: i64 = 141;
const H_CONVNEXT_N_EMBD: i64 = 142;
const H_CONVNEXT_N_LAYER: i64 = 143;
const H_HPARAMS_N_SHORTCONV_L_CACHE: i64 = 144;
const H_HPARAMS_ATTN_RES_BLOCK_SIZE: i64 = 145;
const H_HPARAMS_DENSE_2_FEAT_IN: i64 = 148;
const H_HPARAMS_DENSE_2_FEAT_OUT: i64 = 149;
const H_HPARAMS_DENSE_3_FEAT_IN: i64 = 150;
const H_HPARAMS_DENSE_3_FEAT_OUT: i64 = 151;

// f32 scalar hparams
const HF_HPARAMS_EXPERT_WEIGHTS_SCALE: i64 = 16;
const HF_HPARAMS_EXPERT_GROUP_SCALE: i64 = 19;
const HF_HPARAMS_F_LOGIT_SCALE: i64 = 25;
const HF_HPARAMS_F_ATTN_LOGIT_SOFTCAPPING: i64 = 28;
const HF_HPARAMS_F_ROUTER_LOGIT_SOFTCAPPING: i64 = 29;
const HF_HPARAMS_F_FINAL_LOGIT_SOFTCAPPING: i64 = 30;
const HF_HPARAMS_F_RESIDUAL_SCALE: i64 = 35;
const HF_HPARAMS_F_EMBEDDING_SCALE: i64 = 36;
const HF_HPARAMS_F_MAX_ALIBI_BIAS: i64 = 43;
const HF_HPARAMS_F_CLAMP_KQV: i64 = 44;
const HF_HPARAMS_F_NORM_EPS: i64 = 47;
const HF_HPARAMS_F_NORM_RMS_EPS: i64 = 48;
const HF_HPARAMS_F_POST_NORM_EPS: i64 = 49;
const HF_HPARAMS_F_NORM_GROUP_EPS: i64 = 50;
const HF_HPARAMS_F_ATTENTION_SCALE: i64 = 64;
const HF_HPARAMS_F_ATTN_OUT_SCALE: i64 = 65;
const HF_HPARAMS_F_ATTN_VALUE_SCALE: i64 = 66;
const HF_HPARAMS_F_ATTN_TEMP_SCALE: i64 = 68;
const HF_HPARAMS_DSV4_COMPRESS_ROPE_BASE: i64 = 85;
const HF_HPARAMS_DSV4_HC_EPS: i64 = 88;
const HF_HPARAMS_HC_MAGNITUDE: i64 = 89;
const HF_HPARAMS_ROPE_FREQ_BASE_TRAIN: i64 = 100;
const HF_HPARAMS_ROPE_FREQ_BASE_TRAIN_SWA: i64 = 101;
const HF_HPARAMS_ROPE_FREQ_SCALE_TRAIN: i64 = 102;
const HF_HPARAMS_ROPE_ATTN_FACTOR: i64 = 103;
const HF_HPARAMS_ROPE_YARN_LOG_MUL: i64 = 106;
const HF_HPARAMS_YARN_EXT_FACTOR: i64 = 107;
const HF_HPARAMS_YARN_ATTN_FACTOR: i64 = 108;
const HF_HPARAMS_YARN_BETA_FAST: i64 = 109;
const HF_HPARAMS_YARN_BETA_SLOW: i64 = 110;
const HF_HPARAMS_KDA_GATE_LOWER_BOUND: i64 = 119;
const HF_HPARAMS_SITU_BETA: i64 = 146;
const HF_HPARAMS_SITU_LINEAR_BETA: i64 = 147;

// integer arrays, stride AV
const HA_N_FF_ARR_P: i64 = 0;
const HA_N_FF_EXP_ARR_P: i64 = 1;
const HA_DEEPSTACK_MAPPING_ARR: i64 = 4;
const HA_N_HEAD_ARR_P: i64 = 5;
const HA_N_HEAD_KV_ARR_P: i64 = 6;
const HA_ROPE_PATTERN_P: i64 = 7;
const HA_IS_SWA_IMPL: i64 = 8;
const HA_IS_INDEXER_FULL_IMPL_P: i64 = 9;
const HA_IS_RECR_IMPL_P: i64 = 10;
const HA_DSV4_COMPRESS_RATIOS: i64 = 11;
const HA_DSV4_COMPRESS_RATIOS_P: i64 = 12;
const HA_IS_PLE_IMPL: i64 = 13;
const HA_PLE_LAYER_MULTIPLIERS: i64 = 14;
const HA_PLE_HEAD_OFFSETS: i64 = 15;
const HA_PLE_HEAD_VOCAB_SIZES: i64 = 16;
const HA_ROPE_SECTIONS: i64 = 17;

// f32 arrays, stride AV
const HAF_SWIGLU_CLAMP_EXP: i64 = 2;
const HAF_SWIGLU_CLAMP_SHEXP: i64 = 3;
const HAF_XIELU_ALPHA_N: i64 = 18;
const HAF_XIELU_ALPHA_P: i64 = 19;
const HAF_XIELU_BETA: i64 = 20;
const HAF_XIELU_EPS: i64 = 21;


// The raw IEEE-754 image of an f32, exactly what gguf_set_val_f32 copies.
extern "ms_bits" fn f32_bits(v: f32) -> i64;

fn ks(k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:], i: i64) -> const u8[:] {
    return k_pool[k_off[i]..k_off[i] + k_len[i]];
}

fn ss(s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:], i: i64) -> const u8[:] {
    return s_pool[s_off[i]..s_off[i] + s_len[i]];
}

// Native add_kv scalar overloads, addressed by slot instead of a key string.
fn kv_u32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
          types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
          k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
          slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

fn kv_i32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
          types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
          k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
          slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_i32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

fn kv_u64(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
          types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
          k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
          slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_u64(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

// `value` is the raw f32 bit image, as the native overload stores it.
fn kf32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_f32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

fn kbool(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_bool(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        ks(k_off, k_len, k_pool, slot), value, out);
}

// std::string goes through the native template, which returns before writing
// when the container is empty. vector<string> (kstrs) does not have that early
// return and still writes an empty array.
fn kstr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, val: const u8[:], out: i64[:]) -> i64 {
    if val.len == 0 as i64 {
        out[0] = 0 as i64;
        return 0 as i64;
    }
    return sav_add_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), val, out);
}

// ---- value encoding -------------------------------------------------------
//
// Every numeric array is passed as i64 values with a GGUF element type; f32
// arrays are passed as f32 and encoded through f32_bits. Widths are the same
// table the writer uses for ggml_nbytes.

fn enc_i64(scratch: u8[:], data: const i64[:], etype: i64, n: i64) -> i64 {
    let esize: i64 = k_elem_size(etype);
    if esize < 0 as i64 || n < 0 as i64 || n > data.len {
        return -(1 as i64);
    }
    if n * esize > scratch.len {
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        let v: i64 = data[i];
        var b: i64 = 0;
        while b < esize {
            scratch[i * esize + b] = (v >> (8 as i64 * b)) as! u8;
            b += 1 as i64;
        }
        i += 1 as i64;
    }
    return n * esize;
}

fn enc_f32(scratch: u8[:], data: const f32[:], n: i64) -> i64 {
    if n < 0 as i64 || n > data.len || n * 4 as i64 > scratch.len {
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        let v: i64 = f32_bits(data[i]);
        var b: i64 = 0;
        while b < 4 as i64 {
            scratch[i * 4 + b] = (v >> (8 as i64 * b)) as! u8;
            b += 1 as i64;
        }
        i += 1 as i64;
    }
    return n * 4 as i64;
}

// NUL-separated blob for gguf_set_arr_str, in table order.
fn enc_strs(scratch: u8[:], s_off: const i64[:], s_len: const i64[:],
            s_pool: const u8[:], first: i64, n: i64) -> i64 {
    if n < 0 as i64 || first < 0 as i64 || first + n > s_off.len {
        return -(1 as i64);
    }
    var p: i64 = 0;
    var i: i64 = 0;
    while i < n {
        let e: i64 = first + i;
        let o: i64 = s_off[e];
        let l: i64 = s_len[e];
        if o < 0 as i64 || o + l > s_pool.len || p + l + 1 > scratch.len {
            return -(1 as i64);
        }
        var j: i64 = 0;
        while j < l {
            scratch[p + j] = s_pool[o + j];
            j += 1 as i64;
        }
        scratch[p + l] = 0 as u8;
        p = p + l + 1;
        i += 1 as i64;
    }
    return p;
}

// llama_token_attr -> llama_token_type, the native switch plus its default.
// llama_token_attr is a bit flag (1,2,4,8,16,32), not an ordinal; the gate
// asserts every number against llama.h.
fn tok_type(attr: i64) -> i64 {
    if attr == 1 as i64 { return 2 as i64; }   // ATTR_UNKNOWN      -> TYPE_UNKNOWN
    if attr == 2 as i64 { return 5 as i64; }   // ATTR_UNUSED       -> TYPE_UNUSED
    if attr == 4 as i64 { return 1 as i64; }   // ATTR_NORMAL       -> TYPE_NORMAL
    if attr == 8 as i64 { return 3 as i64; }   // ATTR_CONTROL      -> TYPE_CONTROL
    if attr == 16 as i64 { return 4 as i64; }  // ATTR_USER_DEFINED -> TYPE_USER_DEFINED
    if attr == 32 as i64 { return 6 as i64; }  // ATTR_BYTE         -> TYPE_BYTE
    return 0 as i64;                           // UNDEFINED and the native default
}

// Numeric array with a GGUF element type (u32/i32/u64).
fn karr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, etype: i64, scratch: u8[:], data: const i64[:], n: i64,
        per: i64, out: i64[:]) -> i64 {
    let nb: i64 = enc_i64(scratch, data, etype, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), etype,
                       scratch[0..nb], n, per, out);
}

// f32 array.
fn karrf(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, scratch: u8[:], data: const f32[:], n: i64,
         per: i64, out: i64[:]) -> i64 {
    let nb: i64 = enc_f32(scratch, data, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_F32,
                       scratch[0..nb], n, per, out);
}

// std::vector<std::string> as a NUL-separated GGUF string array.
fn kstrs(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:],
         first: i64, n: i64, scratch: u8[:], out: i64[:]) -> i64 {
    let nb: i64 = enc_strs(scratch, s_off, s_len, s_pool, first, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_strs(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        ks(k_off, k_len, k_pool, slot), scratch[0..nb], n, out);
}

// std::vector<float> token scores.
fn kscores(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
           types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
           tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
           slot: i64, scores: const f32[:], n: i64, scratch: u8[:], out: i64[:]) -> i64 {
    let nb: i64 = enc_f32(scratch, scores, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_F32,
                       scratch[0..nb], n, 0 as i64, out);
}

// std::vector<int32_t> token types from the raw attrs.
fn ktoktypes(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
             types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
             tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
             slot: i64, attrs: const i64[:], n: i64, scratch: u8[:], out: i64[:]) -> i64 {
    if n < 0 as i64 || n > attrs.len || n * 4 as i64 > scratch.len {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        let t: i64 = tok_type(attrs[i]);
        var b: i64 = 0;
        while b < 4 as i64 {
            scratch[i * 4 + b] = (t >> (8 as i64 * b)) as! u8;
            b += 1 as i64;
        }
        i += 1 as i64;
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_I32,
                       scratch[0..n * 4 as i64], n, 0 as i64, out);
}

// prec_policy: one bool per tensor name, GGML_PREC_Q8 -> 0, otherwise 1.
fn kprec(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:],
         first: i64, n: i64, prec: const i64[:], scratch: u8[:], out: i64[:]) -> i64 {
    if n < 0 as i64 || n > prec.len || n > scratch.len {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        scratch[i] = 1 as u8;
        if prec[i] == 30 as i64 {  // GGML_PREC_Q8
            scratch[i] = 0 as u8;
        }
        i += 1 as i64;
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_BOOL,
                       scratch[0..n], n, 0 as i64, out);
}

// PLE layer list: every layer whose is_ple bit is set.
fn kple(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, is_ple: const i64[:], nlayer: i64, scratch: u8[:], out: i64[:]) -> i64 {
    var n: i64 = 0;
    var il: i64 = 0;
    while il < nlayer && il < is_ple.len {
        if is_ple[il] != 0 as i64 {
            if n * 4 + 4 > scratch.len {
                out[0] = -(1 as i64);
                return -(1 as i64);
            }
            var b: i64 = 0;
            while b < 4 as i64 {
                scratch[n * 4 + b] = (il >> (8 as i64 * b)) as! u8;
                b += 1 as i64;
            }
            n += 1 as i64;
        }
        il += 1 as i64;
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_U32,
                       scratch[0..n * 4], n, 0 as i64, out);
}

// 0 when the train scale is exactly 1.0f, otherwise 1/scale (native float math).
fn rope_scale(scale: f32) -> f32 {
    var one: f32 = 1.0;
    if scale == one {
        var zero: f32 = 0.0;
        return zero;
    }
    return one / scale;
}

export fn savm_add_kv_from_model(
        st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        tst: i64[:],
        k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:],
        ix: const i64[:], hv: const i64[:], hf: const f32[:],
        ha: const i64[:], haf: const f32[:],
        attrs: const i64[:], scores: const f32[:], prec: const i64[:],
        charmap_len: i64, scratch: u8[:], out: i64[:]) -> i64 {
    // per-layer reductions use n_layer_all - n_layer_nextn, exactly
    // llama_hparams::n_layer(); the writer's TS_NLAYER slot carries it.
    let nl: i64 = ix[I_NLAYER_ALL] - ix[I_NLAYER_NEXTN];
    if nl < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    tst[TS_NLAYER] = nl;
    let av: i64 = ix[I_AV];
    var rc: i64 = 0;

    rc = kstr(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_GENERAL_ARCHITECTURE, ss(s_off, s_len, s_pool, S_ARCH), out);
    rc = kstr(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_GENERAL_NAME, ss(s_off, s_len, s_pool, S_NAME), out);
    if ix[I_NPREC] > 0 as i64 {
        rc = kstrs(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_GENERAL_TENSOR_EXTRA_NAME, s_off, s_len, s_pool, ix[I_PREC_FIRST], ix[I_NPREC], scratch, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        rc = kprec(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_GENERAL_TENSOR_EXTRA_PREC_A4, s_off, s_len, s_pool, ix[I_PREC_FIRST], ix[I_NPREC], prec, scratch, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_VOCAB_SIZE, hv[0], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_CONTEXT_LENGTH, hv[1], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EMBEDDING_LENGTH, hv[2], out);
    if hv[3] > 0 as i64 {
        rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EMBEDDING_LENGTH_OUT, hv[3], out);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_BLOCK_COUNT, hv[4], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_LEADING_DENSE_BLOCK_COUNT, hv[5], out);
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_FEED_FORWARD_LENGTH, K_U32, scratch, ha[0 * av..(0 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_EXPERT_FEED_FORWARD_LENGTH, K_U32, scratch, ha[1 * av..(1 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_LATENT_LENGTH, hv[6], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_MOE_LATENT_SIZE, hv[7], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_SHARED_FEED_FORWARD_LENGTH, hv[8], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_CHUNK_FEED_FORWARD_LENGTH, hv[9], out);
    rc = karrf(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_SWIGLU_CLAMP_EXP, scratch, haf[2 * av..(2 + 1) * av], ix[I_NLAYER_ALL], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = karrf(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_SWIGLU_CLAMP_SHEXP, scratch, haf[3 * av..(3 + 1) * av], ix[I_NLAYER_ALL], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_USE_PARALLEL_RESIDUAL, hv[10], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_COUNT, hv[11], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_USED_COUNT, hv[12], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_SHARED_COUNT, hv[13], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_GROUP_COUNT, hv[14], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_GROUP_USED_COUNT, hv[15], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_WEIGHTS_SCALE, f32_bits(hf[16]), out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_WEIGHTS_NORM, hv[17], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_GATING_FUNC, hv[18], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERT_GROUP_SCALE, f32_bits(hf[19]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EXPERTS_PER_GROUP, hv[20], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_MOE_EVERY_N_LAYERS, hv[21], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_NEXTN_PREDICT_LAYERS, hv[22], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_NUM_DEEPSTACK_LAYERS, hv[23], out);
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_DEEPSTACK_MAPPING, K_I32, scratch, ha[4 * av..(4 + 1) * av], ix[I_AV], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_POOLING_TYPE, hv[24], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_LOGIT_SCALE, f32_bits(hf[25]), out);
    rc = kv_i32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_DECODER_START_TOKEN_ID, hv[26], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_DECODER_BLOCK_COUNT, hv[27], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTN_LOGIT_SOFTCAPPING, f32_bits(hf[28]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROUTER_LOGIT_SOFTCAPPING, f32_bits(hf[29]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_FINAL_LOGIT_SOFTCAPPING, f32_bits(hf[30]), out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SWIN_NORM, hv[31], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_RESCALE_EVERY_N_LAYERS, hv[32], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TIME_MIX_EXTRA_DIM, hv[33], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TIME_DECAY_EXTRA_DIM, hv[34], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_RESIDUAL_SCALE, f32_bits(hf[35]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EMBEDDING_SCALE, f32_bits(hf[36]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HRM_LAYERS_PER_STACK, hv[37], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HRM_H_CYCLES, hv[38], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HRM_L_CYCLES, hv[39], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HRM_PREFIX_LM, hv[40], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKEN_SHIFT_COUNT, hv[41], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_INTERLEAVE_MOE_LAYER_STEP, hv[42], out);
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_HEAD_COUNT, K_U32, scratch, ha[5 * av..(5 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_HEAD_COUNT_KV, K_U32, scratch, ha[6 * av..(6 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_MAX_ALIBI_BIAS, f32_bits(hf[43]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_CLAMP_KQV, f32_bits(hf[44]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_KEY_LENGTH, hv[45], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_LENGTH, hv[46], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_LAYERNORM_EPS, f32_bits(hf[47]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_LAYERNORM_RMS_EPS, f32_bits(hf[48]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_POST_NORM_EPS, f32_bits(hf[49]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_GROUPNORM_EPS, f32_bits(hf[50]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_GROUPNORM_GROUPS, hv[51], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_EXPERT_COUNT, hv[52], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_EXPERT_USED_COUNT, hv[53], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_CAUSAL, hv[54], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_Q_LORA_RANK, hv[55], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_KV_LORA_RANK, hv[56], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_DECAY_LORA_RANK, hv[57], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_ICLR_LORA_RANK, hv[58], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_RESIDUAL_MIX_LORA_RANK, hv[59], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_GATE_LORA_RANK, hv[60], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_RELATIVE_BUCKETS_COUNT, hv[61], out);
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_ROPE_PATTERN, K_U32, scratch, ha[7 * av..(7 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_SLIDING_WINDOW, hv[62], out);
    if hv[63] != ix[I_SWA_NONE] {
        rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_SLIDING_WINDOW_PATTERN, K_U32, scratch, ha[8 * av..(8 + 1) * av], ix[I_NLAYER_ALL], 0 as i64, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
    }
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_SCALE, f32_bits(hf[64]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_OUTPUT_SCALE, f32_bits(hf[65]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_SCALE, f32_bits(hf[66]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_TEMPERATURE_LENGTH, hv[67], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_TEMPERATURE_SCALE, f32_bits(hf[68]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_KEY_LENGTH_MLA, hv[69], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_LENGTH_MLA, hv[70], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_KEY_LENGTH_SWA, hv[71], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_LENGTH_SWA, hv[72], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_KEY_LENGTH_MLA_SWA, hv[73], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_VALUE_LENGTH_MLA_SWA, hv[74], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_KV_LORA_RANK_SWA, hv[75], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_INDEXER_HEAD_COUNT, hv[76], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_INDEXER_KEY_LENGTH, hv[77], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_INDEXER_TOP_K, hv[78], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_INDEXER_BLOCK_SIZE, hv[79], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_INDEXER_KPOOL, hv[80], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_INDEXER_KPOOL_SELECT_TAIL, hv[81], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_INDEXER_LOCAL_BLOCKS, hv[82], out);
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_INDEXER_TYPES, K_U32, scratch, ha[9 * av..(9 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_RECURRENT_LAYERS, K_U32, scratch, ha[10 * av..(10 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_OUTPUT_GROUP_COUNT, hv[83], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_OUTPUT_LORA_RANK, hv[84], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTENTION_COMPRESS_ROPE_FREQ_BASE, f32_bits(hf[85]), out);
    if ix[I_ARCH] == ix[I_ARCH_DSV4] || hv[86] > 0 as i64 {
        rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_COMPRESS_RATIOS, K_U32, scratch, ha[11 * av..(11 + 1) * av], ix[I_NLAYER_ALL], 0 as i64, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
    } else {
        rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ATTENTION_COMPRESS_RATIOS, K_U32, scratch, ha[12 * av..(12 + 1) * av], tst[TS_NLAYER], 1 as i64, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HYPER_CONNECTION_COUNT, hv[86], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HYPER_CONNECTION_SINKHORN_ITERATIONS, hv[87], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HYPER_CONNECTION_EPSILON, f32_bits(hf[88]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HYPER_CONNECTION_MAGNITUDE, f32_bits(hf[89]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HASH_LAYER_COUNT, hv[90], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_HYPER_CONNECTION_LOW_RANK, hv[91], out);
    if hv[92] > 0 as i64 {
        rc = kple(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_PLE_LAYERS, ha[13 * av..(13 + 1) * av], ix[I_NLAYER_ALL], scratch, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_PLE_NGRAM_SIZE, hv[93], out);
        rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_PLE_HEADS_PER_NGRAM, hv[94], out);
        rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_PLE_CONV_KERNEL, hv[95], out);
        rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_PLE_EOS_TOKEN_ID, hv[96], out);
        rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_EMBEDDING_LENGTH_PER_LAYER, hv[97], out);
        rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_PLE_LAYER_MULTIPLIERS, K_U64, scratch, ha[14 * av..(14 + 1) * av], ix[I_PLE_NGRAM], 0 as i64, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_PLE_HEAD_OFFSETS, K_U64, scratch, ha[15 * av..(15 + 1) * av], ix[I_PLE_HEADS], 0 as i64, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_PLE_HEAD_VOCAB_SIZES, K_U64, scratch, ha[16 * av..(16 + 1) * av], ix[I_PLE_HEADS], 0 as i64, out);
        if rc < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_DIMENSION_COUNT, hv[98], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_DIMENSION_COUNT_SWA, hv[99], out);
    rc = karr(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_ROPE_DIMENSION_SECTIONS, K_I32, scratch, ha[17 * av..(17 + 1) * av], ix[I_ROPE_SECTIONS_N], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kstr(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SECTION_ORDER, ss(s_off, s_len, s_pool, S_ROPE_ORDER), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_FREQ_BASE, f32_bits(hf[100]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_FREQ_BASE_SWA, f32_bits(hf[101]), out);
    rc = kstr(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_TYPE, ss(s_off, s_len, s_pool, S_ROPE_SCALE), out);
    let _rsf: f32 = rope_scale(hf[102]);
        rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_FACTOR, f32_bits(_rsf), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_ATTN_FACTOR, f32_bits(hf[103]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_ORIG_CTX_LEN, hv[104], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_FINETUNED, hv[105], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_YARN_LOG_MUL, f32_bits(hf[106]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_YARN_EXT_FACTOR, f32_bits(hf[107]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_YARN_ATTN_FACTOR, f32_bits(hf[108]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_YARN_BETA_FAST, f32_bits(hf[109]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ROPE_SCALING_YARN_BETA_SLOW, f32_bits(hf[110]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SSM_INNER_SIZE, hv[111], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SSM_CONV_KERNEL, hv[112], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SSM_STATE_SIZE, hv[113], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SSM_TIME_STEP_RANK, hv[114], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SSM_GROUP_COUNT, hv[115], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SSM_DT_B_C_RMS, hv[116], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_KDA_HEAD_DIM, hv[117], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_KDA_SAFE_GATE, hv[118], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_KDA_GATE_LOWER_BOUND, f32_bits(hf[119]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_WKV_HEAD_SIZE, hv[120], out);
    rc = kstr(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_MODEL, ss(s_off, s_len, s_pool, S_TOK_MODEL), out);
    rc = kstr(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_PRE, ss(s_off, s_len, s_pool, S_TOK_PRE), out);
    rc = kstrs(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_LIST, s_off, s_len, s_pool, ix[I_TOK_FIRST], ix[I_NTOKENS], scratch, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = ktoktypes(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_TOKENIZER_TOKEN_TYPE, attrs, ix[I_NTOKENS], scratch, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_TOKEN_TYPE_COUNT, hv[121], out);
    rc = kscores(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_TOKENIZER_SCORES, scores, ix[I_NTOKENS], scratch, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kstrs(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_MERGES, s_off, s_len, s_pool, ix[I_MERGE_FIRST], ix[I_NMERGES], scratch, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    if hv[122] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_BOS_ID, hv[122], out);
        }
    if hv[123] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_EOS_ID, hv[123], out);
        }
    if hv[124] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_EOT_ID, hv[124], out);
        }
    if hv[125] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_EOM_ID, hv[125], out);
        }
    if hv[126] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_UNK_ID, hv[126], out);
        }
    if hv[127] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_SEP_ID, hv[127], out);
        }
    if hv[128] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_PAD_ID, hv[128], out);
        }
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_ADD_BOS, hv[129], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_ADD_EOS, hv[130], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_ADD_SEP, hv[131], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_ADD_PREFIX, hv[132], out);
    rc = kbool(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_REMOVE_EXTRA_WS, hv[133], out);
    if charmap_len > 0 as i64 {
            // native reaches the char overload and GGML_ABORTs; the port refuses
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
    if hv[134] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_FIM_PRE_ID, hv[134], out);
        }
    if hv[135] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_FIM_SUF_ID, hv[135], out);
        }
    if hv[136] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_FIM_MID_ID, hv[136], out);
        }
    if hv[137] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_FIM_PAD_ID, hv[137], out);
        }
    if hv[138] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_FIM_REP_ID, hv[138], out);
        }
    if hv[139] != ix[I_TOKEN_NULL] {
            rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_TOKENIZER_FIM_SEP_ID, hv[139], out);
        }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_POSNET_EMBEDDING_LENGTH, hv[140], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_POSNET_BLOCK_COUNT, hv[141], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_CONVNEXT_EMBEDDING_LENGTH, hv[142], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_CONVNEXT_BLOCK_COUNT, hv[143], out);
    rc = kstrs(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_CLASSIFIER_OUTPUT_LABELS, s_off, s_len, s_pool, ix[I_LABEL_FIRST], ix[I_NLABELS], scratch, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_SHORTCONV_L_CACHE, hv[144], out);
    rc = karrf(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_XIELU_ALPHA_N, scratch, haf[18 * av..(18 + 1) * av], ix[I_AV], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = karrf(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_XIELU_ALPHA_P, scratch, haf[19 * av..(19 + 1) * av], ix[I_AV], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = karrf(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_XIELU_BETA, scratch, haf[20 * av..(20 + 1) * av], ix[I_AV], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = karrf(st, pool, key_off, key_len, types, arr, v0, v1, v2, tst, k_off, k_len, k_pool, K_XIELU_EPS, scratch, haf[21 * av..(21 + 1) * av], ix[I_AV], 0 as i64, out);
    if rc < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ATTN_RES_BLOCK_SIZE, hv[145], out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ACTIVATION_SITU_BETA, f32_bits(hf[146]), out);
    rc = kf32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_ACTIVATION_SITU_LINEAR_BETA, f32_bits(hf[147]), out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_DENSE_2_FEAT_IN, hv[148], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_DENSE_2_FEAT_OUT, hv[149], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_DENSE_3_FEAT_IN, hv[150], out);
    rc = kv_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2, k_off, k_len, k_pool, K_DENSE_3_FEAT_OUT, hv[151], out);

    out[0] = 0 as i64;
    return 0 as i64;
}


// ---- S5: add_tensors_from_model (451-488) --------------------------------
//
// The host passes a flat tensor table (name, n_dims, ne[4], type, data,
// pointer identity) plus:
//   gfix   the 28 model-level slots below, a tensor index or -1 for null;
//   lptrs  ln * LN tensor indices in native struct order (LN is the native
//          sizeof(llama_layer)/sizeof(ggml_tensor *), supplied by the gate).
//
// Rules kept from native:
//   - output and tok_embd sharing a name: tok_embd is not written twice, and
//     the alias branch only triggers when output is non-null and the names
//     differ (native dereferences both names at that point);
//   - a null slot is skipped;
//   - the layer scan skips a pointer already added anywhere (looped
//     architectures alias the same tensor across cache slots);
//   - add_tensor itself still skips a duplicate rope tensor and refuses any
//     other duplicate name (native GGML_ABORT).
//
// Returns the number of descriptors added, or -1 on a refusal.

const G_TYPE_EMBD: i64 = 1;
const G_POS_EMBD: i64 = 2;
const G_TOK_NORM: i64 = 3;
const G_TOK_NORM_B: i64 = 4;
const G_OUTPUT_NORM: i64 = 5;
const G_OUTPUT_NORM_B: i64 = 6;
const G_OUTPUT: i64 = 7;
const G_OUTPUT_B: i64 = 8;
const G_OUTPUT_NORM_ENC: i64 = 9;
const G_OUTPUT_S: i64 = 10;
const G_OUTPUT_IN_S: i64 = 11;
const G_OUTPUT_RES_SCORE: i64 = 12;
const G_NEXTN_PROJ_PRE: i64 = 13;
const G_NEXTN_PROJ_POST: i64 = 14;
const G_CLS: i64 = 15;
const G_CLS_B: i64 = 16;
const G_CLS_OUT: i64 = 17;
const G_CLS_OUT_B: i64 = 18;
const G_CLS_NORM: i64 = 19;
const G_HRM_Z_L_INIT: i64 = 20;
const G_HC_HEAD_FN: i64 = 21;
const G_HC_HEAD_BASE: i64 = 22;
const G_HC_HEAD_SCALE: i64 = 23;
const G_PER_LAYER_TOK_EMBD: i64 = 24;
const G_HC_HEAD_NORM: i64 = 25;
const G_HC_HEAD_DOWN: i64 = 26;
const G_HC_HEAD_UP: i64 = 27;
const G_TOK_EMBD: i64 = 28;

fn t_add(tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
         t: i64, tname_off: const i64[:], tname_len: const i64[:], tname_pool: const u8[:],
         tnd: const i64[:], tne4: const i64[:], ttype: const i64[:],
         tdata_off: const i64[:], tdata_len: const i64[:], tdata_pool: const u8[:]) -> i64 {
    if t < 0 as i64 {
        return 0 as i64;
    }
    let name: const u8[:] = tname_pool[tname_off[t]..tname_off[t] + tname_len[t]];
    let base: i64 = t * 4 as i64;
    let ne: const i64[:] = tne4[base..base + 4 as i64];
    let data: const u8[:] = tdata_pool[tdata_off[t]..tdata_off[t] + tdata_len[t]];
    return sav_add_tensor(tst, tpool, tne, trec, name, tnd[t], ne, ttype[t], data);
}

fn t_same_name(t: i64, u: i64,
               tname_off: const i64[:], tname_len: const i64[:], tname_pool: const u8[:]) -> bool {
    if t < 0 as i64 || u < 0 as i64 {
        return false;
    }
    return tname_pool[tname_off[t]..tname_off[t] + tname_len[t]] ==
           tname_pool[tname_off[u]..tname_off[u] + tname_len[u]];
}

export fn savm_add_tensors_from_model(
        tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
        gfix: const i64[:],
        lptrs: const i64[:], ln: i64, lnptrs: i64,
        tname_off: const i64[:], tname_len: const i64[:], tname_pool: const u8[:],
        tnd: const i64[:], tne4: const i64[:], ttype: const i64[:],
        tdata_off: const i64[:], tdata_len: const i64[:], tdata_pool: const u8[:],
        out: i64[:]) -> i64 {
    if ln < 0 as i64 || lnptrs < 0 as i64 || ln * lnptrs > lptrs.len {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    var added: i64 = 0;

    // some models use the same tensor for tok_embd and output
    let out_t: i64 = gfix[G_OUTPUT];
    let tok_t: i64 = gfix[G_TOK_EMBD];
    if out_t >= 0 as i64 && tok_t >= 0 as i64 &&
            !t_same_name(out_t, tok_t, tname_off, tname_len, tname_pool) {
        let r: i64 = t_add(tst, tpool, tne, trec, tok_t, tname_off, tname_len, tname_pool,
                           tnd, tne4, ttype, tdata_off, tdata_len, tdata_pool);
        if r < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        if r == 0 as i64 {
            added += 1 as i64;
        }
    }

    var g: i64 = G_TYPE_EMBD;
    while g < G_TOK_EMBD {
        let t: i64 = gfix[g];
        if t >= 0 as i64 {
            let r: i64 = t_add(tst, tpool, tne, trec, t, tname_off, tname_len, tname_pool,
                               tnd, tne4, ttype, tdata_off, tdata_len, tdata_pool);
            if r < 0 as i64 {
                out[0] = -(1 as i64);
                return -(1 as i64);
            }
            if r == 0 as i64 {
                added += 1 as i64;
            }
        }
        g += 1 as i64;
    }

    var il: i64 = 0;
    while il < ln {
        var f: i64 = 0;
        while f < lnptrs {
            let t: i64 = lptrs[il * lnptrs + f];
            if t >= 0 as i64 {
                // pointer seen already? the identity is the table index
                var seen: bool = false;
                var p: i64 = 0;
                while p < il * lnptrs + f {
                    if lptrs[p] == t {
                        seen = true;
                    }
                    p += 1 as i64;
                }
                if !seen {
                    let r: i64 = t_add(tst, tpool, tne, trec, t, tname_off, tname_len,
                                       tname_pool, tnd, tne4, ttype,
                                       tdata_off, tdata_len, tdata_pool);
                    if r < 0 as i64 {
                        out[0] = -(1 as i64);
                        return -(1 as i64);
                    }
                    if r == 0 as i64 {
                        added += 1 as i64;
                    }
                }
            }
            f += 1 as i64;
        }
        il += 1 as i64;
    }
    return added;
}

export fn savm_supports_arch(arch: i64, no_arch0: i64, no_arch1: i64, no_arch2: i64,
                             no_arch3: i64, no_arch4: i64, no_arch5: i64, no_arch6: i64) -> i64 {
    // llama_model_saver_supports_arch (16): the seven arches whose head
    // tensors are not saved. The host passes their ids so the tree's enum
    // values stay outside the port.
    if arch == no_arch0 || arch == no_arch1 || arch == no_arch2 || arch == no_arch3 ||
            arch == no_arch4 || arch == no_arch5 || arch == no_arch6 {
        return 0 as i64;
    }
    return 1 as i64;
}


const S_N: i64 = 0;
const S_POOL: i64 = 1;

const T_UINT32: i64 = 4;
const T_STRING: i64 = 8;

fn find(st: i64[:], pool: const u8[:], key_off: i64[:], key_len: i64[:], key: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < st[S_N] {
        let o: i64 = key_off[i];
        let n: i64 = key_len[i];
        if pool[o..o + n] == key {
            return i;
        }
        i += 1 as i64;
    }
    return -(1 as i64);
}

fn shift_left(st: i64[:], key_off: i64[:], key_len: i64[:], types: i64[:],
              arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:], at: i64) -> i64 {
    var i: i64 = at;
    while i + (1 as i64) < st[S_N] {
        key_off[i] = key_off[i + (1 as i64)];
        key_len[i] = key_len[i + (1 as i64)];
        types[i] = types[i + (1 as i64)];
        arr[i] = arr[i + (1 as i64)];
        v0[i] = v0[i + (1 as i64)];
        v1[i] = v1[i + (1 as i64)];
        v2[i] = v2[i + (1 as i64)];
        i += 1 as i64;
    }
    st[S_N] = st[S_N] - (1 as i64);
    return at;
}

fn drop(st: i64[:], pool: const u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        key: const u8[:]) -> i64 {
    let at: i64 = find(st, pool, key_off, key_len, key);
    if at < 0 as i64 {
        return -(1 as i64);
    }
    return shift_left(st, key_off, key_len, types, arr, v0, v1, v2, at);
}

fn append_key(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:], key: const u8[:]) -> i64 {
    let off: i64 = st[S_POOL];
    var i: i64 = 0;
    while i < key.len {
        pool[off + i] = key[i];
        i += 1 as i64;
    }
    st[S_POOL] = off + key.len;
    key_off[st[S_N]] = off;
    key_len[st[S_N]] = key.len;
    return off;
}

fn reserved_ok(key: const u8[:], t: i64, value: i64) -> bool {
    if key != "general.alignment" {
        return true;
    }
    if t != T_UINT32 {
        return false;
    }
    return value > 0 as i64 && (value & (value - (1 as i64))) == 0 as i64;
}

fn core_s1_init(st: i64[:], key_off: i64[:], key_len: i64[:], types: i64[:],
                  arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:]) -> i64 {
    st[S_N] = 0 as i64;
    st[S_POOL] = 0 as i64;
    key_off[0] = 0 as i64;
    key_len[0] = 0 as i64;
    types[0] = 0 as i64;
    arr[0] = 0 as i64;
    v0[0] = 0 as i64;
    v1[0] = 0 as i64;
    v2[0] = 0 as i64;
    return 0 as i64;
}

// Scalar setter. `bits` is the raw value (u64 bits for u64/i64 and floats).
fn core_s1_set_val(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], t: i64, bits: i64, out: i64[:]) -> i64 {
    if !reserved_ok(key, t, bits) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    types[st[S_N]] = t;
    arr[st[S_N]] = 0 as i64;
    v0[st[S_N]] = bits;
    v1[st[S_N]] = 0 as i64;
    v2[st[S_N]] = 0 as i64;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

// Scalar string setter (type 8); bytes are copied into the pool.
fn core_s1_set_str(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], val: const u8[:], out: i64[:]) -> i64 {
    if !reserved_ok(key, T_STRING, 0 as i64) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    var i: i64 = 0;
    while i < val.len {
        pool[st[S_POOL] + i] = val[i];
        i += 1 as i64;
    }
    types[st[S_N]] = T_STRING;
    arr[st[S_N]] = 0 as i64;
    v0[st[S_N]] = st[S_POOL];
    v1[st[S_N]] = val.len;
    v2[st[S_N]] = val.len;
    st[S_POOL] = st[S_POOL] + val.len;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

// Numeric/bool array. `data` is already the element byte image; `n` elements.
fn core_s1_set_arr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], atype: i64, data: const u8[:], n: i64, out: i64[:]) -> i64 {
    if !reserved_ok(key, atype, 0 as i64) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    var i: i64 = 0;
    while i < data.len {
        pool[st[S_POOL] + i] = data[i];
        i += 1 as i64;
    }
    types[st[S_N]] = atype;
    arr[st[S_N]] = 1 as i64;
    v0[st[S_N]] = st[S_POOL];
    v1[st[S_N]] = n;
    v2[st[S_N]] = data.len;
    st[S_POOL] = st[S_POOL] + data.len;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

// String array: `data` is the NUL-separated concatenation of `n` strings.
fn core_s1_set_arr_str(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                         key: const u8[:], data: const u8[:], n: i64, out: i64[:]) -> i64 {
    if !reserved_ok(key, T_STRING, 0 as i64) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    var i: i64 = 0;
    while i < data.len {
        pool[st[S_POOL] + i] = data[i];
        i += 1 as i64;
    }
    types[st[S_N]] = T_STRING;
    arr[st[S_N]] = 2 as i64;
    v0[st[S_N]] = st[S_POOL];
    v1[st[S_N]] = n;
    v2[st[S_N]] = data.len;
    st[S_POOL] = st[S_POOL] + data.len;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

export fn s1_remove(st: i64[:], pool: const u8[:], key_off: i64[:], key_len: i64[:],
                    types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                    key: const u8[:]) -> i64 {
    return drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
}

// Copy every KV from src to dst with native gguf_set_kv semantics: source order,
// duplicate keys in dst removed, reserved keys revalidated.
export fn s1_set_kv(dst_st: i64[:], dst_pool: u8[:], dst_key_off: i64[:], dst_key_len: i64[:],
                    dst_types: i64[:], dst_arr: i64[:], dst_v0: i64[:], dst_v1: i64[:], dst_v2: i64[:],
                    src_st: const i64[:], src_pool: const u8[:], src_key_off: i64[:], src_key_len: i64[:],
                    src_types: i64[:], src_arr: i64[:], src_v0: i64[:], src_v1: i64[:], src_v2: i64[:],
                    out: i64[:]) -> i64 {
    var k: i64 = 0;
    while k < src_st[S_N] {
        let key: const u8[:] = src_pool[src_key_off[k]..src_key_off[k] + src_key_len[k]];
        let t: i64 = src_types[k];
        if !reserved_ok(key, t, src_v0[k]) {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        let _drop: i64 = drop(dst_st, dst_pool, dst_key_off, dst_key_len, dst_types,
                              dst_arr, dst_v0, dst_v1, dst_v2, key);
        let _off: i64 = append_key(dst_st, dst_pool, dst_key_off, dst_key_len, key);
        let di: i64 = dst_st[S_N];
        dst_types[di] = t;
        dst_arr[di] = src_arr[k];
        dst_v1[di] = src_v1[k];
        if src_arr[k] == 0 as i64 && t != T_STRING {
            dst_v0[di] = src_v0[k];
            dst_v2[di] = 0 as i64;
        } else {
            let off: i64 = dst_st[S_POOL];
            let m: i64 = src_v2[k];
            var j: i64 = 0;
            while j < m {
                dst_pool[off + j] = src_pool[src_v0[k] + j];
                j += 1 as i64;
            }
            dst_v0[di] = off;
            dst_v2[di] = m;
            dst_st[S_POOL] = off + m;
        }
        dst_st[S_N] = di + (1 as i64);
        k += 1 as i64;
    }
    out[0] = 0 as i64;
    return 0 as i64;
}

// ---- S2: little-endian byte helpers and the metadata section ---------------

const W_MAGIC_0: i64 = 71;
const W_MAGIC_1: i64 = 71;
const W_MAGIC_2: i64 = 85;
const W_MAGIC_3: i64 = 70;

fn w_u8(out: u8[:], p: i64, v: i64) -> i64 {
    out[p] = (v & 255 as i64) as u8;
    return p + (1 as i64);
}

fn w_u32(out: u8[:], p: i64, v: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < 4 as i64 {
        out[q] = ((v >> (8 as i64 * i)) & 255 as i64) as u8;
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_u64(out: u8[:], p: i64, v: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < 8 as i64 {
        out[q] = ((v >> (8 as i64 * i)) & 255 as i64) as u8;
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_bytes(out: u8[:], p: i64, src: const u8[:], off: i64, len: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < len {
        out[q] = src[off + i];
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_str(out: u8[:], p: i64, src: const u8[:], off: i64, len: i64) -> i64 {
    var q = w_u64(out, p, len);
    return w_bytes(out, q, src, off, len);
}

fn w_bits(out: u8[:], p: i64, bits: i64, n: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < n {
        out[q] = ((bits >> (8 as i64 * i)) & 255 as i64) as u8;
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_type_size(t: i64) -> i64 {
    if t == 0 as i64 || t == 1 as i64 || t == 7 as i64 {
        return 1 as i64;
    }
    if t == 2 as i64 || t == 3 as i64 {
        return 2 as i64;
    }
    if t == 4 as i64 || t == 5 as i64 || t == 6 as i64 {
        return 4 as i64;
    }
    return 8 as i64;
}

fn w_pad(out: u8[:], p: i64, alignment: i64) -> i64 {
    var q = p;
    while q % alignment != 0 as i64 {
        out[q] = 0 as u8;
        q = q + (1 as i64);
    }
    return q;
}

// Header plus every KV in list order. Returns the offset after the last KV.
fn w_meta(st: const i64[:], pool: const u8[:], key_off: const i64[:],
          key_len: const i64[:], types: const i64[:], arr: const i64[:],
          v0: const i64[:], v1: const i64[:], v2: const i64[:],
          out: u8[:], version: i64, n_tensors: i64) -> i64 {
    var p: i64 = 0;

    p = w_u8(out, p, W_MAGIC_0);
    p = w_u8(out, p, W_MAGIC_1);
    p = w_u8(out, p, W_MAGIC_2);
    p = w_u8(out, p, W_MAGIC_3);
    p = w_u32(out, p, version);
    p = w_u64(out, p, n_tensors);
    p = w_u64(out, p, st[S_N]);

    var i: i64 = 0;
    while i < st[S_N] {
        p = w_str(out, p, pool, key_off[i], key_len[i]);
        let t = types[i];

        if arr[i] != 0 as i64 {
            p = w_u32(out, p, 9 as i64);
            p = w_u32(out, p, t);
            p = w_u64(out, p, v1[i]);
            if arr[i] == 1 as i64 {
                p = w_bytes(out, p, pool, v0[i], v2[i]);
            } else {
                let lim = v0[i] + v2[i];
                var q = v0[i];
                var k: i64 = 0;
                while k < v1[i] {
                    var e = q;
                    while e < lim && pool[e] != 0 as u8 {
                        e = e + (1 as i64);
                    }
                    p = w_str(out, p, pool, q, e - q);
                    q = e + (1 as i64);
                    k = k + (1 as i64);
                }
            }
        } else {
            p = w_u32(out, p, t);
            if t == 8 as i64 {
                p = w_str(out, p, pool, v0[i], v1[i]);
            } else {
                p = w_bits(out, p, v0[i], w_type_size(t));
            }
        }
        i = i + (1 as i64);
    }
    return p;
}

// S2 entry point: metadata only, zero tensors, padded to the alignment.
export fn s2_write(st: i64[:], pool: const u8[:], key_off: const i64[:],
                   key_len: const i64[:], types: const i64[:], arr: const i64[:],
                   v0: const i64[:], v1: const i64[:], v2: const i64[:],
                   out: u8[:], version: i64, alignment: i64, only_meta: i64) -> i64 {
    let p: i64 = w_meta(st, pool, key_off, key_len, types, arr, v0, v1, v2, out, version, 0 as i64);
    return w_pad(out, p, alignment);
}

// ---- S3: tensor descriptors, data section, type size / block size ---------

const TS_N: i64 = 0;
const TS_ALIGN: i64 = 1;
const TS_POOL: i64 = 2;

const TR_STRIDE: i64 = 8;
const TR_NAME_OFF: i64 = 0;
const TR_NAME_LEN: i64 = 1;
const TR_TYPE: i64 = 2;
const TR_DIMS: i64 = 3;
const TR_DATA_OFF: i64 = 4;
const TR_DATA_LEN: i64 = 5;
const TR_OFFSET: i64 = 6;

const GGML_MAX_NAME: i64 = 64;

// ggml_type_traits[type].blck_size, copied from ggml/src/ggml.c. Removed types
// (4, 5, 31-33, 36-38) have blck_size 0 and are refused by add/set_type.
fn t_blck_size(t: i64) -> i64 {
    if t == 0 as i64 || t == 1 as i64 || t == 24 as i64 || t == 25 as i64 ||
       t == 26 as i64 || t == 27 as i64 || t == 28 as i64 || t == 30 as i64 {
        return 1 as i64;
    }
    if t == 2 as i64 || t == 3 as i64 || t == 6 as i64 || t == 7 as i64 ||
       t == 8 as i64 || t == 9 as i64 || t == 20 as i64 || t == 39 as i64 {
        return 32 as i64;
    }
    if t == 40 as i64 || t == 42 as i64 {
        return 64 as i64;
    }
    if t == 41 as i64 {
        return 128 as i64;
    }
    if t >= 10 as i64 && t <= 23 as i64 {
        return 256 as i64;
    }
    if t == 29 as i64 || t == 34 as i64 || t == 35 as i64 {
        return 256 as i64;
    }
    return 0 as i64;
}

// ggml_type_traits[type].type_size, same source.
fn t_type_size(t: i64) -> i64 {
    if t == 0 as i64 { return 4 as i64; }    // f32
    if t == 1 as i64 { return 2 as i64; }    // f16
    if t == 2 as i64 { return 18 as i64; }   // q4_0
    if t == 3 as i64 { return 20 as i64; }   // q4_1
    if t == 6 as i64 { return 22 as i64; }   // q5_0
    if t == 7 as i64 { return 24 as i64; }   // q5_1
    if t == 8 as i64 { return 34 as i64; }   // q8_0
    if t == 9 as i64 { return 36 as i64; }   // q8_1
    if t == 10 as i64 { return 84 as i64; }  // q2_K
    if t == 11 as i64 { return 110 as i64; } // q3_K
    if t == 12 as i64 { return 144 as i64; } // q4_K
    if t == 13 as i64 { return 176 as i64; } // q5_K
    if t == 14 as i64 { return 210 as i64; } // q6_K
    if t == 15 as i64 { return 292 as i64; } // q8_K
    if t == 16 as i64 { return 66 as i64; }  // iq2_xxs
    if t == 17 as i64 { return 74 as i64; }  // iq2_xs
    if t == 18 as i64 { return 98 as i64; }  // iq3_xxs
    if t == 19 as i64 { return 50 as i64; }  // iq1_s
    if t == 20 as i64 { return 18 as i64; }  // iq4_nl
    if t == 21 as i64 { return 110 as i64; } // iq3_s
    if t == 22 as i64 { return 82 as i64; }  // iq2_s
    if t == 23 as i64 { return 136 as i64; } // iq4_xs
    if t == 24 as i64 { return 1 as i64; }   // i8
    if t == 25 as i64 { return 2 as i64; }   // i16
    if t == 26 as i64 { return 4 as i64; }   // i32
    if t == 27 as i64 { return 8 as i64; }   // i64
    if t == 28 as i64 { return 8 as i64; }   // f64
    if t == 29 as i64 { return 56 as i64; }  // iq1_m
    if t == 30 as i64 { return 2 as i64; }   // bf16
    if t == 34 as i64 { return 54 as i64; }  // tq1_0
    if t == 35 as i64 { return 66 as i64; }  // tq2_0
    if t == 39 as i64 { return 17 as i64; }  // mxfp4
    if t == 40 as i64 { return 36 as i64; }  // nvfp4
    if t == 41 as i64 { return 18 as i64; }  // q1_0
    if t == 42 as i64 { return 18 as i64; }  // q2_0
    return 0 as i64;
}

fn t_align_ok(a: i64) -> bool {
    if a <= 0 as i64 {
        return false;
    }
    return (a & (a - (1 as i64))) == 0 as i64;
}

fn t_pad(v: i64, a: i64) -> i64 {
    return ((v + a - (1 as i64)) / a) * a;
}

// ggml_n_dims: the highest dimension with more than one element, at least 1.
// Native writes this value, not a caller-declared count, so the wire value is
// derived from ne[] even when the API receives ndims explicitly.
fn t_dims(tne: const i64[:], i: i64) -> i64 {
    var d: i64 = 3 as i64;
    while d >= 1 as i64 {
        if tne[i * 4 as i64 + d] > 1 as i64 {
            return d + (1 as i64);
        }
        d -= 1 as i64;
    }
    return 1 as i64;
}

fn t_find(tst: const i64[:], tpool: const u8[:], trec: const i64[:], name: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < tst[TS_N] {
        let o: i64 = trec[i * TR_STRIDE + TR_NAME_OFF];
        let n: i64 = trec[i * TR_STRIDE + TR_NAME_LEN];
        if tpool[o..o + n] == name {
            return i;
        }
        i += 1 as i64;
    }
    return -(1 as i64);
}

// ggml_nbytes for a contiguous tensor, from the type table.
// Returns -1 for a type whose block size is 0.
fn t_nbytes(tne: const i64[:], i: i64, ty: i64) -> i64 {
    let blck: i64 = t_blck_size(ty);
    let tsize: i64 = t_type_size(ty);
    if blck <= 0 as i64 || tsize <= 0 as i64 {
        return -(1 as i64);
    }
    var d: i64 = 0;
    while d < 4 as i64 {
        if tne[i * 4 as i64 + d] <= 0 as i64 {
            return 0 as i64;
        }
        d += 1 as i64;
    }
    let row: i64 = tsize * (tne[i * 4 as i64] / blck);
    return row * tne[i * 4 as i64 + 1] * tne[i * 4 as i64 + 2] * tne[i * 4 as i64 + 3];
}

// Cumulative padded offsets: offset[0] = 0,
// offset[i] = offset[i-1] + PAD(nbytes[i-1], alignment).
fn t_recompute(tst: const i64[:], tne: const i64[:], trec: i64[:], from: i64) -> i64 {
    var i: i64 = from;
    if i < 0 as i64 {
        i = 0 as i64;
    }
    while i < tst[TS_N] {
        if i == 0 as i64 {
            trec[TR_OFFSET] = 0 as i64;
        } else {
            let p: i64 = i - (1 as i64);
            let nb: i64 = t_nbytes(tne, p, trec[p * TR_STRIDE + TR_TYPE]);
            if nb < 0 as i64 {
                return -(1 as i64);
            }
            trec[i * TR_STRIDE + TR_OFFSET] =
                trec[p * TR_STRIDE + TR_OFFSET] + t_pad(nb, tst[TS_ALIGN]);
        }
        i += 1 as i64;
    }
    return 0 as i64;
}

export fn s3_init_tensors(tst: i64[:], alignment: i64) -> i64 {
    if !t_align_ok(alignment) {
        return -(1 as i64);
    }
    tst[TS_N] = 0 as i64;
    tst[TS_ALIGN] = alignment;
    tst[TS_POOL] = 0 as i64;
    return 0 as i64;
}

// Add one tensor descriptor. `ne` carries GGML_MAX_DIMS entries (mirroring the
// native ggml_tensor); `ndims` is the caller's declared count and is validated,
// but the wire value follows ggml_n_dims over ne[] exactly like native.
// Returns the new index, or -1 for a duplicate name, an invalid type, a bad
// ndims, a name that the native reader would reject (empty or >=
// GGML_MAX_NAME), or a data image whose length is not nbytes.
fn core_s3_add_tensor(tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
                        name: const u8[:], ndims: i64, ne: const i64[:],
                        t: i64, data: const u8[:]) -> i64 {
    if ndims < 1 as i64 || ndims > 4 as i64 || ne.len < 4 as i64 {
        return -(1 as i64);
    }
    if name.len < 1 as i64 || name.len >= GGML_MAX_NAME {
        return -(1 as i64);
    }
    if t_blck_size(t) <= 0 as i64 {
        return -(1 as i64);
    }
    if t_find(tst, tpool, trec, name) >= 0 as i64 {
        return -(1 as i64);
    }
    let i: i64 = tst[TS_N];
    if (i + (1 as i64)) * TR_STRIDE > trec.len {
        return -(1 as i64);
    }
    let need: i64 = name.len + data.len;
    if tst[TS_POOL] + need > tpool.len {
        return -(1 as i64);
    }

    let noff: i64 = tst[TS_POOL];
    var k: i64 = 0;
    while k < name.len {
        tpool[noff + k] = name[k];
        k += 1 as i64;
    }
    tst[TS_POOL] = noff + name.len;
    trec[i * TR_STRIDE + TR_NAME_OFF] = noff;
    trec[i * TR_STRIDE + TR_NAME_LEN] = name.len;

    var d: i64 = 0;
    while d < 4 as i64 {
        tne[i * 4 as i64 + d] = ne[d];
        if tne[i * 4 as i64 + d] < 0 as i64 {
            return -(1 as i64);
        }
        d += 1 as i64;
    }
    trec[i * TR_STRIDE + TR_TYPE] = t;
    trec[i * TR_STRIDE + TR_DIMS] = t_dims(tne, i);

    let nb: i64 = t_nbytes(tne, i, t);
    if nb < 0 as i64 {
        return -(1 as i64);
    }
    if data.len > 0 as i64 && data.len != nb {
        return -(1 as i64);
    }

    if data.len > 0 as i64 {
        let doff: i64 = tst[TS_POOL];
        var j: i64 = 0;
        while j < data.len {
            tpool[doff + j] = data[j];
            j += 1 as i64;
        }
        trec[i * TR_STRIDE + TR_DATA_OFF] = doff;
        trec[i * TR_STRIDE + TR_DATA_LEN] = data.len;
        tst[TS_POOL] = doff + data.len;
    } else {
        trec[i * TR_STRIDE + TR_DATA_OFF] = 0 as i64;
        trec[i * TR_STRIDE + TR_DATA_LEN] = -(1 as i64);
    }

    tst[TS_N] = i + (1 as i64);
    let rc: i64 = t_recompute(tst, tne, trec, i);
    if rc != 0 as i64 {
        tst[TS_N] = i;
        return -(1 as i64);
    }
    return i;
}

// Change a tensor type and recompute every later offset, like the native
// function. Refuses an unknown tensor, a removed type and a row that does not
// divide by the new block size (native asserts in those cases).
export fn s3_set_tensor_type(tst: i64[:], tpool: const u8[:], tne: i64[:], trec: i64[:],
                             name: const u8[:], t: i64) -> i64 {
    let id: i64 = t_find(tst, tpool, trec, name);
    if id < 0 as i64 {
        return -(1 as i64);
    }
    let blck: i64 = t_blck_size(t);
    if blck <= 0 as i64 || t_type_size(t) <= 0 as i64 {
        return -(1 as i64);
    }
    if (tne[id * 4 as i64] % blck) != 0 as i64 {
        return -(1 as i64);
    }
    trec[id * TR_STRIDE + TR_TYPE] = t;
    return t_recompute(tst, tne, trec, id);
}

// Attach or replace the data image of a tensor. The bytes are copied into the
// tensor pool; the writer emits nbytes of the current type from this image and
// refuses when fewer bytes were supplied.
export fn s3_set_tensor_data(tst: i64[:], tpool: u8[:], tne: const i64[:], trec: i64[:],
                             name: const u8[:], data: const u8[:]) -> i64 {
    let id: i64 = t_find(tst, tpool, trec, name);
    if id < 0 as i64 {
        return -(1 as i64);
    }
    if tst[TS_POOL] + data.len > tpool.len {
        return -(1 as i64);
    }
    let off: i64 = tst[TS_POOL];
    var j: i64 = 0;
    while j < data.len {
        tpool[off + j] = data[j];
        j += 1 as i64;
    }
    trec[id * TR_STRIDE + TR_DATA_OFF] = off;
    trec[id * TR_STRIDE + TR_DATA_LEN] = data.len;
    tst[TS_POOL] = off + data.len;
    return 0 as i64;
}

export fn s3_find(tst: const i64[:], tpool: const u8[:], trec: const i64[:],
                  name: const u8[:]) -> i64 {
    return t_find(tst, tpool, trec, name);
}

// ggml_nbytes of tensor `i` with the port's own type table.
export fn s3_tensor_size(tne: const i64[:], trec: const i64[:], i: i64) -> i64 {
    return t_nbytes(tne, i, trec[i * TR_STRIDE + TR_TYPE]);
}

// Full writer: header, KV in list order, tensor descriptors, pad to the
// alignment, then (only_meta == 0) every tensor's data padded to the alignment.
// Returns bytes written, or -1 when a data image is missing/short or a type is
// unusable. The alignment comes from the tensor state so offsets and padding
// cannot disagree.
fn core_s3_write(st: i64[:], pool: const u8[:], key_off: const i64[:],
                   key_len: const i64[:], types: const i64[:], arr: const i64[:],
                   v0: const i64[:], v1: const i64[:], v2: const i64[:],
                   tst: const i64[:], tpool: const u8[:], tne: const i64[:],
                   trec: const i64[:], out: u8[:], version: i64, only_meta: i64) -> i64 {
    var p: i64 = w_meta(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        out, version, tst[TS_N]);

    var i: i64 = 0;
    while i < tst[TS_N] {
        p = w_str(out, p, tpool, trec[i * TR_STRIDE + TR_NAME_OFF],
                  trec[i * TR_STRIDE + TR_NAME_LEN]);
        p = w_u32(out, p, trec[i * TR_STRIDE + TR_DIMS]);
        var d: i64 = 0;
        while d < trec[i * TR_STRIDE + TR_DIMS] {
            p = w_u64(out, p, tne[i * 4 as i64 + d]);
            d += 1 as i64;
        }
        p = w_u32(out, p, trec[i * TR_STRIDE + TR_TYPE]);
        p = w_u64(out, p, trec[i * TR_STRIDE + TR_OFFSET]);
        i += 1 as i64;
    }

    let a: i64 = tst[TS_ALIGN];
    p = w_pad(out, p, a);

    if only_meta != 0 as i64 {
        return p;
    }

    i = 0;
    while i < tst[TS_N] {
        let ty: i64 = trec[i * TR_STRIDE + TR_TYPE];
        let nb: i64 = t_nbytes(tne, i, ty);
        let dl: i64 = trec[i * TR_STRIDE + TR_DATA_LEN];
        if nb < 0 as i64 || dl < nb {
            return -(1 as i64);
        }
        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF], nb);
        p = w_pad(out, p, a);
        i += 1 as i64;
    }
    return p;
}
// ---- S4: model saver writer path ------------------------------------------
//
// Port of llama_model_saver (src/llama-model-saver.cpp) without a model:
// the arch constructor (36) chooses the GGUF context, the add_kv overloads
// (55-141) pick a GGUF type and a per-layer reduction, add_tensor (143) adds a
// descriptor with the native duplicate/rope rule, and save (490-496) serializes
// the full file. The model constructor (41), add_kv_from_model (157) and
// add_tensors_from_model (451) read a llama_model and belong to S5.
//
// The resolved metadata key name is a parameter: LLM_KV naming lives in
// llama-arch.cpp, not in the saver. The gate obtains it from the native LLM_KV
// object, so the produced bytes are still compared end to end.
//
// Container rules kept from the native template:
//   - per_layer needs a model; the port stores the layer count in tst[3] and
//     a negative count means "no model" and is refused (native asserts);
//   - a per-layer container uses model->hparams.n_layer() values, refuses a
//     count larger than the container (native asserts) and returns without
//     writing when the count is zero;
//   - all per-layer values equal collapses to one scalar of the element type;
//   - otherwise the first n_values elements are written as a GGUF array of the
//     element type. Supported element types are the native template branches:
//     u8, i8, u32, bool, i32, u64, f32.
//
// Element equality is byte equality. Native compares the values themselves, so
// +0.0/-0.0 and NaN payloads are the only places where the two can disagree;
// the gate uses ordinary values.

const TS_NLAYER: i64 = 3;

const K_U8: i64 = 0;
const K_I8: i64 = 1;
const K_U32: i64 = 4;
const K_I32: i64 = 5;
const K_F32: i64 = 6;
const K_BOOL: i64 = 7;
const K_U64: i64 = 10;

fn k_elem_size(t: i64) -> i64 {
    if t == K_U8 || t == K_I8 || t == K_BOOL {
        return 1 as i64;
    }
    if t == K_U32 || t == K_I32 || t == K_F32 {
        return 4 as i64;
    }
    if t == K_U64 {
        return 8 as i64;
    }
    return -(1 as i64);
}

fn k_elem_bits(data: const u8[:], i: i64, esize: i64) -> i64 {
    var v: i64 = 0;
    var b: i64 = 0;
    while b < esize {
        v = v | ((data[i * esize + b] as i64) << (8 as i64 * b));
        b += 1 as i64;
    }
    return v;
}

fn k_elem_eq(data: const u8[:], a: i64, b: i64, esize: i64) -> bool {
    var k: i64 = 0;
    while k < esize {
        if data[a * esize + k] != data[b * esize + k] {
            return false;
        }
        k += 1 as i64;
    }
    return true;
}

fn k_rope_name(name: const u8[:]) -> bool {
    if name == "rope_freqs.weight" {
        return true;
    }
    if name == "rope_factors_long.weight" {
        return true;
    }
    if name == "rope_factors_short.weight" {
        return true;
    }
    return false;
}

// Constructor (36): empty GGUF context, setup alignment and layer count.
// n_layer < 0 means the arch constructor has no model (native model == nullptr).
export fn sav_init(st: i64[:], key_off: i64[:], key_len: i64[:], types: i64[:],
                   arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                   tst: i64[:], alignment: i64, n_layer: i64) -> i64 {
    let _rc: i64 = core_s1_init(st, key_off, key_len, types, arr, v0, v1, v2);
    let rc: i64 = s3_init_tensors(tst, alignment);
    if rc != 0 as i64 {
        return rc;
    }
    tst[TS_NLAYER] = n_layer;
    return 0 as i64;
}

export fn sav_add_u32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                      types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      key: const u8[:], value: i64, out: i64[:]) -> i64 {
    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_U32, value, out);
}

export fn sav_add_i32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                      types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      key: const u8[:], value: i64, out: i64[:]) -> i64 {
    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_I32, value, out);
}

export fn sav_add_u64(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                      types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      key: const u8[:], value: i64, out: i64[:]) -> i64 {
    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_U64, value, out);
}

// `value` carries the raw IEEE-754 bits of the float, as the native overload
// stores them (gguf_set_val_f32 copies the 4-byte image).
export fn sav_add_f32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                      types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      key: const u8[:], value: i64, out: i64[:]) -> i64 {
    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_F32, value, out);
}

// Native gguf_set_val_bool normalizes any non-zero bool to 1 on the wire.
export fn sav_add_bool(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                       types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                       key: const u8[:], value: i64, out: i64[:]) -> i64 {
    var bits: i64 = 0;
    if value != 0 as i64 {
        bits = 1 as i64;
    }
    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_BOOL, bits, out);
}

export fn sav_add_str(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                      types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      key: const u8[:], val: const u8[:], out: i64[:]) -> i64 {
    return core_s1_set_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, val, out);
}

// The char overload exists only to make the native template compile; native
// calls GGML_ABORT. The port refuses and never writes.
export fn sav_add_char(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                       types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                       key: const u8[:], value: i64, out: i64[:]) -> i64 {
    out[0] = -(1 as i64);
    return -(1 as i64);
}

// numeric container overload with the per_layer flag.
export fn sav_add_arr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                      types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      tst: i64[:], key: const u8[:], etype: i64,
                      data: const u8[:], n: i64, per_layer: i64, out: i64[:]) -> i64 {
    let esize: i64 = k_elem_size(etype);
    if esize < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    var n_values: i64 = n;
    if per_layer != 0 as i64 {
        let nl: i64 = tst[TS_NLAYER];
        if nl < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        if nl > n {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        n_values = nl;
    }
    if n_values == 0 as i64 {
        out[0] = 0 as i64;
        return 0 as i64;
    }
    if data.len < n_values * esize {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    if per_layer != 0 as i64 {
        var same: bool = true;
        var i: i64 = 1;
        while i < n_values {
            if !k_elem_eq(data, 0 as i64, i, esize) {
                same = false;
            }
            i += 1 as i64;
        }
        if same {
            return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                              key, etype, k_elem_bits(data, 0 as i64, esize), out);
        }
    }
    return core_s1_set_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, etype, data[0..n_values * esize], n_values, out);
}

// std::vector<std::string> overload: no per-layer, no reduction.
export fn sav_add_strs(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                       types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                       key: const u8[:], data: const u8[:], n: i64, out: i64[:]) -> i64 {
    return core_s1_set_arr_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                          key, data, n, out);
}

// add_tensor (143): a duplicate rope-frequency tensor is skipped (native
// returns after asserting the name is one of the three), any other duplicate
// is refused (native GGML_ABORTs). Returns 0 added, 1 skipped, -1 refused.
export fn sav_add_tensor(tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
                         name: const u8[:], ndims: i64, ne: const i64[:],
                         t: i64, data: const u8[:]) -> i64 {
    if t_find(tst, tpool, trec, name) >= 0 as i64 {
        if k_rope_name(name) {
            return 1 as i64;
        }
        return -(1 as i64);
    }
    let r: i64 = core_s3_add_tensor(tst, tpool, tne, trec, name, ndims, ne, t, data);
    if r < 0 as i64 {
        return -(1 as i64);
    }
    return 0 as i64;
}

// save(FILE *) (494): the full file, data section included, into the buffer.
export fn sav_save(st: i64[:], pool: const u8[:], key_off: const i64[:],
                   key_len: const i64[:], types: const i64[:], arr: const i64[:],
                   v0: const i64[:], v1: const i64[:], v2: const i64[:],
                   tst: const i64[:], tpool: const u8[:], tne: const i64[:],
                   trec: const i64[:], out: u8[:], version: i64) -> i64 {
    return core_s3_write(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                    tst, tpool, tne, trec, out, version, 0 as i64);
}

// ---- exported S1/S2/S3 entry points --------------------------------------
//
// Goose cannot call an exported function from inside the same unit when the
// signature carries slices, so the accepted writer bodies live in core_* and
// these one-line wrappers keep the accepted C ABI names.

export fn s1_init(st: i64[:], key_off: i64[:], key_len: i64[:], types: i64[:],
                  arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:]) -> i64 {
    return core_s1_init(st, key_off, key_len, types, arr, v0, v1, v2);
}

export fn s1_set_val(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], t: i64, bits: i64, out: i64[:]) -> i64 {
    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                           key, t, bits, out);
}

export fn s1_set_str(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], val: const u8[:], out: i64[:]) -> i64 {
    return core_s1_set_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                           key, val, out);
}

export fn s1_set_arr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], atype: i64, data: const u8[:], n: i64,
                     out: i64[:]) -> i64 {
    return core_s1_set_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                           key, atype, data, n, out);
}

export fn s1_set_arr_str(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                         key: const u8[:], data: const u8[:], n: i64, out: i64[:]) -> i64 {
    return core_s1_set_arr_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                               key, data, n, out);
}

export fn s3_add_tensor(tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
                        name: const u8[:], ndims: i64, ne: const i64[:],
                        t: i64, data: const u8[:]) -> i64 {
    return core_s3_add_tensor(tst, tpool, tne, trec, name, ndims, ne, t, data);
}

export fn s3_write(st: i64[:], pool: const u8[:], key_off: const i64[:],
                   key_len: const i64[:], types: const i64[:], arr: const i64[:],
                   v0: const i64[:], v1: const i64[:], v2: const i64[:],
                   tst: const i64[:], tpool: const u8[:], tne: const i64[:],
                   trec: const i64[:], out: u8[:], version: i64, only_meta: i64) -> i64 {
    return core_s3_write(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                         tst, tpool, tne, trec, out, version, only_meta);
}
