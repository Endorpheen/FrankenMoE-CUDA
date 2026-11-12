from pathlib import Path
import json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/elementwise-vector'
src=r/'variants/elementwise-vector'
v=r/'variants/elementwise-vector-trace'
(v/'build').mkdir(parents=True,exist_ok=True)
(v/'run').mkdir(exist_ok=True)
code=(s/'kernel.goose').read_text()
code=code.replace('extern "fg_element_witness"','extern "fg_element_vector_witness" fn vector_witness(values:i64);\nextern "fg_element_witness"',1)
for name in ['add','sub','mul','div']:
 start=code.index('fn row_v_'+name+'(')
 first=code.index('{',start);level=1;end=first+1
 while level:
  if code[end]=='{':level+=1
  if code[end]=='}':level-=1
  end+=1
 code=code[:end-1]+'vector_witness(p.a.ne[0]);\n'+code[end-1:]
(s/'kernel-trace.goose').write_text(code)
producer=r/'compiler/stage3-81d6d066a675'
assert hashlib.sha256(producer.read_bytes()).hexdigest()=='81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f'
generated=r/'generated/elementwise-vector-trace.c'
cmd=[str(producer),'-o',str(generated),str(s/'kernel-trace.goose')]
p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
(s/'trace-generation.log').write_text(p.stdout+p.stderr)
print('trace generation',p.returncode,p.stderr[-1000:],flush=True);p.check_returncode()
(s/'trace-generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
unit=(s/'unit.c').read_text().replace('generated/elementwise-vector.c','generated/elementwise-vector-trace.c')
needle='static void fg_element_witness('
assert unit.count(needle)==1
primitive='''static atomic_uint_fast64_t fg_el_vector_calls,fg_el_vector_values;
static void fg_element_vector_witness(int64_t values){
 atomic_fetch_add_explicit(&fg_el_vector_calls,1,memory_order_relaxed);
 atomic_fetch_add_explicit(&fg_el_vector_values,values,memory_order_relaxed);
}
'''
unit=unit.replace(needle,primitive+needle)
needle='uint64_t total=0;'
assert unit.count(needle)==1
unit=unit.replace(needle,'fprintf(stderr,"FG_SELFHOST_ELEMENT_VECTOR calls=%llu values=%llu\\n",(unsigned long long)atomic_load(&fg_el_vector_calls),(unsigned long long)atomic_load(&fg_el_vector_values));\n '+needle)
(s/'unit-trace.c').write_text(unit)
cmds=json.loads((src/'build/commands.json').read_text())
compile=[x.replace(str(s/'unit.c'),str(s/'unit-trace.c')).replace(str(src/'build/goose-element.o'),str(v/'build/goose-element.o')) for x in cmds[3]]
p=subprocess.run(compile,capture_output=True,text=True)
(v/'build/unit-compile.log').write_text(p.stdout+p.stderr)
print('trace unit',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode()
link=[x.replace(str(src/'run'),str(v/'run')).replace(str(src/'build/goose-element.o'),str(v/'build/goose-element.o')) for x in cmds[-1]]
p=subprocess.run(link,capture_output=True,text=True)
(v/'build/link.log').write_text(p.stdout+p.stderr);p.check_returncode()
for path in (src/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for name in ['libggml-cpu.so','libggml-cpu.so.0']:
 p=v/'run'/name
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps([cmd,compile,link],indent=2)+'\n')
(v/'build/provenance.json').write_text(json.dumps({'canonical_goose_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'instrumented_goose_sha256':hashlib.sha256((s/'kernel-trace.goose').read_bytes()).hexdigest(),'difference':'Only post-compute vector_witness calls plus primitive declaration; math and selection unchanged','binary_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest()},indent=2)+'\n')
print('Separate Goose vector-path witness library linked')
