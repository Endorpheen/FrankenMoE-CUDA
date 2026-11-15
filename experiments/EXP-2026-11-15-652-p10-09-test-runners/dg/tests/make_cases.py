#!/usr/bin/env python3
# Case builder for the benchmark-diagnostics gate (P10-09 S3).
#
# Writes synthetic CSV + .metrics pairs under build/cases/. Every case is a
# shape the native analyzer has to handle: an extra trailing column, a missing
# stall column, a zero wall, a bad wall, a bad optional field, one row, an even
# count, no summary, a "-1.0" cache hit, CRLF, and an empty file.
from pathlib import Path

S = Path(__file__).resolve().parent.parent
CASEDIR = S / 'build' / 'cases'

HEAD_FULL = "step,seq,wall_ms,span\n"
HEAD_STALL = "step,seq,wall_ms,stall_ms,mgmt_ms\n"
HEAD_PLAIN = "step,seq,wall_ms\n"


def rows(walls, extra=None):
    out = []
    for i, w in enumerate(walls):
        line = "%d,%d,%s" % (i, i, w)
        if extra is not None:
            line += "," + extra[i]
        out.append(line)
    return "\n".join(out) + "\n"


def summary(**kw):
    order = ["tokens", "tok/s", "prefill_tps", "load_s", "prefill_s", "read_MiB",
             "cache_hit_pct", "cache_budget_MiB", "cache_resident_MiB",
             "stall_s/tok", "spec_read_MiB", "spec_useful", "spec_experts"]
    parts = []
    for k in order:
        if k in kw:
            parts.append("%s=%s" % (k, kw[k]))
    return "# summary " + " ".join(parts) + "\n"


def metrics(**kw):
    order = ["peak_rss_kb", "mem_avail_floor_kb", "cpu_temp_before_mC",
             "cpu_temp_max_mC", "batt_temp_max_dC"]
    return "\n".join("%s=%s" % (k, kw[k]) for k in order if k in kw) + "\n"


BASE_SUM = dict(tokens=256, **{"tok/s": 12.34}, prefill_tps=210.5, load_s=1.25,
                prefill_s=0.75, read_MiB=512.0, cache_hit_pct=42.5,
                **{"stall_s/tok": 0.012})

CASES = {
    'full': dict(csv=HEAD_STALL + rows(["41.2", "42.7", "40.1"],
                                       ["1.2", "1.4", "1.1"]), summary=BASE_SUM,
                 metrics=metrics(peak_rss_kb=7340032, mem_avail_floor_kb=1048576,
                                 cpu_temp_before_mC=41000, cpu_temp_max_mC=48500,
                                 batt_temp_max_dC=372),
                 label="streaming O_DIRECT, cache 0, lane 4"),
    'no_stall_column': dict(csv=HEAD_PLAIN + rows(["33.3", "34.1"]),
                            summary=BASE_SUM, label="solo mmap (no streaming)"),
    'hit_absent': dict(csv=HEAD_PLAIN + rows(["25.0"]),
                       summary=dict(tokens=256, prefill_tps=100.0, load_s=0.5,
                                    prefill_s=0.25, read_MiB=128.0),
                       label="fixed cache + overlap (reference)"),
    'hit_minus_one': dict(csv=HEAD_PLAIN + rows(["19.5", "20.5", "18.0"]),
                          summary=dict(BASE_SUM, **{"cache_hit_pct": "-1.0"}),
                          label="auto-sized cache (--cache-mb auto) + overlap"),
    'zero_wall': dict(csv=HEAD_PLAIN + rows(["0", "40.0", "60.0"]),
                      summary=BASE_SUM, label="streaming + cache 2000 MiB, lane 4"),
    'bad_wall_row': dict(csv=HEAD_STALL + rows(["30.0", "x", "45.0"],
                                               ["9.9", "9.9", "2.0"]),
                         summary=BASE_SUM, label="streaming + cache 4000 MiB, lane 2"),
    'bad_stall_field': dict(csv=HEAD_STALL + rows(["30.0", "31.0"], ["1.0", "n/a"]),
                            summary=BASE_SUM, label="streaming + overlap"),
    'single_row': dict(csv=HEAD_PLAIN + rows(["7.5"]),
                       summary=BASE_SUM, label="one token"),
    'two_rows': dict(csv=HEAD_PLAIN + rows(["10.0", "20.0"]),
                     summary=BASE_SUM, label="two tokens"),
    'extra_columns': dict(csv=HEAD_STALL + rows(["12.5", "13.5", "14.5", "15.5"],
                                                ["0.4", "0.5", "0.6", "0.7"]),
                          summary=BASE_SUM, label="prefetch 2"),
    'no_summary': dict(csv=HEAD_PLAIN + rows(["8.0", "9.0"]),
                       label="no trailer"),
    'prefill_only': dict(csv=HEAD_PLAIN + rows(["50.0"]),
                         summary=dict(load_s=2.5, prefill_s=1.5, prefill_tps=333.0),
                         label="prefill only"),
    'spec': dict(csv=HEAD_PLAIN + rows(["22.0", "23.0"]),
                 summary=dict(BASE_SUM, **{"spec_read_MiB": 64.0,
                                           "spec_useful": 12, "spec_experts": 40,
                                           "cache_budget_MiB": 3000,
                                           "cache_resident_MiB": 2900}),
                 label="streaming + prefetch 2"),
    'budget': dict(csv=HEAD_PLAIN + rows(["17.0", "18.0", "19.0"]),
                   summary=dict(BASE_SUM, **{"cache_budget_MiB": 4096,
                                             "cache_resident_MiB": 3800}),
                   label="auto cap"),
    'crlf': dict(csv=(HEAD_PLAIN + rows(["11.0", "12.0"])).replace("\n", "\r\n"),
                 summary=summary(**BASE_SUM).replace("\n", "\r\n"), label="crlf"),
    'empty': dict(csv=HEAD_PLAIN, summary=BASE_SUM, label="no rows"),
}


def main():
    CASEDIR.mkdir(parents=True, exist_ok=True)
    for name, spec in CASES.items():
        summ = spec.get('summary', '')
        if isinstance(summ, dict):
            summ = summary(**summ)
        (CASEDIR / ('%s.csv' % name)).write_text(spec['csv'] + summ)
        (CASEDIR / ('%s.metrics' % name)).write_text(spec.get('metrics', ''))
    print('cases:', len(CASES))


if __name__ == '__main__':
    main()
