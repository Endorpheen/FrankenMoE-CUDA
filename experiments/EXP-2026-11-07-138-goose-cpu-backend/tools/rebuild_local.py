from pathlib import Path
import json,subprocess,tempfile,os,hashlib,struct

packet=Path(__file__).resolve().parents[1]
provenance=json.loads((packet/'payload-provenance.json').read_text())
local=Path(provenance['source_root'])
units=json.loads((packet/'rebuild-units.json').read_text())
reports={}

def run(cmd,env=None,timeout=180):
    p=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=timeout)
    if p.returncode:
        raise RuntimeError(json.dumps({'argv':cmd,'returncode':p.returncode,'stdout_tail':p.stdout[-2000:],'stderr_tail':p.stderr[-4000:]},indent=2))
    return p

with tempfile.TemporaryDirectory(prefix='exp138-rebuild-',dir='/home/end0/AI/LlamaServer') as directory:
    work=Path(directory)
    replacements={}
    compile_records=[]
    for unit in units:
        original=unit['command']
        assert isinstance(original,list) and '-c' in original,unit['label']
        old_output=original[original.index('-o')+1]
        destination=work/(unit['label']+'.o')
        cmd=list(original)
        cmd[cmd.index('-o')+1]=str(destination)
        source=Path(cmd[cmd.index('-c')+1])
        if source.is_relative_to(local/'source/ggml/src/ggml-cpu'):
            source=packet/'reference'/source.name
        elif source.is_relative_to(local/'source'):
            source=packet/source.relative_to(local)
        else:
            raise AssertionError(source)
        cmd[cmd.index('-c')+1]=str(source)
        run(cmd)
        replacements[old_output]=str(destination)
        compile_records.append({'label':unit['label'],'snapshot_source':str(source.relative_to(packet)),'object_sha256':hashlib.sha256(destination.read_bytes()).hexdigest()})
        print('compiled',unit['label'],flush=True)
    link=json.loads((packet/'build-records/elementwise-vector/commands.json').read_text())[-1]
    old_lib=link[link.index('-o')+1]
    replacements[old_lib]=str(work/'libggml-cpu.so.0.27.0')
    link=[replacements.get(arg,arg) for arg in link]
    assert all(path not in link for path in replacements if path!=old_lib)
    run(link)
    for name in ['libggml-cpu.so','libggml-cpu.so.0']:
        (work/name).symlink_to('libggml-cpu.so.0.27.0')
    for lib in (local/'variants/elementwise-vector/run').iterdir():
        if lib.name.startswith('libggml-cpu.so'):continue
        if '.so' in lib.name:(work/lib.name).symlink_to(lib)
    binary_sha=hashlib.sha256((work/'libggml-cpu.so.0.27.0').read_bytes()).hexdigest()
    outputs={}
    for kind,source_name,expected_root in [
        ('elementwise','elementwise-vector/fixture.c','elementwise-vector'),
        ('tiled','cpu-moe-tiled-register/fixture-expanded.c','cpu-moe-tiled-expanded')]:
        source=packet/'source'/source_name
        executable=work/(kind+'-fixture')
        cmd=['/usr/bin/gcc-13','-O3','-I'+str(local/'source/ggml/include'),str(source),'-L'+str(work),'-lggml-cpu','-lggml-base','-lm','-Wl,-rpath,'+str(work),'-o',str(executable)]
        run(cmd)
        for mode in ['off','on']:
            env=dict(os.environ)
            env.pop('LD_PRELOAD',None)
            env.update(LD_LIBRARY_PATH=str(work),GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_MOE='1',GGML_GOOSE_TILED=('0' if mode=='off' else '1') if kind=='tiled' else '1',GGML_GOOSE_ELEMENTWISE=('0' if mode=='off' else '1') if kind=='elementwise' else '1')
            dump=work/(kind+'-'+mode+'.bin')
            p=run([str(executable),str(dump)],env=env)
            summary=json.loads(p.stdout.splitlines()[-1])
            expected=local/'results'/((expected_root+'-'+mode+'.bin') if kind=='elementwise' else (expected_root+'-off.bin'))
            actual=dump.read_bytes()
            assert actual==expected.read_bytes(),(kind,mode,'must reproduce all canonical bytes including recorded NaN behavior')
            outputs[kind+'-'+mode]={'summary':summary,'sha256':hashlib.sha256(actual).hexdigest(),'all_bytes_equal_local_canonical_artifact':True}
            print('replayed',kind,mode,summary,flush=True)
    reports={'status':'PASS_SNAPSHOT_COMPILE_LINK_AND_REAL_GGML_REPLAY','compiled_snapshot_units':compile_records,'linked_cpu_sha256':binary_sha,'runs':outputs,'production_changed':False,'compiler_rebuilt':False,'scope':'Uses the exact original-host GGML baseline objects/headers. Does not re-run model inference. NaN payload variation versus original C++ remains the recorded limitation.'}
(packet/'local-rebuild-verification.json').write_text(json.dumps(reports,indent=2)+'\n')
print(json.dumps({'status':reports['status'],'snapshot_units':len(compile_records),'replayed_graphs':sum(row['summary']['graph_cases'] for row in outputs.values())},indent=2))
