import json, subprocess, sys, time, urllib.request, urllib.error, os
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
CASES=[
 ("empty_prompt",      {"prompt":"", "n_predict":4, "temperature":0}),
 ("zero_predict",      {"prompt":"Hi", "n_predict":0, "temperature":0}),
 ("stop_string",       {"prompt":"The", "n_predict":16, "temperature":0, "stop":["e"]}),
 ("big_predict",       {"prompt":"The", "n_predict":64, "temperature":0.9, "seed":1}),
 ("n_probs",           {"prompt":"The", "n_predict":4, "temperature":0, "n_probs":3}),
 ("cache_prompt",      {"prompt":"The the the", "n_predict":4, "temperature":0, "cache_prompt":True}),
]
def run(arm, port):
    b=os.path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","512","-np","2","--port",str(port),"-t","4"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    out={}
    try:
        for _ in range(90):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        for name, body in CASES:
            print("  sending", name, file=sys.stderr, flush=True)
            r=urllib.request.Request(f"http://127.0.0.1:{port}/completion",data=json.dumps(body).encode(),headers={"Content-Type":"application/json"})
            try:
                with urllib.request.urlopen(r,timeout=90) as resp:
                    j=json.load(resp); out[name]=f"{j.get('tokens_predicted')}|{j.get('stop_type')}|{j.get('content')}"
            except Exception as e:
                out[name]="FAIL:"+type(e).__name__+":"+str(e)[:40]
    finally:
        p.terminate(); p.wait(timeout=20)
    return out
a=run("native",18371); b=run("ours",18372)
bad=0
for n,_ in CASES:
    same=a[n]==b[n]; bad+=0 if same else 1
    print(f"{n:13s} {'SAME' if same else 'DIFF'}  native={str(a[n])[:44]!r}", flush=True)
    if not same: print(f"                   ours  ={str(b[n])[:44]!r}")
print("cases",len(CASES),"differences",bad)
sys.exit(1 if bad else 0)
