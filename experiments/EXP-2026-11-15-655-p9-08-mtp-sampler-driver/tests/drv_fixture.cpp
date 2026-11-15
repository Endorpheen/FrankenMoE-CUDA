// Differential gate for P9-08 G7: the CLI driver loop's argument projection.
//
// The oracle is the carrier's text. tests/native_extract.py copies out of
// cli/main.cpp:
//
//   native_parse_loop.inc   606-776   the whole argument loop
//   native_workload_gate.inc 876-883  the workload/mode rejection
//   native_mode.inc         880, 885  the session and perplexity conditions
//   native_list_filter.inc  893-895   the --ppl-list line filter
//
// The loop is compiled into a function whose cfg, locals and seen set are this
// fixture's, and it runs in a forked child because two of its paths end the
// process (`std::exit(1)` for a missing value). The child writes a snapshot of
// the resulting configuration into shared memory, including the exit it took;
// the parent compares that snapshot with the same configuration built by the
// Goose module from drv_flag_id / drv_span / drv_field / drv_kind.
//
// Both arms format their state through ONE snapshot function, so a difference
// is a difference in the parse, never in the printing.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bmoe/config.h"
#include "bmoe/recipe.h"
#include "bmoe/version.h"
#include "drv.h" // drv_*, goose_init

using namespace bmoe; // main.cpp:40 — the carrier's own using-directive

// ── stubs the extracted text calls ─────────────────────────────────────────
namespace {

struct IMetricsSink {
    int tag;
};
struct IRouteTraceSink {
    int tag;
};
struct IComputeTraceSink {
    int tag;
};
struct IIoTraceSink {
    int tag;
};

int g_usage_calls = 0;

} // namespace

void print_usage(const char * argv0) {
    (void) argv0;
    ++g_usage_calls;
}

bool console_is_ours_alone() { return false; }

// The mode probe: returning 1 says "session mode was taken".
int run_session_loop(const RunConfig &, IMetricsSink *, IRouteTraceSink *, IComputeTraceSink *, IIoTraceSink *) {
    return 1;
}

namespace bmoe {
int n_moe_recipes() { return 0; }
const MoeRecipe * moe_recipe_at(int) { return nullptr; }
} // namespace bmoe

namespace {

// ── the parse state the carrier text reads and writes ──────────────────────
RunConfig cfg;
std::string csv_path;
std::string route_trace_path;
std::string ppl_path;
int ppl_skip = 8;
bool ppl_step = false;
std::string ppl_list;
std::vector<std::string> ppl_choices;
std::string compute_trace_path;
std::string io_trace_path;
bool session_mode = false;
std::set<std::string> seen;

// The extracted loop reads the process arguments by these names, as the
// carrier's main() does; native_parse() is the only reader.
int argc = 0;
char ** argv = nullptr;

// The sink holders main() builds after the parse; native_mode() passes them to
// run_session_loop. Null is the state of a run with no --*-trace flag.
std::unique_ptr<IMetricsSink> sink;
std::unique_ptr<IRouteTraceSink> route_trace;
std::unique_ptr<IComputeTraceSink> compute_trace;
std::unique_ptr<IIoTraceSink> io_trace;

// The carrier's loop, verbatim. -1 means "the loop finished"; any other value is
// the early exit the loop took (0 for an action, 1/2 for a refusal).
int native_parse() {
#include "native_parse_loop.inc"
    return -1;
}

// The native refusal paths print to stderr; the gate keeps its own log clean
// while still running them.
struct SilenceErr {
    int saved;
    SilenceErr() {
        std::fflush(stderr);
        saved = dup(2);
        const int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) dup2(dn, 2);
    }
    ~SilenceErr() {
        std::fflush(stderr);
        if (saved >= 0) {
            dup2(saved, 2);
            close(saved);
        }
    }
};

// The two --ppl open refusals, compiled as functions that return the code the
// carrier would exit with. The reader bodies are outside the extracted ranges;
// what is compared is the refusal code and the condition that reaches it.
int native_ppl_list_open(const std::string & ppl_list) {
    std::vector<std::string> paths;
#include "native_ppl_list_open.inc"
    return 0;
}

int native_ppl_file_open(const std::string & path) {
    std::vector<std::string> paths;
#include "native_ppl_file_open.inc"
    return 0;
}

int native_workload_gate() {
#include "native_workload_gate.inc"
    return 0;
}

