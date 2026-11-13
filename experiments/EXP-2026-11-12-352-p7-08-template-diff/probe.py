import json, subprocess, sys, time, urllib.request, os
STAGE="/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage"
NATIVE="/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL="/home/end0/.unsloth/.cache/stories260K.gguf"

TOOLS=[{"type":"function","function":{"name":"get_weather","description":"Weather for a city","parameters":{"type":"object","properties":{"city":{"type":"string"}},"required":["city"]}}}]

CASES = {
 "plain": {"messages":[{"role":"user","content":"hi"}]},
 "system": {"messages":[{"role":"system","content":"You are terse."},{"role":"user","content":"hi"}]},
 "multi": {"messages":[{"role":"user","content":"a"},{"role":"assistant","content":"b"},{"role":"user","content":"c"}]},
 "tools": {"messages":[{"role":"user","content":"weather?"}],"tools":TOOLS},
 "toolcall": {"messages":[{"role":"user","content":"weather?"},{"role":"assistant","content":"","tool_calls":[{"type":"function","function":{"name":"get_weather","arguments":"{\"city\":\"Oslo\"}"}}]},{"role":"tool","content":"7C"}],"tools":TOOLS},
 "reasoning": {"messages":[{"role":"user","content":"q"},{"role":"assistant","content":"answer","reasoning_content":"thinking"}]},
 "unicode": {"messages":[{"role":"user","content":"привет 🌍\n\ttab \"quoted\" <tag> & amp"}]},
 "emptycontent": {"messages":[{"role":"user","content":""},{"role":"assistant","content":"x"}]},
 "jinja_filters": {"messages":[{"role":"user","content":"hello world"}],
   "chat_template":"{% for m in messages %}[{{ m['role'] | upper }}] {{ m['content'] | trim | replace('world','WORLD') }}{% endfor %}<|end|>"},
 "jinja_cond": {"messages":[{"role":"user","content":"q"}],
   "chat_template":"{% if tools %}TOOLS={{ tools | length }}{% else %}NOTOOLS{% endif %}|{% if messages | length > 1 %}MANY{% else %}ONE{% endif %}|{{ messages[-1]['content'] }}"},
 "jinja_loop": {"messages":[{"role":"user","content":"a"},{"role":"user","content":"b"}],
   "chat_template":"{% for m in messages %}{{ loop.index }}:{{ m['content'] }}{% if not loop.last %},{% endif %}{% endfor %}"},
}

def run(arm, port):
    b=os.path.join(NATIVE if arm=="native" else STAGE,"llama-server")
    env={"PATH":"/usr/bin:/bin","HOME":"/tmp","LD_LIBRARY_PATH":os.path.dirname(b)}
    p=subprocess.Popen([b,"-m",MODEL,"-c","512","-np","1","--port",str(port),"-t","4","--jinja"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
    out={}
    try:
        for _ in range(120):
            try: urllib.request.urlopen(f"http://127.0.0.1:{port}/health",timeout=1); break
            except Exception: time.sleep(0.5)
        for name, body in CASES.items():
            req=urllib.request.Request(f"http://127.0.0.1:{port}/apply-template",data=json.dumps(body).encode(),headers={"Content-Type":"application/json"})
            try:
                with urllib.request.urlopen(req,timeout=60) as r: out[name]=json.load(r)["prompt"]
            except urllib.error.HTTPError as e:
                out[name]="HTTPERROR:"+e.read().decode()[:160]
    finally:
        p.terminate(); p.wait(timeout=20)
    return out

a=run("native",18141); b=run("ours",18142)
bad=0
for n in CASES:
    same=a[n]==b[n]; bad+=0 if same else 1
    print(f"{n:13s} {'SAME' if same else 'DIFF'}")
    if not same: print("  native:",repr(a[n])[:160],"\n  ours  :",repr(b[n])[:160])
json.dump({"native":a,"ours":b},open(os.path.join(os.path.dirname(os.path.abspath(__file__)),"templates.json"),"w"),indent=1)
print("cases",len(CASES),"differences",bad)
sys.exit(1 if bad else 0)
