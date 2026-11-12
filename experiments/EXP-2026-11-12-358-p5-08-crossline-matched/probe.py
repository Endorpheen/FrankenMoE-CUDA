import json, subprocess, sys, time, urllib.request, os, hashlib
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112-matched"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
SAVE="/tmp/xline"
def post(port, path, body):
    r=urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=json.dumps(body).encode(),
                             headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(r, timeout=180) as resp: return json.load(resp)
def server(arm, port):
    b=os.path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","512","-np","1","--slot-save-path",SAVE,"--port",str(port),"-t","4"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    for _ in range(90):
        try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); return p
        except Exception: time.sleep(0.5)
    raise SystemExit("no health "+arm)
PROMPT="The quick brown fox jumps over the lazy dog and then runs away"
def write_state(arm, port, fname):
    p=server(arm,port)
    try:
        post(port,"/completion",{"prompt":PROMPT,"n_predict":8,"temperature":0,"cache_prompt":True})
        s=post(port,f"/slots/0?action=save",{"filename":fname})
    finally:
        p.terminate(); p.wait(timeout=20)
    return s
def read_state(arm, port, fname):
    p=server(arm,port)
    try:
        r=post(port,f"/slots/0?action=restore",{"filename":fname})
        out=post(port,"/completion",{"prompt":PROMPT,"n_predict":8,"temperature":0,"cache_prompt":True})
        return r, out.get("content"), out.get("timings",{}).get("cache_n")
    except Exception as e:
        return "ERR:"+type(e).__name__+":"+str(e)[:50], None, None
    finally:
        p.terminate(); p.wait(timeout=20)
n=write_state("native",18591,"n.bin"); g=write_state("ours",18592,"g.bin")
for f in ("n.bin","g.bin"):
    path=os.path.join(SAVE,f)
    print(f, "bytes=", os.path.getsize(path) if os.path.exists(path) else "absent")
print("native save:", n.get("n_saved"), "| ours save:", g.get("n_saved"))
r1,c1,k1 = read_state("ours",18593,"n.bin")     # candidate reads the native file
r2,c2,k2 = read_state("native",18594,"g.bin")   # native reads the candidate file
print("ours reads native:", str(r1)[:60], "| content:", repr(c1)[:40], "cache_n:", k1)
print("native reads ours:", str(r2)[:60], "| content:", repr(c2)[:40], "cache_n:", k2)
