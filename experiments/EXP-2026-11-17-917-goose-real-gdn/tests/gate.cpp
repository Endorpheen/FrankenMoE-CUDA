#define main exp913_unused_main
#include "../../EXP-2026-11-17-913-goose-real-attention/tests/gate.cpp"
#undef main

static int64_t handle(void * p) { return (int64_t) (uintptr_t) p; }

static void oracle(ggml_context * c, ggml_tensor * x, ggml_tensor * conv, ggml_tensor * state,
                   ggml_tensor ** w, const int64_t * cfg, ggml_tensor ** n) {
    const int64_t h=cfg[0], nk=cfg[1], nv=cfg[2], kernel=cfg[3], t=x->ne[1];
    const int64_t vd=h*nv, channels=h*(2*nk+nv);
    const float eps=unbits(cfg[4]), l2eps=eps/(float)h, scale=1.0f/std::sqrt((float)h);
    n[0]=ggml_rms_norm(c,x,eps); n[1]=ggml_mul(c,n[0],w[0]);
    n[2]=ggml_mul_mat(c,w[1],n[1]); n[3]=ggml_reshape_3d(c,n[2],channels,t,1);
    n[4]=ggml_mul_mat(c,w[2],n[1]); n[5]=ggml_mul_mat(c,w[3],n[1]);
    n[6]=ggml_reshape_4d(c,n[5],1,nv,t,1); n[7]=ggml_sigmoid(c,n[6]);
    n[8]=ggml_mul_mat(c,w[4],n[1]); n[9]=ggml_reshape_3d(c,n[8],nv,t,1);
    n[10]=ggml_add(c,n[9],w[5]); n[11]=ggml_softplus(c,n[10]); n[12]=ggml_mul(c,n[11],w[6]);
    n[13]=ggml_reshape_4d(c,n[12],1,nv,t,1);
    n[14]=ggml_concat(c,conv,ggml_transpose(c,n[3]),0);
    n[15]=ggml_view_4d(c,n[14],kernel-1,channels,1,1,n[14]->nb[1],n[14]->nb[2],n[14]->nb[2],t*sizeof(float));
    n[16]=ggml_cpy(c,n[15],conv); n[17]=ggml_ssm_conv(c,n[14],w[7]); n[18]=ggml_silu(c,n[17]);
    n[19]=ggml_view_4d(c,n[18],h,nk,t,1,h*4,channels*4,channels*t*4,0);
    n[20]=ggml_view_4d(c,n[18],h,nk,t,1,h*4,channels*4,channels*t*4,h*nk*4);
    n[21]=ggml_view_4d(c,n[18],h,nv,t,1,h*4,channels*4,channels*t*4,2*h*nk*4);
    n[22]=ggml_rms_norm(c,n[19],l2eps); n[23]=ggml_scale(c,n[22],scale);
    n[24]=ggml_rms_norm(c,n[20],l2eps); n[25]=ggml_scale(c,n[24],scale);
    n[26]=ggml_gated_delta_net(c,n[23],n[25],n[21],n[13],n[7],state,1);
    n[27]=ggml_view_4d(c,n[26],h,nv,t,1,h*4,vd*4,vd*t*4,0);
    n[28]=ggml_view_4d(c,n[26],h,h,nv,1,h*4,h*h*4,h*h*nv*4,vd*t*4);
    n[29]=ggml_cpy(c,n[28],state); n[30]=ggml_reshape_4d(c,n[4],h,nv,t,1);
    n[31]=ggml_rms_norm(c,n[27],eps); n[32]=ggml_mul(c,n[31],w[8]); n[33]=ggml_silu(c,n[30]);
    n[34]=ggml_mul(c,n[32],n[33]); n[35]=ggml_reshape_2d(c,n[34],vd,t);
    n[36]=ggml_mul_mat(c,w[9],n[35]); n[37]=ggml_add(c,n[36],x);
    n[38]=ggml_rms_norm(c,n[37],eps); n[39]=ggml_mul(c,n[38],w[10]);
}

struct GData {
    std::vector<Weight> raw;
    std::vector<float> input;
    int64_t cfg[7], hidden, channels, conv_count, state_count;
};

