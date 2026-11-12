#!/usr/bin/env python3
"""Derive the HTTP route table from tools/server/server.cpp and emit the Goose
module plus the fixture's expected-route include.

The native source is the oracle: every ctx_http.get/post/del/ws registration is
parsed, the handler name is mapped to an endpoint category, and duplicate
(method, path) registrations must agree. Dynamic registrations (GCP AIP routes
from register_gcp_compat) are excluded and listed in the README.
"""
from pathlib import Path
import json
import re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-449-p8-02-route-table'
SRC = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/tools/server/server.cpp')

METHODS = {'get': 1, 'post': 2, 'del': 3, 'ws': 4}

# endpoint category by handler suffix
HANDLER_CAT = {
    'get_health': 1,
    'get_metrics': 2,
    'get_props': 3,
    'post_props': 4,
    'get_models': 5,
    'get_router_models': 5,
    'post_router_models': 6,
    'post_router_models_load': 7,
    'post_router_models_unload': 8,
    'get_router_models_sse': 9,
    'del_router_models': 10,
    'post_completions': 11,
    'post_completions_oai': 12,
    'post_chat_completions': 13,
    'post_control': 14,
    'post_responses_oai': 15,
    'post_transcriptions_oai': 16,
    'post_tts': 17,
    'ws_realtime': 18,
    'post_anthropic_messages': 19,
    'post_infill': 20,
    'post_embeddings': 21,
    'post_embeddings_oai': 22,
    'post_rerank': 23,
    'post_systemone': 24,
    'post_tokenize': 25,
    'post_detokenize': 26,
    'post_apply_template': 27,
    'post_chat_completions_tok': 28,
    'post_responses_tok_oai': 29,
    'post_anthropic_count_tokens': 30,
    'get_lora_adapters': 31,
    'post_lora_adapters': 32,
    'get_slots': 33,
    'post_slots': 34,
    'stream_get_h': 35,
    'streams_lookup_h': 36,
    'stream_delete_h': 37,
    'proxy_handler_get': 38,
    'proxy_handler_post': 38,
}
PATH_CAT = {'/cors-proxy': 38, '/tools': 39}

CAT_NAME = {
    0: 'none', 1: 'health', 2: 'metrics', 3: 'props_get', 4: 'props_post',
    5: 'models_get', 6: 'models_post', 7: 'models_load', 8: 'models_unload',
    9: 'models_sse', 10: 'models_delete', 11: 'completions', 12: 'completions_oai',
    13: 'chat', 14: 'chat_control', 15: 'responses', 16: 'transcriptions',
    17: 'tts', 18: 'realtime', 19: 'anthropic', 20: 'infill', 21: 'embeddings',
    22: 'embeddings_oai', 23: 'rerank', 24: 'systemone', 25: 'tokenize',
    26: 'detokenize', 27: 'apply_template', 28: 'chat_tokens', 29: 'responses_tokens',
    30: 'anthropic_count', 31: 'lora_get', 32: 'lora_post', 33: 'slots_get',
    34: 'slots_post', 35: 'stream_get', 36: 'stream_lookup', 37: 'stream_delete',
    38: 'cors_proxy', 39: 'tools',
}

ROUTE_RE = re.compile(
    r'ctx_http\.(get|post|del|ws)\s*\(\s*"([^"]+)"\s*,\s*(.+?)\);\s*(?://.*)?$')


def category_of(path, expr):
    if path in PATH_CAT:
        return PATH_CAT[path]
    # last identifier of the handler expression, e.g. routes.get_health or
    # models_routes->post_router_models
    m = re.findall(r'[A-Za-z_][A-Za-z0-9_]*', expr)
    for name in reversed(m):
        if name in HANDLER_CAT:
            return HANDLER_CAT[name]
    raise SystemExit(f'cannot classify handler {expr!r} for {path}')


def parse_routes():
    routes = {}
    dynamic = []
    for line in SRC.read_text().splitlines():
        if 'ctx_http.register_gcp_compat()' in line:
            dynamic.append('GCP AIP health/predict routes (register_gcp_compat)')
            continue
        m = ROUTE_RE.search(line.strip())
        if not m:
            continue
        method = METHODS[m.group(1)]
        path = m.group(2)
        cat = category_of(path, m.group(3))
        key = (method, path)
        if key in routes and routes[key] != cat:
            raise SystemExit(f'conflicting category for {key}: {routes[key]} vs {cat}')
        routes[key] = cat
    return routes, dynamic


