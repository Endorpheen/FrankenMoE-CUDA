from pathlib import Path
import shutil

r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/cpu-backend'
c=s/'control'
archive=r/'diagnostics/b1-278-planner-before-job'
archive.mkdir(parents=True,exist_ok=True)
shutil.copytree(c,archive/'control',dirs_exist_ok=True)
for name in ['cpu-backend-composite-summary.json','cpu-backend-composite-sanitizer-summary.json','cpu-control-planner-summary.json','cpu-control-planner-sanitizer-summary.json']:
    shutil.copy2(r/'results'/name,archive/name)

p=Path('/tmp/fg143_control.goose');text=p.read_text()
assert 'fn graph_job(' not in text
text=text.replace('fn entry(',Path('/tmp/fg143_job.goose').read_text()+'\nfn entry(')
old='else {assert(mode==2);dispatch(c);}'
assert text.count(old)==1
text=text.replace(old,'else if mode==2 {dispatch(c);}\n    else if mode==3 {graph_job(c,out);}\n    else {assert(mode==4);job_worker(c,false);}')
p.write_text(text)
shutil.copy2('/tmp/fg143_job_services.c',c/'job-services.c')

p=Path('/tmp/fg143_control_build.py');text=p.read_text()
old='static atomic_uint_fast64_t control_calls[3];'
assert old in text
text=text.replace(old,'static atomic_uint_fast64_t control_calls[5];').replace('kind>2','kind>4')
declarations='''int64_t fg143_job_get(int64_t,int64_t);
void fg143_job_set(int64_t,int64_t,int64_t);
void fg143_job_create(int64_t,int64_t);
void fg143_job_free(int64_t);
void fg143_job_team(int64_t,int64_t);
void fg143_job_barrier(void);
void fg143_job_place(int64_t,int64_t);
void fg143_job_compute(int64_t,int64_t);
void fg143_job_clear_affinity(void);
'''
text=text.replace('static atomic_uint_fast64_t control_calls[5];',declarations+'static atomic_uint_fast64_t control_calls[5];')
text=text.replace('dispatches=%llu','dispatches=%llu jobs=%llu worker_jobs=%llu').replace('(unsigned long long)atomic_load(control_calls+2));','(unsigned long long)atomic_load(control_calls+2),(unsigned long long)atomic_load(control_calls+3),(unsigned long long)atomic_load(control_calls+4));')
p.write_text(text)

p=Path('/tmp/fg143_integrate.py');text=p.read_text()
marker="pattern=r'(void ggml_compute_forward_mul_mat"
at=text.index(marker)
addition='''core,n=re.subn(r'(enum ggml_status ggml_graph_compute\\(struct ggml_cgraph \\* cgraph, struct ggml_cplan \\* cplan\\) \\{)',lambda m:(s/'control/job-services.c').read_text()+'\\n'+m.group(1),core,count=1)
assert n==1
job_needle='    ggml_cpu_init();\\n\\n    GGML_ASSERT(cplan);'
assert core.count(job_needle)==1
core=core.replace(job_needle,'    ggml_cpu_init();\\n    if(fg143_control_is_enabled()&&fg143_control_supports_jobs())return fg143_control_graph_compute(cgraph,cplan);\\n\\n    GGML_ASSERT(cplan);',1)
'''
text=text[:at]+addition+text[at:]
p.write_text(text)
