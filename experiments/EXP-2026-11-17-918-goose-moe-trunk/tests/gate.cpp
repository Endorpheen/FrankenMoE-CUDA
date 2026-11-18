#include GDN_ORACLE_HEADER
#include <sys/mman.h>
#include <array>
#include <limits>

static_assert(GGML_OP_CLAMP==50 && GGML_GLU_OP_SWIGLU==2 && GGML_SORT_ORDER_DESC==1,"native routing ABI");

struct Mapping {
    int fd; uint8_t * base; size_t size; gguf_context * metadata; ggml_context * meta_tensors;
    ggml_context * weights; int bindings=0;
    explicit Mapping(const char * path) {
        struct stat st;fd=open(path,O_RDONLY);require(fd>=0 && fstat(fd,&st)==0,"model FD");size=st.st_size;
        base=(uint8_t*)mmap(nullptr,size,PROT_READ,MAP_PRIVATE,fd,0);require(base!=MAP_FAILED,"read-only mapping");
        meta_tensors=nullptr;metadata=gguf_init_from_file(path,{true,&meta_tensors});require(metadata && meta_tensors,"GGUF metadata");
        weights=ggml_init({8*1024*1024,nullptr,true});require(weights && ggml_get_no_alloc(weights),"metadata-only weight context");
    }
    ~Mapping(){ggml_free(weights);gguf_free(metadata);ggml_free(meta_tensors);munmap(base,size);close(fd);}
    ggml_tensor * weight(const std::string & name,bool candidate) {
        const int64_t id=gguf_find_tensor(metadata,name.c_str());require(id>=0,name.c_str());
        const int64_t * ne=gguf_get_tensor_ne(metadata,id);const ggml_type ty=gguf_get_tensor_type(metadata,id);
        const uint64_t offset=gguf_get_data_offset(metadata)+gguf_get_tensor_offset(metadata,id);
        const size_t n=gguf_get_tensor_size(metadata,id);require(offset<=size && n<=size-offset,"native descriptor bounds");
        ggml_tensor * t;int64_t shape[4];std::memcpy(shape,ne,sizeof shape);
        if(candidate)t=(ggml_tensor*)(uintptr_t)q35_bind_weight(handle(weights),(uint64_t)(uintptr_t)base,size,offset,ty,{shape,4});
        else {t=ggml_new_tensor_4d(weights,ty,ne[0],ne[1],ne[2],ne[3]);t->data=base+offset;}
        require(t && t->data==base+offset && ggml_nbytes(t)==n,"zero-copy tensor binding");
        if(candidate)++bindings;return t;
    }
    void moe(int layer,bool candidate,ggml_tensor ** w) {
        const char * names[8]={"ffn_gate_inp.weight","ffn_up_exps.weight","ffn_gate_exps.weight","ffn_down_exps.weight",
                              "ffn_up_shexp.weight","ffn_gate_shexp.weight","ffn_down_shexp.weight","ffn_gate_inp_shexp.weight"};
        for(int i=0;i<8;++i)w[i]=weight("blk."+std::to_string(layer)+"."+names[i],candidate);
    }
};

static int bind_contracts(Mapping & m) {
    int checks=0;int64_t shape[4]={256,2,1,1};
    auto reject=[&](int64_t c,uint64_t base,int64_t size,int64_t off,int64_t ty,int64_t length){
        const auto used=ggml_used_mem(m.weights);
        require(q35_bind_weight(c,base,size,off,ty,{shape,length})==0,"invalid mapping accepted");
        require(ggml_used_mem(m.weights)==used,"invalid mapping allocated metadata");++checks;
    };
    const auto base=(uint64_t)(uintptr_t)m.base;const auto ctx=handle(m.weights);
    reject(0,base,m.size,0,12,4);reject(ctx,0,m.size,0,12,4);reject(ctx,UINT64_MAX,m.size,0,12,4);
    reject(ctx,base,0,0,12,4);reject(ctx,base,m.size,-8,12,4);reject(ctx,base,m.size,1,12,4);
    reject(ctx,base,m.size,m.size+8,12,4);reject(ctx,base,287,0,12,4);reject(ctx,base,m.size,0,43,4);
    reject(ctx,base,m.size,0,4,4);reject(ctx,base,m.size,0,12,3);reject(ctx,UINT64_MAX-100,1024,0,12,4);
    for(int axis=0;axis<4;++axis){const auto saved=shape[axis];shape[axis]=0;reject(ctx,base,m.size,0,12,4);shape[axis]=-1;reject(ctx,base,m.size,0,12,4);shape[axis]=saved;}
    shape[0]=255;reject(ctx,base,m.size,0,12,4);shape[0]=256;
    shape[1]=INT64_MAX;reject(ctx,base,INT64_MAX,0,12,4);shape[1]=2;
    shape[0]=(INT64_MAX/256)*256;reject(ctx,0x1000,INT64_MAX,0,10,4);shape[0]=256;
    auto * allocated=ggml_init({1024*1024,nullptr,false});reject(handle(allocated),base,m.size,0,12,4);ggml_free(allocated);
    std::printf("PASS binder invalid checks=%d\n",checks);return checks;
}

