// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace VideoCore {

// Copy is constructed as {staging offset, guest address, size}. The consumer may
// rewrite those offsets when it reserves staging. Preserve insertion order and
// flush before changing arenas; never join across a hole by copying its bytes.
template <typename Arena, typename Copy>
class DmaUploadBatch {
public:
    static constexpr std::uint64_t MaxBytes = 4 * 1024 * 1024;
    static constexpr std::size_t MaxCopies = 256;

    template <typename Flush>
    void Add(Arena next_arena, std::uint64_t address, std::uint64_t size, Flush&& flush) {
        if (!size) return;
        // A new source range previously started at aligned staging offset zero.
        // Preserve VkBufferCopy's four-byte source alignment across reservations.
        auto offset = (bytes + 3) & ~std::uint64_t{3};
        if (!copies.empty() && (next_arena != arena || copies.size() == MaxCopies ||
                               size > MaxBytes || offset > MaxBytes - size)) {
            Finish(flush);
            offset = 0;
        }
        arena = next_arena;
        if (copies.capacity() == 0) copies.reserve(MaxCopies);
        copies.emplace_back(offset, address, size);
        bytes = offset + size;
        // An individual pre-existing upload larger than the bound stays intact.
        if (bytes >= MaxBytes) Finish(flush);
    }

    template <typename Flush>
    void Finish(Flush&& flush) {
        if (copies.empty()) return;
        flush(arena, std::span<Copy>{copies}, bytes);
        copies.clear();
        bytes = 0;
    }

private:
    Arena arena{};
    std::uint64_t bytes = 0;
    std::vector<Copy> copies;
};

} // namespace VideoCore
