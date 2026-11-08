from pathlib import Path
import subprocess,json,os,struct
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=A/'source/cuda-unified';B=A/'build/cuda-unified';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');R=A/'variants/cpu-backend/run';T=B/'run'
p=S/'joined-device-fixture.cpp';s=p.read_text();old='FILE *f=fopen(argv[1],"wb");if(!f)return 8;fwrite(out.data(),4,out.size(),f);fclose(f);';new='''FILE *f=fopen(argv[1],"wb");if(!f)return 8;
 for(auto stage : {conv,norm,silu,mul,full,result}) { std::vector<float> temp(ggml_nelements(stage)); ggml_backend_tensor_get(stage,temp.data(),0,temp.size()*4); fwrite(temp.data(),4,temp.size(),f); }
 fclose(f);''';assert old in s;s=s.replace(old,new);p.write_text(s)
args=['/usr/bin/g++-13','-std=c++17','-O2','-I'+str(U/'ggml/include'),str(p),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-cuda','-lggml','-lggml-base','-o',str(B/'joined-device-fixture-stages')];subprocess.run(args,check=True)
for name,d in [('native',R),('goose',T)]:
 env=dict(os.environ,LD_LIBRARY_PATH=str(d)+':'+str(R),CUDA_LAUNCH_BLOCKING='1');p=subprocess.run([str(B/'joined-device-fixture-stages'),str(B/(name+'-stages.bin'))],capture_output=True,text=True,env=env);(B/(name+'-stage.stderr')).write_text(p.stderr);print(name,p.returncode,p.stdout,flush=True);assert p.returncode==0
ar=[struct.unpack('<6144f',(B/(n+'-stages.bin')).read_bytes()) for n in ['native','goose']];j=[]
for k,name in enumerate(['conv','norm','silu','mul','F16-roundtrip','Q4-roundtrip']):
 a=ar[0][k*1024:(k+1)*1024];b=ar[1][k*1024:(k+1)*1024];err=max(abs(x-y) for x,y in zip(a,b));idx=max(range(1024),key=lambda i:abs(a[i]-b[i]));r=dict(stage=name,max_abs_diff=err,index=idx,native=a[idx],goose=b[idx]);j.append(r);print(r)
(B/'stage-localization.json').write_text(json.dumps(j,indent=2)+'\n')
