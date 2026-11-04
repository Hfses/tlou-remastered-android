// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Index only overlapping blocks of a large GPU write. An oversized write
// retains a coarse 1 MiB fallback, so no producer wait can be skipped.
#include <algorithm>
#include <cstdint>
#include <limits>
namespace VideoCore::TlouWriteTick {
constexpr std::uint64_t FineBytes=64*1024, WindowBytes=1024*1024;
constexpr bool FineIndexable(std::uint64_t n) {return n>FineBytes && n<=WindowBytes;}
template<class F> void ForEachBlock(std::uint64_t address,std::uint64_t size,std::uint64_t bytes,F&& fn) {
    if(!size || !bytes || address>std::numeric_limits<std::uint64_t>::max()-size+1) return;
    for(auto i=address/bytes, last=(address+size-1)/bytes; i<=last; ++i) fn(i);
}
constexpr std::uint64_t ReadTick(std::uint64_t page,std::uint64_t coarse,std::uint64_t fine,
                                  std::uint64_t huge,bool enabled) {
    return std::max(page, enabled ? std::max(fine,huge) : coarse);
}
} // namespace VideoCore::TlouWriteTick