int native_mode() {
#include "native_mode.inc"
        // The fixture closes the two blocks the extracted conditions open; the
        // decision being compared is which condition wins, not what they do.
        return 2;
    }
    return 0;
}

// The snapshot: one producer, two consumers, so the formatting cannot differ.
struct Local {
    std::string csv, route, compute, io, ppl, ppl_list, wcap, wrep;
    std::vector<std::string> choices;
    int ppl_skip = 8;
    int ppl_step = 0;
    int session = 0;
};

Local from_native() {
    Local L;
    L.csv = csv_path;
    L.route = route_trace_path;
    L.compute = compute_trace_path;
    L.io = io_trace_path;
    L.ppl = ppl_path;
    L.ppl_list = ppl_list;
    L.wcap = cfg.workload_capture_path;
    L.wrep = cfg.workload_replay_path;
    L.choices = ppl_choices;
    L.ppl_skip = ppl_skip;
    L.ppl_step = ppl_step ? 1 : 0;
    L.session = session_mode ? 1 : 0;
    return L;
}

int dense_id(DenseWeightsMode m) {
    switch (m) {
    case DenseWeightsMode::Mmap: return 0;
    case DenseWeightsMode::Warmed: return 1;
    case DenseWeightsMode::Anonymous: return 2;
    case DenseWeightsMode::Pinned: return 3;
    }
    return -1;
}

int source_id(DraftSource s) {
    switch (s) {
    case DraftSource::none: return 0;
    case DraftSource::mtp: return 1;
    case DraftSource::ngram: return 2;
    }
    return -1;
}

std::string snapshot(const RunConfig & c, const Local & L) {
    std::ostringstream o;
    o << "model=" << c.model_path << "\n";
    o << "prompt=" << c.prompt << "\n";
    o << "n_predict=" << c.n_predict << "\n";
    o << "n_threads=" << c.n_threads << "\n";
    o << "n_ctx=" << c.n_ctx << "\n";
    o << "n_ubatch=" << c.n_ubatch << "\n";
    o << "n_expert_used=" << c.n_expert_used << "\n";
    o << "n_gpu_layers=" << c.n_gpu_layers << "\n";
    o << "vram_cache_mb=" << c.vram_cache_mb << "\n";
    o << "expert_hot_slots=" << c.expert_hot_slots << "\n";
    o << "temp=" << std::to_string(c.sampling.temp) << "\n";
    o << "top_k=" << c.sampling.top_k << "\n";
    o << "top_p=" << std::to_string(c.sampling.top_p) << "\n";
    o << "seed=" << c.sampling.seed << "\n";
    o << "source=" << source_id(c.spec.source) << "\n";
    o << "draft_max=" << c.spec.draft_max << "\n";
    o << "draft_p_min=" << std::to_string(c.spec.draft_p_min) << "\n";
    o << "ngram_min_match=" << c.spec.ngram_min_match << "\n";
    o << "chatml=" << (c.chatml ? 1 : 0) << "\n";
    o << "think=" << (c.think ? 1 : 0) << "\n";
    o << "progress=" << (c.progress ? 1 : 0) << "\n";
    o << "session=" << L.session << "\n";
    o << "csv=" << L.csv << "\n";
    o << "route=" << L.route << "\n";
    o << "compute=" << L.compute << "\n";
    o << "compute_layers=" << (c.compute_trace_layers ? 1 : 0) << "\n";
    o << "io=" << L.io << "\n";
    o << "wcap=" << L.wcap << "\n";
    o << "wrep=" << L.wrep << "\n";
    o << "moe=" << (c.moe.enabled ? 1 : 0) << "\n";
    o << "cache_mb=" << c.moe.cache_mb << "\n";
    o << "cache_auto=" << (c.moe.cache_auto ? 1 : 0) << "\n";
    o << "cache_floor=" << c.moe.cache_floor_mb << "\n";
    o << "cache_ceil=" << c.moe.cache_ceil_mb << "\n";
    o << "io_threads=" << c.moe.io_threads << "\n";
    o << "o_direct=" << (c.moe.o_direct ? 1 : 0) << "\n";
    o << "row_stream=" << (c.moe.row_stream ? 1 : 0) << "\n";
    o << "row_stream_mb=" << c.moe.row_stream_mb << "\n";
    o << "dense=" << dense_id(c.moe.dense_weights) << "\n";
    o << "load_all=" << (c.moe.load_all ? 1 : 0) << "\n";
    o << "force_cache=" << (c.moe.force_cache ? 1 : 0) << "\n";
    o << "overlap=" << (c.moe.overlap ? 1 : 0) << "\n";
    o << "io_two_wave=" << (c.moe.io_two_wave ? 1 : 0) << "\n";
    o << "prefetch=" << c.moe.prefetch_layers << "\n";
    o << "prefetch_sync=" << (c.moe.prefetch_sync ? 1 : 0) << "\n";
    o << "drop_cold=" << std::to_string(c.moe.drop_cold_frac) << "\n";
    o << "substitute=" << std::to_string(c.moe.substitute_lambda) << "\n";
    o << "ppl=" << L.ppl << "\n";
    o << "ppl_skip=" << L.ppl_skip << "\n";
    o << "ppl_step=" << L.ppl_step << "\n";
    o << "ppl_list=" << L.ppl_list << "\n";
    o << "drop_renorm=" << (c.moe.drop_renorm ? 1 : 0) << "\n";
    o << "drop_prefill=" << (c.moe.drop_prefill ? 1 : 0) << "\n";
    o << "route_ahead=" << c.moe.route_ahead << "\n";
    o << "predict_log=" << (c.moe.predict_log ? 1 : 0) << "\n";
    o << "predict_prefetch=" << (c.moe.predict_prefetch ? 1 : 0) << "\n";
    o << "predict_spec_max=" << c.moe.predict_spec_max << "\n";
    o << "n_choices=" << (int) L.choices.size() << "\n";
    for (size_t k = 0; k < L.choices.size(); ++k) o << "choice" << k << "=" << L.choices[k] << "\n";
    return o.str();
}

