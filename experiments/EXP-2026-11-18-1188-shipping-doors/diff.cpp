// Differential: Goose sm_apply_typical vs the native llama_sampler_typical_apply
// algorithm transcribed with std::sort, on random logits.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <random>
#include <vector>
extern "C" int64_t gate_dispatch_c(int64_t, const float*, float*, int64_t*, int64_t, float, int64_t, float, float);
extern "C" int64_t gate_dry_c(const int64_t*, const int64_t*, const int64_t*, const int64_t*, const int64_t*, int64_t, int64_t);
static int64_t native_typical(const std::vector<float>& logits_in, float tp, int64_t mk,
                              std::vector<int64_t>& ids_out, std::vector<float>* probs_out) {
    std::vector<float> lv = logits_in;
    std::vector<int64_t> id(lv.size());
    std::iota(id.begin(), id.end(), 0);
    // sort by logit desc (softmax_impl(..., true) precondition)
    std::sort(id.begin(), id.end(), [&](int64_t a, int64_t b){ return lv[a] > lv[b]; });
    std::vector<float> sl(lv.size()), p(lv.size());
    for (size_t i = 0; i < id.size(); ++i) sl[i] = lv[id[i]];
    float mx = sl[0], cum = 0;
    for (size_t i = 0; i < sl.size(); ++i) { p[i] = expf(sl[i]-mx); cum += p[i]; }
    for (size_t i = 0; i < p.size(); ++i) p[i] /= cum;
    float H = 0; for (float v : p) H -= v * logf(v);
    std::vector<float> shifted(p.size());
    for (size_t i = 0; i < p.size(); ++i) shifted[i] = fabsf(-logf(p[i]) - H);
    std::vector<size_t> idx(p.size()); std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b){ return shifted[a] < shifted[b]; });
    float c2 = 0; size_t last = idx.size();
    for (size_t i = 0; i < idx.size(); ++i) {
        c2 += p[idx[i]];
        if (c2 > tp && (mk == 0 || (int64_t)i >= mk - 1)) { last = i + 1; break; }
    }
    ids_out.clear(); if (probs_out) probs_out->clear();
    for (size_t i = 0; i < last; ++i) { ids_out.push_back(id[idx[i]]); if (probs_out) probs_out->push_back(p[idx[i]]); }
    return (int64_t) last;
}
int main() {
    std::mt19937 rng(12345); std::normal_distribution<float> nd(0.f, 3.f);
    const int N = 64; int bad = 0, cases = 0;
    const float tps[] = {0.5f, 0.9f, 0.99f};
    const int64_t mks[] = {0, 1, 5};
    for (int it = 0; it < 200; ++it) {
        std::vector<float> logits(N); for (auto& v : logits) v = nd(rng);
        for (float tp : tps) for (int64_t mk : mks) {
            std::vector<int64_t> nid; std::vector<float> np;
            int64_t nn = native_typical(logits, tp, mk, nid, &np);
            std::vector<int64_t> gid(N); std::iota(gid.begin(), gid.end(), 0);
            std::vector<float> glv = logits, gp(N, 0.f);
            int64_t gn = gate_dispatch_c(0, glv.data(), gp.data(), gid.data(), N, tp, mk, 0.f, 0.f);
            cases++;
            bool eq = (gn == nn);
            if (eq) for (int64_t i = 0; i < nn; ++i) if (gid[i] != nid[i]) { eq = false; break; }
            if (!eq) { bad++; if (bad <= 3) printf("MISMATCH tp=%g mk=%lld goose=%lld native=%lld first_ids %lld/%lld\n",
                (double)tp,(long long)mk,(long long)gn,(long long)nn,(long long)gid[0],(long long)nid[0]); }
        }
    }
    printf("typical-p: cases=%d mismatches=%d\n", cases, bad);
    int bad2 = 0, cases2 = 0;
    for (int it = 0; it < 100; ++it) {
        std::vector<float> logits(N); for (auto& v : logits) v = nd(rng);
        for (float tp : tps) for (int64_t mk : mks) {
            std::vector<int64_t> id(N); std::iota(id.begin(), id.end(), 0);
            std::vector<float> sl = logits;
            std::sort(id.begin(), id.end(), [&](int64_t a, int64_t b){ return sl[a] > sl[b]; });
            std::vector<float> sv(N), p(N); for (int i=0;i<N;++i) sv[i]=sl[id[i]];
            float mx=sv[0], c=0; for (int i=0;i<N;++i){p[i]=expf(sv[i]-mx);c+=p[i];}
            for (int i=0;i<N;++i) p[i]/=c;
            size_t last=N; float cum=0;
            for (size_t i=0;i<(size_t)N;++i){ cum+=p[i]; if (cum>=tp && (int64_t)(i+1)>=mk){last=i+1;break;} }
            std::vector<int64_t> gid(N); std::iota(gid.begin(), gid.end(), 0);
            std::vector<float> glv = logits, gp(N,0.f);
            int64_t gn = gate_dispatch_c(1, glv.data(), gp.data(), gid.data(), N, tp, mk, 0.f, 0.f);
            cases2++;
            bool eq = (gn == (int64_t)last);
            if (eq) for (size_t i=0;i<last;++i) if (gid[i]!=id[i]) { eq=false; break; }
            if (!eq) { bad2++; if (bad2<=3) printf("TOP-P MISMATCH tp=%g mk=%lld goose=%lld native=%zu\n",(double)tp,(long long)mk,(long long)gn,last); }
        }
    }
    printf("top-p unsorted: cases=%d mismatches=%d\n", cases2, bad2);
    int bad3 = 0, cases3 = 0;
    const int N2 = 2048;
    for (int it = 0; it < 5; ++it) {
        std::vector<float> logits(N2); for (auto& v : logits) v = nd(rng);
        for (float tp : tps) for (int64_t mk : mks) {
            std::vector<int64_t> id(N2); std::iota(id.begin(), id.end(), 0);
            std::vector<float> sl = logits;
            std::sort(id.begin(), id.end(), [&](int64_t a, int64_t b){ return sl[a] > sl[b]; });
            std::vector<float> sv(N2), p(N2); for (int i=0;i<N2;++i) sv[i]=sl[id[i]];
            float mx=sv[0], c=0; for (int i=0;i<N2;++i){p[i]=expf(sv[i]-mx);c+=p[i];}
            for (int i=0;i<N2;++i) p[i]/=c;
            size_t last=N2; float cum=0;
            for (size_t i=0;i<(size_t)N2;++i){ cum+=p[i]; if (cum>=tp && (int64_t)(i+1)>=mk){last=i+1;break;} }
            std::vector<int64_t> gid(N2); std::iota(gid.begin(), gid.end(), 0);
            std::vector<float> glv = logits, gp(N2,0.f);
            int64_t gn = gate_dispatch_c(1, glv.data(), gp.data(), gid.data(), N2, tp, mk, 0.f, 0.f);
            cases3++;
            bool eq = (gn == (int64_t)last);
            if (eq) for (size_t i=0;i<last;++i) if (gid[i]!=id[i]) { eq=false; break; }
            if (!eq) { bad3++; if (bad3<=3) printf("TOP-P-LARGE MISMATCH tp=%g mk=%lld goose=%lld native=%zu\n",(double)tp,(long long)mk,(long long)gn,last); }
        }
    }
    printf("top-p large: cases=%d mismatches=%d\n", cases3, bad3);
    int bad4 = 0, cases4 = 0;
    const float thr[] = {0.1f, 0.3f};
    const int64_t xtcmk[] = {1, 3};
    const float chances[] = {0.0f, 0.4f, 0.9f};
    for (int it = 0; it < 60; ++it) {
        std::vector<float> logits(N); for (auto& v : logits) v = nd(rng);
        std::vector<int64_t> id(N); std::iota(id.begin(), id.end(), 0);
        std::sort(id.begin(), id.end(), [&](int64_t a, int64_t b){ return logits[a] > logits[b]; });
        std::vector<float> sv(N), p(N); for (int i=0;i<N;++i) sv[i]=logits[id[i]];
        float mx=sv[0], c=0; for (int i=0;i<N;++i){p[i]=expf(sv[i]-mx);c+=p[i];}
        for (int i=0;i<N;++i) p[i]/=c;
        for (float th : thr) for (int64_t mk : xtcmk) for (float ch : chances) {
            int64_t pos_last = 0;
            for (int i=0;i<N;++i){ if (p[i] >= th) pos_last = i; else break; }
            int64_t cut = N;
            bool apply = (0.5f > 0 && th <= 0.5f && N >= 2 && ch <= 0.5f);
            if (apply && N - pos_last >= mk && pos_last > 0) cut = N - pos_last;
            std::vector<int64_t> gid = id;                    // token ids in sorted order
            std::vector<float> glv(N), gp(N,0.f);
            for (int i=0;i<N;++i) glv[i] = sv[i];             // sorted-desc logits
            int64_t gn = gate_dispatch_c(2, glv.data(), gp.data(), gid.data(), N, 0.5f, mk, th, ch);
            cases4++;
            bool eq = (gn == cut);
            if (eq && cut < N) { for (int64_t i=0;i<cut;++i) if (gid[i] != id[pos_last + i]) { eq=false; break; } }
            if (!eq) { bad4++; if (bad4<=3) printf("XTC MISMATCH th=%g mk=%lld ch=%g goose=%lld native=%lld\n",(double)th,(long long)mk,(double)ch,(long long)gn,(long long)cut); }
        }
    }
    printf("xtc: cases=%d mismatches=%d\n", cases4, bad4);
    // dry step 1: rep_limit from the nearest restart sequence
    int bad5 = 0, cases5 = 0;
    const int NN = 32, V = 8;
    for (int it = 0; it < 200; ++it) {
        std::vector<int64_t> last(NN); for (auto& v : last) v = (int64_t)(rng() % V);
        int nb = 1 + (int)(rng() % 5);
        std::vector<int64_t> bh(nb), bts(nb), btl(nb), btail(nb*4, 0);
        int off = 0;
        for (int b = 0; b < nb; ++b) { bh[b] = (int64_t)(rng() % V); btl[b] = (int64_t)(rng() % 3);
            bts[b] = off; for (int j = 0; j < btl[b]; ++j) btail[off++] = (int64_t)(rng() % V); }
        // The real caller (llama_sampler_dry_apply) builds last[i] = last_tokens.rat(i), i.e. the array
        // is newest-first. The harness used to mirror it here, which made it agree with a port that
        // mirrored too - and hid the convention error found by EXP-2026-11-19-1281.
        auto rat = [&](int k){ return last[k]; };
        int64_t rep_limit = NN;
        for (int i = 0; i < NN; ++i) {
            int64_t tok = rat(i); int64_t longest = -1;
            for (int b = 0; b < nb; ++b) if (bh[b] == tok) {
                int64_t sl = btl[b];
                if (sl > longest && sl <= i) { bool ok = true;
                    for (int o = 0; o < sl; ++o) if (btail[bts[b]+o] != rat(i-o-1)) { ok = false; break; }
                    if (ok) longest = sl; } }
            if (longest >= 0) { rep_limit = i - longest; break; }
        }
        int64_t gn = gate_dry_c(last.data(), bh.data(), bts.data(), btl.data(), btail.data(), NN, nb);
        cases5++;
        if (gn != rep_limit) { bad5++; if (bad5<=3) printf("DRY MISMATCH goose=%lld native=%lld\n",(long long)gn,(long long)rep_limit); }
    }
    printf("dry step1: cases=%d mismatches=%d\n", cases5, bad5);
    return (bad||bad2||bad3||bad4||bad5) ? 1 : 0;
}
