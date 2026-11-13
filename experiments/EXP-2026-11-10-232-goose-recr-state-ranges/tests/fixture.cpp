// Differential gate for the state_write cell-range grouping.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg232r_plan_ranges(int64_t,int32_t*,const int32_t*,const int32_t*,int32_t*,int32_t*,int32_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<15)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

static void run_case(const std::vector<int32_t>& match_, const std::vector<int32_t>& rs,
                     const std::vector<int32_t>& src, int64_t size, const char* tag) {
    // transcription of the native loop
    std::vector<std::pair<uint32_t,uint32_t>> ranges, data;
    uint32_t cell_count = 0, begin = (uint32_t) size;
    for (uint32_t i = 0; i < (uint32_t) size; ++i) {
        if (match_[i]) {
            ++cell_count;
            const uint32_t row = src[i] >= 0 ? (uint32_t) src[i] : i;
            const uint32_t cid = (uint32_t) rs[i] * (uint32_t) size + row;
            if (data.empty() || data.back().second != cid) data.emplace_back(cid, cid+1);
            else data.back().second++;
            if (begin == (uint32_t) size) begin = i;
        } else if (begin != (uint32_t) size) {
            ranges.emplace_back(begin, i); begin = (uint32_t) size;
        }
    }
    if (begin != (uint32_t) size) ranges.emplace_back(begin, (uint32_t) size);

    std::vector<int32_t> o_r(2*size+2, -7), o_d(2*size+2, -7), counts(3, -7);
    std::vector<int32_t> m = match_, s = src, r = rs;
    if (m.empty()) { m.push_back(0); s.push_back(0); r.push_back(0); }
    const int64_t got = fg232r_plan_ranges(size, m.data(), r.data(), s.data(),
                                           o_r.data(), o_d.data(), counts.data());
    ck(got == (int64_t) cell_count, "count", got, (int64_t) cell_count, 0);
    ck(counts[0] == (int) ranges.size(), "n_ranges", counts[0], (int) ranges.size(), 0);
    ck(counts[1] == (int) data.size(),   "n_data",   counts[1], (int) data.size(), 0);
    for (size_t k = 0; k < ranges.size(); ++k) {
        ck(o_r[2*k] == (int32_t) ranges[k].first,  tag, o_r[2*k],   (int32_t) ranges[k].first,  (int)(2*k));
        ck(o_r[2*k+1] == (int32_t) ranges[k].second, tag, o_r[2*k+1], (int32_t) ranges[k].second, (int)(2*k+1));
    }
    for (size_t k = 0; k < data.size(); ++k) {
        ck(o_d[2*k] == (int32_t) data[k].first,  tag, o_d[2*k],   (int32_t) data[k].first,  (int)(2*k));
        ck(o_d[2*k+1] == (int32_t) data[k].second, tag, o_d[2*k+1], (int32_t) data[k].second, (int)(2*k+1));
    }
    ck(counts[2] == -7, "no overrun", counts[2], -7, 0);
}

int main(void) {
    // empty, all free, all used contiguous, split by a hole, and the case that separates index
    // ranges from data ranges: two adjacent used cells whose src jumps.
    run_case({}, {}, {}, 0, "empty");
    run_case({0,0,0,0}, {0,0,0,0}, {-1,-1,-1,-1}, 4, "all-free");
    run_case({1,1,1,1}, {0,0,0,0}, {-1,-1,-1,-1}, 4, "contiguous");
    run_case({1,0,1,0}, {0,0,0,0}, {-1,-1,-1,-1}, 4, "holes");
    run_case({1,1,0,1}, {0,0,0,0}, {-1,-1,-1,-1}, 4, "tail-and-hole");
    // adjacent used cells, src jumps: ONE index range but TWO data ranges
    run_case({1,1,1}, {0,0,0}, {-1, 10, 11}, 3, "src-jump");
    run_case({1,1,1}, {0,0,0}, {5, 6, 7}, 3, "src-consecutive");
    run_case({1,1,1}, {0,0,0}, {5, 5, 5}, 3, "src-duplicate");
    run_case({1,1}, {1,1}, {-1,-1}, 2, "rs-offset");        // rs shifts cell_id by rs*size

    std::mt19937 rng(232011);
    for (int t = 0; t < 20000; ++t) {
        const int size = (int)(rng()%9);
        std::vector<int32_t> m(size), rs(size), s(size);
        for (int i = 0; i < size; ++i) {
            m[i] = (int32_t)(rng()%2);
            rs[i] = (int32_t)(rng()%3);
            s[i] = (rng()%3==0) ? -1 : (int32_t)(rng()%6);
        }
        run_case(m, rs, s, size, "rand");
    }
    printf("FG232R_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP232_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP232_LOCAL_GATES_PASS\n"); return 0;
}