static void native_moe(ggml_context * c,ggml_tensor * x,ggml_tensor ** w,const int64_t * cfg,std::vector<ggml_tensor*> & n) {
    const int64_t experts=cfg[0],used=cfg[1],hidden=x->ne[0],t=x->ne[1],base=17+2*used;
    n.resize(26+2*used);
    n[0]=ggml_mul_mat(c,w[0],x);n[1]=ggml_soft_max(c,n[0]);n[2]=ggml_argsort(c,n[1],GGML_SORT_ORDER_DESC);
    n[3]=ggml_view_4d(c,n[2],used,t,1,1,n[2]->nb[1],n[2]->nb[2],n[2]->nb[3],0);
    n[4]=ggml_reshape_3d(c,n[1],1,experts,t);n[5]=ggml_get_rows(c,n[4],n[3]);n[6]=ggml_reshape_2d(c,n[5],used,t);
    n[7]=ggml_sum_rows(c,n[6]);n[8]=ggml_clamp(c,n[7],6.103515625e-5f,INFINITY);n[9]=ggml_div(c,n[6],n[8]);
    n[10]=ggml_reshape_3d(c,n[9],1,used,t);n[11]=n[10];const float scale=unbits(cfg[2]);
    if(scale!=0.0f && scale!=1.0f)n[11]=ggml_scale(c,n[10],scale);
    n[12]=ggml_reshape_3d(c,x,hidden,1,t);n[13]=ggml_mul_mat_id(c,w[1],n[12],n[3]);n[14]=ggml_mul_mat_id(c,w[2],n[12],n[3]);
    n[15]=ggml_swiglu_split(c,n[14],n[13]);n[16]=ggml_mul_mat_id(c,w[3],n[15],n[3]);n[17]=ggml_mul(c,n[16],n[11]);
    for(int i=0;i<used;++i)n[18+i]=ggml_view_2d(c,n[17],hidden,t,n[17]->nb[2],i*n[17]->nb[1]);
    auto * sum=n[18];for(int i=1;i<used;++i){sum=ggml_add(c,sum,n[18+i]);n[17+used+i]=sum;}
    if(used==1)sum=ggml_cont(c,sum);n[base]=sum;
    n[base+1]=ggml_mul_mat(c,w[4],x);n[base+2]=ggml_mul_mat(c,w[5],x);n[base+3]=ggml_swiglu_split(c,n[base+2],n[base+1]);
    n[base+4]=ggml_mul_mat(c,w[6],n[base+3]);n[base+5]=ggml_mul_mat(c,w[7],x);n[base+6]=ggml_sigmoid(c,n[base+5]);
    n[base+7]=ggml_mul(c,n[base+4],n[base+6]);n[base+8]=ggml_add(c,sum,n[base+7]);
}

static ggml_tensor * source_map(ggml_tensor * v,const std::vector<ggml_tensor*> & a,const std::vector<ggml_tensor*> & b,
                               ggml_tensor ** wa,ggml_tensor ** wb) {
    for(size_t i=0;i<a.size();++i)if(v==a[i])return b[i];
    for(int i=0;i<8;++i)if(v==wa[i])return wb[i];return v;
}

