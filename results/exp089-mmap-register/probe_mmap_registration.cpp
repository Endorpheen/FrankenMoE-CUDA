// probe_mmap_registration.cpp — EXP089 draft
//
// Test: can the pages the loaded model already holds mapped on the host be
// registered with cudaHostRegister without copying the weights?
//
// The registration is only attempted when EXP089_REGISTER_MMAP=1 is set at
// run time and the run uses params.load_mode == LLAMA_LOAD_MODE_MMAP. Without
// it the file behaves like the original probe_host_weights.cpp: it loads the
// model, reports how CUDA classifies the host side of it, and exits.
//
// Test-only control:
//  - no weights are copied or modified and no llama_decode is called;
//  - every successfully registered range is unregistered before the loaded
//    model/context is destroyed, even if an exception unwinds the scope;
//  - only memory of the loaded model is registered;
//  - no raw addresses or weight data reach stdout.

#include "arg.h"
#include "common.h"
#include "llama.h"
#include "llama-model.h"
#include "ggml-backend.h"
#include "json.hpp"
#include <cuda_runtime_api.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>      // sysconf(_SC_PAGESIZE)
#include <vector>

namespace {

// One address range of the model on the host, [start, end), aligned out to
// the page grid and merged with its neighbours so that no page is registered
// twice.
struct Interval {
    uintptr_t start = 0;
    uintptr_t end = 0;
};

// A CUDA API failure: its code plus the message from cudaGetErrorString().
struct CUDAError {
    int code = 0;
    std::string message;
};

struct HostStats {
    size_t host_tensors = 0;
    size_t host_bytes = 0;
    size_t pinned_tensors = 0;
    size_t pinned_bytes = 0;
};

// One /proc/self/status + /proc/self/smaps_rollup snapshot: the fields the
// kernel does not expose stay at -1 and are emitted as null.
struct ProcStats {
    // /proc/self/status, as the kernel prints them ("... kB").
    std::string rss;          // VmRSS
    std::string rss_hwm;      // VmHWM
    std::string swap;         // VmSwap
    // /proc/self/smaps_rollup, in kB.
    long rss_of_smaps = -1;   // Rss
    long pss_anon = -1;       // Pss_anon
    long pss_file = -1;       // Pss_file
    long anonymous = -1;      // Anonymous
    long locked = -1;         // Locked
    bool smaps = false;
};

struct MmapRegistration {
    int requested = 0;
    int successful = 0;
    size_t registered_bytes = 0;
    double seconds = 0.0;
    std::vector<CUDAError> register_errors;
    int untracked = 0;  // succeeded but could not be recorded
};

bool exp089_mmap_registration_enabled() {
    // Test-only control: EXP089_REGISTER_MMAP=1 (run time) is the only way to
    // turn the registration in this file on; there is no compile time opt-in.
    const char *env = getenv("EXP089_REGISTER_MMAP");
    if (env != nullptr) {
        std::string value = env;
        return value == "1" || value == "true" || value == "yes" ||
               value == "ON" || value == "on";
    }
    return false;
}

// The pinning signal the original probe uses: the host side of every model
// tensor, as CUDA classifies it. It does not depend on the buffer's name.
HostStats query_host_stats(const llama_model *model) {
    HostStats stats{};
    for (const auto &entry : model->tensors_by_name) {
        const auto *tensor = entry.second;
        if (!tensor->data || !tensor->buffer ||
            !ggml_backend_buffer_is_host(tensor->buffer)) {
            continue;
        }
        const size_t bytes = ggml_nbytes(tensor);
        cudaPointerAttributes attributes{};
        const auto error = cudaPointerGetAttributes(&attributes, tensor->data);
        const bool pinned = error == cudaSuccess &&
            attributes.type == cudaMemoryTypeHost;
        if (error != cudaSuccess) { (void)cudaGetLastError(); }
        ++stats.host_tensors;
        stats.host_bytes += bytes;
        if (pinned) { ++stats.pinned_tensors; stats.pinned_bytes += bytes; }
    }
    return stats;
}

std::vector<Interval> align_and_merge(
    const std::vector<Interval> &raw, uintptr_t page) {
    std::vector<Interval> aligned;
    for (const auto &range : raw) {
        if (range.start == range.end) { continue; }
        // round down: always safe
        uintptr_t start = range.start & ~(page - 1);
        // round up, after checking the addition does not wrap the address space
        if (range.end > UINTPTR_MAX - (page - 1)) {
            throw std::runtime_error("page alignment overflow");
        }
        uintptr_t end = (range.end + page - 1) & ~(page - 1);
        if (end <= start) { throw std::runtime_error("invalid aligned interval"); }
        aligned.push_back({start, end});
    }

    std::sort(aligned.begin(), aligned.end(),
              [](const Interval &a, const Interval &b) {
                  return a.start < b.start;
              });

    std::vector<Interval> merged;
    for (const auto &range : aligned) {
        if (merged.empty()) { merged.push_back(range); continue; }
        auto &last = merged.back();
        if (range.start <= last.end) {
            // overlapping or adjacent: the pages are contiguous
            if (range.end > last.end) { last.end = range.end; }
        } else {
            merged.push_back(range);
        }
    }
    return merged;
}

// RAII over cudaHostUnregister: release() unregisters everything that was
// registered, exactly once, and records the results; the destructor calls it
// for the exception path.
//
// Its storage is sized for exactly this many ranges before the first
// registration happens, so nothing in the unregister loop can allocate, and
// one failed unregister cannot abort cleanup and leave the remaining owned
// ranges pinned.
struct MmapRegistrationGuard {
    std::vector<Interval> ranges;
    std::vector<int> unregister_codes;  // POD: capacity set before registration
    bool released = false;

