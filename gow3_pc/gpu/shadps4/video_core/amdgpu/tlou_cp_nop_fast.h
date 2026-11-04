// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace AmdGpu::TLOU {
// Port of SotC PC's IsInertNop gate. In the TLOU stream we additionally
// disallow any skipping while guest markers are enabled; this keeps every
// debug-marker side effect and preserves patched flip packets.
constexpr bool InertNop(bool valid_packet, bool guest_markers_enabled,
                        bool has_payload, bool patched_flip) {
    return valid_packet && !guest_markers_enabled && (!has_payload || !patched_flip);
}
} // namespace AmdGpu::TLOU
