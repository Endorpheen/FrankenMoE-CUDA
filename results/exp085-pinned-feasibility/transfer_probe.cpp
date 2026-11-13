// EXP-085 standalone byte-transfer feasibility probe; no model execution.
#include <cuda_runtime.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

using json = nlohmann::ordered_json;
using clock_type = std::chrono::steady_clock;
static constexpr size_t slot_bytes = 16ull << 20;

static void check(cudaError_t result) {
    if (result != cudaSuccess) { throw std::runtime_error(cudaGetErrorString(result)); }
}

static double milliseconds(clock_type::time_point first, clock_type::time_point last) {
    return std::chrono::duration<double, std::milli>(last - first).count();
}

struct ring {
    std::array<void *, 2> slots{};
    std::array<cudaEvent_t, 2> events{};
    std::array<bool, 2> pending{};
    size_t next = 0;
    size_t chunks = 0;
    bool active = false;

    explicit ring(bool partial_allocation_failure = false) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if ((partial_allocation_failure && i == 1) ||
                cudaHostAlloc(&slots[i], slot_bytes, cudaHostAllocDefault) != cudaSuccess ||
                cudaEventCreateWithFlags(&events[i], cudaEventDisableTiming) != cudaSuccess) {
                release();
                (void) cudaGetLastError();
                return;
            }
        }
        active = true;
    }

    ring(const ring &) = delete;
    ring & operator=(const ring &) = delete;

    bool copy(cudaStream_t stream, uint8_t * dst, const uint8_t * src, size_t size) {
        if (!active) { return false; }
        cudaStreamCaptureStatus capture;
        check(cudaStreamIsCapturing(stream, &capture));
        if (capture != cudaStreamCaptureStatusNone) { return false; }
        size_t offset = 0;
        while (offset < size) {
            const size_t index = next;
            if (pending[index]) { check(cudaEventSynchronize(events[index])); }
            const size_t amount = std::min(slot_bytes, size - offset);
            std::memcpy(slots[index], src + offset, amount);
            check(cudaMemcpyAsync(dst + offset, slots[index], amount, cudaMemcpyHostToDevice, stream));
            check(cudaEventRecord(events[index], stream));
            pending[index] = true;
            next = (next + 1) % slots.size();
            offset += amount;
            ++chunks;
        }
        return true;
    }

    void release() noexcept {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (events[i]) {
                if (pending[i]) { (void) cudaEventSynchronize(events[i]); }
                (void) cudaEventDestroy(events[i]);
                events[i] = nullptr;
            }
            if (slots[i]) { (void) cudaFreeHost(slots[i]); slots[i] = nullptr; }
            pending[i] = false;
        }
        active = false;
    }
    ~ring() { release(); }
};

struct gpu_resources {
    cudaStream_t stream = nullptr;
    uint8_t * data = nullptr;
    explicit gpu_resources(size_t size) {
        check(cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking));
        check(cudaMalloc(reinterpret_cast<void **>(&data), size));
    }
    ~gpu_resources() {
        if (stream) { (void) cudaStreamSynchronize(stream); }
        if (data) { (void) cudaFree(data); }
        if (stream) { (void) cudaStreamDestroy(stream); }
    }
};

static uint64_t counter(const char * path, const char * key) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream input(line);
        std::string name;
        uint64_t value;
        if (input >> name >> value && name == key) { return value; }
    }
    throw std::runtime_error(std::string("missing resource counter: ") + key);
}

static json correctness() {
    const size_t capacity = 2 * slot_bytes + 4096;
    std::vector<uint8_t> source(capacity), expected(capacity), actual(capacity);
    for (size_t i = 0; i < source.size(); ++i) { source[i] = uint8_t(i * 131 + i / 257); }
    gpu_resources gpu(capacity);
    const std::vector<size_t> sizes{0, 1, 7, 511, 512, 513, slot_bytes - 1,
                                    slot_bytes, slot_bytes + 1, 2 * slot_bytes + 37};
    size_t cases = 0;
    for (bool fail : {false, true}) {
        ring staging(fail);
        if (staging.active == fail) { throw std::runtime_error("incorrect allocation/fallback state"); }
        for (size_t repeat = 0; repeat < 3; ++repeat) {
            for (size_t size : sizes) {
                const size_t from = 3 + repeat;
                const size_t to = 67 + repeat;
                std::fill(expected.begin(), expected.end(), uint8_t(0xa5));
                std::copy_n(source.data() + from, size, expected.data() + to);
                check(cudaMemsetAsync(gpu.data, 0xa5, capacity, gpu.stream));
                if (!staging.copy(gpu.stream, gpu.data + to, source.data() + from, size)) {
                    check(cudaMemcpyAsync(gpu.data + to, source.data() + from, size,
                                          cudaMemcpyHostToDevice, gpu.stream));
                }
                check(cudaStreamSynchronize(gpu.stream));
                check(cudaMemcpy(actual.data(), gpu.data, capacity, cudaMemcpyDeviceToHost));
                if (actual != expected) { throw std::runtime_error("copied bytes or destination guards differ"); }
                ++cases;
            }
        }
        if (!fail) {
            check(cudaStreamBeginCapture(gpu.stream, cudaStreamCaptureModeThreadLocal));
            const bool accepted = staging.copy(gpu.stream, gpu.data, source.data(), 512);
            cudaGraph_t graph = nullptr;
            check(cudaStreamEndCapture(gpu.stream, &graph));
            if (graph) { check(cudaGraphDestroy(graph)); }
            if (accepted) { throw std::runtime_error("ring accepted work inside graph capture"); }
        }
    }
    return {{"kind", "correctness"}, {"pass", true}, {"byte_guard_cases", cases},
            {"partial_allocation_fallback", true}, {"graph_capture_declined", true},
            {"pinned_budget_bytes", 2 * slot_bytes}};
}

