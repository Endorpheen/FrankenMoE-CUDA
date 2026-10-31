import hashlib
import importlib.util
import json
import math
import re
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / 'results/archive/EXP-2026-10-31-132/raw'
OUT = Path(__file__).parent
spec = importlib.util.spec_from_file_location('verify_sse', ROOT / 'results/exp087-lazy-reserve/verify_sse.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


def read_response(tag, kind):
    path = RAW / f'{tag}-{kind}.sse'
    events = [json.loads(line[5:]) for line in path.read_text().splitlines() if line.startswith('data:')]
    final = events[-1]
    expected = 128 if kind == 'long' else 256
    ids = [token for event in events for token in event.get('tokens', [])]
    records = [record for event in events for record in event.get('completion_probabilities', [])]
    assert final.get('stop') and not final.get('truncated')
    assert final['timings']['predicted_n'] == len(ids) == len(records) == expected
    assert all(token == record['id'] for token, record in zip(ids, records))
    assert all(len(record['top_logprobs']) == 5 for record in records)
    parsed = {
        'token_ids': ids,
        'chosen_id': [record['id'] for record in records],
        'chosen_prob': [verify.probability(record['logprob']) for record in records],
        'top5_id': [[entry['id'] for entry in record['top_logprobs']] for record in records],
        'top5_prob': [[verify.probability(entry['logprob']) for entry in record['top_logprobs']] for record in records],
    }
    return parsed, final


def window(tag, kind):
    anchor = json.loads((RAW / f'{tag}-start.json').read_text())
    date = time.localtime(anchor['wall_ns'] // 10**9)
    midnight = int(time.mktime((*date[:3], 0, 0, 0, 0, 0, -1))) * 10**9
    begin = finish = first = current = None
    for line in (RAW / f'{tag}-{kind}.trace').read_text().splitlines():
        match = re.match(r'(\d\d):(\d\d):(\d\d)\.(\d{6}) ', line)
        if match:
            h, m, s, us = map(int, match.groups())
            current = midnight + (h * 3600 + m * 60 + s) * 10**9 + us * 1000
            if '=> Send header' in line and begin is None:
                begin = current
            if '<= Recv data' in line:
                finish = current
        if first is None and re.search(r'"content"\s*:\s*"[^"\s]', line):
            first = current
    assert begin is not None and finish is not None
    return begin, finish, first


result = {'status': 'REJECTED_FOR_PROMOTION', 'scope': 'One-token native speculation diagnostic screen; not a five-pair acceptance benchmark', 'requests': {}}
for tag in ['A1', 'M1', 'S1', 'K1', 'V1', 'N1', 'S2', 'A2']:
    rows = [json.loads(line) for line in (RAW / f'{tag}-resources.jsonl').read_text().splitlines()]
    gpu = json.loads((RAW / f'{tag}-resources.jsonl.vram.json').read_text())
    pid = json.loads((RAW / f'{tag}-start.json').read_text())['pid']
    for kind in ['long', 'code']:
        parsed, final = read_response(tag, kind)
        control, _ = read_response('A1', kind)
        comparison = verify.compare_streams(control, parsed)
        lo, hi, first = window(tag, kind)
        before = [row for row in rows if row['wall_ns'] <= lo]
        after = [row for row in rows if row['wall_ns'] >= hi]
        covered = bool(before and after)
        lower, upper = before[-1] if before else rows[0], after[0] if after else rows[-1]
        within = [row for row in rows if lower['wall_ns'] <= row['wall_ns'] <= upper['wall_ns']]
        vr = [row['used_mib'] for row in gpu if row['pid'] == pid and lower['t_ns'] <= row['t_ns'] <= upper['t_ns']]
        t = final['timings']
        result['requests'][f'{tag}-{kind}'] = {
            'timings': t,
            'client_total_s': (hi - lo) / 1e9,
            'client_ttft_s': (first - lo) / 1e9 if first is not None else None,
            'resource_gates': {
                'covered': covered,
                'swapout_pages': upper['swapout_pages'] - lower['swapout_pages'],
                'major_faults': upper['majflt'] - lower['majflt'],
                'process_swap_max_kib': max(row['swap_kib'] for row in within),
            },
            'sampled_vram_peak_mib': max(vr) if vr else None,
            'sampled_vram_end_mib': vr[-1] if vr else None,
            'rss_peak_kib': max(row['rss_kib'] for row in within),
            'correctness_vs_A1': {
                'gate_passed': comparison[0], 'maximum_probability_delta': comparison[1],
                'description': ('Selected IDs match; ordered top5 or probability gate failed' if parsed['token_ids'] == control['token_ids'] and not comparison[0] else comparison[2]), 'token_ids_exact': parsed['token_ids'] == control['token_ids'],
                'compared_tokens': len(parsed['token_ids']),
                'ordered_top5_exact': parsed['top5_id'] == control['top5_id'],
                'first_ordered_top5_mismatch': next((i for i,(a,b) in enumerate(zip(parsed['top5_id'],control['top5_id'])) if a!=b),None),
            },
            'response_sha256': hashlib.sha256((RAW / f'{tag}-{kind}.sse').read_bytes()).hexdigest(),
        }
result['control_drift'] = {}
for kind in ['long', 'code']:
    a = result['requests'][f'A1-{kind}']['timings']['predicted_per_second']
    b = result['requests'][f'A2-{kind}']['timings']['predicted_per_second']
    result['control_drift'][kind] = {'range_tps': [min(a,b),max(a,b)], 'last_vs_first_pct': (b/a-1)*100}
for name, req in result['requests'].items():
    kind = name.split('-')[-1]
    controls = result['control_drift'][kind]['range_tps']
    req['observed_decode_delta_vs_control_range_pct'] = [(req['timings']['predicted_per_second']/x-1)*100 for x in reversed(controls)]
result['qualification_notes'] = {
    'long': 'A1/M1/S1/K1/V1/N1 full request windows contain system swap-out and are excluded from clean full-request performance claims. S2/A2 clean.',
    'code': 'A1 contains 2 major faults. Use resource-clean A2 as the diagnostic performance control. All candidate code requests fully covered and resource-clean.',
    'correctness': 'All selected token IDs match both controls. M1-code and S2-code fail strict ordered-top5/probability gates; tolerance0.001 unchanged.',
    'scope': 'No five-pair acceptance, quality/stochastic/prefix-session expansion: no candidate qualifies.'
}
result['qualified_code_screen'] = {}
control_tps = result['requests']['A2-code']['timings']['predicted_per_second']
for tag in ['M1', 'S1', 'K1', 'V1', 'N1', 'S2']:
    req = result['requests'][tag+'-code']
    assert req['resource_gates'] == {'covered': True, 'swapout_pages': 0, 'major_faults': 0, 'process_swap_max_kib': 0}
    t = req['timings']
    result['qualified_code_screen'][tag] = {
        'decode_tps': t['predicted_per_second'],
        'observed_delta_vs_A2_pct': (t['predicted_per_second']/control_tps-1)*100,
        'drafted': t.get('draft_n', 0), 'accepted': t.get('draft_n_accepted', 0),
        'eligible_for_acceptance': False,
        'reason': 'No drafts actually generated' if not t.get('draft_n', 0) else ('Strict top5/probability gate failed' if not req['correctness_vs_A1']['gate_passed'] else 'Observed slower than clean control'),
    }

(OUT / 'screen-results.json').write_text(json.dumps(result, indent=2)+'\n')
for name, req in result['requests'].items():
    t=req['timings']; c=req['correctness_vs_A1']
    print(name, round(t['predicted_per_second'],2), 'draft', t.get('draft_n',0), 'accepted',t.get('draft_n_accepted',0), 'ids',c['token_ids_exact'],'gate',c['gate_passed'],'delta',c['maximum_probability_delta'],'vram',req['sampled_vram_peak_mib'],req['sampled_vram_end_mib'],'resources',req['resource_gates'])
