// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <limits>

namespace VideoCore::GeneralEpoch {

// Mirrors the SOTC PC GENERAL->GENERAL fence-epoch guard, but conservatively
// only permits *whole-image* same-stage, same-access repeats. Any change,
// partial transition, disabled mode or missing write re-arms the next barrier.
// The caller must guarantee that a guest fence advances the current epoch.
constexpr bool ShouldSkip(bool enabled, bool whole_image, bool same_general_layout,
                          bool same_access, bool same_stage, bool previous_write,
                          std::uint64_t fence_epoch, std::uint64_t& last_barrier_epoch) {
    if (!enabled || !whole_image || !same_general_layout ||
        !same_access || !same_stage || !previous_write) {
        last_barrier_epoch = std::numeric_limits<std::uint64_t>::max();
        return false;
    }
    const bool already_synchronized = last_barrier_epoch == fence_epoch;
    last_barrier_epoch = fence_epoch;
    return already_synchronized;
}

} // namespace VideoCore::GeneralEpoch
