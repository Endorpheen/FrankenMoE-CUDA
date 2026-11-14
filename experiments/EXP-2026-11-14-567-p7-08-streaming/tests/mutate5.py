#!/usr/bin/env python3
# Mutation run for the caller example (examples/stream_consumer.c).
#   python3 mutate5.py 1 | 2
from pathlib import Path
import subprocess, sys

B = Path('/tmp/opencode/sub-568')
SRC = B / 'examples/stream_consumer.c'
RUN = B / 'run'
L = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
D = L / 'build-cuda/common/CMakeFiles/llama-common.dir'
OBJS = [str(D / p) for p in ('jinja/lexer.cpp.o', 'jinja/parser.cpp.o', 'jinja/runtime.cpp.o',
                             'jinja/value.cpp.o', 'jinja/string.cpp.o', 'jinja/caps.cpp.o',
                             'unicode.cpp.o', 'json.cpp.o', 'trie.cpp.o')]

M1 = [
    ('a published message is reported as kept',
     '            s->published++;\n            return SC_PUBLISHED;',
     '            return SC_KEPT;'),
    ('a kept step is reported as published',
     '        case 1:\n            return SC_KEPT;',
     '        case 1:\n            return SC_PUBLISHED;'),
    ('input_full is not retryable',
     '            return SC_NEED_INPUT_SPACE;',
     '            return SC_BAD_STATE;'),
    ('after_finish is reported as unparsed',
     '            return SC_PROTOCOL;',
     '            return SC_UNPARSED;'),
    ('unparsed output is reported as rejected',
     '            return SC_UNPARSED;',
     '            return SC_REJECTED;'),
    ('output_too_small is reported as a protocol error',
     '            return SC_NEED_WORK_SPACE;',
     '            return SC_PROTOCOL;'),
    ('bad_state is reported as rejected',
     '            return SC_BAD_STATE;',
     '            return SC_REJECTED;'),
    ('rejected is reported as published',
     '            s->rejected++;\n            return SC_REJECTED;',
     '            s->rejected++;\n            return SC_PUBLISHED;'),
]

M2 = [
    ('a rejection is not counted',
     '            s->rejected++;',
     '            ;'),
    ('sc_ok ignores the persistent error',
     '    return s->st[7] == 0;',
     '    return 1;'),
    ('sc_done ignores the finished flag',
     '    return s->st[0] != 0;',
     '    return 0;'),
    ('growth drops the accumulated input bytes',
     '    if (s->st[1] > 0) {\n        memcpy(p, s->inbuf, (size_t) s->st[1]);\n    }',
     '    (void) p;'),
    ('the tool buffer keeps its old capacity',
     '    sl_u8 wtcb_s = {s->wtcb, s->work_cap};',
     '    sl_u8 wtcb_s = {s->wtcb, 4};'),
    ('the message content length comes from reasoning',
     '    v->content_len   = s->st[2];',
     '    v->content_len   = s->st[3];'),
    ('call fields use a broken stride',
     '    const int64_t base = i * 6 + 2 * field;',
     '    const int64_t base = i * 6 + field;'),
]

MUT = {1: M1, 2: M2}


def run(cmd, timeout=400):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    half = int(sys.argv[1])
    orig = SRC.read_text()
    caught = survived = broken = 0
    for i, (name, old, new) in enumerate(MUT[half], 1):
        tag = f'h{half}-{i}'
        n = orig.count(old)
        if n != 1:
            print(f'MUT {tag} SETUP-ERROR occurrences={n} :: {name}')
            broken += 1
            continue
        (RUN / 'consumer_mut.c').write_text(orig.replace(old, new))
        c = run(f'gcc-13 -O1 -std=c11 -I {RUN} -I {B}/examples -c {RUN}/consumer_mut.c -o {RUN}/consumer_mut.o')
        if c.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (gcc) :: {name}')
            broken += 1
            continue
        l = run(f'g++-13 {RUN}/caller_fixture.o {RUN}/consumer_mut.o {RUN}/gen-o0.o ' + ' '.join(OBJS) +
                f' {L}/build-cuda/bin/libggml-base.so.0.27.0 -L{L}/build-cuda/bin -lllama-common'
                f' -Wl,-rpath,{L}/build-cuda/bin -o {RUN}/caller-mut')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (link) :: {name}')
            broken += 1
        else:
            r = run(f'timeout 120 {RUN}/caller-mut')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG568_CALLER')]
            line = tail[-1] if tail else 'no summary (crash)'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('consumer_mut.c', 'consumer_mut.o', 'caller-mut'):
            (RUN / f).unlink(missing_ok=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
