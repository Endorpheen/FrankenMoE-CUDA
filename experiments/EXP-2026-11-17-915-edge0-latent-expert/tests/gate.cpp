#include "ggml.h"
#include "ggml-cpu.h"
#include "runtime.h"
#include <openssl/sha.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

static void require(bool ok, const char * why) {
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", why); std::exit(3); }
}
static int64_t h(void * p) { return (int64_t) (uintptr_t) p; }
static uint64_t packet_reads;
extern "C" int64_t fg158_arg(int64_t packet, int64_t index) {
    ++packet_reads;
    return ((int64_t *) (uintptr_t) packet)[index];
}
static std::string digest(const void * p, size_t n) {
    unsigned char bytes[SHA256_DIGEST_LENGTH]; SHA256((const unsigned char *) p, n, bytes);
    char text[65]; for (int i = 0; i < 32; ++i) std::sprintf(text + 2 * i, "%02x", bytes[i]);
    return text;
}
static std::vector<uint8_t> read(const std::string & file, uint64_t offset, size_t bytes) {
    std::ifstream f(file, std::ios::binary); require((bool) f, "open weight segment");
    f.seekg((std::streamoff) offset); std::vector<uint8_t> data(bytes);
    f.read((char *) data.data(), bytes); require((size_t) f.gcount() == bytes, "short weight segment");
    std::printf("PAYLOAD file=%s offset=%llu bytes=%zu sha256=%s\n", file.c_str(),
                (unsigned long long) offset, bytes, digest(data.data(), bytes).c_str());
    return data;
}
static void scale_checks() {
    std::array<float, 1280> scales; scales.fill(2.0f);
    std::array<float, 1282> result; result.fill(-99.0f);
    require(e0_inverse_scales({scales.data(), 1279}, {result.data()+1,1280}) == -1, "short scales");
    require(e0_inverse_scales({scales.data(),1280}, {result.data()+1,1279}) == -1, "short inverse output");
    for (float bad : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::denorm_min()}) {
        scales[1279] = bad;
        require(e0_inverse_scales({scales.data(),1280}, {result.data()+1,1280}) == -2, "invalid scale accepted");
        require(std::all_of(result.begin(),result.end(),[](float x){return x == -99.0f;}), "partial inverse write");
    }
    scales[1279] = 2.0f;
    require(e0_inverse_scales({scales.data(),1280},{result.data()+1,1280}) == 1280, "valid scale conversion");
    require(result.front() == -99.0f && result.back() == -99.0f, "scale output guard");
    require(e0_inverse_scales({scales.data(),1280},{scales.data(),1280}) == 1280 && scales[0] == .5f, "in-place scale conversion");
}
static ggml_tensor * mapped(ggml_tensor * value, ggml_tensor ** candidate, ggml_tensor ** reference,
                            ggml_tensor * inverse, ggml_tensor * native_inverse) {
    if (value == inverse) return native_inverse;
    for (int i = 0; i < 8; ++i) if (value == candidate[i]) return reference[i];
    return value;
}
int main(int argc, char ** argv) {
    require(argc == 2, "pack argument"); goose_init(0, nullptr); scale_checks();
    const std::string pack = argv[1];
    auto raw_labels = read(pack + "/labels.bin",0,98304);
    int cases = 0, invalid = 0; int64_t values = 0;
    for (int layer : {0,47}) for (int bits : {4,8}) for (int group : {0,3,7}) for (int nt : {1,3}) {
        int expert = -1;
        for (int i = 0; i < 512; ++i) {
            int32_t g; std::memcpy(&g,raw_labels.data()+(layer*512+i)*4,4);
            if (g == group) { expert = i; break; }
        }
        require(expert >= 0, "no expert in selected group");
        require(e0_route_group({raw_labels.data(),(int64_t)raw_labels.size()},layer,expert) == group, "route group");
        ggml_context * ctx = ggml_init({64 * 1024 * 1024,nullptr,false}); require(ctx != nullptr,"context");
        ggml_tensor * w[5]; int64_t wh[7]; std::fill(wh,wh+7,-777);
        std::array<std::vector<uint8_t>,5> payload;
        const int64_t group_bytes = 2560LL*1280/32*(bits == 4 ? 18 : 34);
        const int64_t record = 2LL*(1280/256)*110*640+(640/32)*18*1280;
        for (int i=0;i<5;++i) {
            int64_t desc[8];
            const bool du = i < 2;
            const int part = i-2;
            const int64_t offset = du ? layer*16*group_bytes+i*8*group_bytes+group*group_bytes :
                (layer*512LL+expert)*record+(part < 2 ? part*352000 : 704000);
            const int64_t count = du ? group_bytes : (part < 2 ? 352000 : 460800);
            const int n0 = du ? (i == 0 ? 2560 : 1280) : (part < 2 ? 1280 : 640);
            const int n1 = du ? (i == 0 ? 1280 : 2560) : (part < 2 ? 640 : 1280);
            const ggml_type ty = du ? (bits == 4 ? GGML_TYPE_Q4_0 : GGML_TYPE_Q8_0) :
                (part < 2 ? GGML_TYPE_Q3_K : GGML_TYPE_IQ4_NL);
            int64_t rc = du ? e0_du_desc(layer,group,bits,i,{desc,8}) : e0_expert_desc(layer,expert,part,{desc,8});
            require(rc == 0 && desc[1] == offset && desc[2] == count && desc[3] == ty &&
                    desc[4] == n0 && desc[5] == n1 && desc[6] == (int64_t)ggml_row_size(ty,n0), "descriptor differs from arithmetic oracle");
            payload[i] = read(pack + (du ? "/latent_dense_q"+std::to_string(bits)+".bin" : "/experts.bin"),offset,count);
            w[i] = ggml_new_tensor_2d(ctx,ty,n0,n1);
            require(ggml_nbytes(w[i]) == payload[i].size(),"weight payload size");
            std::memcpy(w[i]->data,payload[i].data(),count); wh[i+1] = h(w[i]);
        }
        auto scale_payload = read(pack+"/scales/l"+std::to_string(layer)+".f32",0,5120);
        std::array<float,1280> scales; std::memcpy(scales.data(),scale_payload.data(),5120);
        ggml_tensor * inverse = ggml_new_tensor_1d(ctx,GGML_TYPE_F32,1280);
        ggml_tensor * ni = ggml_new_tensor_1d(ctx,GGML_TYPE_F32,1280);
        require(e0_inverse_scales({scales.data(),1280},{(float*)inverse->data,1280}) == 1280,"real scale conversion");
        for (int i=0;i<1280;++i) ((float*)ni->data)[i] = 1.0f/scales[i];
        require(std::memcmp(ni->data,inverse->data,5120) == 0,"real scale reciprocal bits");
        ggml_tensor * x = ggml_new_tensor_2d(ctx,GGML_TYPE_F32,2560,nt);
        for (int i=0;i<2560*nt;++i) ((float*)x->data)[i] = (float)((i*17+layer*7)%47-23)/64.0f;
        int64_t work[18], nh[10]; std::fill(work,work+18,-888); std::fill(nh,nh+10,-999);
        const auto before_used = ggml_used_mem(ctx);
        auto rejected = [&](int64_t c,int64_t input,int64_t inv, int64_t wl,int64_t al,int64_t nl) {
            require(e0_latent_expert(c,input,inv,{wh+1,wl},{work+1,al},{nh+1,nl}) == 0,"invalid graph accepted");
            require(ggml_used_mem(ctx) == before_used,"invalid input allocated graph");
            require(std::all_of(work,work+18,[](int64_t v){return v == -888;}) &&
                    std::all_of(nh,nh+10,[](int64_t v){return v == -999;}),"invalid input wrote workspace");
            ++invalid;
        };
        rejected(0,h(x),h(inverse),5,16,8); rejected(h(ctx),0,h(inverse),5,16,8);
        rejected(h(ctx),h(x),0,5,16,8); rejected(h(ctx),h(x),h(inverse),4,16,8);
        rejected(h(ctx),h(x),h(inverse),5,15,8); rejected(h(ctx),h(x),h(inverse),5,16,7);
        for (int i=0;i<5;++i) { const int64_t saved=wh[i+1]; wh[i+1]=0;
            rejected(h(ctx),h(x),h(inverse),5,16,8); wh[i+1]=saved; }
        std::swap(wh[1],wh[2]); rejected(h(ctx),h(x),h(inverse),5,16,8); std::swap(wh[1],wh[2]);
        int64_t packet[7] = {0,h(ctx),h(x),0,0,GGML_OP_DUP,0}; packet_reads=0;
        require(gb_common(h(packet)) != 0 && packet_reads > 0,"packet positive control"); packet_reads=0;
        require(e0_latent_expert(h(ctx),h(x),h(inverse),{wh+1,5},{work+1,16},{nh+1,8}) == nh[8] && nh[8] != 0 && packet_reads == 0,"Goose latent graph construction");
        require(work[0] == -888 && work[17] == -888 && nh[0] == -999 && nh[9] == -999 &&
                wh[0] == -777 && wh[6] == -777,"workspace boundary guards");
        ggml_tensor * a[8], *b[8]; for(int i=0;i<8;++i) a[i]=(ggml_tensor*)(uintptr_t)nh[i+1];
        b[0]=ggml_mul_mat(ctx,w[0],x); b[1]=ggml_mul(ctx,b[0],ni);
        b[2]=ggml_mul_mat(ctx,w[2],b[1]); b[3]=ggml_mul_mat(ctx,w[3],b[1]);
        b[4]=ggml_silu(ctx,b[2]); b[5]=ggml_mul(ctx,b[4],b[3]);
        b[6]=ggml_mul_mat(ctx,w[4],b[5]); b[7]=ggml_mul_mat(ctx,w[1],b[6]);
        for (int i=0;i<8;++i) {
            require(a[i]->type==b[i]->type && a[i]->op==b[i]->op && ggml_are_same_shape(a[i],b[i]) &&
                    std::memcmp(a[i]->nb,b[i]->nb,sizeof a[i]->nb)==0 &&
                    std::memcmp(a[i]->op_params,b[i]->op_params,sizeof a[i]->op_params)==0,"graph metadata");
            for(int j=0;j<GGML_MAX_SRC;++j) require(mapped(a[i]->src[j],a,b,inverse,ni)==b[i]->src[j],"graph dependency");
            ggml_set_output(a[i]); ggml_set_output(b[i]);
        }
        ggml_cgraph * graph=ggml_new_graph(ctx); ggml_build_forward_expand(graph,a[7]); ggml_build_forward_expand(graph,b[7]);
        require(ggml_graph_compute_with_ctx(ctx,graph,4)==GGML_STATUS_SUCCESS,"CPU graph compute");
        for(int i=0;i<8;++i) {
            if(std::memcmp(a[i]->data,b[i]->data,ggml_nbytes(a[i])) != 0) { std::fprintf(stderr,"stage=%d\n",i); require(false,"intermediate F32 bits differ"); }
            float peak = 0.0f;
            for(int64_t j=0;j<ggml_nelements(a[i]);++j) {
                const float v = ((float*)a[i]->data)[j]; require(std::isfinite(v),"nonfinite output");
                peak = std::max(peak,std::fabs(v));
            }
            require(peak > 0.0f,"degenerate all-zero stage");
            values+=ggml_nelements(a[i]);
        }
        for(int i=0;i<5;++i) require(std::memcmp(w[i]->data,payload[i].data(),payload[i].size())==0,"weight modified");
        require(std::memcmp(scales.data(),scale_payload.data(),5120)==0 && std::memcmp(ni->data,inverse->data,5120)==0,"scale modified");
        std::printf("CASE layer=%d group=%d expert=%d DU=%d tokens=%d stages=8 values=%lld output_sha256=%s PASS\n",layer,group,expert,bits,nt,(long long)values,digest(a[7]->data,ggml_nbytes(a[7])).c_str());
        ggml_free(ctx); ++cases;
    }
    std::printf("PASS real-weight latent expert cases=%d invalid_graphs=%d stages=%d exact_values=%lld model_runs=0 gpu_runs=0\n",cases,invalid,cases*8,(long long)values);
}
