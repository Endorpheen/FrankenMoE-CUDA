#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace llama_mmap_expert_pin {
struct span {
    uintptr_t begin;
    uintptr_t end;
};

inline std::vector<span> merge_pages(std::vector<span> ranges, uintptr_t page) {
    if (!page || (page & (page - 1))) {
        throw std::runtime_error("invalid page size");
    }
    for (auto & range : ranges) {
        if (range.end <= range.begin || range.end > UINTPTR_MAX - (page - 1)) {
            throw std::runtime_error("invalid weight interval");
        }
        range.begin &= ~(page - 1);
        range.end = (range.end + page - 1) & ~(page - 1);
    }
    std::sort(ranges.begin(), ranges.end(), [](const span & a, const span & b) {
        return a.begin < b.begin;
    });
    std::vector<span> result;
    result.reserve(ranges.size());
    for (const auto & range : ranges) {
        if (!result.empty() && range.begin <= result.back().end) {
            result.back().end = std::max(result.back().end, range.end);
        } else {
            result.push_back(range);
        }
    }
    return result;
}

inline bool fits_budget(uint64_t bytes, uint64_t total, uint64_t available) {
    constexpr uint64_t headroom = UINT64_C(8) << 30;
    return bytes > 0 && bytes <= total / 2 && available >= bytes &&
           available - bytes >= headroom;
}

// Storage is allocated before registration. Cleanup never allocates and owns
// only successful calls, including rollback after a later range fails.
struct owner {
    using register_fn = bool (*)(void *, size_t);
    using unregister_fn = void (*)(void *);
    std::vector<span> ranges;
    unregister_fn unregister = nullptr;
    size_t owned = 0;

    owner() = default;
    owner(const owner &) = delete;
    owner & operator=(const owner &) = delete;
    ~owner() { release(); }

    void release() noexcept {
        while (owned) {
            unregister(reinterpret_cast<void *>(ranges[--owned].begin));
        }
    }

    bool acquire(std::vector<span> input, register_fn reg, unregister_fn unreg) {
        if (owned || !reg || !unreg) { return false; }
        ranges = std::move(input);
        unregister = unreg;
        for (const auto & range : ranges) {
            if (!reg(reinterpret_cast<void *>(range.begin), range.end - range.begin)) {
                release();
                return false;
            }
            ++owned;
        }
        return owned > 0;
    }
};
} // namespace llama_mmap_expert_pin
