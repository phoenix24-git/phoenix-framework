// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "core/vram_allocator.h"

#include <algorithm>

namespace phoenix {

void VramAllocator::init(std::uint32_t page_count, std::uint64_t page_size) {
    pages_.assign(page_count, FreeList{});
    page_size_ = page_size;
    used_ = 0;
    for (auto& page : pages_) {
        page.emplace(0, page_size);
    }
}

void VramAllocator::add_page() {
    pages_.emplace_back().emplace(0, page_size_);
}

std::optional<VramAllocation> VramAllocator::allocate(std::uint64_t size, std::uint64_t alignment) {
    if (size == 0 || size > page_size_ || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return std::nullopt;
    }
    // Best fit across all pages: smallest free block that satisfies the request.
    std::uint32_t best_page = 0;
    FreeList::iterator best;
    std::uint64_t best_waste = UINT64_MAX;
    bool found = false;
    for (std::uint32_t p = 0; p < pages_.size(); ++p) {
        for (auto it = pages_[p].begin(); it != pages_[p].end(); ++it) {
            const std::uint64_t aligned = (it->first + alignment - 1) & ~(alignment - 1);
            const std::uint64_t padding = aligned - it->first;
            if (it->second < padding || it->second - padding < size) {
                continue;
            }
            const std::uint64_t waste = it->second - size;
            if (waste < best_waste) {
                best_waste = waste;
                best_page = p;
                best = it;
                found = true;
            }
        }
    }
    if (!found) {
        return std::nullopt;
    }
    FreeList& list = pages_[best_page];
    const std::uint64_t block_offset = best->first;
    const std::uint64_t block_size = best->second;
    const std::uint64_t aligned = (block_offset + alignment - 1) & ~(alignment - 1);
    list.erase(best);
    if (aligned > block_offset) {
        list.emplace(block_offset, aligned - block_offset);  // leading padding stays free
    }
    const std::uint64_t tail = block_offset + block_size - (aligned + size);
    if (tail > 0) {
        list.emplace(aligned + size, tail);
    }
    used_ += size;
    return VramAllocation{best_page, aligned, size};
}

void VramAllocator::free(const VramAllocation& allocation) {
    if (allocation.page >= pages_.size() || allocation.size == 0) {
        return;
    }
    FreeList& list = pages_[allocation.page];
    std::uint64_t offset = allocation.offset;
    std::uint64_t size = allocation.size;
    auto next = list.lower_bound(offset);
    if (next != list.begin()) {
        auto prev = std::prev(next);
        if (prev->first + prev->second == offset) {
            offset = prev->first;
            size += prev->second;
            list.erase(prev);
        }
    }
    if (next != list.end() && offset + size == next->first) {
        size += next->second;
        list.erase(next);
    }
    list.emplace(offset, size);
    used_ -= std::min(used_, allocation.size);
}

std::uint64_t VramAllocator::largest_free_block() const {
    std::uint64_t best = 0;
    for (const auto& page : pages_) {
        for (const auto& [offset, size] : page) {
            best = std::max(best, size);
        }
    }
    return best;
}

}  // namespace phoenix
