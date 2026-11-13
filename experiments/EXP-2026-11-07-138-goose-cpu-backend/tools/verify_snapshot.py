from pathlib import Path
import json,hashlib,re
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'payload-provenance.json').read_text())
for row in manifest['files']:
    p=root/row['path']
    assert hashlib.sha256(p.read_bytes()).hexdigest()==row['snapshot_sha256'],row['path']
    if p.suffix in ['.c','.cpp','.h']:
        for include in re.findall(r'^#include\s+"([^"]+)"',p.read_text(),re.M):
            if include.startswith('.') or include=='bridge.h':
                assert (p.parent/include).is_file(),(row['path'],include)
for line in (root/'SHA256SUMS').read_text().splitlines():
    expected,name=line.split('  ',1)
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected,name
assert not any(p.name in ['COMPILER_FEEDBACK.md','AGENTS.override.md','migration-plan.json'] for p in root.rglob('*'))
print(json.dumps({'status':'PASS_PAYLOAD_INTEGRITY_AND_RELATIVE_INCLUDES','files':len(manifest['files']),'inference_claim':False},indent=2))
