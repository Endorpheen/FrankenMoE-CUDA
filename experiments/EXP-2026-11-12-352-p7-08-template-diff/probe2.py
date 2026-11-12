import json, subprocess, sys, time, urllib.request, os
STAGE="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"
MSG=[{"role":"system","content":"be terse"},{"role":"user","content":"hello world"}]
MSGS=[{"role":"user","content":"a"},{"role":"user","content":"b"}]
TPL={
 "default": None,
 "filters": "{% for m in messages %}[{{ m['role'] | upper }}] {{ m['content'] | trim | replace('world','WORLD') }}\n{% endfor %}<|end|>",
 "cond":    "{% if messages | length > 1 %}MANY{% else %}ONE{% endif %}|{{ messages[-1]['content'] }}|{{ messages[0]['role'] }}",
 "loop":    "{% for m in messages %}{{ loop.index }}:{{ m['content'] }}{% if not loop.last %},{% endif %}{% endfor %}",
 "tools":   "{% if tools %}{{ tools | length }} TOOL {% for t in tools %}{{ t['function']['name'] }}({{ t['function']['parameters']['properties'] | list | join(',') }}){% endfor %}{% endif %}|{% for m in messages %}{{ m['content'] }}{% endfor %}",
}
TOOLS=[{"type":"function","function":{"name":"get_weather","description":"w","parameters":{"type":"object","properties":{"city":{"type":"string"}},"required":["city"]}}}]
BODY_BY_TPL={"default":MSG,"filters":MSG,"cond":MSG,"loop":MSGS,"tools":MSG}

def run(arm, port, tpl):
    b=os.path.join(NATIVE if arm=="native" else STAGE,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    cmd=[b,"-m",MODEL,"-c","256","-np","1","--port",str(port),"-t","4","--jinja"]
    if TPL[tpl]: cmd += ["--chat-template",TPL[tpl]]
    p=subprocess.Popen(cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    try:
        for _ in range(120):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        body={"messages":BODY_BY_TPL[tpl]}
        if tpl=="tools": body["tools"]=TOOLS
        req=urllib.request.Request(f"http://127.0.0.1:{port}/apply-template",data=json.dumps(body).encode(),headers={"Content-Type":"application/json"})
        with urllib.request.urlopen(req,timeout=60) as r: return json.load(r)["prompt"]
    except urllib.error.HTTPError as e:
        return "HTTPERROR:"+e.read().decode()[:200]
    finally:
        p.terminate(); p.wait(timeout=20)

bad=0; res={}
port=18151
for tpl in TPL:
    a=run("native",port,tpl); port+=1
    b=run("ours",port,tpl); port+=1
    same=a==b; bad+=0 if same else 1
    res[tpl]={"native":a,"ours":b}
    print(f"{tpl:8s} {'SAME' if same else 'DIFF'} len={len(a)} | {a[:70]!r}")
    if not same: print("  ours:",repr(b)[:160])
json.dump(res,open(os.path.join(os.path.dirname(os.path.abspath(__file__)),"templates2.json"),"w"),indent=1)
print("templates",len(TPL),"differences",bad)
sys.exit(1 if bad else 0)