static std::vector<float> input_rows(Mapping & m,int64_t hidden) {
    auto * emb=m.weight("token_embd.weight",false);const auto * traits=ggml_get_type_traits(emb->type);require(traits && traits->to_float,"embedding decoder");
    std::vector<float> result(hidden*3);const int ids[3]={1,42,100};
    for(int i=0;i<3;++i)traits->to_float((uint8_t*)emb->data+ids[i]*emb->nb[1],result.data()+i*hidden,hidden);return result;
}

static void ffn_gate(Mapping & m) {
    const int invalid=bind_contracts(m);const int64_t hidden=integer(m.metadata,"qwen35moe.embedding_length");
    auto input=input_rows(m,hidden);ggml_tensor * wa[8],*wb[8];m.moe(0,true,wa);m.moe(0,false,wb);
    int cases=0,rejected=0;int64_t compared=0;
    for(bool observed:{true,false})for(int count:{1,3})for(int variant:{0,1,2}) {
        auto * c=ggml_init({128*1024*1024,nullptr,false});require(c,"FFN arena");
        auto * x=ggml_new_tensor_2d(c,GGML_TYPE_F32,hidden,count);std::memcpy(x->data,input.data(),hidden*count*sizeof(float));
        ggml_tensor * aweights[8],*bweights[8];std::copy(wa,wa+8,aweights);std::copy(wb,wb+8,bweights);
        if(variant==2){auto * tied=ggml_new_tensor_2d(c,GGML_TYPE_F32,hidden,256);std::memset(tied->data,0,ggml_nbytes(tied));aweights[0]=bweights[0]=tied;}
        int64_t cfg[3]={256,variant==1?1:8,bits(variant==1?.5f:0.0f)},wh[8],work[16]={0};
        const int length=26+2*cfg[1];std::vector<int64_t> nodes(length);
        for(int i=0;i<8;++i)wh[i]=handle(aweights[i]);
        auto reject=[&](int64_t ctx,int64_t ih,int wl,int cl,int al,int nl){
            const auto used=ggml_used_mem(c);require(q35_moe_ffn(ctx,ih,{wh,wl},{cfg,cl},{work,al},{nodes.data(),nl})==0,"invalid FFN input accepted");
            require(ggml_used_mem(c)==used && std::all_of(nodes.begin(),nodes.end(),[](int64_t v){return v==0;}),"invalid FFN allocated/wrote nodes");++rejected;
        };
        reject(0,handle(x),8,3,16,length);reject(handle(c),0,8,3,16,length);reject(handle(c),handle(x),7,3,16,length);
        reject(handle(c),handle(x),8,2,16,length);reject(handle(c),handle(x),8,3,15,length);reject(handle(c),handle(x),8,3,16,length-1);
        for(int i=0;i<8;++i){auto saved=wh[i];wh[i]=0;reject(handle(c),handle(x),8,3,16,length);wh[i]=saved;}
        int64_t packet[7]={0,handle(c),handle(x),0,0,GGML_OP_DUP,0};packet_reads=0;require(gb_common(handle(packet)) && packet_reads>0,"packet positive witness");packet_reads=0;
        const auto output=q35_moe_ffn(handle(c),handle(x),{wh,8},{cfg,3},{work,16},{nodes.data(),length});
        require(output && output==nodes.back() && packet_reads==0,"Goose MoE build");
        std::vector<ggml_tensor*> a(length),b;for(int i=0;i<length;++i)a[i]=(ggml_tensor*)(uintptr_t)nodes[i];native_moe(c,x,bweights,cfg,b);
        for(int i=0;i<length;++i){
            if(a[i]->op!=b[i]->op || a[i]->type!=b[i]->type || !ggml_are_same_shape(a[i],b[i]) || std::memcmp(a[i]->nb,b[i]->nb,sizeof a[i]->nb) || std::memcmp(a[i]->op_params,b[i]->op_params,sizeof a[i]->op_params)){
                std::fprintf(stderr,"META stage=%d op=%d/%d\n",i,a[i]->op,b[i]->op);require(false,"MoE metadata");}
            for(int j=0;j<GGML_MAX_SRC;++j)require(source_map(a[i]->src[j],a,b,aweights,bweights)==b[i]->src[j],"MoE graph edge");
            if(observed){ggml_set_output(a[i]);ggml_set_output(b[i]);}
        }
        auto * graph=ggml_new_graph(c);ggml_build_forward_expand(graph,a.back());ggml_build_forward_expand(graph,b.back());
        require(ggml_graph_compute_with_ctx(c,graph,4)==GGML_STATUS_SUCCESS,"MoE compute");
        for(int i=0;i<length;++i){if(!observed && i!=length-1)continue;
            if(std::memcmp(a[i]->data,b[i]->data,ggml_nbytes(a[i]))){std::fprintf(stderr,"NUM stage=%d\n",i);require(false,"MoE computed bits");}
            if(a[i]->type==GGML_TYPE_F32)for(int64_t j=0;j<ggml_nelements(a[i]);++j)require(std::isfinite(((float*)a[i]->data)[j]),"MoE nonfinite");
            compared+=ggml_nelements(a[i]);
        }
        bool nonzero=false;for(int64_t j=0;j<ggml_nelements(a.back());++j)nonzero=nonzero || ((float*)a.back()->data)[j]!=0.0f;require(nonzero,"zero MoE result");
        std::printf("CASE FFN T=%d variant=%d observed=%d PASS\n",count,variant,observed);ggml_free(c);++cases;
    }
    std::printf("PASS real FFN cases=%d binder_rejections=%d graph_rejections=%d compared_values=%lld zero_copy_weights=%d model_server_runs=0\n",cases,invalid,rejected,(long long)compared,m.bindings);
}

