#!/usr/bin/env python3
"""Reuse frozen EXP918 native oracles; replace its candidate setup with Goose."""
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
out=Path(sys.argv[1]);name=sys.argv[2]
base=ROOT/'experiments/EXP-2026-11-17-918-goose-moe-trunk/tests/gate.cpp'
s=base.read_text()


def replace(old,new):
    global s
    assert s.count(old)==1,old[:100]
    s=s.replace(old,new)


replace('ggml_context * weights; int bindings=0;','ggml_context * weights; int bindings=0; int candidate_native_lookups=0;')
replace('ggml_tensor * weight(const std::string & name,bool candidate) {',
        'ggml_tensor * weight(const std::string & name,bool candidate) {\n        if(candidate){++candidate_native_lookups;require(false,"candidate attempted native GGUF/name path");}')
replace('std::vector<int64_t> records;','std::vector<int64_t> records;\n    int64_t loaded_owner[12]={0},loaded_handles[4]={0};\n    int64_t parsed[8]={0};\n    std::vector<int64_t> parsed_kv,parsed_tensors,kv_slots,tensor_slots;')
replace('row[1+j]=handle(model.weight(name,true));branch[i][j]=model.weight(name,false);','branch[i][j]=model.weight(name,false);')
replace('ggml_tensor * mw[8];model.moe(i,true,mw);model.moe(i,false,moes[i].data());\n            for(int j=0;j<8;++j)row[12+j]=handle(mw[j]);',
        'model.moe(i,false,moes[i].data());')
replace('auto & s=j?refs[i]:stores[i];s.fill(nullptr);','auto & s=j?refs[i]:stores[i];s.fill(nullptr);if(j==0)continue;')
replace('emb=model.weight("token_embd.weight",true);nemb=model.weight("token_embd.weight",false);','nemb=model.weight("token_embd.weight",false);')
replace('const bool tied=gguf_find_tensor(m,"output.weight")<0;head=tied?emb:model.weight("output.weight",true);nhead=tied?nemb:model.weight("output.weight",false);',
        'const bool tied=gguf_find_tensor(m,"output.weight")<0;nhead=tied?nemb:model.weight("output.weight",false);')
