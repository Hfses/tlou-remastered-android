// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

// The original TLOU compute result bindings use 48-byte records: 89 records
// (4272 bytes) in slot 6, or 135 records (6480 bytes) in slot 5.
// The shader hash changes when the compiled program changes. Matching the
// complete buffer shape keeps the Android fallback narrow and avoids a
// frame-wide or all-buffer readback when the original hashes differ.
// Only use this predicate on compute storage buffers marked GPU-written.
namespace TlouPhysicsReadback {
constexpr bool Shape(std::uint32_t slot, std::uint32_t stride,
                     std::uint64_t records, std::uint64_t bytes) noexcept {
    if (stride != 48 || records == 0 || bytes != records * 48) return false;
    return (slot == 6 && records == 89 && bytes == 4272) ||
           (slot == 5 && records == 135 && bytes == 6480);
}
} // namespace TlouPhysicsReadback
