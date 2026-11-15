// Carrier of the model-management decisions (staged S3.1 trial).
//
// The six decisions below are the ones P8-06 ported to Goose in
// goose/server/{model-manager,hot-reload,connectors}.goose. Each one has a native
// transcription (always compiled) and a dispatched entry point. With
// FG_GOOSE_MODELS=OFF the dispatched entry calls the transcription; with ON it
// crosses into the composed Goose unit through common/fg-goose-model-bridge.cpp.
//
// Nothing here owns a clock, a mutex, a child process, an SSE writer or a socket:
// these are the decisions the manager takes over state it already holds, which is
// what lets the gate run without a model, a server or a GPU.
//
// Provenance of the transcriptions (reference/ carries the two files):
//   server-models.h 89-105      the four meta predicates
//   server-models.cpp 1172-1195 unload()
//   server-models.h 274-296     server_lru_sched::pick_victim
//   server-models.cpp 633-638   preset_options_for_compare
//   server-models.cpp 706-709   the reload's unload set
//   server-connect.cpp 128-150  upstream_host
//   server-connect.cpp 136-165  find_binary
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fg_model {

// server-models.h status enum, mirrored so the carrier needs no server header
constexpr int kDownloading = 0;
constexpr int kIncomplete = 1;
constexpr int kDownloaded = 2;
constexpr int kUnloaded = 3;
constexpr int kLoading = 4;
constexpr int kLoaded = 5;
constexpr int kSleeping = 6;

// server-models.h source enum
constexpr int kSrcPreset = 0;
constexpr int kSrcModelsDir = 1;
constexpr int kSrcCache = 2;

// decision 1: the status predicates -----------------------------------------
struct pred_t {
    int ready = 0;
    int running = 0;
    int ready_or_sleep = 0;
    int failed = 0;
};

// decision 2: what unload() does for a status --------------------------------
// action: 0 nothing, 1 cancel the download and wait, 2 stop the child,
//         3 "not running" warning
struct unload_t {
    int action = 0;
    int send_exit = 0;  // the child gets the exit command
    int terminate = 0;  // force-kill first (a child that is still loading)
};

// decision 3: the LRU victim over the mapping -------------------------------
struct row_t {
    std::string name;
    int status = kUnloaded;
    long long req_count = 0;
    long long last_used = 0;
    int stopping = 0;
    int queued = 0;
};

// decision 4/5: a mapping entry as the reload sees it ------------------------
struct model_t {
    std::string name;
    int status = kUnloaded;
    // "KEY=VALUE" entries separated by '\n', unique keys in byte order: the
    // shape std::map<option> iterates in the native preset
    std::string options;
};

// decision 5: rows to unload, in name order, with the reason
// reason: 1 the source dropped the name, 2 the preset changed
struct unload_set_t {
    std::vector<long long> rows;
    std::vector<int> reasons;
};

// decision 6: how a model is reached ----------------------------------------
struct connector_t {
    long long binary = -1;  // -1 nothing found, 0 the sibling, 1+k the k-th PATH entry
    std::string host;       // the address the child dials, empty for a unix-socket only bind
};

// --- dispatched entry points (OFF: native, ON: Goose) -----------------------
pred_t pred(int status, long long exit_code);
unload_t unload_action(int mapped, int status);
long long victim(const std::vector<row_t> & rows);
std::string preset_norm(const std::string & options);
unload_set_t unload_set(const std::vector<model_t> & cur, const std::vector<model_t> & next);
connector_t connector_target(int sibling, const std::vector<int> & dir_exists,
                             const std::vector<std::string> & hosts);

// --- the native transcriptions, compiled in both configurations -------------
pred_t pred_native(int status, long long exit_code);
unload_t unload_action_native(int mapped, int status);
long long victim_native(const std::vector<row_t> & rows);
std::string preset_norm_native(const std::string & options);
unload_set_t unload_set_native(const std::vector<model_t> & cur,
                               const std::vector<model_t> & next);
connector_t connector_target_native(int sibling, const std::vector<int> & dir_exists,
                                    const std::vector<std::string> & hosts);

// Decisions taken by the Goose module in this process; the gate prints it as
// the witness that the dispatch really entered Goose. 0 on the OFF build.
long long goose_calls();

}  // namespace fg_model
