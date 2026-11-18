#include <cstdio>
#include <cstdint>
extern "C" int64_t g_prefix_call(const int64_t*, const int64_t*, int64_t, int64_t*, int64_t, int64_t);
struct C { const char * name; int64_t d[4]; int64_t t[4]; int64_t k; int64_t want; int64_t want_pos; };
int main() {
    const C cases[] = {
      {"all agree",        {5,6,7,0}, {5,6,7,0}, 3, 3, 3},
      {"first differs",    {5,6,7,0}, {9,6,7,0}, 3, 0, 0},
      {"second differs",   {5,6,7,0}, {5,9,7,0}, 3, 1, 1},
      {"last differs",     {5,6,7,0}, {5,6,9,0}, 3, 2, 2},
      {"k=0",              {5,6,7,0}, {5,6,7,0}, 0, 0, 0},
      {"k=1",              {5,6,7,0}, {5,9,7,0}, 1, 1, 1},
      {"partial window",   {5,6,9,0}, {5,6,7,0}, 2, 2, 2},
    };
    int fails=0, n=(int)(sizeof(cases)/sizeof(cases[0]));
    for (int i=0;i<n;++i) {
        const C & c = cases[i];
        int64_t out[2]={-1,-1};
        int64_t r = g_prefix_call(c.d, c.t, c.k, out, 4, 4);
        if (r != c.want || out[0] != c.want_pos) { fails++; printf("MISMATCH %s got=%lld pos=%lld want=%lld/%lld\n",
            c.name,(long long)r,(long long)out[0],(long long)c.want,(long long)c.want_pos); }
    }
    printf("spec prefix: cases=%d fails=%d\n", n, fails);
    return fails?1:0;
}
