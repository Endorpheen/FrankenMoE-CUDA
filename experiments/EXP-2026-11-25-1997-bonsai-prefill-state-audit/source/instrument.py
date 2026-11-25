"""Capture prefill state and support explicit KV counterfactuals in a test build."""
from pathlib import Path
import sys

s=Path(sys.argv[1]).read_text()


def replace(a,b):
    global s
    if s.count(a)!=1:
        raise ValueError(a)
    s=s.replace(a,b)


replace('    float epsilon;','''    float epsilon;
    FILE *audit, *reference_kv;
    unsigned char *audit_buffer;
    unsigned audit_prompt, audit_layer, audit_kv;
    unsigned overrides;''')
start=s.index('int64_t bonsai_launch(')
s=s[:start]+'''enum { AUDIT_LAYER = 0, AUDIT_K_INPUT = 1, AUDIT_V_INPUT = 2,
       AUDIT_K_PACKED = 3, AUDIT_V_PACKED = 4, AUDIT_GDN = 5 };

static int audit_write(model *m,unsigned kind,CUdeviceptr address,unsigned bytes) {
    const uint32_t header[5]={kind,m->position,m->count,m->audit_layer,bytes};
    if (check(cuMemcpyDtoH(m->audit_buffer,address,bytes),"audit device read")) return -1;
    return fwrite(header,sizeof(header),1,m->audit)==1 &&
           fwrite(m->audit_buffer,bytes,1,m->audit)==1 ? 0 : -1;
}

static int audit_reference(model *m,unsigned kind,CUdeviceptr address,unsigned bytes) {
    uint32_t header[5];
    if (fread(header,sizeof(header),1,m->reference_kv)!=1 || header[0]!=kind ||
        header[1]!=m->position || header[2]!=m->count ||
        header[3]!=m->audit_layer || header[4]!=bytes ||
        fread(m->audit_buffer,bytes,1,m->reference_kv)!=1) return -1;
    if (!(m->overrides & (kind==AUDIT_K_PACKED ? 1 : 2))) return 0;
    return check(cuMemcpyHtoD(address,m->audit_buffer,bytes),"counterfactual KV");
}

'''+s[start:]
replace('''        case ADD:
            return launch(m, ADD, 20 * count, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2] });''',
'''        case ADD: {
            const int rc=launch(m,ADD,20*count,1,1,256,(void *[]){&p[0],&p[1],&p[2]});
            if (rc) return rc;
            if (p[2]==(CUdeviceptr)m->b[0] && m->position<m->audit_prompt) {
                if (m->audit) {
                    for (unsigned i=0;i<4;++i) {
                        const unsigned row=(count-1)*i/3;
                        if (audit_write(m,AUDIT_LAYER,p[2]+(size_t)row*5120*4,5120*4)) return -1;
                    }
                }
                ++m->audit_layer;
            }
            return 0;
        }''')
replace('''        case GDN:
            return gdn(m, p);''','''        case GDN: {
            const int rc=gdn(m,p);
            if (rc) return rc;
            if (m->audit && m->position<m->audit_prompt) {
                for (unsigned head=0;head<48;head+=16) {
                    if (audit_write(m,AUDIT_GDN,p[7]+(size_t)head*128*128*4,128*8*4)) return -1;
                }
            }
            return 0;
        }''')
replace('''            return launch(m, Q4, (tasks+127)/128, 1, 1, 128, (void *[]){ &p[0], &p[1], &stride, &blocks, &tasks });''',
'''            const int rc=launch(m,Q4,(tasks+127)/128,1,1,128,(void *[]){&p[0],&p[1],&stride,&blocks,&tasks});
            if (rc || m->position>=m->audit_prompt) return rc;
            const unsigned part=m->audit_kv++%2, packed=AUDIT_K_PACKED+part;
            if (m->audit && (audit_write(m,AUDIT_K_INPUT+part,p[0],count*4096) ||
                             audit_write(m,packed,p[1],count*KV_BYTES))) return -1;
            if (m->reference_kv && audit_reference(m,packed,p[1],count*KV_BYTES)) return -1;
            return 0;''')
replace('''    m->count = count;
    m->packed_source = 0;''','''    m->count = count;
    m->packed_source = 0;
    m->audit_layer = 0;
    m->audit_kv = 0;''')
replace('''    const unsigned prompt = (unsigned)(argc - first);''',
'''    const unsigned prompt = (unsigned)(argc - first);
    const char *audit_path=getenv("BONSAI_AUDIT_FILE");
    const char *kv_path=getenv("BONSAI_AUDIT_REFERENCE_KV");
    const char *which=getenv("BONSAI_AUDIT_OVERRIDE");
    if (audit_path || kv_path) {
        m->audit_prompt=prompt;
        if (audit_path) m->audit=fopen(audit_path,"wb");
        if (kv_path) m->reference_kv=fopen(kv_path,"rb");
        m->audit_buffer=malloc((size_t)ubatch*4096+5120*4);
        if ((audit_path && !m->audit) || (kv_path && !m->reference_kv) || !m->audit_buffer) status=1;
        m->overrides=!which || strcmp(which,"both")==0 ? 3 : strcmp(which,"k")==0 ? 1 : strcmp(which,"v")==0 ? 2 : 0;
        if (kv_path && !m->overrides) status=1;
    }''')
replace('''    free(ids);
    free(logits);''','''    if (m->reference_kv) {
        if (fgetc(m->reference_kv)!=EOF || ferror(m->reference_kv)) status=1;
        fclose(m->reference_kv);
    }
    if (m->audit && fclose(m->audit)!=0) status=1;
    free(m->audit_buffer);
    free(ids);
    free(logits);''')
Path(sys.argv[2]).write_text(s)
