// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace phoenix {

/// A sub-allocation inside one VRAM page.
struct VramAllocation {
    std::uint32_t page = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

/// CPU-side bookkeeping for the console VRAM pool.
///
/// The pool is a fixed set of equally sized pages (each one GPU buffer).
/// Allocations never span pages. Free ranges are kept per page in an ordered
/// map and coalesced on free, giving O(log n) allocate/free with no GPU calls,
/// which keeps it fully unit-testable.
class VramAllocator {
  public:
    void init(std::uint32_t page_count, std::uint64_t page_size);
    /// Appends one fully free page (used when VRAM is reserved on demand).
    void add_page();

    std::optional<VramAllocation> allocate(std::uint64_t size, std::uint64_t alignment = 256);
    void free(const VramAllocation& allocation);

    std::uint32_t page_count() const { return static_cast<std::uint32_t>(pages_.size()); }
    std::uint64_t page_size() const { return page_size_; }
    std::uint64_t capacity() const { return page_size_ * pages_.size(); }
    std::uint64_t used() const { return used_; }
    /// Largest contiguous free range in any page (fragmentation indicator).
    std::uint64_t largest_free_block() const;

  private:
    using FreeList = std::map<std::uint64_t, std::uint64_t>;  // offset -> size
    std::vector<FreeList> pages_;
    std::uint64_t page_size_ = 0;
    std::uint64_t used_ = 0;
};

}  // namespace phoenix
