"""Compute exact operation counts, hardware lower bounds and a slack allocation."""
from pathlib import Path
import json
import sys

root=Path(sys.argv[1])
traces=json.loads((root/'trace-summary.json').read_text())
weights=json.loads((root/'weights.json').read_text())['trunk']
gpu={'sms':46,'clock_ghz':2.775,'peak_observed_ghz':2.835,'stock_ghz':2.48,
     'dense_ops_per_sm_cycle':{'FP16_to_FP32':512,'FP8_to_FP32':1024,'INT8':2048,'INT4':4096}}
def peaks(clock):
    return {n:gpu['sms']*clock*rate/1000 for n,rate in gpu['dense_ops_per_sm_cycle'].items()}
peak=peaks(gpu['clock_ghz'])
out=[]
for trace in traces:
    n=trace['tokens']
    p=trace['phases']['prefill']
    goal_ns=n*1e9/5000
    mmq_ops=2*weights['weights']*(n-1)
    attention_ops=4*(n*(n+1)//2)*24*256*16
    decay_ops=2*n*48*2*48*5120
    budgets={}
    for ref,fr in [('stock',gpu['stock_ghz']),('trace_median',gpu['clock_ghz']),('trace_max',gpu['peak_observed_ghz'])]:
        rates=peaks(fr)
        budgets[ref]={
            mode+' + '+atn:(mmq_ops/(rates[mode]*1e12)+attention_ops/(rates[atn]*1e12))*1000
            for mode,atn in [('FP16_to_FP32','FP16_to_FP32'),('INT8','FP16_to_FP32'),
                             ('INT8','FP8_to_FP32'),('INT4','FP16_to_FP32'),('INT4','FP8_to_FP32')]}
    measured={key:data['event_ns'] for key,data in p['kernel_groups'].items()}
    measured['GPU transfer']=p['copy_union_ns']
    measured['GPU idle']=p['gpu_idle_ns']
    assert abs(sum(measured.values())-p['wall_ns'])<1000
    floors={key:0 for key in measured}
    floors['PQ2 batched matrix']=mmq_ops/(peak['INT4']*1000)
    floors['Attention stage, tiles and merge']=attention_ops/(peak['FP8_to_FP32']*1000)
    fp32_cuda_peak_tops=gpu['sms']*gpu['clock_ghz']*128*2/1000
    floors['F32 decay and beta']=decay_ops/(fp32_cuda_peak_tops*1000)
    # A planning envelope, not a projected timing. Allocate the remaining
    # slack in proportion to measured time above the modeled floors.
    fraction=(goal_ns-sum(floors.values()))/(p['wall_ns']-sum(floors.values()))
    assert 0<fraction<1
    plan={key:dict(measured_ms=current/1e6,analytic_floor_ms=floors[key]/1e6,
        budget_ms=(floors[key]+fraction*(current-floors[key]))/1e6,
        required_reduction_ms=(1-fraction)*(current-floors[key])/1e6)
        for key,current in measured.items()}
    assert abs(sum(v['budget_ms'] for v in plan.values())-goal_ns/1e6)<1e-4
    out.append(dict(tokens=n,target_ms=goal_ns/1e6,elapsed_ms=p['wall_ns']/1e6,
        throughput=n*1e9/p['wall_ns'],budget_gap_ms=(p['wall_ns']-goal_ns)/1e6,
        mmq_ops=mmq_ops,attention_ops=attention_ops,decay_ops=decay_ops,
        observed_mmq_tops=mmq_ops/measured['PQ2 batched matrix']/1000,
        observed_attention_tops=attention_ops/measured['Attention stage, tiles and merge']/1000,
        two_subsystem_floor_ms=budgets,slack_fraction=fraction,planning_budget=plan))
print(json.dumps(dict(assumptions=gpu,observed_frequency_dense_peaks_tops=peak,requests=out),indent=2))
