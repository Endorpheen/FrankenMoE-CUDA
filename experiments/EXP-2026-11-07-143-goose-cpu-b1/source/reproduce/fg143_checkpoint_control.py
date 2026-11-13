from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess

r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
w=Path('/home/end0/AI/LlamaServer/franken-goose-checkpoint-20261107')
s=r/'source/cpu-backend'
dest=w/'experiments/EXP-2026-11-07-143-goose-cpu-b1'
out=r/'results'
composite=json.loads((out/'cpu-backend-composite-summary.json').read_text())
contract=json.loads((out/'cpu-backend-apex-contract-summary.json').read_text())
assert len(composite['checks'])==326 and all(c['within_predeclared_tolerance'] for c in composite['checks'])
assert len(contract['checks'])==66 and all(not c['violations'] for c in contract['checks'])
assert len([c for c in contract['checks'] if '-op2-' in c['name'] and c['exact']])==9
for name,count in [('cpu-backend-composite-sanitizer-summary.json',326),('cpu-control-planner-sanitizer-summary.json',960),('cpu-backend-apex-contract-sanitizer-summary.json',66)]:
    report=json.loads((out/name).read_text());assert report['status'].startswith('PASS_') and report['cases']==count
log=(out/'cpu-backend-composite-candidate.log').read_text()
trace=next(line for line in log.splitlines() if line.startswith('FG143_CONTROL '))
scope_path=s/'control/scope.json';scope=json.loads(scope_path.read_text())
scope.update(status='PASS_SELECTED_CONTROL_JOB_CORPUS_NOT_FULL_B1',
             active_job_backend='OpenMP',
             job_scope='Goose owns plan validation,create/reuse/free decisions,reset policy,team launch selection,worker publication/barrier ordering and result propagation. Physical pool allocation/initialization and OS placement remain native primitives.',
             compute_cases=326,planner_cases=960,apex_contract_cases=66,
             current_composite_trace=trace,
             limitations='Native-extra and tiled scratch providers,unfinished operator leaves,pool physical initialization,OS affinity,non-OpenMP path and full model equivalence remain open.')
scope_path.write_text(json.dumps(scope,indent=2)+'\n')
p=s/'glu-numerical-contract.json';d=json.loads(p.read_text());d.update(status='PASS_SELECTED_NUMERICAL_CONTRACTS_MODEL_GATE_OPEN',swiglu_wide_byte_exact='9/9',signed_zero='Use comparison/select semantics matching native std::min/std::clamp in GLU,avoiding optimizer-dependent fminf/fmaxf zero signs.',apex_contract_cases=66,apex_contract_byte_exact=sum(c['exact'] for c in contract['checks']),instrumented_output_equals_release=True);p.write_text(json.dumps(d,indent=2)+'\n')

for name in ['fg143_control_build.py','fg143_control_integrate.py','fg143_control_test.py','fg143_job_integrate.py',
             'fg143_integrate.py','fg143_generate.py','fg143_sanitize.py','fg143_composite.py','fg143_apex_contract.py',
             'fg143_fix_glu_contract.py','fg143_checkpoint_control.py']:
    shutil.copy2(Path('/tmp')/name,s/'reproduce'/name)
for name in ['fg143_control.goose','fg143_control_fixture.c','fg143_job.goose','fg143_job_services.c',
             'fg143_pool_lifecycle.c','fg143_simd_exp.goose','fg143_fixture.c']:
    shutil.copy2(Path('/tmp')/name,s/'reproduce'/name)