replace('norm=model.weight("output_norm.weight",true);nnorm=model.weight("output_norm.weight",false);','nnorm=model.weight("output_norm.weight",false);')
replace('require(q35_kv_init(capacity,32,{state,6})==0,"model-step init");','prepare_candidate();')
replace('model.bindings,ggml_used_mem(owner));','(int)loaded_owner[11],ggml_used_mem(owner));')
replace('~Trunk(){ggml_free(owner);}',r'''
    void prepare_candidate(){
        int64_t old_a[14],old_g[7],old_m[3];int old_sections[4];
        std::memcpy(old_a,acfg,sizeof old_a);std::memcpy(old_g,gcfg,sizeof old_g);std::memcpy(old_m,mcfg,sizeof old_m);std::memcpy(old_sections,sections,sizeof old_sections);
        parsed_kv.resize(512*8);parsed_tensors.resize(4096*12);kv_slots.resize(1024);tensor_slots.resize(8192);
        const auto bytes=(int64_t)std::min<size_t>(model.size,64*1024*1024);
        require(gr_parse({model.base,bytes},model.size,{parsed,8},{parsed_kv.data(),(int64_t)parsed_kv.size()},
            {parsed_tensors.data(),(int64_t)parsed_tensors.size()},{kv_slots.data(),(int64_t)kv_slots.size()},
            {tensor_slots.data(),(int64_t)tensor_slots.size()})==0,"candidate Goose header reader");
        int64_t sec[4]={0};std::vector<int64_t> scratch((size_t)parsed[3]+4);uint8_t name_work[64]={0};
        require(qr_model_open({model.base,bytes},(uint64_t)(uintptr_t)model.base,model.size,{parsed,8},
            {parsed_kv.data(),(int64_t)parsed_kv.size()},{parsed_tensors.data(),(int64_t)parsed_tensors.size()},
            {kv_slots.data(),(int64_t)kv_slots.size()},{tensor_slots.data(),(int64_t)tensor_slots.size()},capacity,96*1024*1024,
            {loaded_owner,12},{records.data(),(int64_t)records.size()},{loaded_handles,4},{acfg,14},{gcfg,7},{mcfg,3},
            {sec,4},{state,6},{name_work,64},{scratch.data(),(int64_t)scratch.size()})==layer_count,"Goose model plan/bind/ownership");
        require(std::memcmp(old_a,acfg,sizeof old_a)==0 && std::memcmp(old_g,gcfg,sizeof old_g)==0 &&
            std::memcmp(old_m,mcfg,sizeof old_m)==0,"Goose configs differ from native metadata reference");
        for(int i=0;i<4;++i)require(sec[i]==old_sections[i],"Goose section values");
        require(std::memcmp((void*)(uintptr_t)loaded_handles[3],old_sections,sizeof old_sections)==0,"model-owned I32 RoPE storage");
        auto equal=[](ggml_tensor * candidate,ggml_tensor * reference){
            require(candidate && reference && candidate->type==reference->type && candidate->data==reference->data &&
                ggml_are_same_shape(candidate,reference) && std::memcmp(candidate->nb,reference->nb,sizeof candidate->nb)==0,"Goose/native weight descriptor equality");
        };
        auto key=gguf_find_key(model.metadata,"qwen35moe.attention.recurrent_layers");auto * flags=(const uint8_t*)gguf_get_arr_data(model.metadata,key);
        for(int i=0;i<layer_count;++i){auto * row=records.data()+32*i;require(row[0]==(flags[i]?1:0),"Goose layer kinds");
            for(int j=0;j<(row[0]?11:8);++j)equal((ggml_tensor*)(uintptr_t)row[1+j],branch[i][j]);
            for(int j=0;j<8;++j)equal((ggml_tensor*)(uintptr_t)row[12+j],moes[i][j]);
            for(int j=0;j<4;++j)stores[i][j]=(ggml_tensor*)(uintptr_t)row[20+j];
        }
        emb=(ggml_tensor*)(uintptr_t)loaded_handles[0];head=(ggml_tensor*)(uintptr_t)loaded_handles[1];norm=(ggml_tensor*)(uintptr_t)loaded_handles[2];
        equal(emb,nemb);equal(head,nhead);equal(norm,nnorm);
        require(loaded_owner[0]==9191 && loaded_owner[11]==733 && model.candidate_native_lookups==0,"candidate reached native name/loader path");
        std::printf("PASS Goose loader kv=%lld tensors=%lld bound=%lld candidate_native_queries=%d\n",(long long)parsed[2],(long long)parsed[3],(long long)loaded_owner[11],model.candidate_native_lookups);
    }
    ~Trunk(){require(qr_model_close({loaded_owner,12})==0,"Goose model close");require(qr_model_close({loaded_owner,12})==-1,"double model close");ggml_free(owner);}
''')
replace('auto * c=ggml_init({512*1024*1024,nullptr,false});require(c,"full trunk graph arena");',
        'int64_t frame[4]={0};auto * c=(ggml_context*)(uintptr_t)qr_frame_open({loaded_owner,12},512*1024*1024,{frame,4});require(c,"Goose frame ownership");\n        require(qr_model_close({loaded_owner,12})==-1,"closed model with active frame");')
replace('ggml_free(c);return answer;',
        'require(qr_frame_close({loaded_owner,12},{frame,4})==0,"Goose frame close");require(qr_frame_close({loaded_owner,12},{frame,4})==-1,"double frame close");return answer;')
replace('{acfg,14},{gcfg,7},{mcfg,3},handle(sections),{state,6},ticket,',
        '{acfg,14},{gcfg,7},{mcfg,3},loaded_handles[3],{state,6},ticket,')
target=out/(name+'.cpp');target.write_text(s)
gdnp=ROOT/'experiments/EXP-2026-11-17-917-goose-real-gdn/tests/gate.cpp';text=gdnp.read_text().split('struct GData {',1)[0]
text=text.replace('"../../EXP-2026-11-17-913-goose-real-attention/tests/gate.cpp"','"'+str(ROOT/'experiments/EXP-2026-11-17-913-goose-real-attention/tests/gate.cpp')+'"')
(out/(name+'-gdn-oracle.hpp')).write_text(text)
(out/(name+'-fixture-provenance.json')).write_text(json.dumps({'frozen_trunk_fixture':str(base),'frozen_trunk_sha256':hashlib.sha256(base.read_bytes()).hexdigest(),
    'candidate_setup':'Goose gr_parse and qr_model_open; C++ metadata and descriptors reference-only; candidate native name calls forbidden',
    'generated_sha256':hashlib.sha256(target.read_bytes()).hexdigest()},indent=2)+'\n')
print('generated reference fixture with candidate native loader disabled:',target)