    void reserve(size_t count) {
        ranges.reserve(count);
        unregister_codes.reserve(count);
    }
    size_t registered() const { return ranges.size(); }

    void push_back(const Interval &range) { ranges.push_back(range); }

    void release() noexcept {
        if (released) { return; }
        released = true;
        for (const auto &range : ranges) {
            const int err = static_cast<int>(
                cudaHostUnregister(reinterpret_cast<void *>(range.start)));
            // Fits the pre-reserved storage: the loop cannot allocate, so
            // every owned range is still handed back even if some
            // unregistration fails.
            unregister_codes.push_back(err);
        }
    }
    ~MmapRegistrationGuard() noexcept {
        if (!released) { release(); }
    }
};

// Register the pages the model already holds, one merged range at a time.
// Successful registrations are handed to guard as they happen, so that the
// guard's destructor unregisters everything even when an exception unwinds
// the scope.
MmapRegistration register_mmap_pages(const llama_model *model,
                                    uintptr_t page,
                                    MmapRegistrationGuard *guard) {
    MmapRegistration result;
    if (page == 0) { return result; }  // sysconf() did not hand us a page size

    // Only memory of the loaded model: host side, non-empty address ranges.
    std::vector<Interval> raw;
    for (const auto &entry : model->tensors_by_name) {
        const auto *tensor = entry.second;
        if (!tensor->data || !tensor->buffer ||
            !ggml_backend_buffer_is_host(tensor->buffer)) {
            continue;
        }
        const size_t bytes = ggml_nbytes(tensor);
        if (bytes == 0) { continue; }
        const uintptr_t start = reinterpret_cast<uintptr_t>(tensor->data);
        if (start > UINTPTR_MAX - bytes) {
            throw std::runtime_error("tensor interval overflow");
        }
        raw.push_back({start, start + bytes});
    }

    std::vector<Interval> ranges = align_and_merge(raw, page);
    result.requested = static_cast<int>(ranges.size());

    // Exactly this many slots: neither the guard's push_back nor the
    // unregister loop can allocate after a successful registration.
    guard->reserve(ranges.size());
    result.register_errors.reserve(ranges.size());

    const auto begin = std::chrono::steady_clock::now();
    for (const auto &range : ranges) {
        const size_t size = static_cast<size_t>(range.end - range.start);
        cudaError_t err = cudaHostRegister(
            reinterpret_cast<void *>(range.start), size,
            cudaHostRegisterPortable | cudaHostRegisterReadOnly);
        if (err == cudaSuccess) {
            ++result.successful;
            result.registered_bytes += size;
            try {
                guard->push_back(range);
            } catch (const std::bad_alloc &) {
                // Registration succeeded but the guard could not record
                // this range: unregister it right now, `range` is the only
                // pointer to it.
                (void)cudaHostUnregister(reinterpret_cast<void *>(range.start));
                ++result.untracked;
            }
        } else {
            (void)cudaGetLastError();  // record the code and clear it
            result.register_errors.push_back(
                {static_cast<int>(err), cudaGetErrorString(err)});
        }
    }
    result.seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - begin).count();
    return result;
}