struct Trunk {
    static constexpr int capacity=512;
    Mapping & model;ggml_context * owner;
    int64_t layer_count,hidden,acfg[14],gcfg[7],mcfg[3],state[6]={0};int sections[4];int native_past=0;
    std::vector<int64_t> records;
    std::vector<std::array<ggml_tensor*,11>> branch;
    std::vector<std::array<ggml_tensor*,8>> moes;
    std::vector<std::array<ggml_tensor*,4>> stores,refs;
    ggml_tensor * emb,*head,*norm,*nemb,*nhead,*nnorm;
    explicit Trunk(Mapping & mapping):model(mapping) {
        auto * m=model.metadata;layer_count=integer(m,"qwen35moe.block_count")-integer(m,"qwen35moe.nextn_predict_layers");
        require(layer_count==40,"actual trunk layer count");hidden=integer(m,"qwen35moe.embedding_length");
        const auto h=integer(m,"qwen35moe.ssm.state_size"),nk=integer(m,"qwen35moe.ssm.group_count"),nv=integer(m,"qwen35moe.ssm.time_step_rank"),kernel=integer(m,"qwen35moe.ssm.conv_kernel");
        const auto heads=integer(m,"qwen35moe.attention.head_count"),hk=integer(m,"qwen35moe.attention.head_count_kv");
        const auto hd=integer(m,"qwen35moe.attention.key_length");const float eps=real(m,"qwen35moe.attention.layer_norm_rms_epsilon");
        require(q35_gdn_config(h,nk,nv,kernel,bits(eps),{gcfg,7})==7,"trunk GDN config");
        int64_t a[14]={heads,hk,integer(m,"qwen35moe.rope.dimension_count"),GGML_ROPE_TYPE_IMROPE,
            integer(m,"qwen35moe.context_length"),bits(eps),bits(real(m,"qwen35moe.rope.freq_base")),bits(1.0f),bits(0.0f),bits(1.0f),bits(32.0f),bits(1.0f),bits(1.0f/std::sqrt((float)hd)),GGML_PREC_F32};
        std::memcpy(acfg,a,sizeof a);mcfg[0]=integer(m,"qwen35moe.expert_count");mcfg[1]=integer(m,"qwen35moe.expert_used_count");mcfg[2]=bits(0.0f);
        require(mcfg[0]==256 && mcfg[1]==8,"actual Apex routing");
        auto key=gguf_find_key(m,"qwen35moe.rope.dimension_sections");require(key>=0 && gguf_get_arr_n(m,key)==4,"rope sections");std::memcpy(sections,gguf_get_arr_data(m,key),sizeof sections);
        key=gguf_find_key(m,"qwen35moe.attention.recurrent_layers");require(key>=0 && gguf_get_arr_type(m,key)==GGUF_TYPE_BOOL && gguf_get_arr_n(m,key)>=40,"layer flags");
        auto * flags=(const uint8_t*)gguf_get_arr_data(m,key);
        records.resize(layer_count*32);branch.resize(layer_count);moes.resize(layer_count);stores.resize(layer_count);refs.resize(layer_count);
        owner=ggml_init({192*1024*1024,nullptr,false});require(owner,"persistent model-state arena");
        const char * recurrent[11]={"attn_norm.weight","attn_qkv.weight","attn_gate.weight","ssm_beta.weight","ssm_alpha.weight","ssm_dt.bias","ssm_a","ssm_conv1d.weight","ssm_norm.weight","ssm_out.weight","post_attention_norm.weight"};
        const char * attention[8]={"attn_norm.weight","attn_q.weight","attn_k.weight","attn_v.weight","attn_output.weight","attn_q_norm.weight","attn_k_norm.weight","post_attention_norm.weight"};
        for(int i=0;i<layer_count;++i) {
            auto * row=records.data()+32*i;row[0]=flags[i]?1:0;
            for(int j=0;j<(flags[i]?11:8);++j) {
                const std::string name="blk."+std::to_string(i)+"."+(flags[i]?recurrent[j]:attention[j]);
                row[1+j]=handle(model.weight(name,true));branch[i][j]=model.weight(name,false);
            }
            ggml_tensor * mw[8];model.moe(i,true,mw);model.moe(i,false,moes[i].data());
            for(int j=0;j<8;++j)row[12+j]=handle(mw[j]);
            for(int j=0;j<2;++j) {
                auto & s=j?refs[i]:stores[i];s.fill(nullptr);
                if(flags[i]) {
                    s[0]=ggml_new_tensor_2d(owner,GGML_TYPE_F32,kernel-1,h*(2*nk+nv));
                    s[1]=ggml_new_tensor_4d(owner,GGML_TYPE_F32,h,h,nv,1);
                } else {
                    s[2]=ggml_new_tensor_2d(owner,GGML_TYPE_Q4_0,hd*hk,capacity);
                    s[3]=ggml_new_tensor_2d(owner,GGML_TYPE_Q4_0,hd*hk,capacity);
                }
                for(auto * t:s)if(t)std::memset(t->data,0,ggml_nbytes(t));
            }
            for(int j=0;j<4;++j)row[20+j]=handle(stores[i][j]);
        }
        emb=model.weight("token_embd.weight",true);nemb=model.weight("token_embd.weight",false);
        const bool tied=gguf_find_tensor(m,"output.weight")<0;head=tied?emb:model.weight("output.weight",true);nhead=tied?nemb:model.weight("output.weight",false);
        norm=model.weight("output_norm.weight",true);nnorm=model.weight("output_norm.weight",false);
        require(q35_kv_init(capacity,32,{state,6})==0,"model-step init");
        std::printf("MODEL bound_layers=%lld GDN=30 attention=10 zero_copy_bindings=%d persistent_bytes=%zu arena_cap_MiB=192\n",(long long)layer_count,model.bindings,ggml_used_mem(owner));
    }
    ~Trunk(){ggml_free(owner);}

