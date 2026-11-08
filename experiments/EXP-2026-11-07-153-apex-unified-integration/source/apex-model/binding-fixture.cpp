#include "models.h"
#include "bridge.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
struct Name {llm_tensor kind;const char *suffix;int il;};
struct BindingTrace {
 llama_hparams hparams={};std::array<llama_layer,41> layers={};ggml_tensor *tok_embd=nullptr,*output=nullptr,*output_norm=nullptr;int configuration=0,trunk=0;std::vector<std::string> events;
 static constexpr int TENSOR_NOT_REQUIRED=1,TENSOR_DUPLICATED=2,TENSOR_SKIP=4,TENSOR_SKIP_IF_VIRTUAL=8;
 BindingTrace(int c,int f):configuration(c),trunk(f){auto &h=hparams;h.n_embd=2048;h.n_layer_all=41;h.n_layer_nextn=1;h.n_expert=256;h.n_expert_used_arr.fill(8);h.n_ff_arr.fill(4096);h.n_ff_exp_arr.fill(512);h.n_ff_shexp=512;h.n_head_arr.fill(16);h.n_head_kv_arr.fill(2);h.n_embd_head_k_full=256;h.n_embd_head_v_full=256;h.ssm_d_conv=4;h.ssm_d_state=128;h.ssm_n_group=16;h.ssm_dt_rank=32;for(int i=0;i<41;++i)h.is_recr_impl[i]=i<40&&(i+1)%4!=0;}
 Name tn(llm_tensor k,const char *s=nullptr,int il=-1){return {k,s,il};}Name tn(llm_tensor k,int il){return {k,nullptr,il};}
 ggml_tensor *create_tensor(Name n,std::initializer_list<int64_t> d,int flags){std::ostringstream s;s<<n.kind<<":"<<(n.suffix?n.suffix:"")<<":"<<n.il<<":"<<flags;for(auto x:d)s<<":"<<x;events.push_back(s.str());if(flags&TENSOR_SKIP)return nullptr;if(n.kind==LLM_TENSOR_OUTPUT&&(configuration&4))return nullptr;if(n.kind==LLM_TENSOR_FFN_GATE_UP_EXPS&&!(configuration&2))return nullptr;if(n.kind==LLM_TENSOR_ATTN_QKV&&n.il>=0&&(n.il+1)%4==0&&!(configuration&1))return nullptr;if(n.suffix&&std::string(n.suffix)=="bias"&&!(configuration&8))return nullptr;return (ggml_tensor *)(uintptr_t)(4096+64*events.size());}
 void create_tensor_gate_up_exps(llama_layer &,int,int64_t,int64_t,int64_t,int);
 void create_tensor_qkv(llama_layer &,int,int64_t,int64_t,int64_t,int64_t,int);
 void native();
};