static GData data(const char * path) {
    struct stat st; int fd=open(path,O_RDONLY); require(fd>=0 && fstat(fd,&st)==0,"model FD");
    ggml_context * tensors=nullptr; gguf_context * m=gguf_init_from_file(path,{true,&tensors}); require(m && tensors,"metadata");
    GData d;
    for(const char * name : {"blk.0.attn_norm.weight","blk.0.attn_qkv.weight","blk.0.attn_gate.weight",
                            "blk.0.ssm_beta.weight","blk.0.ssm_alpha.weight","blk.0.ssm_dt.bias","blk.0.ssm_a",
                            "blk.0.ssm_conv1d.weight","blk.0.ssm_norm.weight","blk.0.ssm_out.weight","blk.0.post_attention_norm.weight"})
        d.raw.push_back(load_weight(m,fd,st.st_size,name));
    d.hidden=integer(m,"qwen35moe.embedding_length");
    const int64_t h=integer(m,"qwen35moe.ssm.state_size"),nk=integer(m,"qwen35moe.ssm.group_count"),nv=integer(m,"qwen35moe.ssm.time_step_rank"),kernel=integer(m,"qwen35moe.ssm.conv_kernel");
    const float eps=real(m,"qwen35moe.attention.layer_norm_rms_epsilon");
    require(integer(m,"qwen35moe.ssm.inner_size")==h*nv,"value head width");
    require(q35_gdn_config(h,nk,nv,kernel,bits(eps),{d.cfg,7})==7,"Goose config");
    require(d.cfg[5]==bits(eps/(float)h) && d.cfg[6]==bits(1.0f/std::sqrt((float)h)),"derived F32 parameter bits");
    d.channels=h*(2*nk+nv);d.conv_count=(kernel-1)*d.channels;d.state_count=h*h*nv;
    const int64_t id=gguf_find_tensor(m,"token_embd.weight");require(id>=0,"embedding");
    const ggml_type ty=gguf_get_tensor_type(m,id);const size_t row=ggml_row_size(ty,d.hidden);
    const uint64_t offset=gguf_get_data_offset(m)+gguf_get_tensor_offset(m,id);
    d.input.resize(4*d.hidden);const int ids[4]={1,42,100,123};
    for(int i=0;i<4;++i){auto b=read_range(fd,st.st_size,offset+ids[i]*row,row);ggml_get_type_traits(ty)->to_float(b.data(),d.input.data()+i*d.hidden,d.hidden);}
    gguf_free(m);ggml_free(tensors);close(fd);return d;
}

static std::vector<float> floats(ggml_tensor * t) {
    return {(float*)t->data,(float*)t->data+ggml_nelements(t)};
}

