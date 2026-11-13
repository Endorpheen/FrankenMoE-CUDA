import json, subprocess, sys, time, urllib.request, os
STAGE="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
CASES=[
 {"name":"base",     "body":{}},
 {"name":"repeat",   "body":{"repeat_penalty":1.8}},
 {"name":"presence", "body":{"presence_penalty":1.5}},
 {"name":"freq",     "body":{"frequency_penalty":1.5}},
 {"name":"stopsq",   "body":{"stop":["a"]}},
 {"name":"ignoreeos","body":{"ignore_eos":True}},
 {"name":"adaptive","body":{"adaptive_target":0.5,"adaptive_decay":0.9}},
]
def run(arm, port):
    b=os.path.join(NATIVE if arm=="native" else STAGE,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","512","-np","1","--port",str(port),"-t","4"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    out={}
    try:
        for _ in range(120):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        for c in CASES:
            body={"prompt":"The little dog","n_predict":24,"temperature":1.2,"top_k":0,"seed":42,"cache_prompt":False}
            body.update(c["body"])
            r=urllib.request.Request(f"http://127.0.0.1:{port}/completion",data=json.dumps(body).encode(),headers={"Content-Type":"application/json"})
            with urllib.request.urlopen(r,timeout=120) as resp: out[c["name"]]=json.load(resp)["content"]
    finally:
        p.terminate(); p.wait(timeout=20)
    return out
a=run("native",18111); b=run("ours",18112)
bad=0
for c in CASES:
    n=c["name"]; same=a[n]==b[n]; changed=a[n]!=a["base"]
    bad+=0 if same else 1
    print(f"{n:10s} cross-line={'SAME' if same else 'DIFF':4s} changes-output={changed}")
json.dump({"native":a,"ours":b},open(os.path.join(os.path.dirname(os.path.abspath(__file__)),"outputs3.json"),"w"),indent=1)
print("differences",bad); sys.exit(1 if bad else 0)
