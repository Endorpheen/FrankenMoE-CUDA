#include <cstdint>
#include <cstdio>
#include <algorithm>
extern "C" int64_t fg252s_discard(int64_t, int64_t, int64_t);
static int64_t ref(int64_t n_keep, int64_t n_left, int64_t param) {
    (void) n_keep;
    int64_t d = param ? param : n_left / 2;
    return std::clamp<int64_t>(d, 0, std::max<int64_t>(0, n_left - 1));
}
int main() {
    int cases = 0, failures = 0;
    for (int64_t keep : {0, 1, 64, 4096})
        for (int64_t left = 0; left <= 4096; left += 7)
            for (int64_t p : {0, 1, 7, 1024, 100000}) {
                ++cases;
                if (fg252s_discard(keep, left, p) != ref(keep, left, p)) {
                    if (failures < 3) printf("DIFF keep=%lld left=%lld p=%lld goose=%lld ref=%lld\n",
                        (long long)keep,(long long)left,(long long)p,
                        (long long)fg252s_discard(keep,left,p),(long long)ref(keep,left,p));
                    ++failures;
                }
            }
    printf("FG252S_GATE cases=%d failures=%d\n", cases, failures);
    return failures ? 1 : 0;
}
