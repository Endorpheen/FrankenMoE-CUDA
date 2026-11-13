import json, subprocess, sys, time, urllib.request, threading, os
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112-matched"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
PROMPTS=["The quick brown fox","Once upon a time","In the beginning","A small dog named"]
def run(arm, port):
    b=os.path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","1024","-np","4","--port",str(port),"-t","2"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    try:
        for _ in range(90):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        res=[None]*4
        def go(i):
            try:
                r=urllib.request.Request(f"http://127.0.0.1:{port}/completion",
                    data=json.dumps({"prompt":PROMPTS[i],"n_predict":12,"temperature":0}).encode(),
                    headers={"Content-Type":"application/json"})
                with urllib.request.urlopen(r,timeout=150) as resp: res[i]=json.load(resp).get("content")
            except Exception as e: res[i]="FAIL:"+type(e).__name__
        ts=[threading.Thread(target=go,args=(i,)) for i in range(4)]
        [t.start() for t in ts]; [t.join() for t in ts]
        return res
    finally:
        p.terminate(); p.wait(timeout=25)
a=run("native",18701); b=run("ours",18702)
bad=0
for i in range(4):
    same=a[i]==b[i]; bad+=0 if same else 1
    print(f"req{i} {'SAME' if same else 'DIFF'} native={str(a[i])[:40]!r}")
    if not same: print(f"      ours  ={str(b[i])[:40]!r}")
print("4 concurrent at np=4, differences:", bad)
sys.exit(0 if bad==0 else 1)
