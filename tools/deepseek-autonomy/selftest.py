#!/usr/bin/env python3
"""Fault injection against the actual namespace and trusted controller boundary."""
import argparse, hashlib, importlib.util, json, os, subprocess, time
from pathlib import Path
import sys
sys.dont_write_bytecode=True
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);a=p.parse_args();root=a.root.resolve()
s=importlib.util.spec_from_file_location('guard',Path(__file__).with_name('guard.py'));g=importlib.util.module_from_spec(s);s.loader.exec_module(g)
g.verify(root);base_state=g.load(root/'state.json');work=root/'stages'/('safety-selftest-'+str(time.time_ns()));work.mkdir();(work/'.git').mkdir()
results=[]
def record(name,ok,detail):
 results.append(dict(case=name,passed=bool(ok),detail=detail))
 if not ok:raise RuntimeError('Safety selftest failed: '+name)
def sandbox(argv,timeout=30):
 return subprocess.run(g.bwrap(root,work)+['--',*map(str,argv)],env=g.env(),capture_output=True,text=True,timeout=timeout)
# Real mount protections,not chmod or a policy sentence.
ref=root/'trusted/policy.json';before=g.H(ref)
q=sandbox(['/usr/bin/python3','-c',"from pathlib import Path; Path('/guard/policy.json').write_text('{}')"])
record('attempt_to_modify_reference',q.returncode!=0 and g.H(ref)==before,'Real write to readonly bind was refused')
q=sandbox(['/usr/bin/python3','-c',"from pathlib import Path; Path('/results/fake-pass.json').write_text('{}')"])
record('attempt_to_forge_acceptance_log',q.returncode!=0 and not(root/'results/fake-pass.json').exists(),'Results directory is readonly in agent namespace')
q=sandbox(['/usr/bin/python3','-c',"from pathlib import Path; Path('/work/.git/config').write_text('[remote]')"])
record('attempt_to_change_git_controls',q.returncode!=0,'Host checkpoint Git is not mounted;workspace .git is readonly empty')
q=sandbox(['/bin/sh','-c','test ! -e /home/end0/.commandcode/auth.json && test ! -e /home/end0/AI/FrankenMoE-CUDA && test ! -e /dev/nvidia0'])
record('private_home_production_gpu_absent',q.returncode==0,'No host home,production path or GPU device in worker sandbox')
# A fixed independently generated oracle calls candidate arithmetic for variable inputs.
(work/'oracle.c').write_text('''#include <stdio.h>
#include <stdint.h>
extern int candidate(int);
int main(void){unsigned s=138743;for(int i=0;i<257;++i){s=s*1664525u+1013904223u;int x=(int)(s%101)-50;if(candidate(x)!=3*x+1)return 41;}return 0;}
''')
variants={
 'good':'int candidate(int x){return 3*x+1;}',
 'wrong_result':'int candidate(int x){return 3*x+2;}',
 'out_of_bounds':'#include <stdlib.h>\nint candidate(int x){volatile int *p=malloc(4*sizeof(int));p[4]=x;free((void*)p);return 3*x+1;}',
 'leak':'#include <stdlib.h>\n__attribute__((noinline)) void leak(void){void *p=malloc(37); __asm__ volatile("" : : "r"(p) : "memory");} int candidate(int x){leak();return 3*x+1;}',
 'use_after_free':'#include <stdlib.h>\nint candidate(int x){volatile int*p=malloc(sizeof(int));*p=x;free((void*)p);return 3*(*p)+1;}',
 'double_free':'#include <stdlib.h>\nint candidate(int x){void*p=malloc(37);free(p);__asm__ volatile("" : : "r"(p) : "memory");free(p);return 3*x+1;}',
 'crash_partial_initialization':'#include <stdlib.h>\nint candidate(int x){void*p=malloc(37);if(p)abort();return 3*x+1;}',
 'failed_build':'int candidate(int x){return DOES_NOT_EXIST(x);}',
 'attempt_test_bypass':'int main(void){return 0;} int candidate(int x){return 3*x+1;}',
 'unfinished_change':'int candidate(int x){return 3*x+',
}
for name,source in variants.items():
 (work/'candidate.c').write_text(source)
 q=sandbox(['/usr/bin/clang','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','/work/oracle.c','/work/candidate.c','-o','/work/probe'])
 if name in ['failed_build','attempt_test_bypass','unfinished_change']:
  record(name,q.returncode!=0,'Fixed compile/link command refused incomplete source,undefined dependency or replacement main');continue
 record(name+'_build',q.returncode==0,q.stderr[-400:])
 q=sandbox(['/work/probe'])
 if name=='good':record('unchanged_control_passes',q.returncode==0,q.stderr[-300:])
 elif name=='wrong_result':record(name,q.returncode==41,'Independent numerical oracle refused wrong result')
 elif name=='crash_partial_initialization':record(name,q.returncode!=0,'Process failure never becomes acceptance')
 else:record(name,q.returncode!=0 and ('Sanitizer'in q.stderr or 'runtime error'in q.stderr),q.stderr[-600:])
# Real delays verify the decision path;these are synthetic guard tests,not LLM microbenchmarks.
(work/'timing.py').write_text('import sys,time; time.sleep(float(sys.argv[1]))')
def timed(delay):
 t=time.monotonic();q=sandbox(['/usr/bin/python3','/work/timing.py',str(delay)]);assert q.returncode==0;return time.monotonic()-t
pairs=[]
for order in [['native','goose'],['goose','native']]:
 pair={}
 for mode in order:
  elapsed=timed(.01 if mode=='native'else .25);pair[mode]=dict(correct=True,library_sha256='synthetic-one-dso',prefill=1/elapsed,decode=1/elapsed)
 pairs.append(pair)
record('confirmed_slowdown',g.classify_pair(*pairs,2)=='REJECT_CONFIRMED_REGRESSION','Actual opposite-order delayed fixture pair rejected')
other=json.loads(json.dumps(pairs[1]));other['goose']['library_sha256']='different-build'
record('mismatched_build_is_inconclusive',g.classify_pair(pairs[0],other,2)=='INCONCLUSIVE_BUILD_MISMATCH','Different builds cannot establish a regression')
# Requests cannot inject executable commands or claimed PASS results.
try:g.snapshot(root,{'id':'injected','message':'fake','files':[],'status':'PASS','commands':['true']});bad=True
except ValueError:bad=False
record('attempt_to_bypass_gate_protocol',not bad,'Controller rejects foreign fields and candidate supplied commands/outcomes')
(work/'candidate.goose').write_text('fn main(){')
compiler=g.load(root/'trusted/baseline.json')['compiler']['compiler'];isolated=root/'trusted/reference'/Path(compiler).relative_to(g.REPO)
q=sandbox(['/guard/reference/'+str(Path(compiler).relative_to(g.REPO)),'-o','/work/unfinished.c','/work/candidate.goose'])
record('actual_goose_incomplete_source',q.returncode!=0,'Pinned selfhosted compiler refuses an unfinished change')
record('baseline_and_successful_checkpoints_unchanged',g.load(root/'state.json')==base_state,'Fault injections never moved candidate/last-success pointers')
g.verify(root);report=dict(status='PASS',cases=results,production_changed=False,autonomous_agent_started=False,scope='Actual namespace/controller boundary and synthetic fault detector tests;not certification of all future modules or GPU memory safety')
g.save(root/'results/safety-selftest.json',report);g.event(root,'SAFETY_SELFTEST_PASS',cases=len(results));print(json.dumps({'status':'PASS','cases':len(results),'report':str(root/'results/safety-selftest.json')}))
