// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// SotC-derived readback granularity; only applicable to Precise mode.
#include <algorithm>
#include <cstdint>
#include <utility>
namespace TlouPreciseWindow {
inline constexpr std::uint64_t OriginalBytes=512*1024;
inline constexpr std::uint64_t NarrowBytes=64*1024;
constexpr std::uint64_t Width(bool enabled,bool precise) {
    return enabled&&precise?NarrowBytes:OriginalBytes;
}
constexpr std::pair<std::uint64_t,std::uint64_t> Window(
    std::uint64_t address,std::uint64_t length,
    std::uint64_t arena_start,std::uint64_t arena_size,std::uint64_t width) {
    const auto arena_end=arena_start+arena_size;
    const auto start=std::max((address/width)*width,arena_start);
    const auto end=std::min(std::max(start+width,address+length),arena_end);
    return {start,end};
}
} // namespace TlouPreciseWindow
