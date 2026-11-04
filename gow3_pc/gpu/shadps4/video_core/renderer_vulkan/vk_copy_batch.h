// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <limits>
#include <span>

namespace Vulkan::CopyBatch {

// A single vkCmdCopyBuffer with multiple regions may not rely on one region
// overwriting bytes that another region copies. Limit batching to disjoint
// destinations (and a different source buffer, checked by the caller).
// Overflowed or empty ranges cannot be merged.
template <typename Region>
constexpr bool DestinationsDisjoint(std::span<const Region> previous, const Region& next) {
    constexpr auto Limit = std::numeric_limits<std::uint64_t>::max();
    if (!next.size || next.dstOffset > Limit - next.size) return false;
    const auto next_end = next.dstOffset + next.size;
    for (const auto& existing : previous) {
        if (!existing.size || existing.dstOffset > Limit - existing.size) return false;
        const auto end = existing.dstOffset + existing.size;
        if (existing.dstOffset < next_end && next.dstOffset < end) return false;
    }
    return true;
}

} // namespace Vulkan::CopyBatch