// First `prefix` characters of `line`, compared with `prefix` itself: true
// when `line` starts with prefix and is longer than it.
bool line_starts_with(const std::string &line, const char *prefix) {
    const size_t length = strlen(prefix);
    return line.size() > length &&
           line.compare(0, length, prefix) == 0;
}

// Parse one field of a smaps/smaps_rollup mapping section: "Name: value kB".
long parse_smaps_field(const std::string &line, const char *name) {
    const size_t prefix = strlen(name);
    if (line.size() <= prefix) { return -1; }
    std::istringstream tail(line.substr(prefix + 1));
    long value = 0;
    if (tail >> value) { return value; }
    return -1;
}

// Read-only snapshots of this process: /proc/self/status (RSS, high-water
// mark, swap) and /proc/self/smaps_rollup (page ownership in kB). The call
// never throws; fields the kernel does not expose are -1.
ProcStats read_proc_stats() {
    ProcStats stats;
    std::ifstream status("/proc/self/status");
    if (status.is_open()) {
        std::string line;
        while (std::getline(status, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                stats.rss = line.substr(sizeof("VmRSS:") - 1);
            } else if (line.rfind("VmHWM:", 0) == 0) {
                stats.rss_hwm = line.substr(sizeof("VmHWM:") - 1);
            } else if (line.rfind("VmSwap:", 0) == 0) {
                stats.swap = line.substr(sizeof("VmSwap:") - 1);
            }
        }
    }
    std::ifstream smaps("/proc/self/smaps_rollup");
    if (!smaps.is_open()) { return stats; }
    stats.smaps = true;
    std::string line;
    while (std::getline(smaps, line)) {
        // One pass over the file: which fields the kernel prints, on which
        // lines, and which ones it omits entirely are all unknown, so every
        // line is checked against every field. A field the kernel omits
        // stays at -1 and is emitted as null.
        if (line_starts_with(line, "Rss:")) {
            stats.rss_of_smaps = parse_smaps_field(line, "Rss:");
        } else if (line_starts_with(line, "Pss_anon:")) {
            stats.pss_anon = parse_smaps_field(line, "Pss_anon:");
        } else if (line_starts_with(line, "Pss_file:")) {
            stats.pss_file = parse_smaps_field(line, "Pss_file:");
        } else if (line_starts_with(line, "Anonymous:")) {
            stats.anonymous = parse_smaps_field(line, "Anonymous:");
        } else if (line_starts_with(line, "Locked:")) {
            stats.locked = parse_smaps_field(line, "Locked:");
        }
    }
    return stats;
}

nlohmann::json to_json_or_null(long value) {
    return value >= 0 ? nlohmann::json(value) : nullptr;
}

