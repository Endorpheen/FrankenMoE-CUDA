import json, subprocess, sys, time, urllib.request, os
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112-matched"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
LONG=" ".join(["the cat sat on the mat and looked around"]*8)   # ~200 tokens
HUGE=" ".join(["the cat sat on the mat and looked around"]*60)   # ~600 tokens, over a 256 context
CASES=[("near_cap", LONG, 8), ("over_cap", HUGE, 8), ("big_predict", "Hi", 512)]
def run(arm, port):
    b=os.path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","256","-np","1","--port",str(port),"-t","4"],
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
                with urllib.request.urlopen(r,timeout=120) as resp:
                    j=json.load(resp); out[name]=f"{j.get('tokens_predicted')}|{j.get('stop_type')}|{j.get('content')[:40]}"
            except urllib.error.HTTPError as e:
                out[name]=f"HTTP{e.code}"
            except Exception as e:
                out[name]="FAIL:"+type(e).__name__
    finally:
        p.terminate(); p.wait(timeout=25)
    return out
a=run("native",18711); b=run("ours",18712)
bad=0
for n,_,_ in CASES:
    same=a[n]==b[n]; bad+=0 if same else 1
    print(f"{n:12s} {'SAME' if same else 'DIFF'} native={a[n]!r}")
    if not same: print(f"{'':12s}      ours  ={b[n]!r}")
print("cases",len(CASES),"differences",bad)