struct GSession {
    GData & d; bool observed; ggml_context * owner; ggml_tensor * gc,*gs,*nc,*ns;
    int steps=0,aborts=0,rejected=0;int64_t compared=0;
    GSession(GData & values,bool materialized):d(values),observed(materialized) {
        owner=ggml_init({16*1024*1024,nullptr,false});require(owner,"state owner");
        gc=ggml_new_tensor_2d(owner,GGML_TYPE_F32,d.cfg[3]-1,d.channels);nc=ggml_dup_tensor(owner,gc);
        gs=ggml_new_tensor_4d(owner,GGML_TYPE_F32,d.cfg[0],d.cfg[0],d.cfg[2],1);ns=ggml_dup_tensor(owner,gs);
        for(int64_t i=0;i<d.conv_count;++i)((float*)gc->data)[i]=(float)(i%17-8)*.001f;
        for(int64_t i=0;i<d.state_count;++i)((float*)gs->data)[i]=(float)(i%11-5)*.00001f;
        std::memcpy(nc->data,gc->data,ggml_nbytes(gc));std::memcpy(ns->data,gs->data,ggml_nbytes(gs));
    }
    ~GSession(){ggml_free(owner);}
    std::vector<float> step(int first,int count,bool abort=false) {
        auto pc=floats(gc),ps=floats(gs);
        ggml_context * c=ggml_init({256*1024*1024,nullptr,false});require(c,"graph owner");
        ggml_tensor * w[11];int64_t wh[11],work[16]={0},nodes[40]={0};
        for(int i=0;i<11;++i){auto & r=d.raw[i];w[i]=ggml_new_tensor_4d(c,r.type,r.ne[0],r.ne[1],r.ne[2],r.ne[3]);std::memcpy(w[i]->data,r.data.data(),r.data.size());wh[i]=handle(w[i]);}
        auto * x=ggml_new_tensor_2d(c,GGML_TYPE_F32,d.hidden,count);std::memcpy(x->data,d.input.data()+first*d.hidden,d.hidden*count*sizeof(float));
        auto * wide=ggml_new_tensor_2d(c,GGML_TYPE_F32,d.cfg[3],d.channels);
        auto * strided=ggml_view_2d(c,wide,d.cfg[3]-1,d.channels,wide->nb[1],0);
        auto * flat_state=ggml_new_tensor_2d(c,GGML_TYPE_F32,d.state_count,1);
        auto reject=[&](int64_t ctx,int64_t input,int64_t cv,int64_t ss,int wl,int cl,int al,int nl){
            const auto used=ggml_used_mem(c);
            require(q35_gdn_layer(ctx,input,cv,ss,{wh,wl},{d.cfg,cl},{work,al},{nodes,nl})==0,"invalid GDN input accepted");
            require(ggml_used_mem(c)==used,"invalid GDN allocated graph");
            for(auto v:work)require(v==0,"invalid GDN wrote work");for(auto v:nodes)require(v==0,"invalid GDN wrote nodes");++rejected;
        };
        reject(0,handle(x),handle(gc),handle(gs),11,7,16,40);
        reject(handle(c),0,handle(gc),handle(gs),11,7,16,40);
        reject(handle(c),handle(x),0,handle(gs),11,7,16,40);
        reject(handle(c),handle(x),handle(gc),0,11,7,16,40);
        reject(handle(c),handle(x),handle(gc),handle(gs),10,7,16,40);
        reject(handle(c),handle(x),handle(gc),handle(gs),11,6,16,40);
        reject(handle(c),handle(x),handle(gc),handle(gs),11,7,15,40);
        reject(handle(c),handle(x),handle(gc),handle(gs),11,7,16,39);
        reject(handle(c),handle(x),handle(strided),handle(gs),11,7,16,40);
        reject(handle(c),handle(x),handle(gc),handle(flat_state),11,7,16,40);
        for(int i=0;i<11;++i){auto saved=wh[i];wh[i]=0;reject(handle(c),handle(x),handle(gc),handle(gs),11,7,16,40);wh[i]=saved;}
        int64_t packet[7]={0,handle(c),handle(x),0,0,GGML_OP_DUP,0};packet_reads=0;
        require(gb_common(handle(packet)) && packet_reads>0,"packet positive witness");packet_reads=0;
        const int64_t result=q35_gdn_layer(handle(c),handle(x),handle(gc),handle(gs),{wh,11},{d.cfg,7},{work,16},{nodes,40});
        require(result && result==nodes[39] && packet_reads==0,"Goose GDN construction");
        if(abort){require(std::memcmp(pc.data(),gc->data,ggml_nbytes(gc))==0 && std::memcmp(ps.data(),gs->data,ggml_nbytes(gs))==0,"unexecuted graph changed state");ggml_free(c);++aborts;return {};}
        ggml_tensor * a[40],*b[40];for(int i=0;i<40;++i)a[i]=(ggml_tensor*)(uintptr_t)nodes[i];
        oracle(c,x,nc,ns,w,d.cfg,b);
        auto map=[&](ggml_tensor * t){if(t==gc)return nc;if(t==gs)return ns;for(int i=0;i<40;++i)if(t==a[i])return b[i];return t;};
        for(int i=0;i<40;++i){
            if(a[i]->type!=b[i]->type || a[i]->op!=b[i]->op || !ggml_are_same_shape(a[i],b[i]) || std::memcmp(a[i]->nb,b[i]->nb,sizeof a[i]->nb) || std::memcmp(a[i]->op_params,b[i]->op_params,sizeof a[i]->op_params)){
                std::fprintf(stderr,"META stage=%d op=%d/%d\n",i,a[i]->op,b[i]->op);require(false,"GDN metadata");}
            for(int j=0;j<GGML_MAX_SRC;++j) {
                auto * mapped=map(a[i]->src[j]); auto * expected=b[i]->src[j];
                if(mapped && expected && mapped!=expected && mapped->op==GGML_OP_TRANSPOSE && expected->op==GGML_OP_TRANSPOSE) {
                    // conv_input constructs this view inline on each branch;
                    // separate allocations have equal structure, not addresses.
                    require(mapped->type==expected->type && ggml_are_same_shape(mapped,expected) &&
                            std::memcmp(mapped->nb,expected->nb,sizeof mapped->nb)==0 &&
                            std::memcmp(mapped->op_params,expected->op_params,sizeof mapped->op_params)==0 &&
                            map(mapped->view_src)==expected->view_src && mapped->view_offs==expected->view_offs,
                            "inline transpose metadata/owner");
                    for(int s=0;s<GGML_MAX_SRC;++s)require(map(mapped->src[s])==expected->src[s],"inline transpose dependency");
                    continue;
                }
                if(mapped!=expected) {
                    std::fprintf(stderr,"EDGE stage=%d src=%d op=%d/%d\n",i,j,mapped?mapped->op:-1,expected?expected->op:-1);
                    require(false,"GDN graph edge");
                }
            }
            if(observed){ggml_set_output(a[i]);ggml_set_output(b[i]);}
        }
        auto * graph=ggml_new_graph(c);
        for(int root:{16,29,39})ggml_build_forward_expand(graph,a[root]);
        for(int root:{16,29,39})ggml_build_forward_expand(graph,b[root]);
        require(ggml_graph_compute_with_ctx(c,graph,4)==GGML_STATUS_SUCCESS,"GDN compute");
        require(std::memcmp(gc->data,nc->data,ggml_nbytes(gc))==0 && std::memcmp(gs->data,ns->data,ggml_nbytes(gs))==0,"persistent GDN state bits");
        bool core_nonzero=false;for(auto v:floats(a[27]))core_nonzero=core_nonzero || v!=0.0f;
        require(core_nonzero,"degenerate GDN core output");
        for(int i=0;i<40;++i){
            if(!observed && i!=39)continue;
            if(std::memcmp(a[i]->data,b[i]->data,ggml_nbytes(a[i]))){std::fprintf(stderr,"NUM stage=%d\n",i);require(false,"GDN computed bits");}
            for(int64_t j=0;j<ggml_nelements(a[i]);++j)require(std::isfinite(((float*)a[i]->data)[j]),"GDN nonfinite");
            compared+=ggml_nelements(a[i]);
        }
        for(int i=0;i<11;++i)require(std::memcmp(w[i]->data,d.raw[i].data.data(),d.raw[i].data.size())==0,"GDN weight modified");
        auto answer=floats(a[39]);ggml_free(c);++steps;
        std::printf("STEP T=%d observed=%d state_survives_graph=1 PASS\n",count,observed);return answer;
    }
};

