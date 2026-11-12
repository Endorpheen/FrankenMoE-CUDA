import json, subprocess, sys, time, urllib.request, os
STAGE="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
TPL=("{% if enable_thinking %}<think>{% endif %}{% for m in messages %}{{ m['role'] }}:{{ m['content'] }};{% endfor %}"
     "{% if add_generation_prompt %}|GEN{% endif %}{% if custom_key is defined %}K={{ custom_key }}{% endif %}")
MSG=[{"role":"user","content":"q"}]
CASES={
 "kw_true":  {"messages":MSG,"chat_template_kwargs":{"enable_thinking":True}},
 "kw_false": {"messages":MSG,"chat_template_kwargs":{"enable_thinking":False}},
 "kw_extra": {"messages":MSG,"chat_template_kwargs":{"enable_thinking":True,"add_generation_prompt":True,"custom_key":"v1"}},
 "kw_bad":   {"messages":MSG,"chat_template_kwargs":{"nonexistent_arg":1}},
}
def run(arm, port):
    b=os.path.join(NATIVE if arm=="native" else STAGE,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","256","-np","1","--port",str(port),"-t","4","--jinja","--chat-template",TPL],
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    out={}
    try:
        for _ in range(120):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        for n,body in CASES.items():
            req=urllib.request.Request(f"http://127.0.0.1:{port}/apply-template",data=json.dumps(body).encode(),headers={"Content-Type":"application/json"})
            try:
                with urllib.request.urlopen(req,timeout=60) as r: out[n]=json.load(r)["prompt"]
            except urllib.error.HTTPError as e: out[n]="HTTP"+str(e.code)+":"+e.read().decode()[:120]
    finally:
        p.terminate(); p.wait(timeout=20)
    return out
a=run("native",18161); b=run("ours",18162)
bad=0
for n in CASES:
    same=a[n]==b[n]; bad+=0 if same else 1
    print(f"{n:9s} {'SAME' if same else 'DIFF'} | {a[n][:70]!r}")
    if not same: print("  ours:",repr(b[n])[:150])
json.dump({"native":a,"ours":b},open(os.path.join(os.path.dirname(os.path.abspath(__file__)),"kwargs.json"),"w"),indent=1)
print("differences",bad); sys.exit(1 if bad else 0)