// ── the child's result ─────────────────────────────────────────────────────
constexpr size_t BLOB = 16384;
struct Out {
    int32_t finished;   // 1: the loop ran to the end
    int32_t exit_code;  // what the run would exit with at the parse stage
    int32_t mode;       // native mode condition: 0 one-shot, 1 session, 2 ppl
    int32_t workload;   // native workload/mode rejection
    int32_t dbg_argc;
    int32_t dbg_rc;
    char blob[BLOB];
};

Out * g_out = nullptr;
bool g_filled = false;

void fill_out(int finished, int exit_code) {
    if (g_filled) return;
    g_filled = true;
    g_out->finished = finished;
    g_out->exit_code = exit_code;
    g_out->mode = native_mode();
    g_out->workload = native_workload_gate();
    const std::string s = snapshot(cfg, from_native());
    std::snprintf(g_out->blob, BLOB, "%s", s.c_str());
}

// A path that ends the process inside the loop (a missing value) still has to
// report the state it reached: std::exit runs this handler.
void on_exit() { fill_out(0, 1); }

struct NativeRun {
    int finished = 0;
    int exit_code = 0;
    int mode = 0;
    int workload = 0;
    std::string snap;
};

NativeRun run_native(const std::vector<std::string> & args) {
    NativeRun r;
    g_out = (Out *) mmap(nullptr, sizeof(Out), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_out == MAP_FAILED) {
        std::printf("  FAIL [G7] mmap for the child result\n");
        std::exit(2);
    }
    std::memset(g_out, 0, sizeof(Out));
    const pid_t pid = fork();
    if (pid == 0) {
        // Silence the carrier's own printing: this gate compares state, and the
        // usage text and version banner are the carrier's, not the module's.
        const int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) {
            dup2(dn, 1);
            dup2(dn, 2);
        }
        g_filled = false;
        atexit(on_exit);
        static std::vector<char *> av;
        av.clear();
        av.push_back((char *) "bmoe-cli");
        for (const std::string & s : args) av.push_back(const_cast<char *>(s.c_str()));
        argc = (int) av.size();
        argv = av.data();
        g_out->dbg_argc = argc;
        const int rc = native_parse();
        g_out->dbg_rc = rc;
        fill_out(rc == -1 ? 1 : 0, rc == -1 ? 0 : rc);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    r.finished = g_out->finished;
    r.exit_code = g_out->exit_code;
    r.mode = g_out->mode;
    r.workload = g_out->workload;
    r.snap = g_out->blob;

    munmap(g_out, sizeof(Out));
    return r;
}

// ── the module arm ─────────────────────────────────────────────────────────
struct Proj {
    RunConfig cfg;
    Local L;
};

