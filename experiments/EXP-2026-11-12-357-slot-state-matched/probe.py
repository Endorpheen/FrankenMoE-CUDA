import json, subprocess, sys, time, urllib.request, os
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112-matched"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
def post(port, path, body):
    r=urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=json.dumps(body).encode(),
                             headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(r, timeout=120) as resp: return json.load(resp)
def run(arm, port, slotfile):
    b=os.path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","512","-np","1","--slot-save-path","/tmp/slots","--port",str(port),"-t","4"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    out={}
    try:
        for _ in range(90):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        body={"prompt":"The quick brown fox jumps","n_predict":8,"temperature":0,"cache_prompt":True}
        first=post(port,"/completion",body); out["first"]=first.get("content")
        out["saved"]=post(port,"/slots/0?action=save",{"filename":slotfile}).get("id_slot")
        post(port,"/slots/0?action=erase",{})
        out["restored"]=post(port,"/slots/0?action=restore",{"filename":slotfile}).get("id_slot")
        again=post(port,"/completion",body); out["second"]=again.get("content")
        out["n_past"]=again.get("timings",{}).get("cache_n")
    except Exception as e:
        out["error"]=type(e).__name__+":"+str(e)[:60]
    finally:
        p.terminate(); p.wait(timeout=20)
    return out
a=run("native",18561,"sn.bin")
b=run("ours",18562,"so.bin")
keys=["saved","restored","first","second","error"]
print("native:", {k:a.get(k) for k in keys})
print("ours  :", {k:b.get(k) for k in keys})
print("first equal:", a.get("first")==b.get("first"), "| second equal:", a.get("second")==b.get("second"))
sys.exit(0 if a.get("first")==b.get("first") and a.get("second")==b.get("second") else 1)
