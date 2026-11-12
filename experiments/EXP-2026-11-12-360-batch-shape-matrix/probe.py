import json, subprocess, sys, time, urllib.request
CAND="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/candidate-20261112-matched"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
COMBOS=[("-b","32","-ub","16","-np","1"), ("-b","64","-ub","64","-np","2"), ("-b","256","-ub","256","-np","4")]
PROMPT="The quick brown fox jumps over the lazy dog and stops"
def run(arm, port, flags):
    b=__import__("os").path.join(NATIVE if arm=="native" else CAND,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":__import__("os").path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","1024"]+list(flags)+["--port",str(port),"-t","2"],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    try:
        ok=False
        for _ in range(90):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); ok=True; break
            except Exception: time.sleep(0.5)
        if not ok: return "NOHEALTH"
        r=urllib.request.Request(f"http://127.0.0.1:{port}/completion",
            data=json.dumps({"prompt":PROMPT,"n_predict":24,"temperature":0}).encode(),
            headers={"Content-Type":"application/json"})
        with urllib.request.urlopen(r,timeout=120) as resp:
            j=json.load(resp); return f"{j.get('tokens_predicted')}|{j.get('content')}"
    except Exception as e:
        return "FAIL:"+type(e).__name__
    finally:
        p.terminate(); p.wait(timeout=25)
bad=0; port=18691
for flags in COMBOS:
    a=run("native",port,flags); port+=1
    b=run("ours",port,flags); port+=1
    same=a==b; bad+=0 if same else 1
    print(f"{' '.join(flags):32s} {'SAME' if same else 'DIFF'} native={str(a)[:36]!r}")
    if not same: print(f"{'':32s}      ours  ={str(b)[:36]!r}")
print("combos",len(COMBOS),"differences",bad)
sys.exit(0 if bad==0 else 1)