// The comma count the csv split is sized from: drv_csv_count takes the count,
// the carrier counts the separators (std::string::find in a loop).
int64_t comma_count(const std::string & v) {
    int64_t n = 0;
    for (char c : v)
        if (c == ',') ++n;
    return n;
}

// The one native mapping the module deliberately does not own: the
// --dense-weights vocabulary is cc_dense_mode's (goose/cli/config-resolve.goose,
// P9-01 S3). This three-line copy is the wiring, not a second decision.
int dense_mode_of(const std::string & v) {
    if (v == "mmap") return 0;
    if (v == "warm") return 1;
    if (v == "anon") return 2;
    if (v == "ahwb") return 3;
    return -1;
}

struct ModRun {
    int finished = 0;
    int exit_code = 0;
    int mode = 0;
    int workload = 0;
    std::string snap;
};

ModRun run_module(const std::vector<std::string> & args) {
    ModRun r;
    Proj p;
    // Defaults come from the same struct the carrier parses into.
    p.L.choices.clear();
    int spec_source = 0; // DraftSource::none
    bool stop = false;

    // args holds the flags only; the native argv's element 0 is the program name.
    for (size_t i = 0; i < args.size() && !stop; ++i) {
        const std::string & a = args[i];
        const int64_t id = drv_flag_id(sl_u8{(uint8_t *) a.data(), (int64_t) a.size()});
        if (id < 0) {
            r.exit_code = (int) drv_exit(2); // unknown argument
            stop = true;
            break;
        }
        std::string value;
        if (drv_span(id) == 1) {
            if (i + 1 >= args.size()) {
                r.exit_code = (int) drv_exit(1); // missing value
                stop = true;
                break;
            }
            value = args[++i];
        }
        const int64_t field = drv_field(id);
        const int64_t kind = drv_kind(id);

        // The field write, table-driven: the module says where, the carrier writes.
        switch (field) {
        case 0: p.cfg.model_path = value; break;
        case 1: p.cfg.prompt = value; break;
        case 2: p.cfg.n_predict = std::atoi(value.c_str()); break;
        case 3: p.cfg.n_threads = std::atoi(value.c_str()); break;
        case 4: p.cfg.n_ctx = std::atoi(value.c_str()); break;
        case 5: p.cfg.n_ubatch = std::atoi(value.c_str()); break;
        case 6: p.cfg.n_expert_used = std::atoi(value.c_str()); break;
        case 7: p.cfg.n_gpu_layers = std::atoi(value.c_str()); break;
        case 8: p.cfg.vram_cache_mb = std::atoi(value.c_str()); break;
        case 9: p.cfg.expert_hot_slots = std::atoi(value.c_str()); break;
        case 10: p.cfg.sampling.temp = (float) std::atof(value.c_str()); break;
        case 11: p.cfg.sampling.top_k = std::atoi(value.c_str()); break;
        case 12: p.cfg.sampling.top_p = (float) std::atof(value.c_str()); break;
        case 13: p.cfg.sampling.seed = (uint32_t) std::strtoul(value.c_str(), nullptr, 10); break;
        case 14: {
            const int want = kind == 10 ? 1 : 2;
            if (drv_draft_conflict(spec_source, want) != 0) {
                r.exit_code = (int) drv_exit(3);
                stop = true;
            } else {
                spec_source = want;
                p.cfg.spec.source = want == 1 ? DraftSource::mtp : DraftSource::ngram;
            }
            break;
        }
        case 15: p.cfg.spec.draft_max = std::atoi(value.c_str()); break;
        case 16: p.cfg.spec.draft_p_min = (float) std::atof(value.c_str()); break;
        case 17: p.cfg.spec.ngram_min_match = std::atoi(value.c_str()); break;
        case 18: p.cfg.chatml = kind == 4; break;
        case 19: p.cfg.think = kind == 4; break;
        case 20: p.cfg.progress = kind == 4; break;
        case 21: p.L.session = kind == 4 ? 1 : 0; break;
        case 22: p.L.csv = value; break;
        case 23: p.L.route = value; break;
        case 24:
            p.L.compute = value;
            if (kind == 12) p.cfg.compute_trace_layers = true;
            break;
        case 25: p.cfg.compute_trace_layers = kind == 4; break;
        case 26: p.L.io = value; break;
        case 27: p.L.wcap = value; break;
        case 28: p.L.wrep = value; break;
        case 29: p.cfg.moe.enabled = kind == 4; break;
        case 30:
            if (kind == 6 && value == "auto")
                p.cfg.moe.cache_auto = true;
            else
                p.cfg.moe.cache_mb = std::atoi(value.c_str());
            break;
        case 31: p.cfg.moe.cache_auto = kind == 4; break;
        case 32: p.cfg.moe.cache_floor_mb = std::atoi(value.c_str()); break;
        case 33: p.cfg.moe.cache_ceil_mb = std::atoi(value.c_str()); break;
        case 34: p.cfg.moe.io_threads = std::atoi(value.c_str()); break;
        case 35: p.cfg.moe.o_direct = kind == 4; break;
        case 36: p.cfg.moe.row_stream = kind == 4; break;
        case 37: p.cfg.moe.row_stream_mb = std::atoi(value.c_str()); break;
        case 38: {
            int m = -1;
            if (kind == 7) m = dense_mode_of(value);
            else m = (int) drv_dense_alias(id);
            if (m < 0) {
                r.exit_code = (int) drv_exit(4);
                stop = true;
            } else {
                p.cfg.moe.dense_weights = (DenseWeightsMode) m;
            }
            break;
        }
        case 39: p.cfg.moe.load_all = kind == 4; break;
        case 40: p.cfg.moe.force_cache = kind == 4; break;
        case 41: p.cfg.moe.overlap = kind == 4; break;
        case 42: p.cfg.moe.io_two_wave = kind == 4; break;
        case 43: p.cfg.moe.prefetch_layers = std::atoi(value.c_str()); break;
        case 44: p.cfg.moe.prefetch_sync = kind == 4; break;
        case 45: p.cfg.moe.drop_cold_frac = (float) std::atof(value.c_str()); break;
        case 46: p.cfg.moe.substitute_lambda = (float) std::atof(value.c_str()); break;
        case 47: p.L.ppl = value; break;
        case 48: p.L.ppl_skip = std::atoi(value.c_str()); break;
        case 49: p.L.ppl_step = kind == 4 ? 1 : 0; break;
        case 50: p.L.ppl_list = value; break;
        case 51: {
            // --ppl-choices: the split itself is drv_csv_next's, the storage is
            // the carrier's, exactly as the native loop does it.
            const int64_t n = drv_csv_count(comma_count(value));
            int64_t start = 0;
            sl_u8 sv{(uint8_t *) value.data(), (int64_t) value.size()};
            for (int64_t k = 0; k < n; ++k) {
                const int64_t comma = drv_csv_next(sv, start);
                const int64_t end = comma < 0 ? (int64_t) value.size() : comma;
                p.L.choices.push_back(value.substr((size_t) start, (size_t) (end - start)));
                if (comma < 0) break;
                start = comma + 1;
            }
            break;
        }
        case 52: p.cfg.moe.drop_renorm = kind == 4; break;
        case 53: p.cfg.moe.drop_prefill = kind == 4; break;
        case 54: p.cfg.moe.route_ahead = std::atoi(value.c_str()); break;
        case 55: p.cfg.moe.predict_log = kind == 4; break;
        case 56: p.cfg.moe.predict_prefetch = kind == 4; break;
        case 57: p.cfg.moe.predict_spec_max = std::atoi(value.c_str()); break;
        case 58: {
            const int64_t act = drv_action(id);
            if (act >= 0) {
                r.finished = 0;
                r.exit_code = 0;
                stop = true;
            }
            break;
        }
        default:
            break;
        }
    }

    // The decisions the carrier makes after the loop, from the same inputs.
    const int64_t has_ppl = p.L.ppl.empty() ? 0 : 1;
    const int64_t has_list = p.L.ppl_list.empty() ? 0 : 1;
    r.mode = (int) drv_mode(p.L.session, has_ppl, has_list);
    r.workload = (int) drv_workload_conflict(p.L.session, has_ppl, has_list, p.L.wcap.empty() ? 0 : 1,
                                             p.L.wrep.empty() ? 0 : 1);
    if (!stop) {
        r.finished = 1;
        r.exit_code = 0;
    }
    r.snap = snapshot(p.cfg, p.L);
    return r;
}