    struct Snapshot {
        int prefix;
        std::vector<std::array<int64_t,8>> meta;
        std::vector<std::vector<float>> recurrent;
        std::vector<std::vector<uint8_t>> kv,native;
    };
    Snapshot save() {
        require(state[4]==0 && state[2]==native_past,"quiescent model snapshot");
        Snapshot s;s.prefix=native_past;s.meta.resize(layer_count);s.recurrent.resize(layer_count);s.kv.resize(layer_count);s.native.resize(layer_count);
        for(int i=0;i<layer_count;++i) {
            if(records[32*i]) {
                const int64_t nc=ggml_nelements(stores[i][0]),ns=ggml_nelements(stores[i][1]);s.recurrent[i].resize(nc+ns);
                require(q35_gdn_host_save({(float*)stores[i][0]->data,nc},{(float*)stores[i][1]->data,ns},{gcfg,7},{s.meta[i].data(),8},
                    {s.recurrent[i].data(),nc+ns})==nc+ns,"model GDN save");
                const auto bytes=(nc+ns)*sizeof(float);s.native[i].resize(bytes);
                std::memcpy(s.native[i].data(),refs[i][0]->data,nc*sizeof(float));std::memcpy(s.native[i].data()+nc*sizeof(float),refs[i][1]->data,ns*sizeof(float));
            } else {
                const int64_t width=stores[i][2]->ne[0],bytes=native_past*ggml_row_size(GGML_TYPE_Q4_0,width);s.kv[i].resize(2*bytes);s.native[i].resize(2*bytes);
                require(q35_kv_host_save({state,6},width,{(uint8_t*)stores[i][2]->data,(int64_t)ggml_nbytes(stores[i][2])},
                    {(uint8_t*)stores[i][3]->data,(int64_t)ggml_nbytes(stores[i][3])},{s.meta[i].data(),8},{s.kv[i].data(),2*bytes})==2*bytes,"model Q4 save");
                std::memcpy(s.native[i].data(),refs[i][2]->data,bytes);std::memcpy(s.native[i].data()+bytes,refs[i][3]->data,bytes);
            }
        }
        return s;
    }
    void restore(Snapshot & s) {
        require(state[4]==0,"quiescent model restore");
        for(int i=0;i<layer_count;++i) {
            if(records[32*i]) {
                const int64_t nc=ggml_nelements(stores[i][0]),ns=ggml_nelements(stores[i][1]);
                require(q35_gdn_host_restore({(float*)stores[i][0]->data,nc},{(float*)stores[i][1]->data,ns},{gcfg,7},{s.meta[i].data(),8},
                    {s.recurrent[i].data(),(int64_t)s.recurrent[i].size()})==nc+ns,"model GDN restore");
                std::memcpy(refs[i][0]->data,s.native[i].data(),nc*sizeof(float));std::memcpy(refs[i][1]->data,s.native[i].data()+nc*sizeof(float),ns*sizeof(float));
            } else {
                const int64_t width=stores[i][2]->ne[0],bytes=s.prefix*ggml_row_size(GGML_TYPE_Q4_0,width);
                require(q35_kv_host_restore({state,6},width,{(uint8_t*)stores[i][2]->data,(int64_t)ggml_nbytes(stores[i][2])},
                    {(uint8_t*)stores[i][3]->data,(int64_t)ggml_nbytes(stores[i][3])},{s.meta[i].data(),8},{s.kv[i].data(),2*bytes})==s.prefix,"model Q4 restore");
                std::memcpy(refs[i][2]->data,s.native[i].data(),bytes);std::memcpy(refs[i][3]->data,s.native[i].data()+bytes,bytes);
            }
        }
        native_past=s.prefix;require(state[2]==s.prefix && state[4]==0,"restored model prefix");
    }