void BindingTrace::create_tensor_gate_up_exps(llama_layer & layer, int bid, int64_t n_embd_, int64_t n_ff_, int64_t n_expert_, int flags) {
    if (flags & TENSOR_SKIP) {
        const int skip = TENSOR_NOT_REQUIRED | TENSOR_SKIP;

        create_tensor(tn(LLM_TENSOR_FFN_GATE_UP_EXPS, "weight", bid), {n_embd_, n_ff_ * 2, n_expert_}, skip | TENSOR_SKIP_IF_VIRTUAL);
        create_tensor(tn(LLM_TENSOR_FFN_GATE_EXPS,    "weight", bid), {n_embd_, n_ff_,     n_expert_}, skip);
        create_tensor(tn(LLM_TENSOR_FFN_UP_EXPS,      "weight", bid), {n_embd_, n_ff_,     n_expert_}, skip);
        return;
    }

    layer.ffn_gate_up_exps = create_tensor(tn(LLM_TENSOR_FFN_GATE_UP_EXPS, "weight", bid), {n_embd_, n_ff_ * 2, n_expert_}, TENSOR_NOT_REQUIRED);
    if (layer.ffn_gate_up_exps == nullptr) {
        layer.ffn_gate_exps = create_tensor(tn(LLM_TENSOR_FFN_GATE_EXPS, "weight", bid), {n_embd_, n_ff_, n_expert_}, flags);
        layer.ffn_up_exps   = create_tensor(tn(LLM_TENSOR_FFN_UP_EXPS,   "weight", bid), {n_embd_, n_ff_, n_expert_}, flags);
    }
}
void BindingTrace::create_tensor_qkv(llama_layer & layer, int bid,
        int64_t n_embd_, int64_t n_embd_q_, int64_t n_embd_k_, int64_t n_embd_v_,
        int flags) {
    const int64_t n_embd_qkv = n_embd_q_ + n_embd_k_ + n_embd_v_;

    if (flags & TENSOR_SKIP) {
        const int skip = TENSOR_NOT_REQUIRED | TENSOR_SKIP;

        create_tensor(tn(LLM_TENSOR_ATTN_QKV, "weight", bid), {n_embd_, n_embd_qkv}, skip | TENSOR_SKIP_IF_VIRTUAL);
        create_tensor(tn(LLM_TENSOR_ATTN_QKV, "bias",   bid), {n_embd_qkv},          skip | TENSOR_SKIP_IF_VIRTUAL);
        create_tensor(tn(LLM_TENSOR_ATTN_Q,   "weight", bid), {n_embd_, n_embd_q_},  skip);
        create_tensor(tn(LLM_TENSOR_ATTN_K,   "weight", bid), {n_embd_, n_embd_k_},  skip);
        create_tensor(tn(LLM_TENSOR_ATTN_V,   "weight", bid), {n_embd_, n_embd_v_},  skip);
        create_tensor(tn(LLM_TENSOR_ATTN_Q,   "bias",   bid), {n_embd_q_},           skip);
        create_tensor(tn(LLM_TENSOR_ATTN_K,   "bias",   bid), {n_embd_k_},           skip);
        create_tensor(tn(LLM_TENSOR_ATTN_V,   "bias",   bid), {n_embd_v_},           skip);
        return;
    }

    layer.wqkv = create_tensor(tn(LLM_TENSOR_ATTN_QKV, "weight", bid), {n_embd_, n_embd_qkv}, TENSOR_NOT_REQUIRED | TENSOR_SKIP_IF_VIRTUAL);
    if (layer.wqkv) {
        layer.wqkv_b = create_tensor(tn(LLM_TENSOR_ATTN_QKV, "bias", bid), {n_embd_qkv}, TENSOR_NOT_REQUIRED | TENSOR_SKIP_IF_VIRTUAL);
        // Fused weights may coexist with separate Q/K/V biases in legacy or custom GGUFs.
        if (!layer.wqkv_b) {
            layer.wq_b = create_tensor(tn(LLM_TENSOR_ATTN_Q, "bias", bid), {n_embd_q_}, TENSOR_NOT_REQUIRED);
            layer.wk_b = create_tensor(tn(LLM_TENSOR_ATTN_K, "bias", bid), {n_embd_k_}, TENSOR_NOT_REQUIRED);
            layer.wv_b = create_tensor(tn(LLM_TENSOR_ATTN_V, "bias", bid), {n_embd_v_}, TENSOR_NOT_REQUIRED);
        }
    } else {
        layer.wq = create_tensor(tn(LLM_TENSOR_ATTN_Q, "weight", bid), {n_embd_, n_embd_q_}, flags);
        layer.wk = create_tensor(tn(LLM_TENSOR_ATTN_K, "weight", bid), {n_embd_, n_embd_k_}, flags);
        layer.wv = create_tensor(tn(LLM_TENSOR_ATTN_V, "weight", bid), {n_embd_, n_embd_v_}, flags);
        layer.wq_b = create_tensor(tn(LLM_TENSOR_ATTN_Q, "bias", bid), {n_embd_q_}, TENSOR_NOT_REQUIRED);
        layer.wk_b = create_tensor(tn(LLM_TENSOR_ATTN_K, "bias", bid), {n_embd_k_}, TENSOR_NOT_REQUIRED);
        layer.wv_b = create_tensor(tn(LLM_TENSOR_ATTN_V, "bias", bid), {n_embd_v_}, TENSOR_NOT_REQUIRED);
    }
}
void BindingTrace::native(){const int64_t n_embd=2048,n_vocab=248064,n_ff=4096,n_expert=256,n_expert_used=8,n_embd_head_k=256,n_head=16,n_embd_k_gqa=512,n_embd_v_gqa=512;const int trunk_flags=trunk;    tok_embd = create_tensor(tn(LLM_TENSOR_TOKEN_EMBD, "weight"), { n_embd, n_vocab }, 0);

    // output
    output_norm = create_tensor(tn(LLM_TENSOR_OUTPUT_NORM, "weight"), { n_embd }, 0);
    output = create_tensor(tn(LLM_TENSOR_OUTPUT, "weight"), { n_embd, n_vocab }, TENSOR_NOT_REQUIRED);

    // if output is NULL, init from the input tok embed
    if (output == NULL) {
        output = create_tensor(tn(LLM_TENSOR_TOKEN_EMBD, "weight"), { n_embd, n_vocab }, TENSOR_DUPLICATED);
    }

    auto load_block_trunk = [&](int il, int flags) {
        auto & layer = layers[il];

        const int64_t n_ff_exp   = hparams.n_ff_exp() ? hparams.n_ff_exp() : n_ff / n_expert_used;
        const int64_t n_ff_shexp = hparams.n_ff_shexp ? hparams.n_ff_shexp : n_ff;

        // Calculate dimensions from hyperparameters
        const int64_t head_k_dim = hparams.ssm_d_state;
        const int64_t head_v_dim = hparams.ssm_d_state;
        const int64_t n_k_heads  = hparams.ssm_n_group;
        const int64_t n_v_heads  = hparams.ssm_dt_rank;
        const int64_t key_dim    = head_k_dim * n_k_heads;
        const int64_t value_dim  = head_v_dim * n_v_heads;
        const int64_t conv_dim   = key_dim * 2 + value_dim;

        layer.attn_norm      = create_tensor(tn(LLM_TENSOR_ATTN_NORM,      "weight", il), { n_embd }, flags);
        layer.attn_post_norm = create_tensor(tn(LLM_TENSOR_ATTN_POST_NORM, "weight", il), { n_embd }, flags);

        if (!hparams.is_recr(il)) {
            // Attention layers
            create_tensor_qkv(layer, il, n_embd, n_embd_head_k * n_head * 2, n_embd_k_gqa, n_embd_v_gqa, flags);
            layer.wo = create_tensor(tn(LLM_TENSOR_ATTN_OUT, "weight", il), { n_embd_head_k * n_head, n_embd }, flags);

            // Q/K normalization for attention layers
            layer.attn_q_norm = create_tensor(tn(LLM_TENSOR_ATTN_Q_NORM, "weight", il), { n_embd_head_k }, flags);
            layer.attn_k_norm = create_tensor(tn(LLM_TENSOR_ATTN_K_NORM, "weight", il), { n_embd_head_k }, flags);
        } else {
            // Linear attention (gated delta net) specific tensors
            // Create tensors with calculated dimensions
            layer.wqkv           = create_tensor(tn(LLM_TENSOR_ATTN_QKV,       "weight", il), { n_embd, key_dim * 2 + value_dim }, TENSOR_NOT_REQUIRED);
            layer.wqkv_gate      = create_tensor(tn(LLM_TENSOR_ATTN_GATE,      "weight", il), { n_embd, value_dim }, TENSOR_NOT_REQUIRED);
            layer.ssm_conv1d     = create_tensor(tn(LLM_TENSOR_SSM_CONV1D,     "weight", il), { hparams.ssm_d_conv, conv_dim }, flags);
            layer.ssm_dt         = create_tensor(tn(LLM_TENSOR_SSM_DT,         "bias",   il), { hparams.ssm_dt_rank }, flags);
            layer.ssm_a          = create_tensor(tn(LLM_TENSOR_SSM_A_NOSCAN,             il), { hparams.ssm_dt_rank }, flags);
            layer.ssm_beta       = create_tensor(tn(LLM_TENSOR_SSM_BETA,       "weight", il), { n_embd, n_v_heads }, flags);
            layer.ssm_alpha      = create_tensor(tn(LLM_TENSOR_SSM_ALPHA,      "weight", il), { n_embd, n_v_heads }, flags);
            layer.ssm_norm       = create_tensor(tn(LLM_TENSOR_SSM_NORM,       "weight", il), { head_v_dim }, flags);
            layer.ssm_out        = create_tensor(tn(LLM_TENSOR_SSM_OUT,        "weight", il), { value_dim, n_embd }, flags);
        }

        // Routed experts
        layer.ffn_gate_inp  = create_tensor(tn(LLM_TENSOR_FFN_GATE_INP,  "weight", il), { n_embd, n_expert }, flags);
        layer.ffn_down_exps = create_tensor(tn(LLM_TENSOR_FFN_DOWN_EXPS, "weight", il), { n_ff_exp, n_embd, n_expert }, flags);
        create_tensor_gate_up_exps(layer, il, n_embd, n_ff_exp, n_expert, flags);

        // Shared experts
        layer.ffn_gate_inp_shexp = create_tensor(tn(LLM_TENSOR_FFN_GATE_INP_SHEXP, "weight", il), { n_embd }, flags);
        layer.ffn_gate_shexp     = create_tensor(tn(LLM_TENSOR_FFN_GATE_SHEXP,     "weight", il), { n_embd, n_ff_shexp }, flags);
        layer.ffn_up_shexp       = create_tensor(tn(LLM_TENSOR_FFN_UP_SHEXP,       "weight", il), { n_embd, n_ff_shexp }, flags);
        layer.ffn_down_shexp     = create_tensor(tn(LLM_TENSOR_FFN_DOWN_SHEXP,     "weight", il), { n_ff_shexp, n_embd }, flags);
    };

for(int i=0;i<40;++i)load_block_trunk(i,trunk_flags);}
extern "C" int64_t fg153_am_lget(int64_t e,int64_t k){auto &v=*(BindingTrace *)(intptr_t)e;auto &h=v.hparams;if(k>=100)return h.is_recr(k-100);switch(k){
case 0:return h.n_embd;
case 1:return 248064;
case 2:return h.n_layer();
case 3:return h.n_expert;
case 4:return h.n_expert_used();
case 5:return h.n_ff();
case 6:return h.n_ff_exp();
case 7:return h.n_ff_shexp;
case 8:return h.n_embd_head_k();
case 9:return h.n_head();
case 10:return h.n_embd_k_gqa();
case 11:return h.n_embd_v_gqa();
case 12:return h.ssm_d_state;
case 13:return h.ssm_n_group;
case 14:return h.ssm_dt_rank;
case 15:return h.ssm_d_conv;
case 16:return v.trunk;
case 20:return 1;
case 21:return 4;
case 22:return 8;
case 23:return 2;
case 30:return 41;
case 31:return 4;
default:abort();}}
extern "C" void fg153_am_recurrent(int64_t e,int64_t il,bool v){((BindingTrace *)(intptr_t)e)->hparams.is_recr_impl[il]=v;}
extern "C" int64_t fg153_am_bind(int64_t e,int64_t k,int64_t il,int64_t nd,int64_t d0,int64_t d1,int64_t d2,int64_t flags){auto &v=*(BindingTrace *)(intptr_t)e;Name name={LLM_TENSOR_TOKEN_EMBD,"weight",(int)il};switch(k){
case 0:name.kind=LLM_TENSOR_TOKEN_EMBD;name.suffix="weight";break;
case 5:name.kind=LLM_TENSOR_OUTPUT;name.suffix="weight";break;
case 3:name.kind=LLM_TENSOR_OUTPUT_NORM;name.suffix="weight";break;
case 1:name.kind=LLM_TENSOR_ATTN_NORM;name.suffix="weight";break;
case 2:name.kind=LLM_TENSOR_ATTN_POST_NORM;name.suffix="weight";break;
case 7:name.kind=LLM_TENSOR_ATTN_QKV;name.suffix="weight";break;
case 39:name.kind=LLM_TENSOR_ATTN_QKV;name.suffix="bias";break;
case 9:name.kind=LLM_TENSOR_ATTN_GATE;name.suffix="weight";break;
case 13:name.kind=LLM_TENSOR_ATTN_OUT;name.suffix="weight";break;
case 11:name.kind=LLM_TENSOR_ATTN_Q_NORM;name.suffix="weight";break;
case 12:name.kind=LLM_TENSOR_ATTN_K_NORM;name.suffix="weight";break;
case 21:name.kind=LLM_TENSOR_SSM_CONV1D;name.suffix="weight";break;
case 19:name.kind=LLM_TENSOR_SSM_DT;name.suffix="bias";break;
case 20:name.kind=LLM_TENSOR_SSM_A_NOSCAN;name.suffix=nullptr;break;
case 15:name.kind=LLM_TENSOR_SSM_BETA;name.suffix="weight";break;
case 17:name.kind=LLM_TENSOR_SSM_ALPHA;name.suffix="weight";break;
case 22:name.kind=LLM_TENSOR_SSM_NORM;name.suffix="weight";break;
case 23:name.kind=LLM_TENSOR_SSM_OUT;name.suffix="weight";break;
case 25:name.kind=LLM_TENSOR_FFN_GATE_INP;name.suffix="weight";break;
case 28:name.kind=LLM_TENSOR_FFN_DOWN_EXPS;name.suffix="weight";break;
case 29:name.kind=LLM_TENSOR_FFN_GATE_UP_EXPS;name.suffix="weight";break;
case 27:name.kind=LLM_TENSOR_FFN_GATE_EXPS;name.suffix="weight";break;
case 26:name.kind=LLM_TENSOR_FFN_UP_EXPS;name.suffix="weight";break;
case 6:name.kind=LLM_TENSOR_FFN_GATE_INP_SHEXP;name.suffix="weight";break;
case 35:name.kind=LLM_TENSOR_FFN_GATE_SHEXP;name.suffix="weight";break;
case 33:name.kind=LLM_TENSOR_FFN_UP_SHEXP;name.suffix="weight";break;
case 37:name.kind=LLM_TENSOR_FFN_DOWN_SHEXP;name.suffix="weight";break;
case 43:name.kind=LLM_TENSOR_ATTN_Q;name.suffix="weight";break;
case 40:name.kind=LLM_TENSOR_ATTN_Q;name.suffix="bias";break;
case 45:name.kind=LLM_TENSOR_ATTN_K;name.suffix="weight";break;
case 41:name.kind=LLM_TENSOR_ATTN_K;name.suffix="bias";break;
case 47:name.kind=LLM_TENSOR_ATTN_V;name.suffix="weight";break;
case 42:name.kind=LLM_TENSOR_ATTN_V;name.suffix="bias";break;
default:abort();}
if(k==5&&(flags&2))name.kind=LLM_TENSOR_TOKEN_EMBD;ggml_tensor *out=nullptr;if(nd==1)out=v.create_tensor(name,{d0},flags);if(nd==2)out=v.create_tensor(name,{d0,d1},flags);if(nd==3)out=v.create_tensor(name,{d0,d1,d2},flags);return (int64_t)(intptr_t)out;}
int main(){int passed=0;for(int configuration=0;configuration<16;++configuration){for(int flags:{0,1,4}){BindingTrace n(configuration,flags),g(configuration,flags);n.native();fg153_apex_bind((int64_t)(intptr_t)&g,0);if(n.events!=g.events){std::cerr<<"binding mismatch config="<<configuration<<" flags="<<flags<<" native="<<n.events.size()<<" goose="<<g.events.size()<<"\n";for(size_t i=0;i<std::min(n.events.size(),g.events.size());++i)if(n.events[i]!=g.events[i]){std::cerr<<i<<" "<<n.events[i]<<" / "<<g.events[i]<<"\n";break;}return 1;}++passed;}}BindingTrace p(0,0);p.hparams.is_recr_impl.fill(false);fg153_apex_bind((int64_t)(intptr_t)&p,1);int nr=0;for(int i=0;i<41;++i){bool expected=i<40&&(i+1)%4!=0;if(p.hparams.is_recr_impl[i]!=expected)return 2;nr+=expected;}if(nr!=30)return 3;std::cout<<"PASS "<<passed<<" native/Goose binding traces and 41-layer recurrent policy (30 recurrent,10 attention,1 excluded MTP)\n";}