manifest_path=r/'variants/cpu-backend/build/integration-manifest.json'
manifest=json.loads(manifest_path.read_text());manifest.update(status='PASS_SELECTED_CORRECTNESS_AND_SCOPED_SAFETY_NOT_FULL_B1',cpu_sha256=hashlib.sha256((r/'variants/cpu-backend/run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),model_equivalence='OPEN:historical one-off254case candidate diverged; current control/numerical fixes not re-run on model',functional_graph_cases=326,planning_cases=960,apex_operator_contract_cases=66,requirements_closed=0)
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
shutil.copytree(s,dest/'source',dirs_exist_ok=True)
for name in ['cpu-backend.c','cpu-executor.c','cpu-stateful.c','cpu-control.c','cpu-backend-codec.c']:
    shutil.copy2(r/'generated'/name,dest/'generated'/name)
patterns=['cpu-backend-composite-*','cpu-backend-apex-contract-*','cpu-control-planner-*']
for pattern in patterns:
    for path in out.glob(pattern):
        if path.suffix in ['.json','.jsonl','.log']:
            shutil.copy2(path,dest/'results'/path.name)
for name in ['integration-commands.json','integration-manifest.json','control-command.json','control-planner-command.json',
             'renamed-native-symbols.json','composite-command.json','executor-command.json','stateful-command.json']:
    shutil.copy2(r/'variants/cpu-backend/build'/name,dest/'build-receipts'/name)
shutil.copy2(r/'variants/cpu-backend-sanitize/build/commands.json',dest/'build-receipts/sanitizer-commands.json')

p=dest/'README.md';text=p.read_text()
marker='## Functional evidence\n';assert text.count(marker)==1
section='''## Latest control and numerical-contract checkpoint

The same common candidate now executes operation selection,task-count policy,
scratch arithmetic and the active OpenMP graph-job lifecycle in Goose. There
are96operation leaves; this is not96migrated arithmetic kernels. Native backend
extra/tiled scratch providers,unfinished compute,physical pool initialization,
OS affinity and non-OpenMP support remain explicit residuals.

Current evidence supersedes earlier bounded counts below:326/326 differential
graph/status cases pass (including reuse,cancellation and recovery on1/2/8/16
workers);960/960planning cases match the preserved pre-control B1 and rollback;
66/66wide Apex operator contracts pass. All three suites pass scoped
ASan/UBSan/leak checks. Instrumented compute/contract output matches release
bytes; instrumented planning matches the preserved reference bytes.

F32 SWIGLU now reproduces the original AVX2 FMA/exponent polynomial for complete
8lane chunks with scalar tails,implemented in Goose. All9wide SWIGLU cases
match native bytes. GLU clamp/OAI comparison-selection preserves the source
std::min/std::clamp signed-zero contract; no speed optimization was performed.
The prior zero-sign-only cross-optimization mismatch is retained in local
diagnostic records and described in compiler feedback; it was not a sanitizer
memory finding. General NaN payload/ISA and full model equivalence remain open.

The earlier model timings below belong to the frozen254case diagnostic binary,
not this newer control/numerical candidate. No repeated model request was run.
Disable the whole new control/job policy with`GGML_GOOSE_CPU_CONTROL=0`.

'''
text=text.replace(marker,section+marker)
p.write_text(text)

ledger_path=w/'docs/goose-migration/tasks.json';ledger=json.loads(ledger_path.read_text())
for task in ledger['tasks']:
    if task['id']=='P1-09':
        task['status']='IN_PROGRESS';task['evidence']='EXP143 Goose active OpenMP graph-job lifecycle and worker publication/barrier/result policy;326graph/status cases include1/2/8/16thread persistent pool,cancellation/recovery. Physical allocation/init,OS placement,expert chunk claims and non-OpenMP implementation remain native/incomplete.'
    if task['id']=='P2-15':
        task['evidence']='EXP143 Goose operation dispatch (96leaves),task-count/scratch policy and active OpenMP job lifecycle:960planning cases exact vs previous B1 and rollback;326graph/status cases pass;scoped sanitizers pass. Native backend scratch providers,pool physical initialization and OS/non-OpenMP paths remain; whole-model equivalence open.'
    if task['id']=='P2-01':
        task['evidence']+=' EXP143 follow-up ports original AVX2 SWIGLU numerical contract into Goose:9/9wide cases byte-exact; GLU signed-zero selection corrected.326general and66Apex-contract cases pass scoped sanitizers. No new model request; full probability equivalence/NaN/type closure remain open.'
ledger['history'].append({'checkpoint':'EXP143 CPU control/job and numerical-contract checkpoint',
                         'event':'Goose owns shared CPU plan/dispatch and active OpenMP graph-job policy.326/326graph/status,960/960planning/rollback and66/66Apex operator contracts pass with scoped sanitizers. SWIGLU9/9wide configurations now byte-exact after matching AVX2 math; GLU signed-zero difference repaired. Full Apex response equivalence remains open,no repeated model request/performance tuning. B1 is incomplete; residual source remains visible.'})
ledger_path.write_text(json.dumps(ledger,indent=2)+'\n')

feedback=w/'COMPILER_FEEDBACK.md'
assert subprocess.run(['git','check-ignore','-q','COMPILER_FEEDBACK.md'],cwd=w).returncode==0
assert not subprocess.check_output(['git','ls-files','--','COMPILER_FEEDBACK.md'],cwd=w).strip()
assert not subprocess.check_output(['git','log','--all','--format=%h','--','COMPILER_FEEDBACK.md'],cwd=w).strip()
assert '## CF-029 ' not in feedback.read_text()
with feedback.open('a') as handle:
    handle.write('''

## CF-029 — Larger CPU control/OpenMP module and numerical semantics

Date2026-11-07. Same pinned EXP300 Stage3 producer454151b3449b... handles a
whole CPU control module:operation dispatch96leaves,task counts,scratch arithmetic,
and active OpenMP graph-job lifecycle. Actual compile flags include
-DGGML_USE_OPENMP/-fopenmp; do not misrepresent pthread-only code as active evidence.
960planning/rollback cases byte-exact to previous B1;326graph/status cases pass,
including persistent1/2/8/16thread pools,cancellation and recovery.66Apex operator
contracts pass. Scoped ASan/UBSan/leak checks pass all3suites. New control,stateful,
executor/common units and core/metadata are instrumented; retained native objects,
vendor OpenMP runtime,CUDA and other subsystems are not. NoTSan/full safety claim.

Functional model-drift investigation found a real numerical-contract difference:
new SWIGLU used scalar expf while original F32 full8lane chunks use the AVX2
FMA/bit-exponent polynomial. That algorithm is now Goose math,not a call to native
activation.9/9wide SWIGLU cases are byte-exact. This is precision compatibility,
not local speed tuning; no timings or model rerun were made. It is a plausible
contributor,not proof that the historical full-Apex distribution drift is resolved.

Strict release/instrumented byte comparison exposed signed-zero drift in packed
SWIGLU_CLAMP with limit0:24402values/case differed only0x00000000/0x80000000 at
1/8/16threads,zero ordinary numerical-tolerance violations,no sanitizer finding.
fminf/fmaxf zero-sign selection differs by optimizer/runtime; native C++ uses
std::min/std::clamp. Replaced GLU min/max with source-equivalent comparison/select.
Final66instrumented outputs now equal release bytes;39cases match native exactly.
Keep signed zeros in compiler/runtime numerical contracts; do not label all
floating differences compiler defects or silently waive a byte gate.

New generator application errors were preserved:pad is a reserved Goose name
(renamedalign_size),u32conditional literals need explicit branch casts; a Python
integration variable shadowed the baselinePath before linking (renamed). No
compiler source edits. Remaining native extra/tiled scratch providers,physical
pool init,OS placement and non-OpenMP paths are explicit incomplete scope. Whole
Apex correctness remains OPEN; native deployment and Qwen are untouched.
''')

subprocess.run(['python3',str(w/'scripts/update_goose_migration_plan.py')],cwd=w,check=True)
lines=[]
for path in sorted(dest.rglob('*')):
    if path.is_file() and path.name!='SHA256SUMS':
        lines.append(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+str(path.relative_to(dest)))
(dest/'SHA256SUMS').write_text('\n'.join(lines)+'\n')
print('Checkpoint payload:',len(lines),'files; model correctness remains open; no task marked done.')
