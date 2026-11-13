import json, subprocess, sys, time, urllib.request, os
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112-matched"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
CASES=[("shift64",  "Hi", 200), ("shift128", "The quick brown fox", 400)]
def run(arm, port, ctx):
    b=os.path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c",str(ctx),"-np","1","--port",str(port),"-t","4"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    out={}
    try:
        for _ in range(90):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        for name, prompt, npred in CASES:
            r=urllib.request.Request(f"http://127.0.0.1:{port}/completion",
                data=json.dumps({"prompt":prompt,"n_predict":npred,"temperature":0}).encode(),
                headers={"Content-Type":"application/json"})
            try:
                with urllib.request.urlopen(r,timeout=150) as resp:
                    j=json.load(resp); out[name]=f"{j.get('tokens_predicted')}|{j.get('stop_type')}|{j.get('content')}"
            except Exception as e: out[name]="FAIL:"+type(e).__name__+":"+str(e)[:40]
    finally:
        p.terminate(); p.wait(timeout=25)
    return out
bad=0; port=18721
for ctx in (64, 128):
    a=run("native",port,ctx); port+=1
    b=run("ours",port,ctx); port+=1
    for n,_,_ in CASES:
        same=a[n]==b[n]; bad+=0 if same else 1
        print(f"ctx={ctx:4d} {n:9s} {'SAME' if same else 'DIFF'} native={str(a[n])[:52]!r}")
        if not same: print(f"{'':16s} ours  ={str(b[n])[:52]!r}")
print("differences", bad)