struct mapping {
    int fd = -1;
    size_t size = 0;
    const uint8_t * data = nullptr;
    explicit mapping(const std::string & path) {
        fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) { throw std::runtime_error("cannot open GGUF backing file"); }
        struct stat info{};
        if (fstat(fd, &info) != 0 || info.st_size <= 0) { throw std::runtime_error("invalid GGUF backing file"); }
        size = size_t(info.st_size);
        const void * ptr = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) { throw std::runtime_error("cannot mmap GGUF backing file"); }
        data = static_cast<const uint8_t *>(ptr);
    }
    ~mapping() { if (data) { munmap(const_cast<uint8_t *>(data), size); } if (fd >= 0) { close(fd); } }
};

static json performance(const std::string & mode, const std::string & geometry, const std::string & path) {
    if (mode != "direct" && mode != "ring") { throw std::runtime_error("mode must be direct or ring"); }
    std::ifstream file(geometry);
    json input;
    file >> input;
    auto sizes = input.at("sizes_bytes").get<std::vector<size_t>>();
    if (sizes.empty() || input.at("schema_version") != 1) { throw std::runtime_error("invalid geometry"); }
    const size_t largest = *std::max_element(sizes.begin(), sizes.end());
    mapping source(path);
    const size_t first = 1ull << 20;
    if (source.size <= first + largest || !largest) { throw std::runtime_error("backing file too small"); }
    std::vector<size_t> offsets;
    size_t cursor = 0;
    uint64_t total = 0;
    for (size_t amount : sizes) {
        if (!amount || amount > largest) { throw std::runtime_error("invalid copy size"); }
        const size_t offset = first + cursor % (source.size - first - amount + 1);
        offsets.push_back(offset);
        cursor = offset - first + amount + 4096;
        total += amount;
    }
    if (total != input.at("sum_bytes").get<uint64_t>() || sizes.size() != input.at("n_ranges")) {
        throw std::runtime_error("geometry totals differ");
    }
    volatile uint8_t touched = 0;
    for (size_t i = 0; i < source.size; i += 4096) { touched = uint8_t(touched ^ source.data[i]); }
    gpu_resources gpu(largest);
    check(cudaMemcpyAsync(gpu.data, source.data + first, std::min<size_t>(largest, 4096),
                          cudaMemcpyHostToDevice, gpu.stream));
    check(cudaStreamSynchronize(gpu.stream));
    const uint64_t swap_before = counter("/proc/vmstat", "pswpout");
    struct rusage usage_before{}, usage_after{};
    getrusage(RUSAGE_SELF, &usage_before);
    const auto begin = clock_type::now();
    std::unique_ptr<ring> staging;
    if (mode == "ring") {
        staging = std::make_unique<ring>();
        if (!staging->active) { throw std::runtime_error("pinned allocation failed; no performance reading"); }
    }
    const auto initialized = clock_type::now();
    for (size_t i = 0; i < sizes.size(); ++i) {
        if (!staging || !staging->copy(gpu.stream, gpu.data, source.data + offsets[i], sizes[i])) {
            check(cudaMemcpyAsync(gpu.data, source.data + offsets[i], sizes[i], cudaMemcpyHostToDevice, gpu.stream));
        }
    }
    check(cudaStreamSynchronize(gpu.stream));
    const auto end = clock_type::now();
    getrusage(RUSAGE_SELF, &usage_after);
    const uint64_t swap_after = counter("/proc/vmstat", "pswpout");
    const uint64_t process_swap = counter("/proc/self/status", "VmSwap:");
    const bool valid = swap_before == swap_after && process_swap == 0 && usage_after.ru_majflt == usage_before.ru_majflt;
    return {{"kind", "transfer-feasibility"}, {"mode", mode}, {"valid", valid},
            {"ranges", sizes.size()}, {"bytes", total}, {"source_file_bytes", source.size},
            {"source_layout", "deterministic GGUF offsets; not captured expert offsets"},
            {"source_prefault_checksum", unsigned(touched)}, {"gpu_buffer_bytes", largest},
            {"pinned_bytes", staging ? 2 * slot_bytes : 0}, {"chunks", staging ? staging->chunks : sizes.size()},
            {"init_ms", milliseconds(begin, initialized)}, {"transfer_drain_ms", milliseconds(initialized, end)},
            {"total_ms", milliseconds(begin, end)}, {"process_swap_kib", process_swap},
            {"pswpout_before", swap_before}, {"pswpout_after", swap_after},
            {"major_faults_delta", usage_after.ru_majflt - usage_before.ru_majflt},
            {"minor_faults_delta", usage_after.ru_minflt - usage_before.ru_minflt},
            {"max_rss_kib", usage_after.ru_maxrss}};
}

int main(int argc, char ** argv) {
    try {
        rlimit limit{0, 0};
        setrlimit(RLIMIT_CORE, &limit);
        check(cudaSetDevice(0));
        json result;
        if (argc == 2 && std::string(argv[1]) == "--check") { result = correctness(); }
        else if (argc == 4) { result = performance(argv[1], argv[2], argv[3]); }
        else { throw std::runtime_error("usage: transfer-probe --check | direct|ring geometry.json backing.gguf"); }
        std::cout << result.dump(2) << std::endl;
        return result.value("valid", true) ? 0 : 2;
    } catch (const std::exception & error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
