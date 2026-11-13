#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg367s_clear(int64_t n_idx, const int32_t * idxs, int64_t head, int64_t cells_size,
                                int64_t n_layer, const int32_t * k_row, const int32_t * v_present,
                                const int32_t * v_row, int64_t v_trans, int64_t v_el, int64_t contig,
                                int32_t * out_kind, int32_t * out_il, int64_t * out_off, int64_t * out_size,
                                int64_t cap);
extern "C" int fg367s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(367011);
    check(fg367s_enabled() == 1, "enabled by default", 0);
    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_idx = 1 + (int64_t)(rng() % 4);
        const int64_t nl = (int64_t)(rng() % 4);
        const int64_t head = (int64_t)(rng() % 6);
        const int64_t cells_size = 4 + (int64_t)(rng() % 5);
        const int64_t v_trans = rng() & 1;
        const int64_t v_el = 1 + (int64_t)(rng() % 4);
        std::vector<int32_t> idxs((size_t)n_idx), kr((size_t)(nl?nl:1)), vp(kr.size()), vr(kr.size());
        int32_t cur = (int32_t)(rng() % 3);
        for (int64_t i = 0; i < n_idx; ++i) { idxs[(size_t)i] = cur; cur += (rng() % 4 == 0) ? 2 : 1; }
        for (int64_t il = 0; il < nl; ++il) {
            kr[(size_t)il] = (int32_t)(1 + (int64_t)(rng() % 32));
            vr[(size_t)il] = (int32_t)(1 + (int64_t)(rng() % 16));
            vp[(size_t)il] = (rng() % 4) ? 1 : 0;
        }
        bool contig = true;
        for (int64_t i = 1; i < n_idx; ++i) if (idxs[(size_t)i] != idxs[(size_t)(i-1)] + 1) contig = false;

        std::vector<int32_t> wk, wl; std::vector<int64_t> wo, ws;
        auto push=[&](int kind,int il,int64_t off,int64_t sz){wk.push_back(kind);wl.push_back(il);wo.push_back(off);ws.push_back(sz);};
        for (int64_t il = 0; il < nl; ++il) {
            if (contig) push(0,(int)il, head*kr[(size_t)il], n_idx*kr[(size_t)il]);
            else for (int64_t i = 0; i < n_idx; ++i) push(0,(int)il, idxs[(size_t)i]*kr[(size_t)il], kr[(size_t)il]);
        }
        for (int64_t il = 0; il < nl; ++il) {
            if (!vp[(size_t)il]) continue;
            if (!v_trans) {
                if (contig) push(1,(int)il, head*vr[(size_t)il], n_idx*vr[(size_t)il]);
                else for (int64_t i = 0; i < n_idx; ++i) push(1,(int)il, idxs[(size_t)i]*vr[(size_t)il], vr[(size_t)il]);
            } else if (contig) {
                for (int64_t j = 0; j < vr[(size_t)il]; ++j) push(1,(int)il, (head + j*cells_size)*v_el, n_idx*v_el);
            } else {
                for (int64_t j = 0; j < vr[(size_t)il]; ++j)
                    for (int64_t i = 0; i < n_idx; ++i)
                        push(1,(int)il, (idxs[(size_t)i] + j*cells_size)*v_el, v_el);
            }
        }

        const int64_t cap = 256;
        std::vector<int32_t> gk(cap, -1), gl(cap, -1);
        std::vector<int64_t> go(cap, -1), gs(cap, -1);
        const int64_t got = fg367s_clear(n_idx, idxs.data(), head, cells_size, nl, kr.data(), vp.data(),
                                         vr.data(), v_trans, v_el, contig ? 1 : 0, gk.data(), gl.data(),
                                         go.data(), gs.data(), cap);
        check(got == (int64_t)wk.size(), "count", trial);
        for (size_t i = 0; i < wk.size() && i < (size_t)got; ++i) {
            check(gk[i] == wk[i] && gl[i] == wl[i] && go[i] == wo[i] && gs[i] == ws[i], "clear", trial);
        }
    }
    std::printf("FG367S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