def fnv(method, path):
    h = 14695981039346656037
    c = {1: ord('G'), 2: ord('P'), 3: ord('D'), 4: ord('W')}[method]
    h = ((h ^ c) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    for b in path.encode():
        h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    routes, dynamic = parse_routes()
    keys = {}
    for (method, path) in routes:
        k = fnv(method, path)
        if k in keys:
            raise SystemExit(f'hash collision between {keys[k]} and {(method, path)}')
        keys[k] = (method, path)

    method_name = {1: 'GET', 2: 'POST', 3: 'DELETE', 4: 'WS'}
    lines = [
        '// HTTP route table in Goose, derived from tools/server/server.cpp.',
        '//',
        '// Goose has no string type, so each route is keyed by a 64-bit FNV-1a',
        '// value over the method byte followed by the path bytes. The keying and',
        '// the table live in Goose; the gate proves every native registration maps',
        '// to its endpoint category and that no key collides.',
        '//',
        '// Method bytes: GET 71, POST 80, DELETE 68, WS 87.',
        'extern "fg449_ld_u8" fn ld_u8(p:u64,o:u64)->u32;',
        'extern "fg449_st_i64" fn st_i64(p:u64,o:u64,v:i64);',
        '',
        'fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }',
        '',
        'fn route_key(m:i64,p:u64,n:i64)->u64{',
        ' var h:u64=14695981039346656037 as u64;',
        ' var c:u64=71 as u64;',
        ' if m==2 as i64 { c=80 as u64; }',
        ' if m==3 as i64 { c=68 as u64; }',
        ' if m==4 as i64 { c=87 as u64; }',
        ' h=(h^c)*(1099511628211 as u64);',
        ' var i:i64=0 as i64;',
        ' while i<n {',
        '  h=(h^(ld_u8(p,(i as u64)) as u64))*(1099511628211 as u64);',
        '  i=i+1 as i64;',
        ' }',
        ' return h;',
        '}',
        '',
        '// Endpoint category by exact method/path key; 0 means no route.',
        'fn route_id(m:i64,p:u64,n:i64)->i64{',
        ' if n<=0 as i64 {return 0 as i64;}',
        ' if n>64 as i64 {return 0 as i64;}',
        ' let k:u64=route_key(m,p,n);',
    ]
    for (method, path), cat in sorted(routes.items(), key=lambda x: (x[1], x[0][1])):
        lines.append(f' if k=={fnv(method, path)} as u64 {{return {cat} as i64;}} // {method_name[method]} {path} -> {CAT_NAME[cat]}')
    lines += [
        ' // parameterised route: POST /slots/:id_slot',
        ' if m==2 as i64 && n>7 as i64 {',
        '  if (ld_u8(p,0 as u64) as i64)==47 as i64 {',
        '   if (ld_u8(p,1 as u64) as i64)==115 as i64 {',
        '    if (ld_u8(p,2 as u64) as i64)==108 as i64 {',
        '     if (ld_u8(p,3 as u64) as i64)==111 as i64 {',
        '      if (ld_u8(p,4 as u64) as i64)==116 as i64 {',
        '       if (ld_u8(p,5 as u64) as i64)==115 as i64 {',
        '        if (ld_u8(p,6 as u64) as i64)==47 as i64 { return 34 as i64; }',
        '       }',
        '      }',
        '     }',
        '    }',
        '   }',
        '  }',
        ' }',
        ' return 0 as i64;',
        '}',
        '',
        '// Public endpoints skip the API-key middleware: /health and /v1/health.',
        '// Frontend assets are classified outside this table.',
        'fn requires_key(cat:i64)->i64{',
        ' if cat==0 as i64 {return 0 as i64;}',
        ' if cat==1 as i64 {return 0 as i64;}',
        ' return 1 as i64;',
        '}',
        '',
        'fn main(){',
        ' put(0 as u64,0 as i64,route_id(1 as i64,0 as u64,0 as i64));',
        ' put(0 as u64,1 as i64,(route_key(1 as i64,0 as u64,0 as i64) as i64));',
        ' put(0 as u64,2 as i64,requires_key(0 as i64));',
        '}',
        '',
    ]
    (E / 'source').mkdir(parents=True, exist_ok=True)
    (E / 'source/routes.goose').write_text('\n'.join(lines))

    inc = ['// generated by gen_routes.py from tools/server/server.cpp']
    for (method, path), cat in sorted(routes.items(), key=lambda x: (x[1], x[0][1])):
        inc.append(f'{{ {method}, "{path}", {cat} }},')
    (E / 'tests').mkdir(parents=True, exist_ok=True)
    (E / 'tests/expected_routes.inc').write_text('\n'.join(inc) + '\n')

    (E / 'expected.json').write_text(json.dumps(dict(
        routes=[dict(method=m, path=p, category=c, key=f'{fnv(m, p):016x}')
                for (m, p), c in sorted(routes.items(), key=lambda x: (x[1], x[0][1]))],
        dynamic=dynamic), indent=2) + '\n')
    print(f'parsed {len(routes)} routes, {len(keys)} distinct keys, dynamic: {dynamic}')


if __name__ == '__main__':
    main()
