"""Add diagnostic full-vocabulary sampling and fixed-history replay."""
from pathlib import Path
import sys

src=Path(sys.argv[1]).read_text()


def replace(old,new):
    global src
    if src.count(old)!=1:
        raise ValueError(old)
    src=src.replace(old,new)


replace('    const double decoding = seconds();',
'''    const double decoding = seconds();
    const char *dump_path = getenv("BONSAI_LOGITS_FILE");
    const char *replay_path = getenv("BONSAI_REPLAY_IDS");
    FILE *dump = dump_path ? fopen(dump_path,"wb") : NULL;
    FILE *replay = replay_path ? fopen(replay_path,"r") : NULL;
    if ((dump_path && !dump) || (replay_path && !replay)) status = 1;''')
replace('        if (status) break;\n        if (ignore_eos)',
'''        if (status) break;
        if (dump && (i<4 || i%8==0 || i+1==generate)) {
            const uint32_t row_index=i;
            if (fwrite(&row_index,sizeof(row_index),1,dump)!=1 ||
                fwrite(logits,sizeof(*logits),VOCAB,dump)!=VOCAB) {
                status=1;
                break;
            }
        }
        if (ignore_eos)''')
replace('        if (stop) break;\n        if (i+1 < generate && evaluate_batch(m, &best, 1, logits)) status = 1;',
'''        if (stop && !replay) break;
        if (i+1 < generate) {
            unsigned next=best;
            if (replay && (fscanf(replay,"%u",&next)!=1 || next>=VOCAB)) status=1;
            if (!status && evaluate_batch(m,&next,1,logits)) status=1;
        }''')
replace('    free(ids);\n    free(logits);','''    if (dump && fclose(dump)!=0) status=1;
    if (replay) fclose(replay);
    free(ids);
    free(logits);''')
Path(sys.argv[2]).write_text(src)