nlohmann::json proc_snapshot(const ProcStats &stats) {
    nlohmann::json proc = nlohmann::json::object();
    auto &status = proc["self_status"];
    status["VmRSS"] = stats.rss;
    status["VmHWM"] = stats.rss_hwm;
    status["VmSwap"] = stats.swap;
    auto &smaps = proc["smaps_rollup"];
    smaps["Rss"] = to_json_or_null(stats.rss_of_smaps);
    smaps["Pss_anon"] = to_json_or_null(stats.pss_anon);
    smaps["Pss_file"] = to_json_or_null(stats.pss_file);
    smaps["Anonymous"] = to_json_or_null(stats.anonymous);
    smaps["Locked"] = to_json_or_null(stats.locked);
    return proc;
}

nlohmann::json errors_array(const std::vector<CUDAError> &errors) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto &error : errors) {
        array.push_back(
            nlohmann::json{{"code", static_cast<long>(error.code)},
                           {"message", error.message}});
    }
    return array;
}

// Unregister results are plain CUDA error codes: they become messages after
// the guard has done its work, so cleanup itself never allocates.
nlohmann::json errors_array(const std::vector<int> &codes) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto code : codes) {
        array.push_back(nlohmann::json{{"code", static_cast<long>(code)},
                                       {"message", cudaGetErrorString(static_cast<cudaError_t>(code))}});
    }
    return array;
}

}  // namespace

