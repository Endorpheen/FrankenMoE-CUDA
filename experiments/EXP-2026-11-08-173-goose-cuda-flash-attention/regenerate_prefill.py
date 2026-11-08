from pathlib import Path
import importlib.util,subprocess
F=Path(__file__).resolve().parents[2];E=Path(__file__).resolve().parent;S=E/'source'
for part in ['iter','process']:
 spec=importlib.util.spec_from_file_location('specializer_'+part,E/('specialize_'+part+'.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 src=subprocess.check_output(['git','-C',str(F),'show','107e17d:experiments/EXP-2026-11-08-173-goose-cuda-flash-attention/source/mma-'+part+'-device.goose'],text=True)
 out=m.transform(src);(S/('mma-'+part+'-device.goose')).write_text('\n'.join(x.rstrip()for x in out.splitlines())+'\n')
 print('Regenerated actual Goose',part)
