// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Compute-queue-only use of the existing fully guarded FewerDrains wait path.
// Do not enable graphics FewerDrains: it has a documented readback race.
namespace TlouComputeDrain {
enum class PacketKind { Other, NestedBuffer, MemoryWait };
constexpr bool SkipPreDrain(PacketKind kind, bool compute_fast, bool fewer_drains,
                            bool skip_nested_only) noexcept {
    switch (kind) {
    case PacketKind::NestedBuffer:
        // The nested packets are checked one by one in ProcessCompute.
        return compute_fast || fewer_drains || skip_nested_only;
    case PacketKind::MemoryWait:
        // ProcessCompute's existing pending-fence, deferred-label, pending-write
        // and queue-yield checks handle *unmet* waits instead.
        return compute_fast || fewer_drains;
    default: return false;
    }
}
constexpr bool UseGuardedWait(bool compute_fast, bool fewer_drains) noexcept {
    return compute_fast || fewer_drains;
}
} // namespace TlouComputeDrain
