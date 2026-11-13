import json, subprocess, sys, time, urllib.request, threading
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112-matched"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
PROMPTS=["The quick brown fox","Once upon a time there was"]
def post(port, body):
    r=urllib.request.Request(f"http://127.0.0.1:{port}/completion", data=json.dumps(body).encode(),
                             headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(r, timeout=180) as resp:
        j=json.load(resp); return j.get("content"), j.get("id_slot")
def run(arm, port):
    b=__import__("os").path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":__import__("os").path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","512","-np","2","--port",str(port),"-t","2"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    try:
        for _ in range(90):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        res=[None,None]
        def go(i):
            res[i]=post(port,{"prompt":PROMPTS[i],"n_predict":16,"temperature":0,"cache_prompt":False})
        ts=[threading.Thread(target=go,args=(i,)) for i in range(2)]
        [t.start() for t in ts]; [t.join() for t in ts]
        return res
    finally:
        p.terminate(); p.wait(timeout=25)
a=run("native",18601); b=run("ours",18602)
ok=True
for i in range(2):
    same = a[i]==b[i]
    ok = ok and same
    print(f"slot{i}: {'SAME' if same else 'DIFF'} native={str(a[i])[:46]!r}")
    if not same: print(f"        ours  ={str(b[i])[:46]!r}")
print("both slots identical:", ok)
sys.exit(0 if ok else 1)
