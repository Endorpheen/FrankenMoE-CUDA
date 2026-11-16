#!/bin/bash
# $1=port $2=np $3=tag $4=ub $5=batch $6=cache
PORT=$1; NP=$2; TAG=$3; UB=$4; B=$5; CACHE=$6
LIB=/home/end0/AI/FrankenMoE-CUDA/runtime/goose-stage/lib
R=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
export LD_LIBRARY_PATH=$LIB
while IFS='=' read -r k v; do export "$k=$v"; done < <(python3 -c "
import json
e=json.load(open('$R/runtime/exp244/results-apex-or244-or-goose/environment.json'))
for k,v in e.items():
    if k!='LD_LIBRARY_PATH': print(f'{k}={v}')")
python3 -c "
import json
a=json.load(open('$R/runtime/exp244/results-apex-or244-or-goose/argv.json'))
a[0]='$R/runtime/reconstructed/build/stage-run/llama-server'
a[a.index('--port')+1]='$PORT'
a[a.index('-ub')+1]='$UB'
a[a.index('-b')+1]='$B'
a+=['--moe-cache-mib','$CACHE']
print('\n'.join(a))" > /tmp/l5-$TAG.argv
python3 -c "
import subprocess
a=[l for l in open('/tmp/l5-$TAG.argv').read().split(chr(10)) if l]
log=open('/tmp/l5-$TAG.log','w',buffering=1)
p=subprocess.Popen(a,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
open('/tmp/l5-$TAG.pid','w').write(str(p.pid))"
SRV=$(cat /tmp/l5-$TAG.pid)
for i in $(seq 1 80); do
  curl -s -m 2 http://127.0.0.1:$PORT/health 2>/dev/null | grep -q '"ok"' && break
  kill -0 $SRV 2>/dev/null || { echo "$TAG FAIL"; exit 1; }
  sleep 2
done
for i in 1 2 3; do
  curl -s -m 900 -X POST http://127.0.0.1:$PORT/completion -H 'Content-Type: application/json' --data-binary @/tmp/req$NP.json -o /tmp/l5-$TAG-$i.json 2>/dev/null
done
echo "$TAG ub=$UB b=$B cache=$CACHE done"
kill $SRV 2>/dev/null; sleep 5