    std::vector<float> step(const std::vector<int32_t> & tokens) {
        require(state[2]==native_past && state[4]==0,"whole-model prefix");const int count=tokens.size();
        auto * c=ggml_init({512*1024*1024,nullptr,false});require(c,"full trunk graph arena");
        const int64_t rows=q35_kv_read_rows({state,6},count);require(rows>0,"KV read rows");
        auto * ids=ggml_new_tensor_1d(c,GGML_TYPE_I32,count);std::memcpy(ids->data,tokens.data(),count*4);
        auto * positions=ggml_new_tensor_1d(c,GGML_TYPE_I32,count*4);std::vector<int64_t> pos(count*4);
        require(q35_kv_positions({state,6},count,{pos.data(),(int64_t)pos.size()})==count*4,"Goose model positions");
        for(int s=0;s<4;++s)for(int t=0;t<count;++t){const int expected=s<3?native_past+t:0;require(pos[s*count+t]==expected,"native position oracle");((int32_t*)positions->data)[s*count+t]=expected;}
        auto * indices=ggml_new_tensor_1d(c,GGML_TYPE_I64,count);require(q35_kv_indices({state,6},count,{(int64_t*)indices->data,count})==count,"model indices");
        for(int t=0;t<count;++t)require(((int64_t*)indices->data)[t]==native_past+t,"native index oracle");
        auto * mask=ggml_new_tensor_2d(c,GGML_TYPE_F16,rows,count);std::vector<float> raw_mask(rows*count);
        require(q35_kv_mask({state,6},count,{raw_mask.data(),(int64_t)raw_mask.size()})==rows,"model mask");
        for(int t=0;t<count;++t)for(int k=0;k<rows;++k){const auto expected=ggml_fp32_to_fp16(k<=native_past+t?0.0f:-INFINITY);require(ggml_fp32_to_fp16(raw_mask[t*rows+k])==expected,"native mask oracle");((ggml_fp16_t*)mask->data)[t*rows+k]=expected;}
        const auto ticket=q35_kv_begin({state,6},count);require(ticket>0,"begin entire model step");
        int64_t work[16]={0},bn[40]={0},mn[42]={0};std::vector<int64_t> frames(4*layer_count+4),roots(2*layer_count+2);
        packet_reads=0;
        const auto result=q35_trunk(handle(c),handle(ids),handle(emb),handle(head),handle(norm),
            {records.data(),(int64_t)records.size()},layer_count,handle(positions),handle(mask),handle(indices),
            {acfg,14},{gcfg,7},{mcfg,3},handle(sections),{state,6},ticket,{work,16},{bn,40},{mn,42},
            {frames.data(),(int64_t)frames.size()},{roots.data(),(int64_t)roots.size()});
        require(result && result==frames.back() && roots[0]==61 && packet_reads==0,"Goose full trunk build");
        require(state[2]==native_past && state[4]==count && state[5]==ticket,"layer advanced model prefix/ticket");
        int state_root=1;
        for(int i=0;i<layer_count;++i) {
            auto * normalized=(ggml_tensor*)(uintptr_t)frames[1+4*i];auto * ffn=(ggml_tensor*)(uintptr_t)frames[3+4*i];auto * hidden_out=(ggml_tensor*)(uintptr_t)frames[4+4*i];
            require(normalized->op==GGML_OP_MUL && normalized->src[0] && normalized->src[0]->op==GGML_OP_RMS_NORM,"post branch normalization wiring");
            auto * residual=normalized->src[0]->src[0];
            require(hidden_out->op==GGML_OP_ADD && hidden_out->src[0]==ffn && hidden_out->src[1]==residual,"full FFN residual dependency");
            if(records[32*i]) {
                auto * cr=(ggml_tensor*)(uintptr_t)roots[state_root++];auto * sr=(ggml_tensor*)(uintptr_t)roots[state_root++];
                require(cr->op==GGML_OP_CPY && cr->src[1]==stores[i][0] && sr->op==GGML_OP_CPY && sr->src[1]==stores[i][1],"persistent state roots must copy both destinations");
            }
        }
        const int head_slot=1+4*layer_count;auto * weighted_head_norm=(ggml_tensor*)(uintptr_t)frames[head_slot+1];
        require(weighted_head_norm->op==GGML_OP_MUL && weighted_head_norm->src[0]==(ggml_tensor*)(uintptr_t)frames[head_slot] && weighted_head_norm->src[1]==norm,"LM head normalization dependency");
        auto * current=ggml_get_rows(c,nemb,ids);std::vector<ggml_tensor*> nf(frames.size()),nr;nf[0]=current;
        for(int i=0;i<layer_count;++i) {
            ggml_tensor * n[40]={nullptr};ggml_tensor * normed,*residual;
            if(records[32*i]) {
                oracle(c,current,refs[i][0],refs[i][1],branch[i].data(),gcfg,n);nr.push_back(n[16]);nr.push_back(n[29]);normed=n[39];residual=n[37];
            } else {
                native_branch(c,current,positions,mask,branch[i].data(),acfg,sections,n);
                const int64_t width=refs[i][2]->ne[0],hd=width/acfg[1];const size_t row=ggml_row_size(GGML_TYPE_Q4_0,width);
                auto * wk=ggml_set_rows(c,refs[i][2],ggml_view_2d(c,n[14],width,count,n[14]->nb[2],0),indices);
                auto * wv=ggml_set_rows(c,refs[i][3],ggml_view_2d(c,n[8],width,count,n[8]->nb[2],0),indices);nr.push_back(wk);nr.push_back(wv);
                auto * k=ggml_view_4d(c,refs[i][2],hd,acfg[1],rows,1,ggml_row_size(GGML_TYPE_Q4_0,hd),row,row*capacity,0);
                auto * v=ggml_view_4d(c,refs[i][3],hd,acfg[1],rows,1,ggml_row_size(GGML_TYPE_Q4_0,hd),row,row*capacity,0);
                auto * fa=ggml_flash_attn_ext(c,ggml_permute(c,n[13],0,2,1,3),ggml_permute(c,k,0,2,1,3),ggml_permute(c,v,0,2,1,3),mask,unbits(acfg[12]),0.0f,0.0f);
                require(ggml_prec_set_acc(fa,GGML_PREC_F32),"native FA precision");
                auto * gated=ggml_mul(c,ggml_reshape_2d(c,fa,hd*acfg[0],count),ggml_sigmoid(c,n[15]));
                residual=ggml_add(c,ggml_mul_mat(c,branch[i][4],gated),current);
                normed=ggml_mul(c,ggml_rms_norm(c,residual,unbits(acfg[5])),branch[i][7]);
            }
            std::vector<ggml_tensor*> moe;native_moe(c,normed,moes[i].data(),mcfg,moe);
            current=ggml_add(c,moe.back(),residual);nf[1+4*i]=normed;nf[2+4*i]=moe[3];nf[3+4*i]=moe.back();nf[4+4*i]=current;
        }
        const int last=1+4*layer_count;nf[last]=ggml_rms_norm(c,current,unbits(acfg[5]));nf[last+1]=ggml_mul(c,nf[last],nnorm);nf[last+2]=ggml_mul_mat(c,nhead,nf[last+1]);nr.push_back(nf.back());
        for(size_t i=0;i<frames.size();++i){auto * g=(ggml_tensor*)(uintptr_t)frames[i];require(g && nf[i] && g->type==nf[i]->type && ggml_are_same_shape(g,nf[i]),"full-model frame geometry");ggml_set_output(g);ggml_set_output(nf[i]);}
        auto * graph=ggml_new_graph_custom(c,16384,false);for(int i=1;i<=roots[0];++i)ggml_build_forward_expand(graph,(ggml_tensor*)(uintptr_t)roots[i]);for(auto * r:nr)ggml_build_forward_expand(graph,r);
        require(ggml_graph_compute_with_ctx(c,graph,4)==GGML_STATUS_SUCCESS,"full trunk CPU compute");
        int64_t compared=0;
        for(size_t i=0;i<frames.size();++i){auto * g=(ggml_tensor*)(uintptr_t)frames[i];
            if(std::memcmp(g->data,nf[i]->data,ggml_nbytes(g))){std::fprintf(stderr,"FRAME index=%zu layer=%zu\n",i,i?((i-1)/4):0);require(false,"full trunk frame bits");}
            compared+=ggml_nelements(g);
        }
        for(int i=0;i<layer_count;++i)for(int j=0;j<4;++j)if(stores[i][j])require(std::memcmp(stores[i][j]->data,refs[i][j]->data,ggml_nbytes(stores[i][j]))==0,"full model persistent state bytes");
        auto * logits=(ggml_tensor*)(uintptr_t)result;std::vector<float> answer((float*)logits->data,(float*)logits->data+ggml_nelements(logits));
        for(float value:answer)require(std::isfinite(value),"full logits nonfinite");
        require(q35_kv_commit({state,6},ticket)==native_past+count,"single full-model commit");native_past+=count;
        std::printf("PASS TRUNK layers=%lld tokens=%d prefix=%d roots=%lld compared_values=%lld logits=%zu graph_nodes=%d\n",(long long)layer_count,count,native_past,(long long)roots[0],(long long)compared,answer.size(),ggml_graph_n_nodes(graph));
        ggml_free(c);return answer;
    }
};

static void trunk_gate(Mapping & m) {
    Trunk session(m);session.step({1,42,100});auto checkpoint=session.save();auto decoded=session.step({123});session.restore(checkpoint);auto replay=session.step({123});
    require(replay.size()==decoded.size() && std::memcmp(replay.data(),decoded.data(),decoded.size()*sizeof(float))==0,"full-model checkpoint replay logits");
    std::printf("PASS FULL MODEL checkpoint_replay=1 graph_rebuilds=3 layers=40 model_server_runs=0\n");
}

int main(int argc,char ** argv) {
    if(argc!=3)return 2;setvbuf(stdout,nullptr,_IONBF,0);goose_init(0,nullptr);Mapping model(argv[1]);
    if(std::string(argv[2])=="ffn")ffn_gate(model);
    else if(std::string(argv[2])=="bind")bind_contracts(model);
    else if(std::string(argv[2])=="trunk")trunk_gate(model);
    else return 2;
}
