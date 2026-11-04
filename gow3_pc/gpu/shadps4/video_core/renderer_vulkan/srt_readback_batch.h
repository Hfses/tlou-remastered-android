// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace Vulkan {
using SrtReadbackRange = std::pair<std::uintptr_t, std::uint64_t>;
inline constexpr std::uint64_t SrtBatchBytes = 64 * 1024;
inline constexpr std::size_t SrtBatchRanges = 16;

inline bool SrtRangesOverlap(SrtReadbackRange a, SrtReadbackRange b) {
    if (!a.second || !b.second) return false;
    // Subtraction avoids overflowing end addresses.
    return a.first <= b.first ? b.first - a.first < a.second
                             : a.first - b.first < b.second;
}

// Only siblings: their pointer source is already synchronized by the caller.
// Never speculate through a child pointer, merge holes, or bypass queued CPU copies.
// Resolve returns 0 for unmapped/invalid tables; Dirty only observes state.
template <typename Node, std::size_t Extent, typename Resolve, typename Dirty>
std::vector<SrtReadbackRange> PlanSrtSiblingBatch(std::span<Node, Extent> nodes,
        std::size_t first, std::uintptr_t table, std::uintptr_t parent_table,
        bool no_pending_copies, Resolve&& resolve, Dirty&& dirty, std::uint64_t page_bytes = 4096) {
    std::vector<SrtReadbackRange> ranges;
    if (first >= nodes.size() || !table || !nodes[first].extent) return ranges;
    const auto valid = [](SrtReadbackRange r) {
        return r.second && r.first <= std::numeric_limits<std::uintptr_t>::max() - r.second;
    };
    const SrtReadbackRange initial{table, nodes[first].extent};
    ranges.push_back(initial); // unchanged single-table path is always the fallback
    if (!no_pending_copies || nodes[first].dynamic || !valid(initial) ||
        initial.second > SrtBatchBytes) return ranges;
    if (!page_bytes || page_bytes > SrtBatchBytes || (page_bytes & (page_bytes - 1))) return ranges;
    const auto tracked = [page_bytes](SrtReadbackRange r) -> SrtReadbackRange {
        const auto mask = page_bytes - 1;
        const auto begin = r.first & ~mask;
        const auto last = (r.first + r.second - 1) & ~mask;
        return {begin, last - begin + page_bytes};
    };
    const auto tracked_initial = tracked(initial);
    if (tracked_initial.second > SrtBatchBytes) return ranges;
    const auto parent = nodes[first].parent;
    std::uint64_t parent_bytes = 0;
    if (parent >= 0) {
        if (!parent_table) return ranges;
        for (const auto& node : nodes) if (node.parent == parent && !node.dynamic)
            parent_bytes = std::max(parent_bytes, std::uint64_t(node.offset) + 8);
        const SrtReadbackRange source{parent_table, parent_bytes};
        if (!valid(source) || SrtRangesOverlap(tracked_initial, tracked(source))) return ranges;
    }
    std::uint64_t bytes = tracked_initial.second;
    for (std::size_t i = first + 1; i < nodes.size() && ranges.size() < SrtBatchRanges; ++i) {
        const auto& node = nodes[i];
        // A descendant/intervening branch could alias the parent pointer source.
        // Root pointers are immutable user data; other siblings must be adjacent.
        if (parent >= 0 && node.parent != parent) break;
        if (node.dynamic || node.parent != parent || !node.extent ||
            node.extent > SrtBatchBytes) continue;
        const SrtReadbackRange candidate{resolve(i), node.extent};
        if (!candidate.first || !valid(candidate)) continue;
        const auto tracked_candidate = tracked(candidate);
        if (tracked_candidate.second > SrtBatchBytes - bytes ||
            (parent >= 0 && SrtRangesOverlap(tracked_candidate, tracked({parent_table, parent_bytes}))) ||
            std::any_of(ranges.begin(), ranges.end(), [&](auto r) { return SrtRangesOverlap(tracked(r), tracked_candidate); }) ||
            !dirty(candidate.first, candidate.second)) continue;
        ranges.push_back(candidate);
        bytes += tracked_candidate.second;
    }
    return ranges;
}
} // namespace Vulkan