int main(int argc, char **argv) {
    try {
        common_init();
        common_params params;
        if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_SERVER)) {
            throw std::runtime_error("argument parsing failed");
        }
        params.fit_params = false;
        params.warmup = false;
        params.no_alloc = false;
        params.n_outputs_max = 1;
        params.n_outputs_max_per_seq = 1;
        llama_backend_init();
        const auto begin = std::chrono::steady_clock::now();
        auto loaded = common_init_from_params(params);
        auto *ctx = loaded->context();
        if (!ctx) { throw std::runtime_error("context creation failed"); }
        const auto load_and_context_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - begin).count();
        const auto *model = llama_get_model(ctx);

        const bool mmap_mode = params.load_mode == LLAMA_LOAD_MODE_MMAP;
        const bool exp089 = exp089_mmap_registration_enabled();
        if (exp089 && !mmap_mode) {
            throw std::runtime_error(
                "EXP089_REGISTER_MMAP=1 requires load_mode == LLAMA_LOAD_MODE_MMAP");
        }

        if (exp089 && mmap_mode) {
            // The pinning signal before the experiment touches anything.
            const HostStats before = query_host_stats(model);

            const long page_size = sysconf(_SC_PAGESIZE);
            if (page_size <= 0 || (page_size & (page_size - 1)) != 0) {
                throw std::runtime_error(
                    "sysconf(_SC_PAGESIZE) is not a positive power of two");
            }
            const uintptr_t page = static_cast<uintptr_t>(page_size);
            const ProcStats before_stats = read_proc_stats();

            MmapRegistrationGuard guard;
            MmapRegistration reg =
                register_mmap_pages(model, page, &guard);

            // Pinning as CUDA sees it now: after registration, before
            // unregistration, and independent of any buffer name.
            const HostStats after = query_host_stats(model);

            const ProcStats after_stats = read_proc_stats();

            // Explicit release: the ranges come back from CUDA, the results
            // leave through the JSON below. The guard's destructor covers
            // every other path.
            guard.release();
            const HostStats released = query_host_stats(model);

            nlohmann::json result = nlohmann::json::object();
            result["pinned_tensors_after_release"] = released.pinned_tensors;
            result["inference_requests"] = 0;
            result["warmup"] = false;
            result["host_tensors"] = before.host_tensors;
            result["pinned_tensors"] = before.pinned_tensors;
            result["host_logical_bytes"] = before.host_bytes;
            result["pinned_logical_bytes"] = before.pinned_bytes;
            result["load_and_context_seconds"] = load_and_context_seconds;
            result["mmap_registration"] = nlohmann::json::object();
            auto &registration = result["mmap_registration"];
            registration["requested_ranges"] = reg.requested;
            registration["successful_ranges"] = reg.successful;
            registration["registered_bytes"] = reg.registered_bytes;
            registration["duration_seconds"] = reg.seconds;
            registration["register_errors"] = errors_array(reg.register_errors);
            registration["untracked_registrations"] = reg.untracked;
            result["host_tensors_after_registration"] =
                after.host_tensors;
            result["host_bytes_after_registration"] = after.host_bytes;
            result["pinned_tensors_after_registration"] =
                after.pinned_tensors;
            result["pinned_bytes_after_registration"] =
                after.pinned_bytes;
            result["before_registration"] = proc_snapshot(before_stats);
            result["after_registration"] = proc_snapshot(after_stats);
            result["unregister"] = nlohmann::json::object();
            auto &unregister = result["unregister"];
            unregister["ranges"] = static_cast<long>(guard.registered());
            unregister["errors"] = errors_array(guard.unregister_codes);
            std::cout << "MMAP_REGISTRATION_RESULT "
                      << result.dump() << std::endl;
        } else {
            // Load mode is not mmap, or the test flag is off: the original
            // probe reports CUDA's view of the host side and leaves it at
            // that.
            nlohmann::json buffers = nlohmann::json::object();
            size_t host_tensors = 0, pinned_tensors = 0, host_bytes = 0,
                pinned_bytes = 0;
            for (const auto &entry : model->tensors_by_name) {
                const auto *tensor = entry.second;
                if (!tensor->data || !tensor->buffer ||
                    !ggml_backend_buffer_is_host(tensor->buffer)) {
                    continue;
                }
                const size_t bytes = ggml_nbytes(tensor);
                const std::string name =
                    ggml_backend_buffer_name(tensor->buffer);
                cudaPointerAttributes attributes{};
                const auto error =
                    cudaPointerGetAttributes(&attributes, tensor->data);
                const bool pinned = error == cudaSuccess &&
                    attributes.type == cudaMemoryTypeHost;
                if (error != cudaSuccess) { (void)cudaGetLastError(); }
                ++host_tensors;
                host_bytes += bytes;
                if (pinned) {
                    ++pinned_tensors;
                    pinned_bytes += bytes;
                }
                auto &row = buffers[name];
                if (row.is_null()) {
                    row = {{"tensors", 0}, {"logical_bytes", 0},
                        {"pinned_tensors", 0},
                        {"pinned_logical_bytes", 0}};
                }
                row["tensors"] = row["tensors"].get<size_t>() + 1;
                row["logical_bytes"] =
                    row["logical_bytes"].get<size_t>() + bytes;
                if (pinned) {
                    row["pinned_tensors"] =
                        row["pinned_tensors"].get<size_t>() + 1;
                    row["pinned_logical_bytes"] =
                        row["pinned_logical_bytes"].get<size_t>() + bytes;
                }
            }
            if (host_tensors == 0) {
                throw std::runtime_error("no host model tensors inspected");
            }
            nlohmann::json status = nlohmann::json::object();
            std::ifstream input("/proc/self/status");
            for (std::string line; std::getline(input, line);) {
                if (line.rfind("VmRSS:", 0) == 0 ||
                    line.rfind("VmHWM:", 0) == 0 ||
                    line.rfind("VmSwap:", 0) == 0) {
                    status[line.substr(0, line.find(':'))] =
                        line.substr(line.find(':') + 1);
                }
            }
            nlohmann::json final = {
                {"host_tensors", host_tensors},
                {"pinned_tensors", pinned_tensors},
                {"host_logical_bytes", host_bytes},
                {"pinned_logical_bytes", pinned_bytes},
                {"buffers", buffers},
                {"load_and_context_seconds", load_and_context_seconds},
                {"status", status},
                {"inference_requests", 0},
                {"warmup", false}};
            std::cout << "HOST_PROBE_RESULT " << final.dump()
                      << std::endl;
        }
    } catch (const std::exception &error) {
        std::cerr << "HOST_PROBE_ERROR " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