// The carrier trims only the END of a list line (CR and space), never the front;
// rtrim is the comparison form of that.
std::string rtrim(const std::string & s) {
    size_t b = s.size();
    while (b > 0 && (s[b - 1] == ' ' || s[b - 1] == '\r')) --b;
    return s.substr(0, b);
}

std::string trimmed(const std::string & s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\n' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\n' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

// ── the harness ────────────────────────────────────────────────────────────
int g_cases = 0, g_values = 0, g_fails = 0;

void new_case() { ++g_cases; }
void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [G7] %s\n", what.c_str());
    }
}

void run_case(const char * tag, const std::vector<std::string> & args) {
    new_case();
    const NativeRun nat = run_native(args);
    const ModRun mod = run_module(args);
    const std::string t = tag;

    // FG654_DBG=1 prints the two arms' verdicts, which is what a failing case
    // needs; the snapshot diff below names the field and the line that moved.
    if (getenv("FG654_DBG"))
        std::printf("  DBG %s nat(f=%d x=%d) mod(f=%d x=%d)\n", tag, nat.finished, nat.exit_code,
                    mod.finished, mod.exit_code);
    note(!nat.snap.empty(), t + " native snapshot produced");
    note(nat.finished == mod.finished, t + " parse finished");
    note(nat.exit_code == mod.exit_code, t + " exit code");
    note(nat.mode == mod.mode, t + " mode");
    note(nat.workload == mod.workload, t + " workload rejection");

    std::istringstream na(nat.snap), mo(mod.snap);
    std::string nl, ml;
    int line = 0;
    while (std::getline(na, nl)) {
        if (!std::getline(mo, ml)) {
            note(false, t + " module snapshot ended early");
            break;
        }
        note(nl == ml, t + " field[" + std::to_string(line) + "] " + trimmed(nl.substr(0, nl.find('='))));
        ++line;
    }
    note(!std::getline(mo, ml), t + " module snapshot has no extra line");
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    const std::string M = "--moe-stream";

    // The flag table itself: every id the carrier knows resolves to itself, an
    // alias resolves to the same id, and a non-flag does not resolve.
    {
        new_case();
        struct Pair {
            const char * name;
            int64_t id;
        };
        const Pair table[] = {
            {"-m", 0},         {"--model", 0},          {"-p", 1},        {"--prompt", 1},
            {"-n", 2},         {"--n-predict", 2},      {"-t", 3},        {"--threads", 3},
            {"-c", 4},         {"--ctx-size", 4},       {"--ubatch", 5},  {"--n-expert-used", 6},
            {"--gpu-layers", 7}, {"--vram-cache-mb", 8}, {"--expert-hot-slots", 9},
            {"--temp", 10},    {"--top-k", 11},         {"--top-p", 12},  {"--seed", 13},
            {"--mtp", 14},     {"--ngram", 15},         {"--draft", 16},  {"--mtp-p-min", 17},
            {"--ngram-min-match", 18}, {"--chatml", 19}, {"--no-think", 20}, {"--progress", 21},
            {"--session", 22}, {"--csv", 23},           {"--route-trace", 24},
            {"--compute-trace", 25}, {"--compute-trace-layers", 26}, {"--io-trace", 27},
            {"--workload-capture", 28}, {"--workload-replay", 29}, {"--moe-stream", 30},
            {"--cache-mb", 31}, {"--cache-floor-mb", 32}, {"--cache-ceil-mb", 33},
            {"--io-threads", 34}, {"--no-odirect", 35}, {"--row-stream", 36},
            {"--row-stream-mb", 37}, {"--dense-weights", 38}, {"--no-warm-dense", 39},
            {"--dense-odirect", 40}, {"--load-all", 41}, {"--force-cache", 42},
            {"--overlap", 43}, {"--io-two-wave", 44},   {"--prefetch", 45},
            {"--prefetch-sync", 46}, {"--drop-cold-experts", 47}, {"--expert-substitute", 48},
            {"--ppl", 49},     {"--ppl-skip", 50},      {"--ppl-step", 51}, {"--ppl-list", 52},
            {"--ppl-choices", 53}, {"--drop-no-renorm", 54}, {"--drop-in-prefill", 55},
            {"--route-ahead", 56}, {"--predict-log", 57}, {"--predict-prefetch", 58},
            {"--predict-spec-max", 59}, {"--list-archs", 60}, {"-h", 61}, {"--help", 61},
            {"--version", 62},
        };
        for (const Pair & p : table) {
            const std::string s = p.name;
            const int64_t id = drv_flag_id(sl_u8{(uint8_t *) s.data(), (int64_t) s.size()});
            note(id == p.id, std::string("flag id ") + p.name);
            note(drv_field(id) >= 0, std::string("field exists for ") + p.name);
            note(drv_span(id) >= 0, std::string("span known for ") + p.name);
        }
        const std::string bad = "--not-a-flag";
        note(drv_flag_id(sl_u8{(uint8_t *) bad.data(), (int64_t) bad.size()}) == -1, "unknown flag is -1");
        note(drv_field(63) == -1 && drv_kind(63) == -1 && drv_span(63) == -1, "out-of-range id");
    }

    // The bare minimum: one model and nothing else.
    run_case("minimal", {"-m", "/models/x.gguf"});
    // Every integer field at once.
    run_case("ints", {"-m", "/m/a.gguf", "-n", "64", "-t", "8", "-c", "4096", "--ubatch", "256",
                      "--n-expert-used", "4", "--gpu-layers", "10", "--vram-cache-mb", "512",
                      "--expert-hot-slots", "3"});
    // Sampling.
    run_case("sampling", {"--temp", "0.8", "--top-k", "40", "--top-p", "0.95", "--seed", "12345"});
    // Drafting: the two sources, then each one's own knobs.
    run_case("mtp", {"--mtp", "--draft", "4", "--mtp-p-min", "0.5"});
    run_case("ngram", {"--ngram", "--ngram-min-match", "5"});
    // The boolean block.
    run_case("bools", {"--chatml", "--no-think", "--progress"});
    // Traces, including the dual --compute-trace-layers.
    run_case("traces", {"--csv", "a.csv", "--route-trace", "r.csv", "--compute-trace", "c.csv",
                        "--io-trace", "i.csv"});
    run_case("trace-layers", {"--compute-trace-layers", "c.csv"});
    // The streaming block.
    run_case("moe", {M, "--cache-mb", "2048", "--cache-floor-mb", "1000", "--cache-ceil-mb", "9000",
                     "--io-threads", "6", "--no-odirect", "--row-stream", "--row-stream-mb", "128"});
    // The auto split of --cache-mb.
    run_case("cache-auto", {M, "--cache-mb", "auto"});
    run_case("cache-zero", {M, "--cache-mb", "0"});
    // Dense policy: the value, then both deprecated aliases.
    run_case("dense-warm", {"--dense-weights", "warm"});
    run_case("dense-mmap", {"--dense-weights", "mmap"});
    run_case("dense-anon", {"--dense-weights", "anon"});
    run_case("dense-ahwb", {"--dense-weights", "ahwb"});
    run_case("dense-alias-mmap", {"--no-warm-dense"});
    run_case("dense-alias-anon", {"--dense-odirect"});
    // The remaining MoE knobs.
    run_case("moe-rest", {"--load-all", "--force-cache", "--overlap", "--io-two-wave",
                          "--prefetch", "3", "--prefetch-sync", "--drop-cold-experts", "0.3",
                          "--expert-substitute", "0.6", "--drop-no-renorm", "--drop-in-prefill",
                          "--route-ahead", "2", "--predict-log", "--predict-prefetch",
                          "--predict-spec-max", "3"});
    // Perplexity mode.
    run_case("ppl", {"--ppl", "p.txt", "--ppl-skip", "4", "--ppl-step"});
    run_case("ppl-list", {"--ppl-list", "l.txt", "--ppl-choices", "a,b,c"});
    run_case("ppl-choices-empty", {"--ppl-choices", ""});
    run_case("ppl-choices-trailing", {"--ppl-choices", "a,"});
    run_case("ppl-choices-gaps", {"--ppl-choices", ",,x"});
    // Refusals.
    run_case("unknown", {"--nope"});
    run_case("missing-value", {"--temp"});
    run_case("missing-value-last", {"-m", "x", "--draft"});
    run_case("draft-conflict", {"--mtp", "--ngram"});
    run_case("draft-conflict-rev", {"--ngram", "--mtp"});
    run_case("draft-same-twice", {"--mtp", "--mtp"});
    run_case("dense-bad", {"--dense-weights", "banana"});
    // Actions end the run before anything after them is parsed.
    run_case("help", {"-h"});
    run_case("help-long", {"--help", "-m", "/models/x.gguf"});
    run_case("version", {"--version"});
    run_case("list-archs", {"--list-archs", "--temp", "1"});
    // Aliases: the last one wins, as it does natively.
    run_case("alias-model", {"-m", "a.gguf", "--model", "b.gguf"});
    run_case("alias-prompt", {"-p", "hello"});
    // Mode and workload interactions.
    run_case("session", {"--session", "--moe-stream"});
    run_case("session-and-ppl", {"--session", "--ppl", "p.txt"});
    run_case("ppl-and-list", {"--ppl", "p.txt", "--ppl-list", "l.txt"});
    run_case("workload-session", {"--session", "--workload-capture", "w.bin"});
    run_case("workload-ppl", {"--ppl", "p.txt", "--workload-replay", "w.bin"});
    run_case("workload-ppl-list", {"--ppl-list", "l.txt", "--workload-capture", "w.bin"});
    run_case("workload-ok", {"--workload-capture", "w.bin"});
    // A long mixed command line.
    run_case("mixed", {"-m", "/models/qwen.gguf", "-p", "hi", "-n", "32", "-t", "6", "-c", "2048",
                       "--temp", "0.7", "--top-k", "20", "--top-p", "0.9", "--seed", "7", "--chatml",
                       "--moe-stream", "--cache-mb", "auto", "--overlap", "--prefetch", "4",
                       "--row-stream", "--row-stream-mb", "96", "--csv", "out.csv",
                       "--progress", "--dense-weights", "anon"});

    // ── the --ppl open refusals ────────────────────────────────────────────
    {
        const std::string existing = "/tmp/fg654-ppl-probe.txt";
        {
            FILE * f = std::fopen(existing.c_str(), "wb");
            if (f) {
                std::fputs("text\n", f);
                std::fclose(f);
            }
        }
        int list_missing = 0, list_present = 0, file_missing = 0, file_present = 0;
        {
            SilenceErr quiet;
            list_missing = native_ppl_list_open("/nonexistent/fg654-list.txt");
            list_present = native_ppl_list_open(existing);
            file_missing = native_ppl_file_open("/nonexistent/fg654-text.txt");
            file_present = native_ppl_file_open(existing);
        }
        new_case();
        note(list_missing == 2, "native refuses a missing list");
        note(drv_exit(6) == 2, "drv_exit: an unreadable list is exit 2");
        new_case();
        note(list_present == 0, "native accepts a present list");
        new_case();
        note(file_missing == 2, "native refuses a missing text");
        note(drv_exit(7) == 2, "drv_exit: an unreadable text is exit 2");
        new_case();
        note(file_present == 0, "native accepts a present text");
        std::remove(existing.c_str());
    }

    // ── the --ppl-list line filter ─────────────────────────────────────────
    {
        const std::vector<std::string> lines = {"", "  ", "\r", "text.txt", "text.txt  ", "text.txt\r",
                                                "# comment", "#", " #not", "a b  ", "\t", "x\r\r"};
        for (const std::string & ln : lines) {
            new_case();
            // The carrier's filter, verbatim, over one line.
            std::vector<std::string> paths;
            std::string line = ln;
#include "native_list_filter.inc"
            sl_u8 sv{(uint8_t *) ln.data(), (int64_t) ln.size()};
            const int64_t keep = drv_list_keep(sv);
            const std::string t = "list-line '" + trimmed(ln) + "'";
            note((keep >= 0 ? 1 : 0) == (paths.size() == 1 ? 1 : 0), t + " keep decision");
            if (keep >= 0) note(keep == (int64_t) paths[0].size(), t + " trimmed length");
            if (keep >= 0) note(rtrim(ln) == paths[0], t + " trimmed text");
        }
    }

    // ── path count, labels and percentages ─────────────────────────────────
    for (int has_path = 0; has_path < 2; ++has_path)
        for (int lines = 0; lines < 4; ++lines) {
            new_case();
            note(drv_ppl_path_count(has_path, lines) == (int64_t) (has_path + lines), "ppl path count");
            note(drv_ppl_label(has_path + lines) == (has_path + lines > 1 ? 1 : 0), "ppl label");
        }
    for (int scored : {0, 1, 2, 5, 100, 4096}) {
        const std::vector<int> tops = {0, 1, scored > 1 ? scored - 1 : 0, scored};
        for (int top1 : tops) {
            new_case();
            const double nat = scored > 0 ? 100.0 * top1 / scored : 0.0;
            char tag[64];
            std::snprintf(tag, sizeof tag, "pct(%d,%d)", top1, scored);
            note(drv_ppl_pct(top1, scored) == nat, tag);
        }
    }

    std::printf("FG654_DRV cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails;
}
