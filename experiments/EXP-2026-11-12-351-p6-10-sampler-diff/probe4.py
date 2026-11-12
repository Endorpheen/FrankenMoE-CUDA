import json, subprocess, sys, time, urllib.request, os
STAGE="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
def run(arm, port):
    b=os.path.join(NATIVE if arm=="native" else STAGE,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","1024","-np","1","--port",str(port),"-t","4"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    res={}
    try:
        for _ in range(120):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        for name,extra in [("eos_base",{}),("eos_ignore",{"ignore_eos":True})]:
            body={"prompt":"The little dog","n_predict":200,"temperature":0.7,"top_k":40,"seed":42,"cache_prompt":False}
            body.update(extra)
            r=urllib.request.Request(f"http://127.0.0.1:{port}/completion",data=json.dumps(body).encode(),headers={"Content-Type":"application/json"})
            with urllib.request.urlopen(r,timeout=180) as resp:
                j=json.load(resp); res[name]=(j["content"],j.get("stop_type"),j.get("tokens_predicted"))
    finally:
        p.terminate(); p.wait(timeout=20)
    return res
a=run("native",18121); b=run("ours",18122)
for n in ["eos_base","eos_ignore"]:
    print(n,"native",a[n][1],a[n][2],"| ours",b[n][1],b[n][2],"| cross-line",a[n][0]==b[n][0])
print("ignore changes output:", a["eos_ignore"][0]!=a["eos_base"][0])
