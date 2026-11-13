import json, subprocess, sys, time, urllib.request, os
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
BASE="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid"
MODELS=["tiny-hybrid.gguf","tiny-rr.gguf","tiny-mutant.gguf"]
def run(arm, model, port):
    b=os.path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",os.path.join(BASE,model),"-c","256","-np","1","-ngl","0","--port",str(port),"-t","4"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    try:
        ok=False
        for _ in range(90):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); ok=True; break
            except Exception: time.sleep(0.5)
        if not ok: return "NOHEALTH"
        body={"prompt":"The quick brown fox","n_predict":8,"temperature":0,"seed":42,"cache_prompt":False}
        r=urllib.request.Request(f"http://127.0.0.1:{port}/completion",data=json.dumps(body).encode(),headers={"Content-Type":"application/json"})
        with urllib.request.urlopen(r,timeout=120) as resp:
            j=json.load(resp); return f"{j.get('tokens_predicted')}|{j.get('content')}"
    except Exception as e:
        return "ERR:"+str(e)[:60]
    finally:
        p.terminate(); p.wait(timeout=20)
bad=0; port=18311
for m in MODELS:
    a=run("native",m,port); port+=1
    b=run("ours",m,port); port+=1
    same = a==b; bad += 0 if same else 1
    print(f"{m:18s} {'SAME' if same else 'DIFF'}  native={str(a)[:40]!r}")
    if not same: print(f"                   ours  ={str(b)[:40]!r}")
print("models",len(MODELS),"differences",bad)
sys.exit(1 if bad else 0)