int main(int argc,char ** argv){
    if(argc!=2)return 2;setvbuf(stdout,nullptr,_IONBF,0);goose_init(0,nullptr);
    GData d=data(argv[1]);int steps=0,aborts=0,rejected=0;int64_t compared=0;
    for(bool observed:{true,false}){
        GSession s(d,observed);s.step(0,3);
        int64_t meta[8]={0};std::vector<float> saved(d.conv_count+d.state_count);
        require(q35_gdn_host_save({(float*)s.gc->data,d.conv_count},{(float*)s.gs->data,d.state_count},{d.cfg,7},{meta,8},{saved.data(),(int64_t)saved.size()})==(int64_t)saved.size(),"Goose state save");
        auto nc=floats(s.nc),ns=floats(s.ns),decode=s.step(3,1);
        require(q35_gdn_host_restore({(float*)s.gc->data,d.conv_count},{(float*)s.gs->data,d.state_count},{d.cfg,7},{meta,8},{saved.data(),(int64_t)saved.size()})==(int64_t)saved.size(),"Goose state restore");
        std::memcpy(s.nc->data,nc.data(),ggml_nbytes(s.nc));std::memcpy(s.ns->data,ns.data(),ggml_nbytes(s.ns));
        auto replay=s.step(3,1);require(replay.size()==decode.size() && std::memcmp(replay.data(),decode.data(),decode.size()*sizeof(float))==0,"GDN restore replay bits");
        s.step(3,1,true);s.step(3,1);steps+=s.steps;aborts+=s.aborts;rejected+=s.rejected;compared+=s.compared;
    }
    std::printf("PASS real GDN steps=%d aborted_graphs=%d rejected=%d compared_f32=%lld model_runs=0\n",steps,aborts,rejected,(long long)compared);
}
