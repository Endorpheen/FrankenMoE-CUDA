// Differential fixture for the Goose stream session policy (EXP453).
//
// Right arm: a transcription of stream_session::append and the read_from
// if-chain using std::vector, exactly as server-stream.cpp does it.
// Left arm: the Goose module applied to the same chunk sequence.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <random>
#include <string>

extern "C" {
int64_t fg453_session_append(int64_t done, int64_t buf_len, int64_t len, int64_t cap, uint64_t out);
int64_t fg453_session_read(int64_t offset, int64_t dropped, int64_t buf_len, int64_t done, int64_t stop, uint64_t out);
void    fg453_st_i64(uint64_t p, uint64_t o, int64_t v);
}

void fg453_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

// ---- native transcription --------------------------------------------------
struct native_session {
    std::vector<char> buffer;
    int64_t prefix_dropped = 0;
    int64_t cap_bytes;
    bool done = false;

    explicit native_session(int64_t cap) : cap_bytes(cap) {}

    // stream_session::append
    bool append(const char * data, size_t len) {
        if (len == 0) return true;
        if (done) return false;
        if ((int64_t) len >= cap_bytes) {
            size_t skip = len - (size_t) cap_bytes;
            prefix_dropped += (int64_t) buffer.size() + (int64_t) skip;
            buffer.clear();
            buffer.insert(buffer.end(), data + skip, data + len);
        } else {
            size_t needed = buffer.size() + len;
            if ((int64_t) needed > cap_bytes) {
                size_t to_drop = needed - (size_t) cap_bytes;
                buffer.erase(buffer.begin(), buffer.begin() + (long) to_drop);
                prefix_dropped += (int64_t) to_drop;
            }
            buffer.insert(buffer.end(), data, data + len);
        }
        return true;
    }

    int64_t total() const { return prefix_dropped + (int64_t) buffer.size(); }
};

enum { S_WAIT = 0, S_CHUNK = 1, S_LOST = 2, S_END = 3, S_STOP = 4 };

// stream_session::read_from decision, one evaluation of the loop body
static void native_read(int64_t offset, int64_t dropped, int64_t buf_len, bool done, bool stop,
                        int * status, int64_t * n) {
    *n = 0;
    if (stop) { *status = S_STOP; return; }
    if (offset < dropped) { *status = S_LOST; return; }
    int64_t logical_end = dropped + buf_len;
    if (offset < logical_end) { *status = S_CHUNK; *n = logical_end - offset; return; }
    if (done) { *status = S_END; return; }
    *status = S_WAIT;
}

// ---- goose arm -------------------------------------------------------------
struct goose_session {
    std::vector<char> buffer;
    int64_t dropped = 0;
    int64_t cap_bytes;
    bool done = false;

    explicit goose_session(int64_t cap) : cap_bytes(cap) {}

    bool append(const char * data, size_t len) {
        int64_t out[4] = {-1, -1, -1, -1};
        fg453_session_append(done ? 1 : 0, (int64_t) buffer.size(), (int64_t) len, cap_bytes,
                     (uint64_t) (uintptr_t) out);
        int64_t accept = out[0], nbuf = out[1], delta = out[2], skip = out[3];
        if (!accept) return false;
        dropped += delta;
        if (skip > 0) buffer.clear();
        buffer.insert(buffer.end(), data + skip, data + len);
        if ((int64_t) buffer.size() > nbuf) {
            buffer.erase(buffer.begin(), buffer.begin() + (long) ((int64_t) buffer.size() - nbuf));
        }
        return true;
    }

    int64_t total() const { return dropped + (int64_t) buffer.size(); }
};

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const char * detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail);
}

int main(void) {
    std::mt19937 rng(453001);
    const int64_t caps[] = {1, 7, 64, 1000, 4096, 4096 * 1024};
    const char * chunk_sizes = "zero,one,cap-1,cap,cap+1,2cap,random";

    for (size_t ci = 0; ci < sizeof(caps) / sizeof(caps[0]); ++ci) {
        const int64_t cap = caps[ci];
        native_session ns(cap);
        goose_session gs(cap);
        for (int step = 0; step < 200; ++step) {
            int64_t len;
            switch (step % 7) {
                case 0: len = 0; break;
                case 1: len = 1; break;
                case 2: len = cap > 1 ? cap - 1 : 1; break;
                case 3: len = cap; break;
                case 4: len = cap + 1; break;
                case 5: len = 2 * cap; break;
                default: len = (int64_t) (rng() % (uint64_t) (cap * 2 + 3)); break;
            }
            std::vector<char> chunk((size_t) len);
            for (auto & c : chunk) c = (char) (rng() & 0xff);

            // finalize both arms at the same step so `done` semantics are compared
            if (step == 150) { ns.done = true; gs.done = true; }
            bool na = ns.append(chunk.data(), chunk.size());
            bool ga = gs.append(chunk.data(), chunk.size());

            ++cases;
            ++checks;
            if (na != ga) { fail("accept", chunk_sizes); continue; }
            ++checks;
            if (ns.buffer != gs.buffer) {
                char d[160];
                snprintf(d, sizeof d, "cap=%lld step=%d native_len=%zu goose_len=%zu",
                         (long long) cap, step, ns.buffer.size(), gs.buffer.size());
                fail("buffer", d);
            }
            ++checks;
            if (ns.prefix_dropped != gs.dropped) {
                char d[160];
                snprintf(d, sizeof d, "cap=%lld step=%d native=%lld goose=%lld",
                         (long long) cap, step, (long long) ns.prefix_dropped, (long long) gs.dropped);
                fail("dropped", d);
            }
            ++checks;
            if (ns.total() != gs.total()) fail("total", chunk_sizes);
        }

        // read decisions on the final state, before and after finalize
        for (int phase = 0; phase < 2; ++phase) {
            bool done = phase == 1;
            int64_t dropped = ns.prefix_dropped;
            int64_t blen = (int64_t) ns.buffer.size();
            int64_t total = ns.total();
            const int64_t offs[] = {-1, 0, 1, dropped - 1, dropped, dropped + 1,
                                    dropped + blen - 1, dropped + blen, dropped + blen + 1, total, total + 100};
            for (size_t oi = 0; oi < sizeof(offs) / sizeof(offs[0]); ++oi)
            for (int stop = 0; stop < 2; ++stop) {
                int nstatus = 0; int64_t nn = 0;
                native_read(offs[oi], dropped, blen, done, stop != 0, &nstatus, &nn);
                int64_t out[2] = {-1, -1};
                fg453_session_read(offs[oi], dropped, blen, done ? 1 : 0, stop, (uint64_t) (uintptr_t) out);
                ++cases; ++checks;
                if (out[0] != nstatus || out[1] != nn) {
                    char d[200];
                    snprintf(d, sizeof d, "cap=%lld off=%lld done=%d stop=%d native=%d/%lld goose=%lld/%lld",
                             (long long) cap, (long long) offs[oi], done, stop, nstatus, (long long) nn,
                             (long long) out[0], (long long) out[1]);
                    fail("read", d);
                }
            }
        }
    }

    printf("FG453_STREAM caps=6 cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
